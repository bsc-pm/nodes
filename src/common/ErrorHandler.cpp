/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include "ErrorHandler.hpp"


SpinLock ErrorHandler::_errorLock;
SpinLock ErrorHandler::_infoLock;
