/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021-2023 Barcelona Supercomputing Center (BSC)
*/

#ifndef NODES_DEPENDENCIES_H
#define NODES_DEPENDENCIES_H

#include <stddef.h>

#include "major.h"


#pragma GCC visibility push(default)

#ifdef __cplusplus
extern "C" {
#endif


//! \brief Register a task read access on linear region of addresses
//!
//! \param[in] handler the handler received in register_depinfo
//! \param[in] start first address accessed
//! \param[in] length number of bytes until and including the last byte accessed
void nanos6_register_read_depinfo(void *handler, void *start, size_t length, int symbol_index);

//! \brief Register a task write access on linear region of addresses
//!
//! \param[in] handler the handler received in register_depinfo
//! \param[in] start first address accessed
//! \param[in] length number of bytes until and including the last byte accessed
void nanos6_register_write_depinfo(void *handler, void *start, size_t length, int symbol_index);

//! \brief Register a task read and write access on linear region of addresses
//!
//! \param[in] handler the handler received in register_depinfo
//! \param[in] start first address accessed
//! \param[in] length number of bytes until and including the last byte accessed
void nanos6_register_readwrite_depinfo(void *handler, void *start, size_t length, int symbol_index);

//! \brief Register a task commutative access on linear region of addresses
//!
//! \param[in] handler the handler received in register_depinfo
//! \param[in] start first address accessed
//! \param[in] length number of bytes until and including the last byte accessed
void nanos6_register_commutative_depinfo(void *handler, void *start, size_t length, int symbol_index);

//! \brief Register a task concurrent access on linear region of addresses
//!
//! \param[in] handler the handler received in register_depinfo
//! \param[in] start first address accessed
//! \param[in] length number of bytes until and including the last byte accessed
void nanos6_register_concurrent_depinfo(void *handler, void *start, size_t length, int symbol_index);

//! \brief Register a weak task read access on linear region of addresses
//!
//! \param[in] handler the handler received in register_depinfo
//! \param[in] start first address accessed
//! \param[in] length number of bytes until and including the last byte accessed
void nanos6_register_weak_read_depinfo(void *handler, void *start, size_t length, int symbol_index);

//! \brief Register a weak task write access on linear region of addresses
//!
//! \param[in] handler the handler received in register_depinfo
//! \param[in] start first address accessed
//! \param[in] length number of bytes until and including the last byte accessed
void nanos6_register_weak_write_depinfo(void *handler, void *start, size_t length, int symbol_index);

//! \brief Register a weak task read and write access on linear region of addresses
//!
//! \param[in] handler the handler received in register_depinfo
//! \param[in] start first address accessed
//! \param[in] length number of bytes until and including the last byte accessed
void nanos6_register_weak_readwrite_depinfo(void *handler, void *start, size_t length, int symbol_index);

//! \brief Register a task commutative access on linear region of addresses
//!
//! \param[in] handler the handler received in register_depinfo
//! \param[in] start first address accessed
//! \param[in] length number of bytes until and including the last byte accessed
void nanos6_register_weak_commutative_depinfo(void *handler, void *start, size_t length, int symbol_index);

#ifdef __cplusplus
}
#endif

#pragma GCC visibility pop

#endif // NODES_DEPENDENCIES_H
