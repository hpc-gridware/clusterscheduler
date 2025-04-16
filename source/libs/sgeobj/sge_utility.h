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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "sgeobj/sge_answer.h"

typedef enum sge_thread_state_transitions_t_ {
   SGE_THREAD_TRIGGER_NONE = 0,
   SGE_THREAD_TRIGGER_START, 
   SGE_THREAD_TRIGGER_STOP
} sge_thread_state_transitions_t;

#define SGE_CHECK_POINTER_NULL(pointer, answer_list)                 \
   if ((pointer) == nullptr) {                                          \
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,          \
                              ANSWER_QUALITY_ERROR,                  \
                              MSG_NULLELEMENTPASSEDTO_S, __func__);  \
      DRETURN(nullptr);                                                 \
   }

#define SGE_CHECK_POINTER_FALSE(pointer, answer_list)                \
   if ((pointer) == nullptr) {                                          \
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,          \
                              ANSWER_QUALITY_ERROR,                  \
                              MSG_NULLELEMENTPASSEDTO_S, __func__);  \
      DRETURN(false);                                                \
   }

#define KEY_TABLE	(1)
#define QSUB_TABLE	(2)
an_status_t
verify_str_key(lList **alpp, const char *str, size_t str_length, const char *name, int table,
               const char *exceptions = nullptr);

bool verify_host_name(lList **answer_list, const char *host_name);

int reresolve_qualified_hostname();