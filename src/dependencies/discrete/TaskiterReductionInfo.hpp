/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2022 Barcelona Supercomputing Center (BSC)
*/

#ifndef TASKITER_REDUCTION_INFO_HPP
#define TASKITER_REDUCTION_INFO_HPP

#include "ReductionInfo.hpp"
#include "taskiter/TaskiterNode.hpp"

class TaskiterReductionInfo : public ReductionInfo, public TaskiterNode {
    public:
    inline TaskiterReductionInfo(void *address, size_t length, reduction_type_and_operator_index_t typeAndOperatorIndex,
		std::function<void(void *, void *, size_t)> initializationFunction,
		std::function<void(void *, void *, size_t)> combinationFunction, bool inTaskiter_) :
        ReductionInfo(address, length, typeAndOperatorIndex, initializationFunction, combinationFunction, inTaskiter_),
        TaskiterNode(nullptr, (ReductionInfo *) this)
    {
    }

    virtual ~TaskiterReductionInfo() = default;
};

#endif // TASKITER_REDUCTION_INFO_HPP