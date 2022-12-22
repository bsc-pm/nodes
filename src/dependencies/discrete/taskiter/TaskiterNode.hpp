/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2022 Barcelona Supercomputing Center (BSC)
*/

#ifndef TASKITER_NODE_HPP
#define TASKITER_NODE_HPP

#include <cstdint>
#include <variant>

class TaskMetadata;
class ReductionInfo;

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

class TaskiterNode {
	size_t _vertex;
	std::variant<TaskMetadata *, ReductionInfo *> _variant;

	public:
	TaskiterNode(TaskMetadata *taskBase, ReductionInfo *reductionBase) :
		_vertex(0)
	{
		if (taskBase != nullptr)
			_variant = taskBase;
		else
			_variant = reductionBase;
	};

	template <class Visitor>
	void apply(Visitor &vis)
	{
		std::visit(vis, _variant);
	}

	size_t getVertex() const
	{
		return _vertex;
	}

	void setVertex(size_t vertex)
	{
		_vertex = vertex;
	}

	TaskMetadata *getTask()
	{
		return std::visit(overloaded {
			[](auto) { return (TaskMetadata *) nullptr; },
			[](TaskMetadata *arg) { return arg; }
		}, _variant);
	}

	virtual ~TaskiterNode() = default;
};

#endif // TASKITER_NODE_HPP
