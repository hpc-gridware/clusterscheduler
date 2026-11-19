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
 * @brief The queue type: which kinds of job a queue accepts
 *
 * @see sge_qinstance_type.cc
 */

#include <cstdint>
#include <utility>

#include "uti/sge_dstring.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_Transitional.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/sge_answer.h"

/**
 * @brief The queue type names, in the order of the `QU_qtype` bits
 *
 * The position in this array is the bit position, so `sge_parse_bitfield_str`
 * can turn `qtype` back into the stored bit field. `PARALLEL` and
 * `CHECKPOINTING` are deliberately absent: they are not stored bits but
 * derived from whether the queue references a PE or a checkpointing
 * environment - see #qinstance_is_parallel_queue.
 */
inline constexpr const char *const queue_types[] = {
   "BATCH",
   "INTERACTIVE",
   nullptr
};

const char *
qtype_append_to_dstring(uint32_t dtype, dstring *string);

const char *
ocs_qinstance_qtype_append_to_dstring(uint32_t qtype, dstring *string);

std::pair<bool, uint32_t>
ocs_qinstance_qtype_parse(const char *value);

bool qinstance_is_batch_queue(ocs::tom::ConstElement<ocs::tom::QU> this_elem);

bool qinstance_is_interactive_queue(ocs::tom::ConstElement<ocs::tom::QU> this_elem);

bool qinstance_is_checkpointing_queue(ocs::tom::ConstElement<ocs::tom::QU> this_elem);

bool qinstance_is_parallel_queue(ocs::tom::ConstElement<ocs::tom::QU> this_elem);

bool qinstance_print_qtype_to_dstring(ocs::tom::ConstElement<ocs::tom::QU> this_elem, dstring *string,
                                      bool only_first_char);

