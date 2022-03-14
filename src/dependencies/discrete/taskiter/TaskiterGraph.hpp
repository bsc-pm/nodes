/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2022 Barcelona Supercomputing Center (BSC)
*/

#ifndef TASKITER_GRAPH_HPP
#define TASKITER_GRAPH_HPP

#include <iostream>
#include <memory>

#include <boost/variant.hpp>

#include <nosv.h>

#include "dependencies/DataAccessType.hpp"
#include "common/Containers.hpp"
#include "tasks/TaskMetadata.hpp"
#include "dependencies/discrete/CPUDependencyData.hpp"
#include "dependencies/discrete/DataAccess.hpp"
#include "dependencies/discrete/ReductionInfo.hpp"

// A node of the TaskiterGraph may contain either a Task or a ReductionInfo
typedef boost::variant<TaskMetadata *, ReductionInfo *> TaskiterGraphNode;

struct TaskiterGraphEdge {
	TaskiterGraphNode _from;
	TaskiterGraphNode _to;
	bool _crossIterationBoundary;

	TaskiterGraphEdge(TaskiterGraphNode from, TaskiterGraphNode to, bool crossIterationBoundary) :
		_from(from),
		_to(to),
		_crossIterationBoundary(crossIterationBoundary)
	{
	}

	bool operator < (TaskiterGraphEdge const &other) const
	{
		if (_from != other._from)
			return (_from < other._from);

		return (_to < other._to);
	}
};

struct TaskiterGraphAccessChain {
	typedef Container::vector<TaskiterGraphNode> access_chain_t;
	std::unique_ptr<access_chain_t> _lastChain;
	std::unique_ptr<access_chain_t> _prevChain;
	access_chain_t _firstChain;
	access_chain_t _reductionChain;

	ReductionInfo *_reductionInfo;

	DataAccessType _lastChainType;
	DataAccessType _prevChainType;
	DataAccessType _firstChainType;

	bool _inFirst;

	TaskiterGraphAccessChain() :
		_reductionInfo(nullptr),
		_lastChainType(WRITE_ACCESS_TYPE),
		_prevChainType(WRITE_ACCESS_TYPE),
		_firstChainType(WRITE_ACCESS_TYPE),
		_inFirst(true)
	{
		_lastChain = std::make_unique<Container::vector<TaskiterGraphNode>>();
		_prevChain = std::make_unique<Container::vector<TaskiterGraphNode>>();
	}
};

// This class represents a set of tasks that contain dependencies between them
class TaskiterGraph {
public:
	typedef void *access_address_t;

private:
	Container::vector<TaskMetadata *> _tasks;
	Container::vector<ReductionInfo *> _reductions;
	Container::set<TaskiterGraphEdge> _edges;
	Container::unordered_map<access_address_t, TaskiterGraphAccessChain> _bottomMap;
	bool _processed;

	inline void createEdges(TaskiterGraphNode node, Container::vector<TaskiterGraphNode> &chain, bool crossIterationChain = false)
	{
		for (TaskiterGraphNode t : chain)
			_edges.emplace(t, node, crossIterationChain);
	}

	inline void closeLoopWithControl(TaskMetadata *controlTask)
	{
		controlTask->increasePredecessors();
		VisitorSetDegree visitor;

		// In theory, we don't have to worry for reductions, as they depend on all its
		// participants to be combined, and then into the closing task to release the control

		// Register deps from every task into the control task, and from the control task to every other task
		for (TaskMetadata *task : _tasks) {
			_edges.emplace(task, controlTask, false);
			_edges.emplace(controlTask, task, true);
			controlTask->incrementOriginalPredecessorCount();

			boost::apply_visitor(visitor, TaskiterGraphNode(task));
		}

		_tasks.push_back(controlTask);
	}

	inline void closeDependencyLoop()
	{
		// Close every dependency chain by simulating that we are registering the first accesses again
		VisitorSetDegreeCross visitor;

		// TODO Is this wrong for a IN -> OUT -> IN chain for the (chain._firstChainType != chain._lastChainType) condition?
		for (std::pair<const access_address_t, TaskiterGraphAccessChain> &it : _bottomMap) {
			TaskiterGraphAccessChain &chain = it.second;

			if (chain._firstChainType != chain._lastChainType ||
				chain._firstChainType == WRITE_ACCESS_TYPE ||
				chain._firstChainType == READWRITE_ACCESS_TYPE ||
				chain._firstChainType == COMMUTATIVE_ACCESS_TYPE) {

				for (TaskiterGraphNode task : chain._firstChain) {
					for (TaskiterGraphNode from : *(chain._lastChain)) {
						_edges.emplace(from, task, true);
						boost::apply_visitor(visitor, task);
					}
				}
			}
		}
	}

	inline void addTaskToChain(TaskMetadata *task, DataAccessType type, TaskiterGraphAccessChain &chain) {
		chain._lastChain->push_back(task);
		if (chain._inFirst) {
			chain._firstChainType = type;
			chain._firstChain.push_back(task);
		}
	}

	inline void closeReductionChain(TaskMetadata *task, TaskiterGraphAccessChain &chain) {
		// Reduction combination depends on last chain
		createEdges(chain._reductionInfo, *(chain._lastChain));
		// And on all reduction accesses
		createEdges(chain._reductionInfo, chain._reductionChain);

		// What's more, all reduction accesses must depend on last iteration's combination
		for (TaskiterGraphNode &n : chain._reductionChain) {
			_edges.emplace(chain._reductionInfo, n, true);
		}

		chain._reductionChain.clear();
		chain._lastChain->clear();
		chain._lastChainType = REDUCTION_ACCESS_TYPE;
		chain._lastChain->push_back(chain._reductionInfo);
		chain._reductionInfo = nullptr;
	}

	class VisitorSetDegreeCross : public boost::static_visitor<>
	{
	public:
		void operator()(TaskMetadata *t) const
		{
			t->incrementOriginalPredecessorCount();
		}

		void operator()(ReductionInfo *r) const
		{
			r->incrementOriginalRegisteredAccesses();
		}
	};

	class VisitorSetDegree : public boost::static_visitor<>
	{
	public:
		void operator()(TaskMetadata *t) const
		{
			t->increasePredecessors();
			t->incrementOriginalPredecessorCount();
		}

		void operator()(ReductionInfo *r) const
		{
			r->incrementRegisteredAccesses();
			r->incrementOriginalRegisteredAccesses();
		}
	};

	class VisitorApplySuccessor : public boost::static_visitor<>
	{
		TaskiterGraph *_graph;
		std::function<void(TaskMetadata *)> _satisfyTask;
		bool _crossIterationBoundary;
	public:
		VisitorApplySuccessor(
			TaskiterGraph *graph,
			std::function<void(TaskMetadata *)> satisfyTask,
			bool crossIterationBoundary
		) :
			_graph(graph),
			_satisfyTask(satisfyTask),
			_crossIterationBoundary(crossIterationBoundary)
		{ }

		void operator()(TaskMetadata *t) const
		{
			_satisfyTask(t);
		}

		void operator()(ReductionInfo *r) const
		{
			if (r->incrementUnregisteredAccesses()) {
				r->combine();
				r->reinitialize();

				// Release next accesses
				_graph->applySuccessors(r, _crossIterationBoundary, _satisfyTask);
			}
		}
	};

public:
	TaskiterGraph() :
		_processed(false)
	{
	}

	inline void applySuccessors(
		TaskiterGraphNode node,
		bool crossIterationBoundary,
		std::function<void(TaskMetadata *)> satisfyTask
	)
	{
		TaskiterGraphEdge search(node, (TaskMetadata *)nullptr, false);
		Container::set<TaskiterGraphEdge>::iterator it = _edges.lower_bound(search);

		VisitorApplySuccessor visitor(this, satisfyTask, crossIterationBoundary);
		while (it != _edges.end() && it->_from == node) {
			if (crossIterationBoundary || !it->_crossIterationBoundary)
				boost::apply_visitor(visitor, it->_to);
			++it;
		}
	}

	void setTaskDegree(TaskMetadata *controlTask)
	{
		// First, increase predecessors for every task
		for (TaskMetadata *t : _tasks)
			t->increasePredecessors();

		VisitorSetDegree visitor;
		VisitorSetDegreeCross crossIterationVisitor;
		// Now, increment for each edge
		for (const TaskiterGraphEdge &edge : _edges) {
			if (!edge._crossIterationBoundary)
				boost::apply_visitor(visitor, edge._to);
			else
				boost::apply_visitor(crossIterationVisitor, edge._to);
		}

		if (controlTask == nullptr) {
			// Close the dependency loop normally
			closeDependencyLoop();
		} else {
			// Insert a control dependency
			closeLoopWithControl(controlTask);
		}

		// for (ReductionInfo *r: _reductions) {
		// 	r->reinitialize();
		// }

		// Now, decrease predecessors for every task
		for (TaskMetadata *t : _tasks) {
			if (t->decreasePredecessors()) {
				nosv_submit(t->getTaskHandle(), NOSV_SUBMIT_UNLOCKED);
			}
		}
	}

	void process()
	{
		_processed = true;
	}

	inline bool isProcessed() const
	{
		return _processed;
	}

	void addTask(TaskMetadata *task)
	{
		_tasks.push_back(task);
	}

	void addTaskAccess(TaskMetadata *task, DataAccess *access)
	{
		const access_address_t address = access->getAccessRegion().getStartAddress();
		const DataAccessType type = access->getType();

		// Find the existing access chain or create a new one and return it
		TaskiterGraphAccessChain &chain = _bottomMap.emplace(std::piecewise_construct, std::forward_as_tuple(address), std::forward_as_tuple()).first->second;

		// Maybe we can stick with only the "set", would significantly ease up everything

		// We model dependencies similarly to some OpenMP runtimes, which have a simplistic view of the "last task set"
		// Note that this causes a O(n^2) dependency order when following a set of ins with a set of concurrents
		if (type == READ_ACCESS_TYPE || type == CONCURRENT_ACCESS_TYPE) {
			if (chain._reductionInfo) {
				closeReductionChain(task, chain);
				// Close the reduction
			}

			if (type != chain._lastChainType) {
				// chain._prevChainType = chain._lastChainType;
				std::swap(chain._lastChain, chain._prevChain);
				chain._lastChain->clear();
				chain._lastChainType = type;

				if (!chain._prevChain->empty()) {
					chain._inFirst = false;
				}
			}

			addTaskToChain(task, type, chain);
			createEdges(task, *(chain._prevChain));
		} else if (type == WRITE_ACCESS_TYPE || type == READWRITE_ACCESS_TYPE || type == COMMUTATIVE_ACCESS_TYPE) {
			if (chain._reductionInfo) {
				closeReductionChain(task, chain);
				// Close the reduction
			}

			// TODO: Handle commutative well
			if (!chain._lastChain->empty()) {
				createEdges(task, *(chain._lastChain));
				chain._lastChainType = WRITE_ACCESS_TYPE;
				chain._lastChain->clear();
				chain._inFirst = false;
			}

			addTaskToChain(task, type, chain);
			chain._inFirst = false;
		} else if (type == REDUCTION_ACCESS_TYPE) {
			ReductionInfo *reductionInfo = access->getReductionInfo();
			assert(reductionInfo != nullptr);

			chain._reductionChain.push_back(task);
			if (chain._reductionInfo == nullptr) {
				chain._reductionInfo = reductionInfo;
				_reductions.push_back(reductionInfo);
			}

			assert(chain._reductionInfo == reductionInfo);

			// TODO Figure out what to do when reductions are first
		}
	}

	inline size_t getTasks() const
	{
		return _tasks.size();
	}

	inline std::vector<TaskMetadata *> const &getTaskVector() const
	{
		return _tasks;
	}

	~TaskiterGraph()
	{
		// std::cout << "Destroy graph" << std::endl;
	}
};

#endif // TASKITER_GRAPH_HPP
