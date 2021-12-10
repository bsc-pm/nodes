/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef NODES_MAIN_WRAPPER_H
#define NODES_MAIN_WRAPPER_H


typedef int main_function_t(int argc, char **argv, char **envp);

extern main_function_t *_nanos6_loader_wrapped_main;
main_function_t _nanos6_loader_main;

#endif // NODES_MAIN_WRAPPER_H
