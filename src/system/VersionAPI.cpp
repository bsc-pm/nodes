/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2023 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>
#include <cstdint>
#include <string>
#include <sstream>
#include <vector>

#include <nodes/version.h>

#include "common/ErrorHandler.hpp"

// The currently supported API is family 0, version 1.0
static constexpr uint64_t GENERAL_API_FAMILY = 0;
static constexpr uint64_t GENERAL_API_MAJOR = 1;
static constexpr uint64_t GENERAL_API_MINOR = 0;

extern "C" void nanos6_check_version(uint64_t size, nanos6_version_t *versions, const char *source)
{
	assert(source != nullptr);

	std::vector<std::string> errors;
	for (uint64_t v = 0; v < size; ++v) {
		std::ostringstream oss;
		if (versions[v].family != GENERAL_API_FAMILY) {
			// The version family is not recognized
			oss << "Family " << versions[v].family << " not recognized";
		} else if (versions[v].major_version != GENERAL_API_MAJOR || versions[v].minor_version > GENERAL_API_MINOR) {
			// The version family is recognized but not compatible
			oss << "Family " << versions[v].family << " requires " << versions[v].major_version << "."
				<< versions[v].minor_version << ", but runtime supports " << GENERAL_API_MAJOR << "."
				<< GENERAL_API_MINOR;
		} else {
			// The version is supported
			continue;
		}

		errors.push_back(oss.str());
	}

	// If any incompatibilities were found, report them and abort the execution
	if (!errors.empty()) {
		std::ostringstream oss;
		for (uint64_t e = 0; e < errors.size(); ++e) {
			oss << "\n\t" << (e + 1) << ". " << errors[e];
		}
		ErrorHandler::fail("Found API version incompatibilities in ", source, ":", oss.str());
	}
}
