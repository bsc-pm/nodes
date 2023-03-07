/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2022-2023 Barcelona Supercomputing Center (BSC)
*/

#ifndef TASKITER_GRAPH_HPP
#define TASKITER_GRAPH_HPP

#define PRINT_TASKITER_GRAPH 1

#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <numeric>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/transitive_reduction.hpp>
#include <boost/variant.hpp>

#ifdef PRINT_TASKITER_GRAPH
#include <boost/graph/graph_utility.hpp>
#include <boost/graph/graphviz.hpp>
#endif

#include <nosv.h>

#include "common/Containers.hpp"
#include "common/EnvironmentVariable.hpp"
#include "dependencies/DataAccessType.hpp"
#include "dependencies/discrete/CPUDependencyData.hpp"
#include "dependencies/discrete/DataAccess.hpp"
#include "dependencies/discrete/ReductionInfo.hpp"
#include "dependencies/discrete/TaskiterReductionInfo.hpp"
#include "dependencies/discrete/taskiter/TaskGroupMetadata.hpp"
#include "system/TaskFinalization.hpp"
#include "system/SpawnFunction.hpp"
#include "tasks/TaskMetadata.hpp"
#include "tasks/TaskiterChildMetadata.hpp"
#include "tasks/TaskiterChildLoopMetadata.hpp"
#include "TaskiterNode.hpp"

// A node of the TaskiterGraph may contain either a Task or a ReductionInfo
typedef TaskiterNode * TaskiterGraphNode;

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

// This struct stores the needed information for a chain of DataAccesses in a single address
// We store enough to draw the edges between tasks containing an access to this address
struct TaskiterGraphAccessChain {
	typedef Container::vector<TaskiterGraphNode> access_chain_t;
	std::unique_ptr<access_chain_t> _lastChain;
	std::unique_ptr<access_chain_t> _prevChain;
	access_chain_t _firstChain;
	access_chain_t _secondChain;
	access_chain_t _reductionChain;

	TaskiterGraphNode _reductionInfo;

	DataAccessType _lastChainType;
	DataAccessType _prevChainType;
	DataAccessType _firstChainType;
	DataAccessType _secondChainType;

	TaskiterGraphAccessChain() :
		_reductionInfo(nullptr),
		_lastChainType(WRITE_ACCESS_TYPE),
		_prevChainType(WRITE_ACCESS_TYPE),
		_firstChainType(WRITE_ACCESS_TYPE),
		_secondChainType(WRITE_ACCESS_TYPE)
	{
		// Unique ptrs are automatically destructed in ~TaskiterGraphAccessChain()
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

	// This is the actual graph type
	// We have to choose the containers for edges and vertices, as well as the properties
	// that edges and vertices have (which is just stored information in them)
	typedef boost::adjacency_list<
		boost::vecS,           // OutEdgeList
		boost::vecS,           // VertexList
		boost::bidirectionalS, // Directed with access to in_edges
		VertexProperty,        // VertexProperties
		EdgeProperty,          // EdgeProperties
		boost::no_property,    // GraphProperties
		boost::listS           // EdgeList
		>
		graph_t;

	typedef graph_t::vertex_descriptor graph_vertex_t;

private:
	size_t _currentUnroll;
	Container::vector<Container::vector<TaskiterNode *>> _tasks;
	Container::vector<TaskiterNode *> _controlTasks;
	Container::vector<TaskiterNode *> _reductions;
	Container::vector<TaskiterGraphEdge> _edges;
	Container::unordered_map<access_address_t, TaskiterGraphAccessChain> _bottomMap;

	graph_t _graph;
	graph_t _graphCpy;

	bool _processed;

	static EnvironmentVariable<std::string> _graphOptimization;
	static EnvironmentVariable<bool> _criticalPathTrackingEnabled;
	static EnvironmentVariable<bool> _printGraph;
	static EnvironmentVariable<std::string> _tentativeNumaScheduling;
	static EnvironmentVariable<bool> _communcationPriorityPropagation;
	static EnvironmentVariable<bool> _smartIS;
	static EnvironmentVariable<bool> _preferredBinding;
	static EnvironmentVariable<bool> _granularityTuning;

	// Creates edges from chain to node and inserts them into the graph
	inline void	createEdges(TaskiterGraphNode node, Container::vector<TaskiterGraphNode> &chain)
	{
		EdgeProperty props(false);

		for (TaskiterGraphNode t : chain) {
			boost::add_edge(t->getVertex(), node->getVertex(), props, _graph);
		}
	}

	// Inserts a "control task" for taskiter while. This closes every single access chain
	// and makes the control task depend on it, in order to be executed the last task of every
	// iteration. We create all the corresponding edges as well
	inline void insertControlTask(TaskMetadata *controlTask, bool last)
	{
		TaskiterNode *controlTaskNode = (TaskiterNode *) ((TaskiterChildMetadata *) controlTask);
		controlTask->increasePredecessors();
		controlTaskNode->setControlTask(true);
		VisitorSetDegree visitor;

		EdgeProperty propsTrue(true);
		EdgeProperty propsFalse(false);
		VertexProperty propTask(controlTaskNode);

		const graph_vertex_t controlTaskVertex = boost::add_vertex(propTask, _graph);
		controlTaskNode->setVertex(controlTaskVertex);
		_controlTasks.push_back(controlTaskNode);

		// TODO Do we have to properly close reductions here?

		// In theory, we don't have to worry for reductions, as they depend on all its
		// participants to be combined, and then into the closing task to release the control
		// A smart way to do this is:
		// - Pass through every vertex. If the vertex has no out-edges, it means it is a leaf, and then
		// we place a dependency Leaf -> Control task

		for (TaskiterNode *task : _tasks[_currentUnroll]) {
			graph_vertex_t vertex = task->getVertex();

			if (boost::out_degree(vertex, _graph) == 0) {
				// Leaf
				boost::add_edge(vertex, controlTaskVertex, propsFalse, _graph);
				if (last)
					controlTaskNode->apply(visitor);
			}
		}
	}

	// Closes the taskiter with a control task. This function inserts the control task, closing
	// all chains onto it, and then created the dependency loop where every single root of the graph
	// for the next iteration depends on the control task
	inline void closeLoopWithControl(TaskMetadata *controlTask)
	{
		insertControlTask(controlTask, true);

		// If we have an unrolled loop, we will also need to incorporate the proper dependencies
		if (_currentUnroll > 0)
			closeDependencyLoop();

		// At this point, we have n control tasks, each of one depending on tasks from their respective iterations.
		// Now, we have to add cross-iteration edges

		boost::property_map<graph_t, boost::vertex_name_t>::type nodemap = boost::get(boost::vertex_name_t(), _graph);
		EdgeProperty propsTrue(true);
		EdgeProperty propsFalse(false);
		VisitorSetDegree visitor;
		const size_t nControl = _controlTasks.size();

		for (size_t it = 0; it < nControl; ++it) {
			graph_vertex_t controlVertex = _controlTasks[it]->getVertex();

			for (TaskiterNode *task : _tasks[it]) {
				graph_vertex_t vertex = task->getVertex();
				boost::add_edge(controlVertex, vertex, propsTrue, _graph);
				TaskiterGraphNode node = boost::get(nodemap, vertex);
				node->apply(visitor);
			}

			// Additionally, the *next* control task depends on the current one, and the first on the last
			if (it+1 < nControl) {
				// There is a next control task
				TaskiterNode *nextControlTask = _controlTasks[it + 1];
				graph_vertex_t nextControlVertex = nextControlTask->getVertex();
				boost::add_edge(controlVertex, nextControlVertex, propsFalse, _graph);
				nextControlTask->apply(visitor);
			} else if (it > 0) {
				TaskiterNode *nextControlTask = _controlTasks[0];
				graph_vertex_t nextControlVertex = nextControlTask->getVertex();
				boost::add_edge(controlVertex, nextControlVertex, propsTrue, _graph);
				nextControlTask->apply(visitor);
			}
		}
	}

	inline void closeLeftoverReductionChains()
	{
		for (std::pair<const access_address_t, TaskiterGraphAccessChain> &it : _bottomMap) {
			TaskiterGraphAccessChain &chain = it.second;
			if (chain._reductionInfo != nullptr)
				closeReductionChain(chain);
		}
	}

	// Closes the dependency loop *without* a control task. All "open" dependency chains will be matched to the next
	inline void closeDependencyLoop()
	{
		// Close every dependency chain by simulating that we are registering the first accesses again
		VisitorSetDegreeCrossGroup visitor;
		EdgeProperty prop(true);

		for (std::pair<const access_address_t, TaskiterGraphAccessChain> &it : _bottomMap) {
			TaskiterGraphAccessChain &chain = it.second;

			// If there is no first/second chain, fill them
			if (chain._firstChain.empty()) {
				assert(chain._prevChain->empty());
				assert(!chain._lastChain->empty());
				chain._firstChain = *chain._lastChain;
				chain._firstChainType = chain._lastChainType;
			} else if (chain._secondChain.empty()) {
				assert(!chain._lastChain->empty());
				chain._secondChain = *chain._lastChain;
				chain._secondChainType = chain._lastChainType;
			}

			// The idea is to "simulate" the first accesses being registered again.
			// There is a special case, where the first and last accesses are IN or CONCURRENT (but are equal)
			// In that case, we have to try and match them with the first non-IN and the last non-IN.
			// Note that if chain._secondChain.empty(), the first and last chains are the same, and in the case for
			// IN / CONCURRENT, this means no dependencies.

			// TODO: Does this correctly work with reductions?
			// Clearly not

			if (chain._firstChainType != chain._lastChainType ||
				chain._firstChainType == WRITE_ACCESS_TYPE ||
				chain._firstChainType == READWRITE_ACCESS_TYPE ||
				chain._firstChainType == COMMUTATIVE_ACCESS_TYPE) {
				for (TaskiterGraphNode task : chain._firstChain) {
					for (TaskiterGraphNode from : *(chain._lastChain)) {
						boost::add_edge(from->getVertex(), task->getVertex(), prop, _graph);
						task->apply(visitor);
					}
				}
			} else if (!chain._secondChain.empty()) {
				assert(chain._firstChainType == READ_ACCESS_TYPE || chain._firstChainType == CONCURRENT_ACCESS_TYPE);
				assert(chain._firstChainType == chain._lastChainType);

				// From the last chain to the second one
				for (TaskiterGraphNode task : chain._secondChain) {
					for (TaskiterGraphNode from : *(chain._lastChain)) {
						boost::add_edge(from->getVertex(), task->getVertex(), prop, _graph);
						task->apply(visitor);
					}
				}

				// Now from the previous to the first
				for (TaskiterGraphNode task : chain._firstChain) {
					for (TaskiterGraphNode from : *(chain._prevChain)) {
						boost::add_edge(from->getVertex(), task->getVertex(), prop, _graph);
						task->apply(visitor);
					}
				}
			}
		}
	}

	// Adds a task to a specific dependency chain
	inline void addTaskToChain(TaskiterNode *task, TaskiterGraphAccessChain &chain)
	{
		chain._lastChain->push_back(task);
	}

	// Marks the end of reduction accesses in a grain, placing the relevant edges
	inline void closeReductionChain(TaskiterGraphAccessChain &chain)
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

		swapChains(chain);
		chain._reductionChain.clear();
		// chain._lastChain->clear();
		chain._lastChainType = REDUCTION_ACCESS_TYPE;
		chain._lastChain->push_back(chain._reductionInfo);
		chain._reductionInfo = nullptr;
	}

	// Swap the last and previous chains
	inline void swapChains(TaskiterGraphAccessChain &chain)
	{
		std::swap(chain._prevChain, chain._lastChain);
		chain._prevChainType = chain._lastChainType;
		chain._lastChain->clear();

		// If the previous chain was not empty, maybe it was the first or the second
		if (!chain._prevChain->empty()) {
			if (chain._firstChain.empty()) {
				chain._firstChain = *chain._prevChain;
				chain._firstChainType = chain._prevChainType;
			} else if (chain._secondChain.empty()) {
				chain._secondChain = *chain._prevChain;
				chain._secondChainType = chain._prevChainType;
			}
		}
	}

	class VisitorSetDegreeCross {
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

	class VisitorSetDegreeCrossGroup {
	public:
		void operator()(TaskMetadata *t) const
		{
			if (t->getGroup() != nullptr)
				t = t->getGroup();
			t->incrementOriginalPredecessorCount();
		}

		void operator()(ReductionInfo *r) const
		{
			r->incrementOriginalRegisteredAccesses();
		}
	};

	class VisitorSetDegree {
	public:
		void operator()(TaskMetadata *t) const
		{
			if (t->getGroup() != nullptr)
				t = t->getGroup();
			t->increasePredecessors();
			t->incrementOriginalPredecessorCount();
		}

		void operator()(ReductionInfo *r) const
		{
			r->incrementRegisteredAccesses();
			r->incrementOriginalRegisteredAccesses();
		}
	};

	class VisitorApplySuccessor {
		TaskiterGraph *_graph;
		std::function<void(TaskMetadata *)> &_satisfyTask;
		bool _crossIterationBoundary;
		bool _delayedCancellation;

	public:
		VisitorApplySuccessor(
			TaskiterGraph *graph,
			std::function<void(TaskMetadata *)> satisfyTask,
			bool crossIterationBoundary,
			bool delayedCancellation) :
			_graph(graph),
			_satisfyTask(satisfyTask),
			_crossIterationBoundary(crossIterationBoundary),
			_delayedCancellation(delayedCancellation)
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

				TaskiterNode *n = (TaskiterNode *)((TaskiterReductionInfo *) r);
				// Release next accesses
				_graph->applySuccessors(n, _crossIterationBoundary, _satisfyTask, _delayedCancellation);
			}
		}
	};

	struct EdgeHash;
	struct EdgeEqual;

	void prioritizeCriticalPath();
	void transitiveReduction();
	void basicReduction();
	void localityScheduling();
	void localitySchedulingBitset();
	void localitySchedulingMovePages();
	void localitySchedulingMovePagesSimple();
	void immediateSuccessorProcess();
	void communicationPriorityPropagation();
	void granularityTuning();

	inline TaskiterNode *getNodeFromTask(TaskMetadata *task)
	{
		assert(task);
		TaskiterNode *node;
		if (task->isTaskloop())
			node = static_cast<TaskiterNode *>(static_cast<TaskiterChildLoopMetadata *>(task));
		else if (task->isGroup())
			node = static_cast<TaskiterNode *>(static_cast<TaskGroupMetadata *>(task));
		else
			node = static_cast<TaskiterNode *>(static_cast<TaskiterChildMetadata *>(task));

		return node;
	}

	inline void applySuccessorsStd(TaskiterGraphNode node,
		bool crossIterationBoundary,
		VisitorApplySuccessor &visitor)
	{
		graph_vertex_t vertex = node->getVertex();

		graph_t::out_edge_iterator ei, eend;
		boost::property_map<graph_t, boost::edge_name_t>::type edgemap = boost::get(boost::edge_name_t(), _graph);
		boost::property_map<graph_t, boost::vertex_name_t>::type nodemap = boost::get(boost::vertex_name_t(), _graph);

		// Travel through adjacent vertices
		for (boost::tie(ei, eend) = boost::out_edges(vertex, _graph); ei != eend; ++ei) {
			graph_t::edge_descriptor e = *ei;
			graph_vertex_t to = boost::target(e, _graph);
			TaskiterGraphNode toNode = boost::get(nodemap, to);
			bool edgeCrossIteration = boost::get(edgemap, e);

			if ((crossIterationBoundary || !edgeCrossIteration))
				toNode->apply(visitor);
		}
	}

	inline void applySuccessorsSmartIS(TaskiterGraphNode node,
		bool crossIterationBoundary,
		VisitorApplySuccessor &visitor)
	{
		graph_vertex_t vertex = node->getVertex();

		graph_t::out_edge_iterator ei, eend;
		boost::property_map<graph_t, boost::edge_name_t>::type edgemap = boost::get(boost::edge_name_t(), _graph);
		boost::property_map<graph_t, boost::vertex_name_t>::type nodemap = boost::get(boost::vertex_name_t(), _graph);

		// Ask only once for the preferred vertex, since the task calculating IS may still be running
		size_t preferredVertex = node->getPreferredOutVertex();
		size_t preferredOriginal = preferredVertex;

		// Travel through adjacent vertices
		for (boost::tie(ei, eend) = boost::out_edges(vertex, _graph); ei != eend; ++ei) {
			graph_t::edge_descriptor e = *ei;
			graph_vertex_t to = boost::target(e, _graph);
			TaskiterGraphNode toNode = boost::get(nodemap, to);
			bool edgeCrossIteration = boost::get(edgemap, e);

			if(to != preferredVertex) {
				if ((crossIterationBoundary || !edgeCrossIteration))
					toNode->apply(visitor);
			} else {
				preferredVertex = SIZE_MAX;
			}
		}

		if (preferredOriginal != SIZE_MAX) {
			graph_vertex_t to = (graph_vertex_t) preferredOriginal;
			TaskiterGraphNode toNode = boost::get(nodemap, to);
			bool edgeCrossIteration = node->getPreferredOutCrossIteration();

			if (crossIterationBoundary || !edgeCrossIteration)
				toNode->apply(visitor);
		}
	}

	inline void applySuccessorsBinding(TaskiterGraphNode node,
		bool crossIterationBoundary,
		VisitorApplySuccessor &visitor) 
	{	
		graph_vertex_t vertex = node->getVertex();
		TaskMetadata *t = node->getTask();
		
		int preferredExecutionPlace = t ? t->getLastExecutionCore() : -1;

		graph_t::out_edge_iterator ei, eend;
		boost::property_map<graph_t, boost::edge_name_t>::type edgemap = boost::get(boost::edge_name_t(), _graph);
		boost::property_map<graph_t, boost::vertex_name_t>::type nodemap = boost::get(boost::vertex_name_t(), _graph);

		TaskiterGraphNode bestBind = nullptr;

		// Travel through adjacent vertices
		for (boost::tie(ei, eend) = boost::out_edges(vertex, _graph); ei != eend; ++ei) {
			graph_t::edge_descriptor e = *ei;
			graph_vertex_t to = boost::target(e, _graph);
			TaskiterGraphNode toNode = boost::get(nodemap, to);
			bool edgeCrossIteration = boost::get(edgemap, e);

			if ((crossIterationBoundary || !edgeCrossIteration)) {
				TaskMetadata *toTask = toNode->getTask();
				if (bestBind == nullptr && toTask && toTask->getLastExecutionCore() == preferredExecutionPlace) {
					bestBind = toNode;
				} else {
					toNode->apply(visitor);
				}
			}
		}

		if (bestBind != nullptr) {
			bestBind->apply(visitor);
		}
	}

public:
	TaskiterGraph() :
		_currentUnroll(0),
		_processed(false)
	{
		_tasks.emplace_back();
	}

	inline void applySuccessors(
		TaskMetadata *task,
		bool crossIterationBoundary,
		std::function<void(TaskMetadata *)> satisfyTask,
		bool delayedCancellationMode)
	{
		if (_preferredBinding.getValue())
			task->setAffinity(task->getLastExecutionCore(), NOSV_AFFINITY_LEVEL_CPU, NOSV_AFFINITY_TYPE_PREFERRED);

		applySuccessors(getNodeFromTask(task), crossIterationBoundary, satisfyTask, delayedCancellationMode);
	}

	// Applies the function satisfyTask to every successor of node
	// There are two special cases: sometimes we cannot cross the iteration boundary, which implies
	// only satisfying tasks from the current iteration, and in other cases (delayedCancellation)
	// if the task is a control task, it will only satisfy other control tasks. This second case
	// is used when a taskiter while is on its cancellation process
	inline void applySuccessors(
		TaskiterGraphNode node,
		bool crossIterationBoundary,
		std::function<void(TaskMetadata *)> &satisfyTask,
		bool delayedCancellationMode)
	{
		VisitorApplySuccessor visitor(this, satisfyTask, crossIterationBoundary, delayedCancellationMode);

		if (delayedCancellationMode && node->getTask()) {
			// In this mode, control tasks can only pass satisfiability to other control tasks
			// TaskMetadata *task = node->getTask();
			Container::vector<TaskiterNode *>::iterator it = std::find(_controlTasks.begin(), _controlTasks.end(), node);
			if (it != _controlTasks.end()) {
				// Now, we're a control task, so we only satisfy the next control task in the chain
				// Advance one:
				if (++it == _controlTasks.end())
					it = _controlTasks.begin();

				(*it)->apply(visitor);
				return;
			}
		}

		if (_smartIS.getValue())
			applySuccessorsSmartIS(node, crossIterationBoundary, visitor);
		else if (_preferredBinding.getValue())
			applySuccessorsBinding(node, crossIterationBoundary, visitor);
		else
			applySuccessorsStd(node, crossIterationBoundary, visitor);
	}

	// For all tasks, determine the number of prececessors they have. Block the ones with > 0,
	// and schedule the rest
	void setTaskDegree(TaskMetadata *controlTask)
	{
		// First, increase predecessors for every task
		forEach([](TaskMetadata *t) {
			if (t->getGroup())
				t->getGroup()->increasePredecessors();
			else
				t->increasePredecessors();
		});

		VisitorSetDegree visitor;
		VisitorSetDegreeCrossGroup crossIterationVisitor;

		// Close the leftover reduction chains
		// closeLeftoverReductionChains();

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
				node->apply(visitor);
			else
				node->apply(crossIterationVisitor);
		}

		if (controlTask == nullptr) {
			// Close the dependency loop normally
			closeDependencyLoop();
		} else {
			// Insert a control dependency
			closeLoopWithControl(controlTask);
		}

		EdgeProperty prop(true);
		// Now, incorporate delayed edges for closing reductions
		for (TaskiterGraphEdge &edge : _edges) {
			if (edge._from != edge._to) {
				boost::add_edge(edge._from->getVertex(), edge._to->getVertex(), prop, _graph);
				edge._to->apply(crossIterationVisitor);
			}
		}

		_edges.clear();

#if PRINT_TASKITER_GRAPH
		if (_printGraph.getValue()) {
			std::ofstream dot("g.dot");
			boost::write_graphviz(dot, _graph);
		}
#endif

		// Now, decrease predecessors for every task
		forEach([](TaskMetadata *t) {
			if (t->getGroup())
				t = t->getGroup();

			if (t->decreasePredecessors()) {
				nosv_submit(t->getTaskHandle(), NOSV_SUBMIT_UNLOCKED);
			}
		});

		int first = 1;
		// And for control tasks
		for (TaskiterNode *node : _controlTasks) {
			TaskMetadata *t = node->getTask();
			if(t->decreasePredecessors(t->getOriginalPrecessorCount() + first))
				nosv_submit(t->getTaskHandle(), NOSV_SUBMIT_UNLOCKED);
			first = 0;
		}
	}

	// Process the taskiter to optimize away redundant edges
	// this passes through a process called "transitive reduction", which derives
	// the minimal graph which still mantains all the dependencies of the original one
	void process()
	{
		// Close leftover reduction chains
		closeLeftoverReductionChains();

		// Optimize edges. This is done here, as it will affect next steps and the overall closing of the graph
		if (_graphOptimization.getValue() == "transitive")
			transitiveReduction();
		else if (_graphOptimization.getValue() == "basic")
			basicReduction();

		#if PRINT_TASKITER_GRAPH
		if (_printGraph.getValue()) {
			std::ofstream dot("before.dot");
			boost::write_graphviz(dot, _graph);
		}
		#endif

		// Then, perform granularity tuning. This step also alters the number of vertices and edges, so it has to be done
		// before the rest of optimizations
		if (_granularityTuning.getValue())
			granularityTuning();

		if (_tentativeNumaScheduling.getValue() != "none" || _criticalPathTrackingEnabled.getValue() ||
			_communcationPriorityPropagation.getValue() || _smartIS.getValue()) {
			// Copy the graph for optimization
			// Iterators pointing to the graph may change when adding attributes, etc.
			// Operating on a copy will ensure this doesn't become an issue for the delayed optimization.
			_graphCpy = _graph;

			// We'll do the optimization in an offloaded task, but we need to block the existing
			// taskiter tasks from disappearing while we optimize.
			const bool willPostProcess = _communcationPriorityPropagation.getValue() || _smartIS.getValue();
			forEach([willPostProcess, this](TaskMetadata *t) {
				t->increaseRemovalBlockingCount();

				// Wait also for the post-process actions
				if (willPostProcess) {
					t->increaseRemovalBlockingCount();
					if (_communcationPriorityPropagation.getValue()) {
						t->setPriority(INT_MAX);
						t->applyDelayedChanges();
					}
				}
			});

			SpawnFunction::spawnLambda([this]() {
				if (_tentativeNumaScheduling.getValue() == "naive")
					localityScheduling();
				else if (_tentativeNumaScheduling.getValue() == "bitset")
					localitySchedulingBitset();
				else if (_tentativeNumaScheduling.getValue() == "move_pages_simple")
					localitySchedulingMovePagesSimple();
				else if (_tentativeNumaScheduling.getValue() == "move_pages")
					localitySchedulingMovePages();

				// Prioritize tasks in the critical path
				if (_criticalPathTrackingEnabled.getValue())
					prioritizeCriticalPath();

				forEach([](TaskMetadata *t) {
					if (t->decreaseRemovalBlockingCount())
						TaskFinalization::disposeTask(t);
				});
			}, []() {}, "Taskiter processing", true);
		}

		_processed = true;
	}

	void postProcess()
	{
		if (_communcationPriorityPropagation.getValue() || _smartIS.getValue()) {
			SpawnFunction::spawnLambda([this]() {
				// Prioritize communcation tasks
				if (_communcationPriorityPropagation.getValue())
					communicationPriorityPropagation();

				if (_smartIS.getValue())
					immediateSuccessorProcess();

				forEach([](TaskMetadata *t) {
					if (t->decreaseRemovalBlockingCount())
						TaskFinalization::disposeTask(t);
				});
			}, []() {}, "Taskiter post-processing", true);
		}
	}

	inline bool isProcessed() const
	{
		return _processed;
	}

	// Add a task to the graph as a vertex
	void addTask(TaskMetadata *task)
	{
		TaskiterNode *node = getNodeFromTask(task);

		_tasks[_currentUnroll].push_back(node);

		VertexProperty prop(node);
		graph_vertex_t vertex = boost::add_vertex(prop, _graph);
		node->setVertex(vertex);
	}

	// Adds an access to the graph, creating the relevant vertices between tasks
	// This function contains the "meat" of solving dependencies
	void addTaskAccess(TaskMetadata *task, DataAccess *access)
	{
		TaskiterNode *node = getNodeFromTask(task);
		const access_address_t address = access->getAccessRegion().getStartAddress();
		const DataAccessType type = access->getType();

		// Find the existing access chain or create a new one and return it
		TaskiterGraphAccessChain &chain = _bottomMap.emplace(std::piecewise_construct, std::forward_as_tuple(address), std::forward_as_tuple()).first->second;

		// Maybe we can stick with only the "set", would significantly ease up everything

		// We model dependencies similarly to some OpenMP runtimes, which have a simplistic view of the "last task set"
		// Note that this causes a O(n^2) dependency order when following a set of ins with a set of concurrents
		if (type == READ_ACCESS_TYPE || type == CONCURRENT_ACCESS_TYPE) {
			if (chain._reductionInfo) {
				closeReductionChain(chain);
				// Close the reduction
			}

			if (type != chain._lastChainType) {
				swapChains(chain);
				chain._lastChainType = type;
			}

			addTaskToChain(node, chain);
			createEdges(node, *(chain._prevChain));
		} else if (type == WRITE_ACCESS_TYPE || type == READWRITE_ACCESS_TYPE || type == COMMUTATIVE_ACCESS_TYPE) {
			if (chain._reductionInfo) {
				closeReductionChain(chain);
				// Close the reduction
			}

			swapChains(chain);
			chain._lastChainType = WRITE_ACCESS_TYPE;

			// TODO: Handle commutative well
			if (!chain._prevChain->empty()) {
				createEdges(node, *(chain._prevChain));
			}

			addTaskToChain(node, chain);
		} else if (type == REDUCTION_ACCESS_TYPE) {
			ReductionInfo *reductionInfo = access->getReductionInfo();
			assert(reductionInfo != nullptr);

			TaskiterNode *reductionNode = (TaskiterNode *)((TaskiterReductionInfo *) reductionInfo);

			chain._reductionChain.push_back(node);
			if (chain._reductionInfo == nullptr) {
				chain._reductionInfo = reductionNode;
				_reductions.push_back(reductionNode);
				const graph_vertex_t reductionVertex = boost::add_vertex(VertexProperty(reductionNode), _graph);
				reductionNode->setVertex(reductionVertex);
			}

			assert(chain._reductionInfo == reductionNode);
		}
	}

	inline size_t getNumTasks() const
	{
		size_t tasks = 0;
		for (Container::vector<TaskiterNode *> const &taskList : _tasks)
			tasks += taskList.size();

		return tasks;
	}

	// Do something for each task in the graph
	inline void forEach(std::function<void(TaskMetadata *)> fn, bool includeControl = false) const
	{
		for (Container::vector<TaskiterNode *> const &taskVector : _tasks)
			for (TaskiterNode * task : taskVector)
				fn(task->getTask());

		if (includeControl) {
			for (TaskiterNode * task : _controlTasks)
				fn(task->getTask());
		}
	}

	// Inserts a control task but not in the end of a taskiter, but in the middle
	// This happens in unrolled taskiter while loops, which have intermediate control tasks
	inline void insertControlInUnrolledLoop(TaskMetadata *controlTask)
	{
		insertControlTask(controlTask, false);

		_tasks.emplace_back();
		_currentUnroll++;
	}
};

#endif // TASKITER_GRAPH_HPP
