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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Declarations for name verification and other shared checks
 *
 * @see sge_utility.cc
 */

#include "sgeobj/sge_answer.h"

/// What a component's thread control is being asked to do
typedef enum sge_thread_state_transitions_t_ {
   SGE_THREAD_TRIGGER_NONE = 0, ///< nothing to do
   SGE_THREAD_TRIGGER_START,    ///< start the thread
   SGE_THREAD_TRIGGER_STOP      ///< stop the thread
} sge_thread_state_transitions_t;

/**
 * @brief Return nullptr with a message when a pointer argument is nullptr
 *
 * Only usable in a function with a `DENTER`, since it expands to a `DRETURN`.
 */
#define SGE_CHECK_POINTER_NULL(pointer, answer_list)                 \
   if ((pointer) == nullptr) {                                          \
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,          \
                              ANSWER_QUALITY_ERROR,                  \
                              MSG_NULLELEMENTPASSEDTO_S, __func__);  \
      DRETURN(nullptr);                                                 \
   }

/**
 * @brief Return false with a message when a pointer argument is nullptr
 *
 * Only usable in a function with a `DENTER`, since it expands to a `DRETURN`.
 */
#define SGE_CHECK_POINTER_FALSE(pointer, answer_list)                \
   if ((pointer) == nullptr) {                                          \
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,          \
                              ANSWER_QUALITY_ERROR,                  \
                              MSG_NULLELEMENTPASSEDTO_S, __func__);  \
      DRETURN(false);                                                \
   }

/**
 * @name Character tables for verify_str_key
 *
 * Which characters a name may contain depends on where the name is used.
 * @{
 */
#define KEY_TABLE	(1) ///< the stricter set, for names used as a spool file key
#define QSUB_TABLE	(2) ///< the set a submit client accepts, e.g. for a job name
/** @} */
an_status_t
verify_str_key(lList **alpp, const char *str, size_t str_length, const char *name, int table,
               const char *exceptions = nullptr);

an_status_t
verify_obj_name(lList **alpp, const char *str, size_t str_length, const char *name,
                const char *exceptions = nullptr);

bool verify_host_name(lList **answer_list, const char *host_name);

/**
 * @brief Resolve this host's own qualified name again
 *
 * Called after the configuration changed, since `ignore_fqdn` decides whether
 * the domain is part of the name this component reports.
 *
 * @return `CL_RETVAL_OK` on success, otherwise a commlib error code
 */
int reresolve_qualified_hostname();
