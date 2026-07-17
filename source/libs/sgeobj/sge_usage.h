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

#include "sgeobj/cull/sge_usage_UA_L.h"

// make sure that the following enum is in sync with libs/sgeobj/json/UA.json
enum {
   UA_name_POS = 0,
   UA_value_POS                  /* usage value */
};


/*
 * sge standard usage value names
 *
 * use these defined names for refering special usage values
 */

#define USAGE_ATTR_WALLCLOCK "wallclock"
#define USAGE_ATTR_CPU "cpu"

/* integral memory usage */
#define USAGE_ATTR_MEM "mem"
#define USAGE_ATTR_IO "io"
#define USAGE_ATTR_IOOPS "ioops"
#define USAGE_ATTR_IOW "iow"

// memory details
#define USAGE_ATTR_PSS "pss"
#define USAGE_ATTR_MAXPSS "maxpss"
#define USAGE_ATTR_PMEM "pmem"
#define USAGE_ATTR_SMEM "smem"

#define USAGE_ATTR_CPU_ACCT "acct_cpu"

/* these are used for accounting */
#define USAGE_ATTR_MEM_ACCT "acct_mem"
#define USAGE_ATTR_IO_ACCT "acct_io"
#define USAGE_ATTR_IOOPS_ACCT "acct_ioops"
#define USAGE_ATTR_IOW_ACCT "acct_iow"
#define USAGE_ATTR_MAXVMEM_ACCT "acct_maxvmem"
#define USAGE_ATTR_MAXRSS_ACCT "acct_maxrss"
#define USAGE_ATTR_MAXPSS_ACCT "acct_maxpss"

/* current amount and maximum of used memory */
#define USAGE_ATTR_VMEM "vmem"
#define USAGE_ATTR_MAXVMEM "maxvmem"

/* current amount and maximum of resident set size */
#define USAGE_ATTR_RSS "rss"
#define USAGE_ATTR_MAXRSS "maxrss"

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
 * Parse a raw <variable>=<value> pair from the shepherd usage file (or an
 * equivalent source) into a UA_Type element with the correctly-typed value.
 * See CS-849 for the discrimination rule; the CTest module at
 * test/libs/sgeobj/test_sgeobj_usage.cc exhaustively covers origin AE3.
 *
 * The caller owns the returned element (must lFreeElem it or attach it to
 * a usage list). Returns nullptr only when @p name is nullptr; empty string
 * values and empty variable names are accepted.
 */
lListElem *
usage_parse_value(const char *name, const char *value);
