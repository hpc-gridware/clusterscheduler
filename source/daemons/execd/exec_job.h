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
 *  Portions of this software are Copyright (c) 2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Writing a job's configuration to disk and forking its shepherd
 */

#include "sgeobj/sge_daemonize.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "tom/ocs_Element.h"

int sge_exec_job(ocs::tom::Element<ocs::tom::JB> jep, ocs::tom::Element<ocs::tom::JAT> jatep,
                 ocs::tom::Element<ocs::tom::PET> petep, char *err_str, int err_length);
ocs::tom::ConstElement<ocs::tom::QU>
responsible_queue(ocs::tom::ConstElement<ocs::tom::JB> jep, ocs::tom::ConstElement<ocs::tom::JAT> jatep,
                  ocs::tom::ConstElement<ocs::tom::PET> petep);
ocs::tom::Element<ocs::tom::QU>
responsible_queue(ocs::tom::ConstElement<ocs::tom::JB> jep, ocs::tom::Element<ocs::tom::JAT> jatep,
                  ocs::tom::ConstElement<ocs::tom::PET> petep);
bool ocs_execd_parse_queue_limit(ocs::tom::ConstElement<ocs::tom::QU> queue, const char *limit_name,
                                 ocs::CEntry::Type type, const char *limit_str, double *dvalp, uint32_t *uvalp,
                                 bool warn);
// instantiated in exec_job.cc for QU_h_cpu, QU_h_rss, QU_h_vmem, QU_s_cpu, QU_s_rss and QU_s_vmem
template <auto F>
double execd_get_task_limit(const char *limit_name, ocs::CEntry::Type type,
                            ocs::tom::ConstElement<ocs::tom::QU> queue, ocs::tom::ConstElement<ocs::tom::JB> jep,
                            ocs::tom::ConstElement<ocs::tom::JAT> jatep, ocs::tom::ConstElement<ocs::tom::PET> petep,
                            const char *qualified_hostname);
