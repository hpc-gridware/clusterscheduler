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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Parsing of numbers, memory sizes and time values from configuration
 */

#include <sys/resource.h>

#include "cull/cull_list.h"

#include "sgeobj/ocs_CEntry.h"

/// A resource limit value; a thin wrapper over the system's `rlim_t`
typedef rlim_t sge_rlim_t;

sge_rlim_t mul_infinity(sge_rlim_t rlim, sge_rlim_t muli);

int parse_ulong_val(double *dvalp, uint32_t *uvalp, ocs::CEntry::Type type, const char *s, char *err_str, int err_len);


int extended_parse_ulong_val(double *dvalp, uint32_t *uvalp, ocs::CEntry::Type type,
                             const char *s, char *err_str, int err_len,
                             int enable_infinity, bool only_positive);

/**
 * @brief Is a checkpointing `when` bitmask a valid combination?
 *
 * @param bitmask the combination of checkpointing occasions to validate
 * @return non-zero when the combination is valid
 *
 * @note Declared here but implemented in `daemons/qmaster/sge_ckpt_qmaster.cc`,
 *       so this header does not describe a libuti function.
 */
int is_checkpoint_when_valid(int bitmask);

bool sge_parse_loglevel_val(uint32_t *uval, const char *s);

uint32_t sge_parse_num_val(sge_rlim_t *rlimp, double *dvalp,
                           const char *str, const char *where,
                           char *err_str, int err_len);
