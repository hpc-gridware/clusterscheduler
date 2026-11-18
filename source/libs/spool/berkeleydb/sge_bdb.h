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
 *  Portions of this software are Copyright (c) 2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/       

/** @file
 * @brief The Berkeley DB operations behind the berkeleydb spooling backend
 */

#include "cull/cull.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "spool/berkeleydb/sge_bdb_types.h"
#include <cinttypes>
#include "sgeobj/sge_answer.h"

bool
spool_berkeleydb_check_version(ocs::tom::ListPtr<ocs::tom::AN> *answer_list);

bool 
spool_berkeleydb_create_environment(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, 
                                    bdb_info info);

bool 
spool_berkeleydb_open_database(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, bdb_info info, 
                               bool create);

bool 
spool_berkeleydb_check_reopen_database(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, 
                                       bdb_info info);

bool 
spool_berkeleydb_close_database(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, bdb_info info);

bool
spool_berkeleydb_start_transaction(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, bdb_info info);

bool
spool_berkeleydb_end_transaction(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, bdb_info info, 
                                 bool commit);

bool 
spool_berkeleydb_trigger(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, bdb_info info, 
                         uint64_t trigger, uint64_t *next_trigger);

bool 
spool_berkeleydb_read_list(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, bdb_info info,
                           const bdb_database database,
                           lList **list, const lDescr *descr,
                           const char *key);
bool 
spool_berkeleydb_write_object(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, bdb_info info,
                              const bdb_database database,
                              const lListElem *object, const char *key);

bool spool_berkeleydb_write_string(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, bdb_info info,
                              const bdb_database database,
                              const char *key, const char *str);
                              
bool
spool_berkeleydb_write_pe_task(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, bdb_info info,
                               ocs::tom::ConstElement<ocs::tom::PET> object,
                               uint32_t job_id, uint32_t ja_task_id, const char *pe_task_id);
bool
spool_berkeleydb_write_ja_task(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, bdb_info info,
                               ocs::tom::ConstElement<ocs::tom::JAT> object, uint32_t job_id, uint32_t ja_task_id);
bool
spool_berkeleydb_write_job(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, bdb_info info,
                           ocs::tom::ConstElement<ocs::tom::JB> object,
                           uint32_t job_id, uint32_t ja_task_id, bool only_job);
bool
spool_berkeleydb_write_cqueue(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, bdb_info info,
                              ocs::tom::ConstElement<ocs::tom::CQ> object, const char *key);

bool
spool_berkeleydb_delete_object(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, bdb_info info, 
                               const bdb_database database,
                               const char *key, bool sub_objects);
bool
spool_berkeleydb_delete_pe_task(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, bdb_info info,
                                const char *key, bool sub_objects);
bool
spool_berkeleydb_delete_ja_task(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, bdb_info info,
                                const char *key, bool sub_objects);
bool
spool_berkeleydb_delete_job(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, bdb_info info,
                            const char *key, bool sub_objects);
bool
spool_berkeleydb_delete_cqueue(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, bdb_info info,
                               const char *key);

bool
spool_berkeleydb_read_keys(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, bdb_info info,
                           const bdb_database database,
                           lList **list, const char *key);

lListElem *
spool_berkeleydb_read_object(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, bdb_info info,
                             const bdb_database database,
                             const char *key);

char *
spool_berkeleydb_read_string(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, bdb_info info,
                             const bdb_database database,
                             const char *key);
