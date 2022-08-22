/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2022 Barcelona Supercomputing Center (BSC)
*/

#include <boost/graph/topological_sort.hpp>
#include <unordered_set>

#include "TaskiterGraph.hpp"

EnvironmentVariable<std::string> TaskiterGraph::_graphOptimization("NODES_ITER_OPTIMIZE", "basic");
EnvironmentVariable<bool> TaskiterGraph::_criticalPathTrackingEnabled("NODES_ITER_TRACK_CRITICAL", true);
EnvironmentVariable<bool> TaskiterGraph::_printGraphStatistics("NODES_ITER_PRINT_STATISTICS", false);
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
