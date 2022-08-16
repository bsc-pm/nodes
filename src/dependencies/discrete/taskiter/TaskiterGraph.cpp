/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2022 Barcelona Supercomputing Center (BSC)
*/

#include <boost/graph/topological_sort.hpp>

#include "TaskiterGraph.hpp"

EnvironmentVariable<bool> TaskiterGraph::_graphOptimizationEnabled("NODES_ITER_OPTIMIZE", true);
EnvironmentVariable<bool> TaskiterGraph::_criticalPathTrackingEnabled("NODES_ITER_TRACK_CRITICAL", true);

void TaskiterGraph::prioritizeCriticalPath()
{
	if (!_criticalPathTrackingEnabled.getValue())
		return;

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

		maxPriority++;
		priorityMap[vertex] = maxPriority;

		// Now assign it to the node if it is a task
		TaskiterGraphNode node = boost::get(nodemap, vertex);
		TaskMetadata **task = boost::get<TaskMetadata *>(&node);
		if (task)
			(*task)->setPriority(maxPriority);
	}
}
