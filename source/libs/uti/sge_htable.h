#pragma once
/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 *  Portions of this code are Copyright 2011 Univa Inc.
 *
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Hash table with pluggable key handling
 */
/*
 * Based on the code of David Flanagan's Xmt library
 */

/** @defgroup uti_htable Hash table
 * @brief A dynamically resizing hash table
 *
 * Hash tables give fast access to objects held in structures such as linked
 * lists, without traversing the whole list to find one element. An element is
 * identified by a unique key. This implementation grows the table as needed.
 *
 * The table does not know the type of the key, so three callbacks are supplied
 * when it is created with #sge_htable_create. Ready made sets exist for
 * `uint32_t`, `uint64_t`, `long` and string keys.
 *
 * @section uti_htable_dup The dup function
 *
 * The table cannot assume the caller's key stays valid for as long as the entry
 * lives, so it stores its own copy. `dup_func_<type>(const void *key)` returns
 * that copy.
 *
 * @section uti_htable_hash The hash function
 *
 * `hash_func_<type>(const void *key)` returns the hash value for a key.
 * Different key types need different hash functions.
 *
 * @section uti_htable_compare The compare function
 *
 * `compare_func_<type>(const void *a, const void *b)` compares two keys.
 * Syntax and return value follow `strcmp`: 0 when equal, > 0 when the first is
 * greater, < 0 when it is smaller.
 *
 * @note MT-NOTE: this module is MT safe
 * @{
 */

#define True   1     ///< legacy boolean, kept for the Xmt derived code
#define False  0     ///< legacy boolean, kept for the Xmt derived code

#include "sge_dstring.h"

/** @brief Handle of a hash table, created by #sge_htable_create */
typedef struct _htable_rec *htable;

/** @brief Callback invoked by #sge_htable_for_each_ep for every entry
 *
 * Receives the table, the key and the address of the stored data pointer.
 */
typedef void (*sge_htable_for_each_proc)(
        htable, const void *, const void **
);

htable sge_htable_create(int size, const void *(*dup_func)(const void *), int (*hash_func)(const void *),
                         int (*compare_func)(const void *, const void *));

void sge_htable_destroy(htable ht);

void sge_htable_store(htable ht, const void *key, const void *data);

int sge_htable_lookup(htable ht, const void *key, const void **data);

void sge_htable_delete(htable ht, const void *key);

void sge_htable_for_each_ep(htable ht, sge_htable_for_each_proc proc);

long sge_htable_get_size(htable ht);

const char *sge_htable_statistics(htable ht, dstring *buffer);

const void *dup_func_uint32_t(const void *key);

const void *dup_func_uint64_t(const void *key);

const void *dup_func_string(const void *key);

const void *dup_func_long(const void *key);

const void *dup_func_pointer(const void *key);

int hash_func_uint32_t(const void *key);

int hash_func_uint64_t(const void *key);

int hash_func_string(const void *key);

int hash_func_long(const void *key);

int hash_func_pointer(const void *key);


int hash_compare_uint32_t(const void *a, const void *b);

int hash_compare_uint64_t(const void *a, const void *b);

int hash_compare_string(const void *a, const void *b);

int hash_compare_long(const void *a, const void *b);

int hash_compare_pointer(const void *a, const void *b);

int hash_compute_size(int number_of_elem);

/** @} */
