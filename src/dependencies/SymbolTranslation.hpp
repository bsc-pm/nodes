/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef SYMBOL_TRANSLATION_HPP
#define SYMBOL_TRANSLATION_HPP

#include <nosv.h>

#include <api/task-instantiation.h>

#include "dependencies/discrete/DataAccessRegistration.hpp"


class SymbolTranslation {

public:

	// Constexpr because we want to force the compiler to not generate a VLA
	static constexpr int MAX_STACK_SYMBOLS = 20;

	static inline nanos6_address_translation_entry_t *generateTranslationTable(
		nosv_task_t task,
		size_t cpuId,
		nanos6_address_translation_entry_t *stackTable,
		/* output */ size_t &tableSize
	) {
		assert(task != nullptr);

		nosv_task_type_t type = nosv_get_task_type(task);
		assert(type != nullptr);

		nanos6_task_info_t *taskInfo = (nanos6_task_info_t *) nosv_get_task_type_metadata(type);
		assert(taskInfo != nullptr);

		nanos6_address_translation_entry_t *table = nullptr;
		int numSymbols = taskInfo->num_symbols;
		if (numSymbols == 0) {
			return nullptr;
		}

		// Use stack-allocated table if there are just a few symbols, to prevent extra allocations
		if (numSymbols <= MAX_STACK_SYMBOLS) {
			tableSize = 0;
			table = stackTable;
		} else {
			tableSize = numSymbols * sizeof(nanos6_address_translation_entry_t);
			table = (nanos6_address_translation_entry_t *) malloc(tableSize);
		}

		DataAccessRegistration::translateReductionAddresses(task, cpuId, table, numSymbols);

		return table;
	}
};

#endif // SYMBOL_TRANSLATION_HPP
