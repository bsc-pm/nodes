/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2022-2023 Barcelona Supercomputing Center (BSC)
*/

#include <algorithm>
#include <boost/graph/topological_sort.hpp>
#include <deque>
#include <iostream>
#include <nosv/hwinfo.h>
#include <numaif.h>
#include <queue>
#include <unistd.h>
#include <unordered_set>

#include "common/MathSupport.hpp"
#include "hardware/HardwareInfo.hpp"
#include "system/TaskCreation.hpp"
#include "TaskiterGraph.hpp"
#include "TaskGroupMetadata.hpp"

EnvironmentVariable<std::string> TaskiterGraph::_graphOptimization("NODES_ITER_OPTIMIZE", "basic");
EnvironmentVariable<bool> TaskiterGraph::_criticalPathTrackingEnabled("NODES_ITER_TRACK_CRITICAL", false);
EnvironmentVariable<bool> TaskiterGraph::_printGraph("NODES_ITER_PRINT", false);
EnvironmentVariable<std::string> TaskiterGraph::_tentativeNumaScheduling("NODES_ITER_NUMA", "none");
EnvironmentVariable<bool> TaskiterGraph::_communcationPriorityPropagation("NODES_ITER_COMM_PRIORITY", false);
EnvironmentVariable<bool> TaskiterGraph::_smartIS("NODES_ITER_SMART_IS", false);
EnvironmentVariable<bool> TaskiterGraph::_preferredBinding("NODES_ITER_BIND_LAST_EXECUTION", false);
EnvironmentVariable<bool> TaskiterGraph::_granularityTuning("NODES_ITER_GRANULARITY_TUNING", false);

//! Args block of spawned lambdas
struct SpawnedLambdaArgsBlock {
	std::function<void()> _function;
	std::function<void()> _completionCallback;

	SpawnedLambdaArgsBlock() :
		_function(),
		_completionCallback()
	{
	}
};

static void spawnedLambdaWrapper(void *args)
{
 	SpawnedLambdaArgsBlock *block = (SpawnedLambdaArgsBlock *) args;
	block->_function();
}

static void spawnedLambdaCompletion(void *args)
{
	SpawnedLambdaArgsBlock *block = (SpawnedLambdaArgsBlock *) args;
	block->_completionCallback();
	delete block;
}

void TaskiterGraph::spawnLambda(
	std::function<void()> function,
	std::function<void()> completionCallback,
	char const *label,
	bool fromUserCode
) {
	// This could be more efficient if we use the pre-allocated argsblock, but should do for now
	SpawnedLambdaArgsBlock *args = new SpawnedLambdaArgsBlock();
	args->_function = function;
	args->_completionCallback = completionCallback;

	SpawnFunction::spawnFunction(spawnedLambdaWrapper, (void *)args, spawnedLambdaCompletion, (void *)args, label, fromUserCode);
}

void TaskiterGraph::prioritizeCriticalPath()
{
	// Analyze the graph to figure out the critical task path.
	// The first version just assumes every task takes one second.
	// Then, we will add time tracking and take that into account.

	boost::property_map<graph_t, boost::vertex_name_t>::type nodemap = boost::get(boost::vertex_name_t(), _graphCpy);
	std::unordered_map<graph_vertex_t, int> priorityMap;
	std::vector<graph_vertex_t> reverseTopological;
	graph_t::out_edge_iterator ei, eend;

	boost::topological_sort(_graphCpy, std::back_inserter(reverseTopological));

	for (graph_vertex_t vertex : reverseTopological) {
		int maxPriority = -1;

		for (boost::tie(ei, eend) = boost::out_edges(vertex, _graphCpy); ei != eend; ++ei) {
			graph_t::edge_descriptor e = *ei;
			graph_vertex_t to = boost::target(e, _graphCpy);

			int successorPriority = priorityMap.at(to);
			if (successorPriority > maxPriority)
				maxPriority = successorPriority;
		}

		TaskiterGraphNode node = boost::get(nodemap, vertex);
		TaskMetadata *task = node->getTask();

		if (task) {
			// This is adding uint64_t to the int maxPriority, which has a potential to overflow
			// when iterations are very large
			maxPriority += std::max(task->getElapsedTime(), 1UL);
			assert(maxPriority >= 0);
			task->setPriority(maxPriority);
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
		node->setVertex(gToTr[originalVertex]);
	}

	graph_t::edge_iterator ei, eend;
	for (boost::tie(ei, eend) = boost::edges(processedGraph); ei != eend; ei++) {
		graph_t::edge_descriptor e = *ei;
		boost::put(edgemapProcessed, e, false);
	}

	_graph = processedGraph;
}

struct TaskiterGraph::EdgeHash {
	std::size_t operator()(const graph_t::edge_descriptor &e) const
	{
		std::size_t seed = 0;
		boost::hash_combine(seed, e.m_source);
		boost::hash_combine(seed, e.m_target);
		return seed;
	}
};

struct TaskiterGraph::EdgeEqual {
	bool operator()(const graph_t::edge_descriptor &a, const graph_t::edge_descriptor &b) const
	{
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
	},
		_graph);
}

void TaskiterGraph::localityScheduling()
{
	boost::property_map<graph_t, boost::vertex_name_t>::type nodemap = boost::get(boost::vertex_name_t(), _graphCpy);
	int vertices = boost::num_vertices(_graphCpy);
	int clusters = 2;
	int slotsPerCluster = 24;
	int initialPriority = vertices;

	std::vector<uint64_t> coreDeadlines(clusters * slotsPerCluster, 0);
	std::vector<int> predecessors(vertices);
	std::vector<TaskMetadata *> assignedTasks(clusters * slotsPerCluster, nullptr);
	std::deque<graph_vertex_t> readyTasks;

	// Initialize precedessors
	graph_t::vertex_iterator vi, vend;
	for (boost::tie(vi, vend) = boost::vertices(_graphCpy); vi != vend; vi++) {
		graph_vertex_t v = *vi;
		predecessors[v] = boost::in_degree(v, _graphCpy);
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
					return true; });
			}
		}

		TaskMetadata *bestSuccessor = nullptr;
		std::deque<graph_vertex_t>::iterator bestSuccessorIt;
		int matches = -1;

		for (std::deque<graph_vertex_t>::iterator vIterator = readyTasks.begin(); vIterator != readyTasks.end();) {
			graph_vertex_t v = *vIterator;
			TaskiterGraphNode node = boost::get(nodemap, v);
			TaskMetadata *task = node->getTask();

			if (!task) {
				// Node is a ReductionInfo
				// "Release" accesses
				assert(false);
				vIterator = readyTasks.erase(vIterator);
			} else {
				// Calculate score
				int score = 0;
				task->getTaskDataAccesses().forAll([&accessCount, &score](void *address, DataAccess *) -> bool {
					score += accessCount[address];
					return true; });

				if (score > matches) {
					matches = score;
					bestSuccessor = task;
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
			readyTasks.erase(bestSuccessorIt);
		} else {
			assignedTasks[coreIdx] = nullptr;
			noSuccessor = true;
			// TODO In this case, not all tasks will be scheduled
		}

		bool foundTask = false;
		if (oldTask) {
			// Now, "release" deps
			graph_vertex_t v = getNodeFromTask(oldTask)->getVertex();
			graph_t::out_edge_iterator ei, eend;
			for (boost::tie(ei, eend) = boost::out_edges(v, _graphCpy); ei != eend; ei++) {
				graph_vertex_t target = boost::target(*ei, _graphCpy);
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
	const int vertices = boost::num_vertices(_graphCpy);
	int clusters = nosv_get_num_numa_nodes();
	assert(clusters > 0);

	std::vector<int> clustersToSystemNuma(clusters);
	for (int i = 0; i < clusters; ++i)
		clustersToSystemNuma[i] = nosv_get_system_numa_id(i);

	// Filter out clusters without CPUs assigned
	clustersToSystemNuma.erase(
		std::remove_if(clustersToSystemNuma.begin(), clustersToSystemNuma.end(),
			[](const int &i) { return nosv_get_num_cpus_in_numa(i) == 0; }),
		clustersToSystemNuma.end());
	clusters = clustersToSystemNuma.size();

	const int slotsPerCluster = nosv_get_num_cpus_in_numa(clustersToSystemNuma[0]);
	const graph_vertex_t NO_TASK = (graph_vertex_t)-1;
	int initialPriority = vertices;

	boost::property_map<graph_t, boost::vertex_name_t>::type nodemap = boost::get(boost::vertex_name_t(), _graphCpy);

	std::unique_ptr<uint32_t[]> bitset = std::make_unique<uint32_t[]>(bitsetWords * vertices);
	std::unique_ptr<uint32_t[]> tmpBitset = std::make_unique<uint32_t[]>(bitsetWords);

	std::vector<uint64_t> coreDeadlines(clusters * slotsPerCluster, 0);
	std::vector<int> predecessors(vertices);
	std::vector<graph_vertex_t> assignedTasks(clusters * slotsPerCluster, NO_TASK);
	std::deque<graph_vertex_t> readyTasks;

	graph_t::vertex_iterator vi, vend;
	for (boost::tie(vi, vend) = boost::vertices(_graphCpy); vi != vend; vi++) {
		graph_vertex_t v = *vi;
		TaskiterGraphNode node = boost::get(nodemap, v);
		TaskMetadata *task = node->getTask();

		if (task) {
			task->getTaskDataAccesses().forAll([&bitset, &v, bitsetWords, this](void *address, DataAccess *) -> bool {
				Container::unordered_map<access_address_t, TaskiterGraphAccessChain>::iterator mapIterator = _bottomMap.find(address);
				assert(mapIterator != _bottomMap.end());
				int index = std::distance(_bottomMap.begin(), mapIterator);

				uint32_t &currentBitsetLocation = bitset[v * bitsetWords + index / 32];
				int bit = index % 32;
				currentBitsetLocation |= (1 << bit);

				return true; });
		}

		predecessors[v] = boost::in_degree(v, _graphCpy);
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

			std::deque<graph_vertex_t>::iterator bestSuccessorIt;
			TaskMetadata *bestSuccessor = nullptr;
			int matches = -1;
			for (std::deque<graph_vertex_t>::iterator vIterator = readyTasks.begin(); vIterator != readyTasks.end(); vIterator++) {
				graph_vertex_t v = *vIterator;
				TaskiterGraphNode node = boost::get(nodemap, v);
				TaskMetadata *task = node->getTask();

				int score = 0;
				for (int i = 0; i < bitsetWords; ++i)
					score += __builtin_popcountl(tmpBitset[i] & bitset[v * bitsetWords + i]);

				if (score > matches) {
					matches = score;
					bestSuccessor = task;
					bestSuccessorIt = vIterator;
				}
			}

			assignedTasks[cpu] = *bestSuccessorIt;
			coreDeadlines[cpu] += now + bestSuccessor->getElapsedTime();
			bestSuccessor->setAffinity(clustersToSystemNuma[clusterIdx], NOSV_AFFINITY_LEVEL_NUMA, NOSV_AFFINITY_TYPE_PREFERRED);
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

			if (v == NO_TASK)
				break;

			assignedTasks[coreIdx] = NO_TASK;
			emptyCPUs++;

			// Now, "release" deps
			graph_t::out_edge_iterator ei, eend;
			for (boost::tie(ei, eend) = boost::out_edges(v, _graphCpy); ei != eend; ei++) {
				graph_vertex_t target = boost::target(*ei, _graphCpy);
				int remaining = --predecessors[target];
				assert(remaining >= 0);

				if (remaining == 0) {
					readyTasks.push_back(target);
				}
			}
		} while (readyTasks.empty() && scheduledTasks < vertices);
	}
}

static inline void *alignToPageBoundary(void *address, size_t pageSize)
{
	return ((void *)(((uintptr_t)address) & ~(pageSize - 1)));
}

void TaskiterGraph::localitySchedulingMovePages()
{
	// Set up data structures
	const int vertices = boost::num_vertices(_graphCpy);
	int clusters = nosv_get_num_numa_nodes();
	assert(clusters > 0);

	std::vector<int> clustersToSystemNuma(clusters);
	int maxSystemNuma = 0;
	for (int i = 0; i < clusters; ++i) {
		clustersToSystemNuma[i] = nosv_get_system_numa_id(i);

		if (clustersToSystemNuma[i] > maxSystemNuma)
			maxSystemNuma = clustersToSystemNuma[i];
	}

	std::vector<int> systemNumaToClusters(maxSystemNuma + 1);
	for (int i = 0; i < clusters; ++i)
		systemNumaToClusters[clustersToSystemNuma[i]] = i;

	// Filter out clusters without CPUs assigned
	clustersToSystemNuma.erase(
		std::remove_if(clustersToSystemNuma.begin(), clustersToSystemNuma.end(),
			[](const int &i) { return nosv_get_num_cpus_in_numa(i) == 0; }),
		clustersToSystemNuma.end());
	clusters = clustersToSystemNuma.size();

	const int slotsPerCluster = nosv_get_num_cpus_in_numa(clustersToSystemNuma[0]);
	const graph_vertex_t NO_TASK = (graph_vertex_t)-1;
	int initialPriority = vertices;

	boost::property_map<graph_t, boost::vertex_name_t>::type nodemap = boost::get(boost::vertex_name_t(), _graphCpy);

	std::vector<std::vector<uint64_t>> numaScores(vertices, std::vector<uint64_t>(clusters));

	std::vector<uint64_t> coreDeadlines(clusters * slotsPerCluster, 0);
	std::vector<int> predecessors(vertices);
	std::vector<graph_vertex_t> assignedTasks(clusters * slotsPerCluster, NO_TASK);
	std::vector<std::deque<graph_vertex_t>> readyTasks(clusters);
	int nReadyTasks = 0;

	Container::unordered_map<void *, int> pagesToNodes;

	size_t pageSize = sysconf(_SC_PAGESIZE);

	// Ask the OS for the pages NUMA nodes
	// We can do this based on the bottomMap
	for (const std::pair<access_address_t const, TaskiterGraphAccessChain> &access : _bottomMap) {
		void *page = alignToPageBoundary(access.first, pageSize);
		pagesToNodes[page] = -1;
	}

	std::vector<void *> pages;
	pages.reserve(pagesToNodes.size());
	for (const std::pair<void *const, int> &page : pagesToNodes)
		pages.push_back(page.first);

	std::vector<int> nodes(pages.size());
	__attribute__((unused)) long ret = move_pages(0, pages.size(), pages.data(), nullptr, nodes.data(), 0);
	assert(ret >= 0);

	// Pass back to map
	for (size_t i = 0; i < pages.size(); ++i) {
		if (nodes[i] < 0) {
			// Probably un-allocated memory. Assign random cluster
			pagesToNodes[pages[i]] = i % clusters;
		} else {
			pagesToNodes[pages[i]] = systemNumaToClusters[nodes[i]];
		}
	}

	graph_t::vertex_iterator vi, vend;
	for (boost::tie(vi, vend) = boost::vertices(_graphCpy); vi != vend; vi++) {
		graph_vertex_t v = *vi;
		TaskiterGraphNode node = boost::get(nodemap, v);
		TaskMetadata *task = node->getTask();

		if (task) {
			task->getTaskDataAccesses().forAll([&pagesToNodes, &numaScores, &v, this, &pageSize](void *address, DataAccess *access) -> bool {
				void *page = alignToPageBoundary(address, pageSize);
				int numaNode = pagesToNodes[page];

				// This is very simplistic, because we don't care how many bytes are on each node
				// Maybe the best way would be to count bytes _and_ then relativize the scores
				if (numaNode >= 0)
					numaScores[v][numaNode] += access->getAccessRegion().getSize();

				return true; });
		}

		int bestNumaNode = std::distance(numaScores[v].begin(), std::max_element(numaScores[v].begin(), numaScores[v].end()));
		predecessors[v] = boost::in_degree(v, _graphCpy);
		if (!predecessors[v]) {
			readyTasks[bestNumaNode].push_back(v);
			nReadyTasks++;
		}
	}

	// Main scheduling

	assert(!readyTasks.empty() || !vertices);
	int emptyCPUs = clusters * slotsPerCluster;
	int scheduledTasks = 0;
	uint64_t now = 0;

	while (scheduledTasks < vertices) {
		// First scheduling pass
		if (nReadyTasks > 0 && emptyCPUs) {
			for (size_t cpu = 0; cpu < assignedTasks.size(); ++cpu) {
				int clusterIdx = cpu / slotsPerCluster;
				if (assignedTasks[cpu] == NO_TASK && !readyTasks[clusterIdx].empty()) {
					graph_vertex_t v = readyTasks[clusterIdx].front();
					readyTasks[clusterIdx].pop_front();
					assignedTasks[cpu] = v;
					nReadyTasks--;
					emptyCPUs--;
					scheduledTasks++;

					TaskiterGraphNode node = boost::get(nodemap, v);
					TaskMetadata *task = node->getTask();
					if (task) {
						task->setAffinity(clustersToSystemNuma[clusterIdx], NOSV_AFFINITY_LEVEL_NUMA, NOSV_AFFINITY_TYPE_PREFERRED);
						assert(initialPriority - 1 >= 0);
						task->setPriority(initialPriority--);

						coreDeadlines[cpu] = now + task->getElapsedTime();
					} else {
						coreDeadlines[cpu] = now + 1;
					}
				}
			}
		}

		// Second pass
		while (emptyCPUs && nReadyTasks) {
			int cpu = std::distance(assignedTasks.begin(), std::find(assignedTasks.begin(), assignedTasks.end(), NO_TASK));
			graph_vertex_t v = 0;
			int clusterIdx;

			for (clusterIdx = 0; clusterIdx < clusters; ++clusterIdx) {
				if (!readyTasks[clusterIdx].empty()) {
					v = readyTasks[clusterIdx].front();
					readyTasks[clusterIdx].pop_front();
					break;
				}
			}

			assignedTasks[cpu] = v;
			nReadyTasks--;
			emptyCPUs--;
			scheduledTasks++;

			TaskiterGraphNode node = boost::get(nodemap, v);
			TaskMetadata *task = node->getTask();
			if (task) {
				task->setAffinity(clustersToSystemNuma[clusterIdx], NOSV_AFFINITY_LEVEL_NUMA, NOSV_AFFINITY_TYPE_PREFERRED);
				assert(initialPriority - 1 >= 0);
				task->setPriority(initialPriority--);

				coreDeadlines[cpu] = now + task->getElapsedTime();
			} else {
				coreDeadlines[cpu] = now + 1;
			}
		}

		do {
			// Find next task to finish
			std::vector<uint64_t>::iterator earliestCore = std::min_element(coreDeadlines.begin(), coreDeadlines.end());
			int coreIdx = std::distance(coreDeadlines.begin(), earliestCore);

			now = *earliestCore;
			*earliestCore = UINT64_MAX;
			graph_vertex_t v = assignedTasks[coreIdx];

			if (v == NO_TASK)
				break;

			assignedTasks[coreIdx] = NO_TASK;
			emptyCPUs++;

			// Now, "release" deps
			graph_t::out_edge_iterator ei, eend;
			for (boost::tie(ei, eend) = boost::out_edges(v, _graphCpy); ei != eend; ei++) {
				graph_vertex_t target = boost::target(*ei, _graphCpy);
				int remaining = --predecessors[target];
				assert(remaining >= 0);

				if (remaining == 0) {
					int bestNumaNode = std::distance(numaScores[target].begin(), std::max_element(numaScores[target].begin(), numaScores[target].end()));
					readyTasks[bestNumaNode].push_back(target);
					nReadyTasks++;
				}
			}
		} while (readyTasks.empty() && scheduledTasks < vertices);
	}
}

void TaskiterGraph::localitySchedulingMovePagesSimple()
{
	// Set up data structures
	const int vertices = boost::num_vertices(_graphCpy);
	int clusters = nosv_get_num_numa_nodes();
	assert(clusters > 0);

	std::vector<int> clustersToSystemNuma(clusters);
	int maxSystemNuma = 0;
	for (int i = 0; i < clusters; ++i) {
		clustersToSystemNuma[i] = nosv_get_system_numa_id(i);

		if (clustersToSystemNuma[i] > maxSystemNuma)
			maxSystemNuma = clustersToSystemNuma[i];
	}

	std::vector<int> systemNumaToClusters(maxSystemNuma + 1);
	for (int i = 0; i < clusters; ++i)
		systemNumaToClusters[clustersToSystemNuma[i]] = i;

	// Filter out clusters without CPUs assigned
	clustersToSystemNuma.erase(
		std::remove_if(clustersToSystemNuma.begin(), clustersToSystemNuma.end(),
			[](const int &i) { return nosv_get_num_cpus_in_numa(i) == 0; }),
		clustersToSystemNuma.end());
	clusters = clustersToSystemNuma.size();

	boost::property_map<graph_t, boost::vertex_name_t>::type nodemap = boost::get(boost::vertex_name_t(), _graphCpy);
	std::vector<std::vector<uint64_t>> numaScores(vertices, std::vector<uint64_t>(clusters));
	Container::unordered_map<void *, int> pagesToNodes;

	size_t pageSize = sysconf(_SC_PAGESIZE);

	// Ask the OS for the pages NUMA nodes
	// We can do this based on the bottomMap
	for (const std::pair<access_address_t const, TaskiterGraphAccessChain> &access : _bottomMap) {
		void *page = alignToPageBoundary(access.first, pageSize);
		pagesToNodes[page] = -1;
	}

	std::vector<void *> pages;
	pages.reserve(pagesToNodes.size());
	for (const std::pair<void *const, int> &page : pagesToNodes)
		pages.push_back(page.first);

	std::vector<int> nodes(pages.size());
	__attribute__((unused)) long ret = move_pages(0, pages.size(), pages.data(), nullptr, nodes.data(), 0);
	assert(ret >= 0);

	// Pass back to map
	for (size_t i = 0; i < pages.size(); ++i) {
		if (nodes[i] < 0) {
			// Probably un-allocated memory. Assign random cluster
			pagesToNodes[pages[i]] = i % clusters;
		} else {
			pagesToNodes[pages[i]] = systemNumaToClusters[nodes[i]];
		}
	}

	graph_t::vertex_iterator vi, vend;
	for (boost::tie(vi, vend) = boost::vertices(_graphCpy); vi != vend; vi++) {
		graph_vertex_t v = *vi;
		TaskiterGraphNode node = boost::get(nodemap, v);
		TaskMetadata *task = node->getTask();

		if (task) {
			task->getTaskDataAccesses().forAll([&pagesToNodes, &numaScores, &v, this, &pageSize](void *address, DataAccess *access) -> bool {
				void *page = alignToPageBoundary(address, pageSize);
				int numaNode = pagesToNodes[page];

				// This is very simplistic, because we don't care how many bytes are on each node
				// Maybe the best way would be to count bytes _and_ then relativize the scores
				if (numaNode >= 0)
					numaScores[v][numaNode] += access->getAccessRegion().getSize();

				return true; });
			int bestNumaNode = std::distance(numaScores[v].begin(), std::max_element(numaScores[v].begin(), numaScores[v].end()));
			task->setAffinity(clustersToSystemNuma[bestNumaNode], NOSV_AFFINITY_LEVEL_NUMA, NOSV_AFFINITY_TYPE_PREFERRED);
		}
	}
}

void TaskiterGraph::communicationPriorityPropagation()
{
	boost::property_map<graph_t, boost::vertex_name_t>::type nodemap = boost::get(boost::vertex_name_t(), _graphCpy);
	boost::property_map<graph_t, boost::vertex_name_t>::type nodemapCyclic = boost::get(boost::vertex_name_t(), _graph);
	boost::property_map<graph_t, boost::edge_name_t>::type edgemap = boost::get(boost::edge_name_t(), _graph);
	std::unordered_map<graph_vertex_t, int> priorityMap;
	std::vector<graph_vertex_t> reverseTopological;
	graph_t::out_edge_iterator ei, eend;

	// Back-propagate priorities from communication tasks.
	// Using a reverse topological sorts serves to make a single pass through the graph
	// We assign maximum priority to communications, then pass ones less priority to anyone else

	boost::topological_sort(_graphCpy, std::back_inserter(reverseTopological));
	int minNonZeroPriority = INT_MAX;
	bool firstIt = true;

	while (true) {
		for (graph_vertex_t vertex : reverseTopological) {
			int maxPriority = 0;

			TaskiterGraphNode node = boost::get(nodemap, vertex);
			TaskMetadata *task = node->getTask();

			if (task) {
				if (task->isCommunicationTask())
					maxPriority = INT_MAX;
			}

			if (!maxPriority) {
				for (boost::tie(ei, eend) = boost::out_edges(vertex, _graphCpy); ei != eend; ++ei) {
					graph_t::edge_descriptor e = *ei;
					graph_vertex_t to = boost::target(e, _graphCpy);

					int successorPriority = priorityMap.at(to);
					if (successorPriority > maxPriority)
						maxPriority = successorPriority;
				}

				if (maxPriority)
					maxPriority = INT_MAX - 1;
			}

			if (maxPriority > priorityMap[vertex]) {
				if (task) {
					if (maxPriority != INT_MAX)
						task->setPriorityDelta(1);

					assert(maxPriority >= 0);
					task->setPriority(maxPriority);
				}

				assert(priorityMap[vertex] <= maxPriority);

				priorityMap[vertex] = maxPriority;

				if (maxPriority != 0 && maxPriority < minNonZeroPriority)
					minNonZeroPriority = maxPriority;
			}
		}

		if (!firstIt)
			break;

		// Propagate priorities to next iteration
		graph_t::vertex_iterator vi, vend;
		for (boost::tie(vi, vend) = boost::vertices(_graph); vi != vend; vi++) {
			graph_vertex_t vertex = *vi;
			TaskiterGraphNode node = boost::get(nodemapCyclic, vertex);
			TaskMetadata *task = node->getTask();

			int maxPriority = 0;
			for (boost::tie(ei, eend) = boost::out_edges(vertex, _graph); ei != eend; ++ei) {
				graph_t::edge_descriptor e = *ei;
				bool edgeCrossIteration = boost::get(edgemap, e);
				if (edgeCrossIteration) {
					// Try to propagate priority accross one iteration barrier.
					graph_vertex_t to = boost::target(e, _graph);

					int successorPriority = priorityMap.at(to);
					if (successorPriority > maxPriority)
						maxPriority = INT_MAX - 1;
				}
			}

			if (maxPriority > priorityMap[vertex]) {
				if (task) {
					assert(maxPriority != INT_MAX);
					task->setPriorityDelta(1);
					assert(maxPriority >= 0);
					task->setPriority(maxPriority);
				}

				priorityMap[vertex] = maxPriority;
				if (maxPriority != 0 && maxPriority < minNonZeroPriority)
					minNonZeroPriority = maxPriority;
			}
		}

		firstIt = false;
	}
}

void TaskiterGraph::immediateSuccessorProcess()
{
	// Explore the dependencies of a task on its successors, select out -> in edges as IS
	boost::property_map<graph_t, boost::vertex_name_t>::type nodemapCyclic = boost::get(boost::vertex_name_t(), _graph);
	boost::property_map<graph_t, boost::edge_name_t>::type edgemap = boost::get(boost::edge_name_t(), _graph);
	graph_t::out_edge_iterator ei, eend;

	std::vector<void *> outAccesses;

	graph_t::vertex_iterator vi, vend;
	for (boost::tie(vi, vend) = boost::vertices(_graph); vi != vend; vi++) {
		graph_vertex_t vertex = *vi;
		TaskiterGraphNode node = boost::get(nodemapCyclic, vertex);
		TaskMetadata *task = node->getTask();

		if (!task)
			continue;

		task->getTaskDataAccesses().forAll([&outAccesses](void *address, DataAccess *access) -> bool {
			if (access->getType() == READWRITE_ACCESS_TYPE || access->getType() == WRITE_ACCESS_TYPE)
				outAccesses.push_back(address);

			return true; });

		std::sort(outAccesses.begin(), outAccesses.end());

		for (boost::tie(ei, eend) = boost::out_edges(vertex, _graph); ei != eend; ++ei) {
			graph_t::edge_descriptor e = *ei;
			graph_vertex_t to = boost::target(e, _graph);

			TaskiterGraphNode nodeTo = boost::get(nodemapCyclic, to);
			TaskMetadata *taskTo = nodeTo->getTask();

			if (!taskTo)
				continue;

			bool edgeSelected = false;

			taskTo->getTaskDataAccesses().forAll([&outAccesses, &edgeSelected](void *address, DataAccess *access) -> bool {
				if ((access->getType() == READ_ACCESS_TYPE || access->getType() == READWRITE_ACCESS_TYPE) &&
					std::binary_search(outAccesses.begin(), outAccesses.end(), address)) {
					edgeSelected = true;
					return false;
				}

				return true; });

			if (edgeSelected) {
				bool edgeCrossIteration = boost::get(edgemap, e);
				node->setPreferredOutVertex(to, edgeCrossIteration);
				break;
			}
		}

		outAccesses.clear();
	}
}

static TaskiterGraph::graph_vertex_t getOnlySuccessor(TaskiterGraph::graph_vertex_t v, TaskiterGraph::graph_t &graph)
{
	assert(boost::out_degree(v, graph) == 1);
	return (*(boost::out_edges(v, graph).first)).m_target;
}

TaskGroupMetadata *getEmptyGroupTask(TaskMetadata *parent)
{
	nosv_task_t task;
	void *argsBlockPointer;

	TaskCreation::createTask<TaskGroupMetadata>(
		TaskGroupMetadata::getGroupTaskInfo(),
		nullptr,
		nullptr,
		sizeof(TaskGroupMetadata *),
		&argsBlockPointer,
		(void **)&task,
		0,
		0);

	assert(task != nullptr);
	assert(argsBlockPointer != nullptr);

	TaskGroupMetadata *group = (TaskGroupMetadata *)TaskMetadata::getTaskMetadata(task);
	TaskGroupMetadata **ptr = (TaskGroupMetadata **)argsBlockPointer;
	*ptr = group;

	group->setParent(parent->getTaskHandle());
	group->incrementOriginalPredecessorCount();
	group->setIterationCount(parent->getIterationCount());
	// Simulate it has been finished already
	group->increaseWakeUpCount(-1);

	return group;
}

static void graphTransformSequential(TaskiterGraph::graph_t &g, TaskMetadata *parent)
{
	boost::property_map<TaskiterGraph::graph_t, boost::vertex_name_t>::type nodemap = boost::get(boost::vertex_name_t(), g);
	TaskiterGraph::graph_t::edge_iterator ei, eend;
	TaskiterGraph::graph_t::vertex_iterator vi, vend;

	std::vector<TaskiterGraph::graph_vertex_t> topologicalOrder;
	boost::topological_sort(g, std::back_inserter(topologicalOrder));
	int vertices = boost::num_vertices(g);
	std::vector<bool> visited(vertices);
	std::vector<int> equivalence(vertices, -1);

	TaskiterGraph::graph_t transformedGraph;

	int groupsCreated = 0;
	// The idea is to find the relevant chains, group them and place them in the transformed graph.
	// The ordered traversal will allow us to build the graph in a single pass
	for (std::vector<TaskiterGraph::graph_vertex_t>::reverse_iterator it = topologicalOrder.rbegin(); it != topologicalOrder.rend(); ++it) {
		TaskiterGraph::graph_vertex_t v = *it;

		if (visited[v])
			continue;

		visited[v] = true;

		TaskiterGraphNode node = boost::get(nodemap, v);

		// If it's not a part of a potential chain, insert as-is.
		if (boost::out_degree(v, g) != 1 || !node->canBeGrouped()) {
			// Insert into the transformed graph
			TaskiterGraph::VertexProperty prop(node);
			TaskiterGraph::graph_vertex_t newVertex = boost::add_vertex(prop, transformedGraph);
			equivalence[v] = newVertex;
			node->setVertex(newVertex);
		} else {
			// Find the last member of this chain
			TaskiterGraph::graph_vertex_t last = v;
			TaskiterGraph::graph_vertex_t next = getOnlySuccessor(last, g);
			TaskiterGraphNode nextNode = boost::get(nodemap, next);

			while (nextNode->canBeGrouped() && boost::in_degree(next, g) == 1 && boost::out_degree(next, g) == 1) {
				last = next;
				visited[last] = true;
				next = getOnlySuccessor(last, g);
			}

			if (nextNode->canBeGrouped() && boost::in_degree(next, g) == 1) {
				last = next;
				visited[last] = true;
			}

			if (last == v) {
				// No grouping
				TaskiterGraph::VertexProperty prop(node);
				TaskiterGraph::graph_vertex_t newVertex = boost::add_vertex(prop, transformedGraph);
				equivalence[v] = newVertex;
				node->setVertex(newVertex);
			} else {
				// Grouping
				// The issue here is that probably many vertices that are pointed by last won't be in the transformed graph yet.
				// We can add them at this point, but they may be the source

				// Create a new grouped task
				TaskGroupMetadata *group = getEmptyGroupTask(parent);
				TaskiterGraphNode nGroup = (TaskiterNode *)group;
				TaskiterGraph::VertexProperty prop(nGroup);
				// Add it to the graph
				TaskiterGraph::graph_vertex_t groupVertex = boost::add_vertex(prop, transformedGraph);
				group->setVertex(groupVertex);

				while (v != last) {
					TaskiterGraphNode currNode = boost::get(nodemap, v);
					// May delete node
					group->addTask(currNode);
					equivalence[v] = groupVertex;
					v = getOnlySuccessor(v, g);
				}

				TaskiterGraphNode currNode = boost::get(nodemap, last);
				// May delete node
				group->addTask(currNode);
				equivalence[last] = groupVertex;
				groupsCreated++;
			}
		}
	}

	// Copy the edges back
	for (boost::tie(ei, eend) = boost::edges(g); ei != eend; ei++) {
		TaskiterGraph::graph_t::edge_descriptor e = *ei;
		TaskiterGraph::graph_vertex_t from = equivalence[e.m_source];
		TaskiterGraph::graph_vertex_t to = equivalence[e.m_target];
		if (from != to)
			boost::add_edge(from, to, transformedGraph);
	}

	g = transformedGraph;
}

// Given: MAX, DAG
// for h from 0 to DAG.HEIGHT do
//   TASKS = list of tasks of height h
//   sort TASKS by number of successors and predecessors
//   AVERAGE = length(TASKS) / MAX
//   for i from 0 to MAX do
// 	MASTER = first in TASKS
// 		for j from 0 to AVERAGE do
// 			select MINIMAL in TASKS such as the number of
// 			successors and predecessors of MINIMAL union MASTER be as small as possible
// 			remove MINIMAL from TASKS
// 			MASTER becomes MASTER union MINIMAL
// 		end
// 	end
//   end

static void graphTransformFront(TaskiterGraph::graph_t &g, TaskMetadata *parent, uint64_t us)
{
	boost::property_map<TaskiterGraph::graph_t, boost::vertex_name_t>::type nodemap = boost::get(boost::vertex_name_t(), g);
	TaskiterGraph::graph_t::edge_iterator ei, eend;
	TaskiterGraph::graph_t::out_edge_iterator oei, oeend;
	TaskiterGraph::graph_t::vertex_iterator vi, vend;

	int vertices = boost::num_vertices(g);
	std::vector<int> outstanding_deps(vertices);
	std::vector<int> equivalence(vertices);
	std::unique_ptr<std::vector<TaskiterGraph::graph_vertex_t>> ready = std::make_unique<std::vector<TaskiterGraph::graph_vertex_t>>();
	std::unique_ptr<std::vector<TaskiterGraph::graph_vertex_t>> freed = std::make_unique<std::vector<TaskiterGraph::graph_vertex_t>>();

	TaskiterGraph::graph_t transformedGraph;
	uint64_t totalTime = 0;
	uint64_t partialTime = 0;

#define ACUM_TIME(v, var)                                              \
	{                                                                  \
		TaskiterGraphNode __node = boost::get(nodemap, v);             \
		TaskMetadata *__task = __node->getTask();                      \
		if (__task)                                                    \
			var += std::max(__task->getElapsedTime(), (uint64_t)1ULL); \
		else                                                           \
			var += 1;                                                  \
		assert(var > 0);                                               \
	}

	// Initialize outstanding deps for vertices,
	for (boost::tie(vi, vend) = boost::vertices(g); vi != vend; vi++) {
		TaskiterGraph::graph_vertex_t v = *vi;

		int deps = boost::in_degree(v, g);
		outstanding_deps[v] = deps;

		if (deps == 0) {
			ACUM_TIME(v, totalTime);
			ready->push_back(v);
		}
	}

	while (!ready->empty()) {
		int width = ready->size();
		// Calculate the average size of group
		uint64_t groupsToMake = ((totalTime + us - 1) / us);
		if (groupsToMake < HardwareInfo::getNumCpus())
			groupsToMake = HardwareInfo::getNumCpus();

		int average = std::max((width / groupsToMake), (uint64_t)1ULL);
		assert(average >= 1);

		if (average == 1) {
			for (TaskiterGraph::graph_vertex_t v : *ready) {
				for (boost::tie(oei, oeend) = boost::out_edges(v, g); oei != oeend; oei++) {
					TaskiterGraph::graph_vertex_t dest = (*oei).m_target;
					int res = --outstanding_deps[dest];
					assert(res >= 0);
					if (res == 0) {
						ACUM_TIME(v, partialTime);
						freed->push_back(dest);
					}
				}

				TaskiterGraphNode node = boost::get(nodemap, v);
				assert(node);
				TaskiterGraph::VertexProperty prop(node);
				TaskiterGraph::graph_vertex_t newVertex = boost::add_vertex(prop, transformedGraph);
				equivalence[v] = newVertex;
				node->setVertex(newVertex);
			}
		} else {
			TaskGroupMetadata *group = nullptr;
			TaskiterGraph::graph_vertex_t groupVertex;

			int spotsLeft = 0;

			for (TaskiterGraph::graph_vertex_t v : *ready) {
				TaskiterGraphNode node = boost::get(nodemap, v);

				// Prevent grouping control tasks
				if (!node->canBeGrouped()) {
					TaskiterGraph::VertexProperty prop(node);
					TaskiterGraph::graph_vertex_t newVertex = boost::add_vertex(prop, transformedGraph);
					equivalence[v] = newVertex;
					node->setVertex(newVertex);
				} else {
					// Group
					if (spotsLeft == 0) {
						group = getEmptyGroupTask(parent);
						TaskiterGraph::VertexProperty prop((TaskiterNode *)group);
						groupVertex = boost::add_vertex(prop, transformedGraph);
						group->setVertex(groupVertex);
						spotsLeft = average;
					}

					equivalence[v] = groupVertex;
					// May delete node
					group->addTask(node);
					assert(group->getTask()->getGroup() == nullptr);
					--spotsLeft;
				}

				for (boost::tie(oei, oeend) = boost::out_edges(v, g); oei != oeend; oei++) {
					TaskiterGraph::graph_vertex_t dest = (*oei).m_target;
					int res = --outstanding_deps[dest];
					assert(res >= 0);
					if (res == 0) {
						ACUM_TIME(v, partialTime);
						freed->push_back(dest);
					}
				}
			}
		}

		std::swap(ready, freed);
		totalTime = partialTime;
		partialTime = 0;
		freed->clear();
	}

	// Copy the edges back
	for (boost::tie(ei, eend) = boost::edges(g); ei != eend; ei++) {
		TaskiterGraph::graph_t::edge_descriptor e = *ei;
		TaskiterGraph::graph_vertex_t from = equivalence[e.m_source];
		TaskiterGraph::graph_vertex_t to = equivalence[e.m_target];
		if (from != to)
			boost::add_edge(from, to, transformedGraph);
	}

#undef ACUM_TIME

	g = transformedGraph;
}

void TaskiterGraph::granularityTuning()
{
	// There are three possible merging algorithms, that can be applied in different orderings.
	// First there is the "Sequential", merging single-predecessor single-successor task chains
	// Then, "Front", which aggregates tasks in the same DAG depth with other similar ones
	// Lastly, "Depth Front", which is a little bit more complex

	TaskMetadata *parent = _tasks[0][0]->getTask()->getParent();

	// Sequential
	graphTransformSequential(_graph, parent);

#if PRINT_TASKITER_GRAPH
	if (_printGraph.getValue()) {
		std::ofstream dot("after-seq.dot");
		boost::write_graphviz(dot, _graph);
	}
#endif

	// Front
	graphTransformFront(_graph, parent, 1000 /* us minimum task */);

#if PRINT_TASKITER_GRAPH
	if (_printGraph.getValue()) {
		std::ofstream dot("after-front.dot");
		boost::write_graphviz(dot, _graph);
	}
#endif
}