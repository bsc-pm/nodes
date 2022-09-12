/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2022 Barcelona Supercomputing Center (BSC)
*/

#include <algorithm>
#include <boost/graph/topological_sort.hpp>
#include <deque>
#include <unordered_set>

#include "common/MathSupport.hpp"
#include "TaskiterGraph.hpp"

EnvironmentVariable<std::string> TaskiterGraph::_graphOptimization("NODES_ITER_OPTIMIZE", "basic");
EnvironmentVariable<bool> TaskiterGraph::_criticalPathTrackingEnabled("NODES_ITER_TRACK_CRITICAL", true);
EnvironmentVariable<bool> TaskiterGraph::_printGraphStatistics("NODES_ITER_PRINT_STATISTICS", false);
EnvironmentVariable<std::string> TaskiterGraph::_tentativeNumaScheduling("NODES_ITER_NUMA", "naive");
// EnvironmentVariable<bool> TaskiterGraph::_criticalPathTrackingEnabled("NODES_ITER_TRACK_CRITICAL", true);

void TaskiterGraph::prioritizeCriticalPath()
{
	// Analyze the graph to figure out the critical task path.
	// The first version just assumes every task takes one second.
	// Then, we will add time tracking and take that into account.

	boost::property_map<graph_t, boost::vertex_name_t>::type nodemap = boost::get(boost::vertex_name_t(), _graph);
	std::unordered_map<graph_vertex_t, int> priorityMap;
	std::vector<graph_vertex_t> reverseTopological;
	graph_t::out_edge_iterator ei, eend;

	boost::topological_sort(_graph, std::back_inserter(reverseTopological));

	for (graph_vertex_t vertex : reverseTopological) {
		int maxPriority = -1;

		for (boost::tie(ei, eend) = boost::out_edges(vertex, _graph); ei != eend; ++ei) {
			graph_t::edge_descriptor e = *ei;
			graph_vertex_t to = boost::target(e, _graph);

			int successorPriority = priorityMap.at(to);
			if (successorPriority > maxPriority)
				maxPriority = successorPriority;
		}

		TaskiterGraphNode node = boost::get(nodemap, vertex);
		TaskMetadata **task = boost::get<TaskMetadata *>(&node);

		if (task) {
			// This is adding uint64_t to the int maxPriority, which has a potential to overflow
			// when iterations are very large
			maxPriority += (*task)->getElapsedTime();
			(*task)->setPriority(maxPriority);
		} else {
			maxPriority++;
		}

		priorityMap[vertex] = maxPriority;
	}
}

void TaskiterGraph::transitiveReduction()
{
	graph_t processedGraph;

	// Try to reduce the dependency graph
	// This *must* be done on a DAG, because it uses its topological sorting, so
	// we prevent cycles up to this point
	Container::map<graph_vertex_t, graph_vertex_t> gToTr;
	Container::vector<size_t> vertexMap(boost::num_vertices(_graph));
	std::iota(vertexMap.begin(), vertexMap.end(), (size_t)0);

	boost::transitive_reduction(_graph, processedGraph, boost::make_assoc_property_map(gToTr), vertexMap.data());

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

	for (auto &it : _tasksToVertices) {
		it.second = gToTr[it.second];
	}

	graph_t::edge_iterator ei, eend;
	for (boost::tie(ei, eend) = boost::edges(processedGraph); ei != eend; ei++) {
		graph_t::edge_descriptor e = *ei;
		boost::put(edgemapProcessed, e, false);
	}

	_graph = processedGraph;
}

struct TaskiterGraph::EdgeHash {
	std::size_t operator () (const graph_t::edge_descriptor &e) const {
		std::size_t seed = 0;
		boost::hash_combine(seed, e.m_source);
		boost::hash_combine(seed, e.m_target);
		return seed;
	}
};

struct TaskiterGraph::EdgeEqual {
	bool operator () (const graph_t::edge_descriptor &a, const graph_t::edge_descriptor &b) const {
		return (a.m_source == b.m_source && a.m_target == b.m_target);
	}
};

void TaskiterGraph::basicReduction()
{
	using EdgeSet = std::unordered_set<graph_t::edge_descriptor, EdgeHash, EdgeEqual>;
	EdgeSet edgeSet;

	boost::remove_edge_if([&edgeSet](const graph_t::edge_descriptor &e) -> bool {
		std::pair<EdgeSet::iterator, bool> element = edgeSet.insert(e);
		return !element.second;
	}, _graph);
}

// static inline std::vector<int> kernighanLinBiPartition(std::vector<graph_vertex_t> &subgraph, std::vector<int> &assignedPartitions, int partitionTags[2])
// {
// 	int vertices = subgraph.size();

// 	// Start with a balanced partition
// 	for (int i = 0; i < vertices; ++i)
// 		assignedPartitions[subgraph[i]] = partitionTags[(i >= vertices/2)];

// 	std::vector<size_t> d(vertices);
// 	std::vector<size_t> a(vertices);
// 	std::vector<size_t> b(vertices);

// 	size_t g_max;

// 	do {
// 		for (int i = 0; i < vertices; ++i) {
// 			size_t localE = 0;
// 			size_t localI = 0;
// 			graph_vertex_t vertex = subgraph[i];
// 			int assignedPartition = assignedPartitions[vertex];

// 			for (boost::tie(ei, eend) = boost::out_edges(vertex, _graph); ei != eend; ++ei)
// 			{
// 				graph_vertex_t other = boost::target(*ei);
// 				if (assignedPartitions[other] == assignedPartition)
// 					localI++;
// 				else
// 					localE++;
// 			}
// 		}

// 	} while (g_max > 0);

// 	return assignedPartitions;
// }

// static inline void kernighanLin(int partitions)
// {
// 	int vertices = boost::num_vertices(_graph);

// 	// Save where each vertex is assigned
// 	std::vector<int> assignedPartitions(vertices);

// 	// Start with a balanced partition
// 	for (int i = 0; i < vertices; ++i)
// 		assignedPartitions[i] = (i >= vertices/2);

// 	// Now,
// }


void TaskiterGraph::localityScheduling()
{
	// Simulate AMD-Rome with L3 partitioned: total of 16 L3 complexes
	boost::property_map<graph_t, boost::vertex_name_t>::type nodemap = boost::get(boost::vertex_name_t(), _graph);
	int vertices = boost::num_vertices(_graph);
	int clusters = 2;
	int slotsPerCluster = 24;
	int initialPriority = vertices;

	std::vector<uint64_t> coreDeadlines(clusters * slotsPerCluster, 0);
	std::vector<int> predecessors(vertices);
	std::vector<TaskMetadata *> assignedTasks(clusters * slotsPerCluster, nullptr);
	std::deque<graph_vertex_t> readyTasks;

	// Initialize precedessors
	graph_t::vertex_iterator vi, vend;
	for (boost::tie(vi, vend) = boost::vertices(_graph); vi != vend; vi++) {
		graph_vertex_t v = *vi;
		predecessors[v] = boost::in_degree(v, _graph);
		if (!predecessors[v])
			readyTasks.push_back(v);
	}

	assert(!readyTasks.empty() || !vertices);

	// Well-defined number of iterations
	for (int i = 0; i < vertices; ++i) {
		std::vector<uint64_t>::iterator earliestCore = std::min_element(coreDeadlines.begin(), coreDeadlines.end());

		// Find best candidate task for core
		int coreIdx = std::distance(coreDeadlines.begin(), earliestCore);
		int clusterIdx = coreIdx / slotsPerCluster;

		// Map current accesses in cluster
		// Take into account previously assigned tasks as well, for data reuse
		std::unordered_map<void *, int> accessCount;
		int total = 0;
		for (int j = 0; j < slotsPerCluster; ++j) {
			TaskMetadata *task = assignedTasks[clusterIdx * slotsPerCluster + j];
			if (task) {
				task->getTaskDataAccesses().forAll([&accessCount, &total](void *address, DataAccess *) -> bool {
					accessCount[address]++;
					total++;
					return true;
				});
			}
		}

		TaskMetadata *bestSuccessor = nullptr;
		std::deque<graph_vertex_t>::iterator bestSuccessorIt;
		int matches = -1;

		for (std::deque<graph_vertex_t>::iterator vIterator = readyTasks.begin(); vIterator != readyTasks.end(); ) {
			graph_vertex_t v = *vIterator;
			TaskiterGraphNode node = boost::get(nodemap, v);
			TaskMetadata **task = boost::get<TaskMetadata *>(&node);

			if (!task) {
				// Node is a ReductionInfo
				// "Release" accesses
				assert(false);
				vIterator = readyTasks.erase(vIterator);
			} else {
				// Calculate score
				int score = 0;
				(*task)->getTaskDataAccesses().forAll([&accessCount, &score](void *address, DataAccess *) -> bool {
					score += accessCount[address];
					return true;
				});

				if (score > matches) {
					matches = score;
					bestSuccessor = *task;
					bestSuccessorIt = vIterator;
					if (score == total)
						break;
				}

				vIterator++;
			}
		}

		TaskMetadata *oldTask = assignedTasks[coreIdx];
		bool noSuccessor = false;

		// There may not be a successor, in a case where there are not enough tasks to feed every core
		if (bestSuccessor) {
			assignedTasks[coreIdx] = bestSuccessor;
			coreDeadlines[coreIdx] += bestSuccessor->getElapsedTime();
			bestSuccessor->setAffinity(coreIdx / slotsPerCluster, NOSV_AFFINITY_LEVEL_NUMA, NOSV_AFFINITY_TYPE_PREFERRED);
			bestSuccessor->setPriority(initialPriority--);
			graph_vertex_t v = *bestSuccessorIt;
			readyTasks.erase(bestSuccessorIt);
		} else {
			assignedTasks[coreIdx] = nullptr;
			noSuccessor = true;
			// TODO In this case, not all tasks will be scheduled
		}

		bool foundTask = false;
		if (oldTask) {
			// Now, "release" deps
			graph_vertex_t v = _tasksToVertices[oldTask];
			graph_t::out_edge_iterator ei, eend;
			for (boost::tie(ei, eend) = boost::out_edges(v, _graph); ei != eend; ei++) {
				graph_vertex_t target = boost::target(*ei, _graph);
				int remaining = --predecessors[target];
				assert(remaining >= 0);

				if (remaining == 0) {
					readyTasks.push_back(target);
					foundTask = true;
				}
			}
		}

		if (noSuccessor && !foundTask) {
			// Push back the deadline of the current core so other tasks can finish as well
			coreDeadlines[coreIdx] = UINT64_MAX;
			uint64_t earliestDeadline = *std::min_element(coreDeadlines.begin(), coreDeadlines.end());
			coreDeadlines[coreIdx] = earliestDeadline + 1;
		}
	}
}

void TaskiterGraph::localitySchedulingBitset()
{
	const int differentAddresses = _bottomMap.size();
	const int bitsetWords = MathSupport::ceil(differentAddresses, sizeof(uint32_t) * 8);
	const int vertices = boost::num_vertices(_graph);
	const int clusters = 2;
	const int slotsPerCluster = 24;
	const graph_vertex_t NO_TASK = (graph_vertex_t)-1;
	int initialPriority = vertices;

	boost::property_map<graph_t, boost::vertex_name_t>::type nodemap = boost::get(boost::vertex_name_t(), _graph);

	std::unique_ptr<uint32_t[]> bitset = std::make_unique<uint32_t[]>(bitsetWords * vertices);
	std::unique_ptr<uint32_t[]> tmpBitset = std::make_unique<uint32_t[]>(bitsetWords);
	// uint32_t *bitset = new uint32_t[bitsetWords * vertices];
	// uint32_t *tmpBitset = new uint32_t[bitsetWords];

	std::vector<uint64_t> coreDeadlines(clusters * slotsPerCluster, 0);
	std::vector<int> predecessors(vertices);
	std::vector<graph_vertex_t> assignedTasks(clusters * slotsPerCluster, NO_TASK);
	std::deque<graph_vertex_t> readyTasks;

	graph_t::vertex_iterator vi, vend;
	for (boost::tie(vi, vend) = boost::vertices(_graph); vi != vend; vi++) {
		graph_vertex_t v = *vi;
		TaskiterGraphNode node = boost::get(nodemap, v);
		TaskMetadata **task = boost::get<TaskMetadata *>(&node);

		if (task) {
			(*task)->getTaskDataAccesses().forAll([&bitset, &v, bitsetWords, this](void *address, DataAccess *) -> bool {
				Container::unordered_map<access_address_t, TaskiterGraphAccessChain>::iterator mapIterator = _bottomMap.find(address);
				assert(mapIterator != _bottomMap.end());
				int index = std::distance(_bottomMap.begin(), mapIterator);

				uint32_t &currentBitsetLocation = bitset[v * bitsetWords + index / 32];
				int bit = index % 32;
				currentBitsetLocation |= (1 << bit);

				return true;
			});
		}

		predecessors[v] = boost::in_degree(v, _graph);
		if (!predecessors[v])
			readyTasks.push_back(v);
	}

	assert(!readyTasks.empty() || !vertices);
	int emptyCPUs = clusters * slotsPerCluster;
	int scheduledTasks = 0;
	uint64_t now = 0;

	while (scheduledTasks < vertices) {
		while (emptyCPUs && !readyTasks.empty()) {
			int cpu = std::distance(assignedTasks.begin(), std::find(assignedTasks.begin(), assignedTasks.end(), NO_TASK));
			int clusterIdx = cpu / slotsPerCluster;

			// Check assigned tasks in cluster
			for (int i = 0; i < bitsetWords; ++i)
				tmpBitset[i] = 0;

			for (int j = 0; j < slotsPerCluster; ++j) {
				graph_vertex_t task = assignedTasks[clusterIdx * slotsPerCluster + j];

				if (task != NO_TASK) {
					for (int i = 0; i < bitsetWords; ++i)
						tmpBitset[i] |= bitset[task * bitsetWords + i];
				}
			}

			TaskMetadata *bestSuccessor = nullptr;
			std::deque<graph_vertex_t>::iterator bestSuccessorIt;
			int matches = -1;

			for (std::deque<graph_vertex_t>::iterator vIterator = readyTasks.begin(); vIterator != readyTasks.end(); vIterator++) {
				graph_vertex_t v = *vIterator;
				TaskiterGraphNode node = boost::get(nodemap, v);
				TaskMetadata **task = boost::get<TaskMetadata *>(&node);

				int score = 0;
				for (int i = 0; i < bitsetWords; ++i)
					score += __builtin_popcountl(tmpBitset[i] & bitset[v * bitsetWords + i]);

				if (score > matches) {
					matches = score;
					bestSuccessor = *task;
					bestSuccessorIt = vIterator;
				}
			}

			assignedTasks[cpu] = *bestSuccessorIt;
			coreDeadlines[cpu] += now + bestSuccessor->getElapsedTime();
			bestSuccessor->setAffinity(clusterIdx, NOSV_AFFINITY_LEVEL_NUMA, NOSV_AFFINITY_TYPE_PREFERRED);
			bestSuccessor->setPriority(initialPriority--);
			readyTasks.erase(bestSuccessorIt);
			scheduledTasks++;
			emptyCPUs--;
		}

		do {
			// Find next task to finish
			std::vector<uint64_t>::iterator earliestCore = std::min_element(coreDeadlines.begin(), coreDeadlines.end());
			int coreIdx = std::distance(coreDeadlines.begin(), earliestCore);

			now = *earliestCore;
			*earliestCore = UINT64_MAX;
			graph_vertex_t v = assignedTasks[coreIdx];
			assignedTasks[coreIdx] = NO_TASK;
			emptyCPUs++;

			// Now, "release" deps
			graph_t::out_edge_iterator ei, eend;
			for (boost::tie(ei, eend) = boost::out_edges(v, _graph); ei != eend; ei++) {
				graph_vertex_t target = boost::target(*ei, _graph);
				int remaining = --predecessors[target];
				assert(remaining >= 0);

				if (remaining == 0) {
					readyTasks.push_back(target);
				}
			}
		} while (readyTasks.empty() && scheduledTasks < vertices);
	}

	// delete[] bitset;
	// delete[] tmpBitset;
}

void TaskiterGraph::localitySchedulingSimhash()
{

}
