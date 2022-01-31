/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2022 Barcelona Supercomputing Center (BSC)
*/

#ifndef TASKITER_GRAPH_HPP
#define TASKITER_GRAPH_HPP

#include <iostream>

#include <nosv.h>

#include "dependencies/DataAccessType.hpp"
#include "common/Containers.hpp"
#include "tasks/TaskMetadata.hpp"
#include "dependencies/discrete/CPUDependencyData.hpp"

struct TaskiterGraphEdge {
	TaskMetadata *_from;
	TaskMetadata *_to;
	bool _crossIterationBoundary;

	TaskiterGraphEdge(TaskMetadata *from, TaskMetadata *to, bool crossIterationBoundary) :
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
	Container::vector<TaskMetadata *> *_lastChain;
	Container::vector<TaskMetadata *> *_prevChain;
	Container::vector<TaskMetadata *> _firstChain;

	DataAccessType _lastChainType;
	DataAccessType _prevChainType;
	DataAccessType _firstChainType;

	bool _inFirst;

	TaskiterGraphAccessChain() :
		_lastChainType(WRITE_ACCESS_TYPE),
		_prevChainType(WRITE_ACCESS_TYPE),
		_firstChainType(WRITE_ACCESS_TYPE),
		_inFirst(true)
	{
		_lastChain = new Container::vector<TaskMetadata *>();
		_prevChain = new Container::vector<TaskMetadata *>();
	}

	~TaskiterGraphAccessChain() {
		delete _lastChain;
		delete _prevChain;
	}
};

// This class represents a set of tasks that contain dependencies between them
class TaskiterGraph {
public:
	typedef void *access_address_t;

private:
	Container::vector<TaskMetadata *> _tasks;
	Container::set<TaskiterGraphEdge> _edges;
	Container::unordered_map<access_address_t, TaskiterGraphAccessChain> _bottomMap;
	bool _processed;

	inline void createEdges(TaskMetadata *task, Container::vector<TaskMetadata *> &chain)
	{
		for (TaskMetadata *t : chain)
			_edges.emplace(t, task, false);
	}

	inline void closeDependencyLoop()
	{
		// Close every dependency chain by simulating that we are registering the first accesses again

		for (std::pair<const access_address_t, TaskiterGraphAccessChain> &it : _bottomMap) {
			TaskiterGraphAccessChain &chain = it.second;

			if (chain._firstChainType != chain._lastChainType ||
				chain._firstChainType == WRITE_ACCESS_TYPE ||
				chain._firstChainType == READWRITE_ACCESS_TYPE ||
				chain._firstChainType == COMMUTATIVE_ACCESS_TYPE) {

				for (TaskMetadata *task : chain._firstChain) {
					for (TaskMetadata *from : *(chain._lastChain)) {
						_edges.emplace(from, task, true);
						// std::cerr << from << " -> " << task << std::endl;
						task->incrementOriginalPredecessorCount();
					}
				}
			}
		}
	}

public:
	TaskiterGraph() :
		_processed(false)
	{
	}

	inline std::vector<TaskMetadata *> getSuccessors(TaskMetadata *task, bool crossIterationBoundary)
	{
		std::vector<TaskMetadata *> successorList;
		TaskiterGraphEdge search(task, nullptr, false);
		Container::set<TaskiterGraphEdge>::iterator it = _edges.lower_bound(search);

		while(it != _edges.end() && it->_from == task) {
			if (crossIterationBoundary || !it->_crossIterationBoundary)
				successorList.push_back(it->_to);
			++it;
		}

		return successorList;
	}

	void setTaskDegree()
	{
		// First, increase predecessors for every task
		for (TaskMetadata *t : _tasks)
			t->increasePredecessors();

		// Now, increment for each edge
		for (const TaskiterGraphEdge &edge : _edges) {
			edge._to->increasePredecessors();
			edge._to->incrementOriginalPredecessorCount();
		}

		closeDependencyLoop();

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

	void addTaskAccess(TaskMetadata *task, access_address_t address, DataAccessType type)
	{
		// Find the existing access chain or create a new one and return it
		TaskiterGraphAccessChain &chain = _bottomMap.emplace(std::piecewise_construct, std::forward_as_tuple(address), std::forward_as_tuple()).first->second;

		// Maybe we can stick with only the "set", would significantly ease up everything

		// We model dependencies similarly to some OpenMP runtimes, which have a simplistic view of the "last task set"
		// Note that this causes a O(n^2) dependency order when following a set of ins with a set of concurrents
		if (type == READ_ACCESS_TYPE || type == CONCURRENT_ACCESS_TYPE) {
			if (type != chain._lastChainType) {
				// chain._prevChainType = chain._lastChainType;
				std::swap(chain._lastChain, chain._prevChain);
				chain._lastChain->clear();
				chain._lastChainType = type;

				if (!chain._prevChain->empty()) {
					chain._inFirst = false;
				}
			}

			chain._lastChain->push_back(task);
			if (chain._inFirst) {
				chain._firstChainType = type;
				chain._firstChain.push_back(task);
			}

			createEdges(task, *(chain._prevChain));
		} else if (type == WRITE_ACCESS_TYPE || type == READWRITE_ACCESS_TYPE || type == COMMUTATIVE_ACCESS_TYPE) {
			// TODO: Handle commutative well
			if (!chain._lastChain->empty()) {
				createEdges(task, *(chain._lastChain));
				chain._lastChainType = WRITE_ACCESS_TYPE;
				chain._lastChain->clear();
				chain._lastChain->push_back(task);
				chain._inFirst = false;
			}

			if (chain._inFirst) {
				chain._inFirst = false;
				chain._firstChainType = type;
				chain._firstChain.push_back(task);
			}
		} else {
			// TODO: Handle reductions
		}
	}

	inline size_t getTasks() const
	{
		return _tasks.size();
	}

	~TaskiterGraph()
	{
	}
};

#endif // TASKITER_GRAPH_HPP
