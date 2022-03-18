/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2022 Barcelona Supercomputing Center (BSC)
*/

#ifndef TASKITER_GRAPH_HPP
#define TASKITER_GRAPH_HPP

#include <iostream>
#include <memory>
#include <numeric>

#include <boost/variant.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_utility.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/transitive_reduction.hpp>

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

	bool operator<(TaskiterGraphEdge const &other) const
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
	typedef boost::property<boost::vertex_name_t, TaskiterGraphNode> VertexProperty;
	typedef boost::property<boost::edge_name_t, bool> EdgeProperty;

	typedef boost::adjacency_list<
		boost::vecS,		// OutEdgeList
		boost::vecS,		// VertexList
		boost::directedS,	// Directed
		VertexProperty,		// VertexProperties
		EdgeProperty,		// EdgeProperties
		boost::no_property, // GraphProperties
		boost::listS		// EdgeList
		>
		graph_t;

	typedef graph_t::vertex_descriptor graph_vertex_t;

private:
	Container::vector<TaskMetadata *> _tasks;
	Container::vector<ReductionInfo *> _reductions;
	Container::vector<TaskiterGraphEdge> _edges;
	Container::unordered_map<access_address_t, TaskiterGraphAccessChain> _bottomMap;

	graph_t _graph;
	Container::unordered_map<TaskiterGraphNode, graph_vertex_t> _tasksToVertices;

	bool _processed;

	inline void createEdges(TaskiterGraphNode node, Container::vector<TaskiterGraphNode> &chain)
	{
		EdgeProperty props(false);

		for (TaskiterGraphNode t : chain) {
			boost::add_edge(_tasksToVertices[t], _tasksToVertices[node], props, _graph);
		}
	}

	inline void closeLoopWithControl(TaskMetadata *controlTask)
	{
		controlTask->increasePredecessors();
		VisitorSetDegree visitor;

		EdgeProperty propsTrue(true);
		EdgeProperty propsFalse(false);
		VertexProperty propTask(controlTask);

		const graph_vertex_t controlTaskVertex = boost::add_vertex(propTask, _graph);
		_tasksToVertices.emplace(std::make_pair(controlTask, controlTaskVertex));
		_tasks.push_back(controlTask);

		// In theory, we don't have to worry for reductions, as they depend on all its
		// participants to be combined, and then into the closing task to release the control

		// Register deps from every task into the control task, and from the control task to every other task
		for (TaskMetadata *task : _tasks) {
			if (task == controlTask)
				continue;

			boost::add_edge(_tasksToVertices[task], controlTaskVertex, propsFalse, _graph);
			boost::add_edge(controlTaskVertex, _tasksToVertices[task], propsTrue, _graph);
			// _edges.emplace(task, controlTask, false);
			// _edges.emplace(controlTask, task, true);
			controlTask->incrementOriginalPredecessorCount();

			boost::apply_visitor(visitor, TaskiterGraphNode(task));
		}
	}

	inline void closeDependencyLoop()
	{
		// Close every dependency chain by simulating that we are registering the first accesses again
		VisitorSetDegreeCross visitor;

		EdgeProperty prop(true);

		// TODO Is this wrong for a IN -> OUT -> IN chain for the (chain._firstChainType != chain._lastChainType) condition?
		for (std::pair<const access_address_t, TaskiterGraphAccessChain> &it : _bottomMap) {
			TaskiterGraphAccessChain &chain = it.second;

			if (chain._firstChainType != chain._lastChainType || chain._firstChainType == WRITE_ACCESS_TYPE || chain._firstChainType == READWRITE_ACCESS_TYPE || chain._firstChainType == COMMUTATIVE_ACCESS_TYPE) {
				for (TaskiterGraphNode task : chain._firstChain) {
					for (TaskiterGraphNode from : *(chain._lastChain)) {
						boost::add_edge(_tasksToVertices[from], _tasksToVertices[task], prop, _graph);
						// _edges.emplace(from, task, true);
						boost::apply_visitor(visitor, task);
					}
				}
			}
		}
	}

	inline void addTaskToChain(TaskMetadata *task, DataAccessType type, TaskiterGraphAccessChain &chain)
	{
		chain._lastChain->push_back(task);
		if (chain._inFirst) {
			chain._firstChainType = type;
			chain._firstChain.push_back(task);
		}
	}

	inline void closeReductionChain(TaskMetadata *task, TaskiterGraphAccessChain &chain)
	{
		// Reduction combination depends on last chain
		createEdges(chain._reductionInfo, *(chain._lastChain));
		// And on all reduction accesses
		createEdges(chain._reductionInfo, chain._reductionChain);

		EdgeProperty prop(true);

		// What's more, all reduction accesses must depend on last iteration's combination
		for (TaskiterGraphNode &n : chain._reductionChain) {
			// boost::add_edge(_tasksToVertices[chain._reductionInfo], _tasksToVertices[n], prop, _graph);
			_edges.emplace_back(chain._reductionInfo, n, true);
		}

		chain._reductionChain.clear();
		chain._lastChain->clear();
		chain._lastChainType = REDUCTION_ACCESS_TYPE;
		chain._lastChain->push_back(chain._reductionInfo);
		chain._reductionInfo = nullptr;
	}

	class VisitorSetDegreeCross : public boost::static_visitor<> {
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

	class VisitorSetDegree : public boost::static_visitor<> {
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

	class VisitorApplySuccessor : public boost::static_visitor<> {
		TaskiterGraph *_graph;
		std::function<void(TaskMetadata *)> _satisfyTask;
		bool _crossIterationBoundary;

	public:
		VisitorApplySuccessor(
			TaskiterGraph *graph,
			std::function<void(TaskMetadata *)> satisfyTask,
			bool crossIterationBoundary) :
			_graph(graph),
			_satisfyTask(satisfyTask),
			_crossIterationBoundary(crossIterationBoundary)
		{
		}

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
		std::function<void(TaskMetadata *)> satisfyTask)
	{
		graph_vertex_t vertex = _tasksToVertices[node];

		VisitorApplySuccessor visitor(this, satisfyTask, crossIterationBoundary);

		graph_t::out_edge_iterator ei, eend;
		boost::property_map<graph_t, boost::edge_name_t>::type edgemap = boost::get(boost::edge_name_t(), _graph);
		boost::property_map<graph_t, boost::vertex_name_t>::type nodemap = boost::get(boost::vertex_name_t(), _graph);

		// Travel through adjacent vertices
		for (boost::tie(ei, eend) = boost::out_edges(vertex, _graph); ei != eend; ++ei) {
			graph_t::edge_descriptor e = *ei;
			graph_vertex_t to = boost::target(e, _graph);
			TaskiterGraphNode node = boost::get(nodemap, to);
			bool edgeCrossIteration = boost::get(edgemap, e);

			if (crossIterationBoundary || !edgeCrossIteration)
				boost::apply_visitor(visitor, node);
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
		graph_t::edge_iterator ei, eend;
		boost::property_map<graph_t, boost::edge_name_t>::type edgemap = boost::get(boost::edge_name_t(), _graph);
		boost::property_map<graph_t, boost::vertex_name_t>::type nodemap = boost::get(boost::vertex_name_t(), _graph);

		for (boost::tie(ei, eend) = boost::edges(_graph); ei != eend; ++ei) {
			graph_t::edge_descriptor e = *ei;
			graph_vertex_t to = boost::target(e, _graph);
			TaskiterGraphNode node = boost::get(nodemap, to);
			bool crossIterationBoundary = boost::get(edgemap, e);

			if (!crossIterationBoundary)
				boost::apply_visitor(visitor, node);
			else
				boost::apply_visitor(crossIterationVisitor, node);
		}

		if (controlTask == nullptr) {
			// Close the dependency loop normally
			closeDependencyLoop();
		} else {
			// Insert a control dependency
			closeLoopWithControl(controlTask);
		}

		// Now, decrease predecessors for every task
		for (TaskMetadata *t : _tasks) {
			if (t->decreasePredecessors()) {
				nosv_submit(t->getTaskHandle(), NOSV_SUBMIT_UNLOCKED);
			}
		}
	}

	void process()
	{
		graph_t processedGraph;

		// Try to reduce the dependency graph
		// This *must* be done on a DAG, because it uses its topological sorting, so
		// we prevent cycles up to this point
		Container::map<graph_vertex_t, graph_vertex_t> gToTr;
		Container::vector<size_t> vertexMap(boost::num_vertices(_graph));
		std::iota(vertexMap.begin(), vertexMap.end(), (size_t)0);

		boost::transitive_reduction(_graph, processedGraph, boost::make_assoc_property_map(gToTr), vertexMap.data());

		// To print the graphs before and after, uncomment this code.
		// {
		// 	std::ofstream dot("g.dot");
		// 	boost::write_graphviz(dot, _graph);
		// }
		// {
		// 	std::ofstream dot("tr.dot");
		// 	boost::write_graphviz(dot, processedGraph);
		// }

		// Annoyingly, the transitive reduction doesn't transfer the properties
		boost::property_map<graph_t, boost::vertex_name_t>::type nodemapOriginal = boost::get(boost::vertex_name_t(), _graph);
		boost::property_map<graph_t, boost::vertex_name_t>::type nodemapProcessed = boost::get(boost::vertex_name_t(), processedGraph);
		boost::property_map<graph_t, boost::edge_name_t>::type edgemapProcessed = boost::get(boost::edge_name_t(), processedGraph);

		graph_t::vertex_iterator vi, vend;
		for (boost::tie(vi, vend) = boost::vertices(_graph); vi != vend; vi++) {
			graph_vertex_t originalVertex = *vi;
			TaskiterGraphNode node = boost::get(nodemapOriginal, originalVertex);
			boost::put(nodemapProcessed, gToTr[originalVertex], node);
		}

		for (auto& it : _tasksToVertices) {
			it.second = gToTr[it.second];
		}

		graph_t::edge_iterator ei, eend;
		for (boost::tie(ei, eend) = boost::edges(processedGraph); ei != eend; ei++) {
			graph_t::edge_descriptor e = *ei;
			boost::put(edgemapProcessed, e, false);
		}

		_graph = processedGraph;

		EdgeProperty prop(true);
		// Now, incorporate delayed edges for closing reductions
		for (TaskiterGraphEdge &edge : _edges) {
			boost::add_edge(_tasksToVertices[edge._from], _tasksToVertices[edge._to], prop, _graph);
		}

		_processed = true;
	}

	inline bool isProcessed() const
	{
		return _processed;
	}

	void addTask(TaskMetadata *task)
	{
		_tasks.push_back(task);

		VertexProperty prop(task);
		graph_vertex_t vertex = boost::add_vertex(prop, _graph);
		_tasksToVertices.emplace(std::make_pair(task, vertex));
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
				const graph_vertex_t reductionVertex = boost::add_vertex(VertexProperty(reductionInfo), _graph);
				_tasksToVertices.emplace(std::make_pair(reductionInfo, reductionVertex));
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
