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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Declarations and the standard names of the usage attributes
 *
 * @see sge_usage.cc
 */

#include "sgeobj/cull/sge_usage_UA_L.h"

/**
 * @brief The position of each attribute within the object
 *
 * Reading by position skips the lookup by name, which pays off where the
 * same attribute is read for every element of a long list.
 *
 * @warning Must stay in sync with `libs/sgeobj/json/UA.json`, which is what
 *          the attribute order is generated from. A value inserted there and
 *          not here makes every position below it read the wrong attribute.
 */
enum {
   UA_name_POS = 0,                        ///< position of `UA_name`
   UA_value_POS                            ///< position of `UA_value`
};


/**
 * @name Standard usage value names
 *
 * Use these names rather than the string literals: the execution daemon
 * reports usage under exactly these keys, and the accounting file, the share
 * tree policy and `qacct` all refer to the same key.
 *
 * The `acct_*` variants are the values as written to the accounting file,
 * which are not always the same as the live ones.
 * @{
 */

/// The usage attribute holding the elapsed wall clock time of the job
#define USAGE_ATTR_WALLCLOCK "wallclock"
/// The usage attribute holding the accumulated CPU time
#define USAGE_ATTR_CPU "cpu"

/// The usage attribute holding integral memory usage, i.e. memory multiplied by the time it was held
#define USAGE_ATTR_MEM "mem"
/// The usage attribute holding the amount of data transferred by the job
#define USAGE_ATTR_IO "io"
/// The usage attribute holding the number of I/O operations
#define USAGE_ATTR_IOOPS "ioops"
/// The usage attribute holding the time the job spent waiting for I/O
#define USAGE_ATTR_IOW "iow"

/// The usage attribute holding the current proportional set size, i.e. shared pages counted per sharer
#define USAGE_ATTR_PSS "pss"
/// The usage attribute holding the highest proportional set size seen
#define USAGE_ATTR_MAXPSS "maxpss"
/// The usage attribute holding the private, i.e. non shared, part of the resident memory
#define USAGE_ATTR_PMEM "pmem"
/// The usage attribute holding the shared part of the resident memory
#define USAGE_ATTR_SMEM "smem"

/// The usage attribute holding the CPU time as written to the accounting file
#define USAGE_ATTR_CPU_ACCT "acct_cpu"

/// The usage attribute holding the integral memory usage as written to the accounting file
#define USAGE_ATTR_MEM_ACCT "acct_mem"
/// The usage attribute holding the transferred data as written to the accounting file
#define USAGE_ATTR_IO_ACCT "acct_io"
/// The usage attribute holding the I/O operation count as written to the accounting file
#define USAGE_ATTR_IOOPS_ACCT "acct_ioops"
/// The usage attribute holding the I/O wait time as written to the accounting file
#define USAGE_ATTR_IOW_ACCT "acct_iow"
/// The usage attribute holding the peak virtual memory as written to the accounting file
#define USAGE_ATTR_MAXVMEM_ACCT "acct_maxvmem"
/// The usage attribute holding the peak resident set size as written to the accounting file
#define USAGE_ATTR_MAXRSS_ACCT "acct_maxrss"
/// The usage attribute holding the peak proportional set size as written to the accounting file
#define USAGE_ATTR_MAXPSS_ACCT "acct_maxpss"

/// The usage attribute holding the virtual memory currently in use
#define USAGE_ATTR_VMEM "vmem"
/// The usage attribute holding the highest virtual memory seen
#define USAGE_ATTR_MAXVMEM "maxvmem"

/// The usage attribute holding the resident set size currently in use
#define USAGE_ATTR_RSS "rss"
/// The usage attribute holding the highest resident set size seen
#define USAGE_ATTR_MAXRSS "maxrss"
/** @} */

int
usage_list_get_int_usage(const lList *usage_list, const char *name, int def);
uint32_t
usage_list_get_ulong_usage(const lList *usage_list, const char *name, uint32_t def);
uint64_t
usage_list_get_ulong64_usage(const lList *usage_list, const char *name, uint64_t def);
double
usage_list_get_double_usage(const lList *usage_list, const char *name, double def);

void
usage_list_set_ulong_usage(lList *usage_list, const char *name, uint32_t value);
void
usage_list_set_ulong64_usage(lList *usage_list, const char *name, uint64_t value);
void
usage_list_set_double_usage(lList *usage_list, const char *name, double value, bool create_usage = true);
void
usage_list_max_double_usage(lList *usage_list, const char *name, double value, bool create_usage = true);

void
usage_list_sum(lList *usage_list, const lList *add_usage_list);

lList *scale_usage(const lList *scaling, const lList *prev_usage, lList *scaled_usage);

/**
 * @brief Parse a raw `variable=value` pair from the shepherd usage file
 *
 * Also used for equivalent sources. The result is a `UA_Type` element with a
 * correctly typed value; see CS-849 for the discrimination rule, and
 * `test/libs/sgeobj/test_sgeobj_usage.cc` for the cases it covers.
 *
 * The caller owns the returned element and has to free it or attach it to a
 * usage list. An empty value and an empty variable name are both accepted.
 */
lListElem *
usage_parse_value(const char *name, const char *value);
