/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef DATA_TRACKING_SUPPORT_HPP
#define DATA_TRACKING_SUPPORT_HPP

#include <cstdint>


class DataTrackingSupport {

	static const double _rwBonusFactor;
	static const uint64_t _distanceThreshold;
	static const uint64_t _loadThreshold;

public:

	static inline double getRWBonusFactor()
	{
		return _rwBonusFactor;
	}

	static inline double getDistanceThreshold()
	{
		return _distanceThreshold;
	}

	static inline double getLoadThreshold()
	{
		return _loadThreshold;
	}

};

#endif // DATA_TRACKING_SUPPORT_HPP
