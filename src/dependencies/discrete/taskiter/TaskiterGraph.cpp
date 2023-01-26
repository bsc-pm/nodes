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
#include <unistd.h>
#include <unordered_set>

#include "common/MathSupport.hpp"
#include "TaskiterGraph.hpp"

EnvironmentVariable<std::string> TaskiterGraph::_graphOptimization("NODES_ITER_OPTIMIZE", "basic");
EnvironmentVariable<bool> TaskiterGraph::_criticalPathTrackingEnabled("NODES_ITER_TRACK_CRITICAL", false);
EnvironmentVariable<bool> TaskiterGraph::_printGraphStatistics("NODES_ITER_PRINT_STATISTICS", false);
EnvironmentVariable<std::string> TaskiterGraph::_tentativeNumaScheduling("NODES_ITER_NUMA", "none");
EnvironmentVariable<bool> TaskiterGraph::_communcationPriorityPropagation("NODES_ITER_COMM_PRIORITY", false);
EnvironmentVariable<bool> TaskiterGraph::_smartIS("NODES_ITER_SMART_IS", false);

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
					return true;
				});

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
		clustersToSystemNuma.end()
	);
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

				return true;
			});
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
	return ((void *) (((uintptr_t)address) & ~(pageSize - 1)));
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
		clustersToSystemNuma.end()
	);
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
	for (const std::pair<access_address_t const, TaskiterGraphAccessChain>& access : _bottomMap) {
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

				return true;
			});
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
		clustersToSystemNuma.end()
	);
	clusters = clustersToSystemNuma.size();

	boost::property_map<graph_t, boost::vertex_name_t>::type nodemap = boost::get(boost::vertex_name_t(), _graphCpy);
	std::vector<std::vector<uint64_t>> numaScores(vertices, std::vector<uint64_t>(clusters));
	Container::unordered_map<void *, int> pagesToNodes;

	size_t pageSize = sysconf(_SC_PAGESIZE);

	// Ask the OS for the pages NUMA nodes
	// We can do this based on the bottomMap
	for (const std::pair<access_address_t const, TaskiterGraphAccessChain>& access : _bottomMap) {
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

				return true;
			});
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

	// int priorityDelta = 1;
	// assert(priorityDelta >= 0);

	// forEach([priorityDelta](TaskMetadata * t) {
	// 	if (!t->isCommunicationTask())
	// 		t->setPriorityDelta(priorityDelta);
	// }, false);
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

		if(!task)
			continue;

		task->getTaskDataAccesses().forAll([&outAccesses] (void *address, DataAccess *access) -> bool {
			if (access->getType() == READWRITE_ACCESS_TYPE || access->getType() == WRITE_ACCESS_TYPE)
				outAccesses.push_back(address);
			
			return true;
		});

		std::sort(outAccesses.begin(), outAccesses.end());

		for (boost::tie(ei, eend) = boost::out_edges(vertex, _graph); ei != eend; ++ei) {
			graph_t::edge_descriptor e = *ei;
			graph_vertex_t to = boost::target(e, _graph);

			TaskiterGraphNode nodeTo = boost::get(nodemapCyclic, to);
			TaskMetadata *taskTo = nodeTo->getTask();

			if (!taskTo)
				continue;

			bool edgeSelected = false;
			
			taskTo->getTaskDataAccesses().forAll([&outAccesses, &edgeSelected] (void *address, DataAccess *access) -> bool {
				if ((access->getType() == READ_ACCESS_TYPE || access->getType() == READWRITE_ACCESS_TYPE) && 
					std::binary_search(outAccesses.begin(), outAccesses.end(), address)) {
					edgeSelected = true;
					return false;
				}

				return true;
			});

			if (edgeSelected) {
				bool edgeCrossIteration = boost::get(edgemap, e);
				node->setPreferredOutVertex(to, edgeCrossIteration);
				break;
			}
		}

		outAccesses.clear();
	}
}
