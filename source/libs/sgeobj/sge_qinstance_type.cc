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
 * @brief The queue type: which kinds of job a queue accepts
 *
 * Two of the four types are stored bits of `QU_qtype`; parallel and
 * checkpointing are derived from whether the queue references a PE or a
 * checkpointing environment.
 *
 * @see sge_qinstance_type.h
 */

#include "uti/sge_rmon_macros.h"

#include "cull/cull.h"

#include "sgeobj/parse.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_type.h"
#include "sgeobj/sge_utility.h"

#include "msg_common.h"

/// Debug layer the queue type traces are written to
#define QINSTANCE_TYPE_LAYER TOP_LAYER

/**
 * @brief The queue type names, in the order of the `QU_qtype` bits
 *
 * The position in this array is the bit position, so `sge_parse_bitfield_str`
 * can turn `qtype` back into the stored bit field. `PARALLEL` and
 * `CHECKPOINTING` are deliberately absent: they are not stored bits but
 * derived from whether the queue references a PE or a checkpointing
 * environment - see #qinstance_is_parallel_queue.
 */
const char *queue_types[] = {
   "BATCH",
   "INTERACTIVE",
   nullptr
};

static bool
qinstance_has_type(const lListElem *this_elem, uint32_t type);

static bool
qinstance_has_type(const lListElem *this_elem, uint32_t type)
{
   bool ret = false;

   if (lGetUlong(this_elem, QU_qtype) & type) {
      ret = true;
   }
   return ret;
}

/**
 * @brief Creates qtype bitmask as string
 *
 * This functions expects a "qtype" bitmask. Each bit represents a
 * certain queue type. If it is set to 1 the corresponding type name
 * will be appended to "string". If no bit is 1 than "NONE" will be
 * appended to "string".
 *
 * @param qtype bitmask
 * @param string string
 *
 * @return pointer to the internal buffer of string
 *
 * @note MT-NOTE: qtype_append_to_dstring() is MT safe
 */
const char *
qtype_append_to_dstring(uint32_t qtype, dstring *string)
{
   const char *ret = nullptr;

   DENTER(QINSTANCE_TYPE_LAYER);
   if (string != nullptr) {
      const char **ptr = nullptr;
      uint32_t bitmask = 1;
      bool qtype_defined = false;

      for (ptr = queue_types; *ptr != nullptr; ptr++) {
         if (bitmask & qtype) {
            if (qtype_defined) {
               sge_dstring_append(string, " ");
            }
            sge_dstring_append(string, *ptr);
            qtype_defined = true;
         }
         bitmask <<= 1;
      };
      if (!qtype_defined) {
         sge_dstring_append(string, "NONE");
      }
      ret = sge_dstring_get_string(string);
   }
   DRETURN(ret);
}

/**
 * @brief Render a queue's type as qstat prints it
 *
 * The abbreviated form additionally shows `P` for a parallel queue and `C` for
 * a checkpointing one, which the long form does not - those two are derived
 * from references, not from the stored bits.
 *
 * @param this_elem the queue instance to read
 * @param[out] string receives the text, appended
 * @param only_first_char true for the one letter per type form, false for the names
 * @return always true
 */
bool
qinstance_print_qtype_to_dstring(const lListElem *this_elem,
                                 dstring *string, bool only_first_char)
{
   bool ret = true;

   DENTER(QINSTANCE_TYPE_LAYER);
   if (this_elem != nullptr && string != nullptr) {
      const char **ptr = nullptr;
      uint32_t bitmask = 1;
      bool qtype_defined = false;

      for (ptr = queue_types; *ptr != nullptr; ptr++) {
         if (bitmask & lGetUlong(this_elem, QU_qtype)) {
            qtype_defined = true;
            if (only_first_char) {
               sge_dstring_sprintf_append(string, "%c", (*ptr)[0]);
            } else {
               sge_dstring_sprintf_append(string, "%s ", *ptr);
            }
         }
         bitmask <<= 1;
      };
      if (only_first_char) {
         if (qinstance_is_parallel_queue(this_elem)) {
            sge_dstring_sprintf_append(string, "%c", 'P');
            qtype_defined = true;
         }
         if (qinstance_is_checkpointing_queue(this_elem)) {
            sge_dstring_sprintf_append(string, "%c", 'C');
            qtype_defined = true;
         }
      }
      if (!qtype_defined) {
         if (only_first_char) {
            sge_dstring_append(string, "N");
         } else {
            sge_dstring_append(string, "NONE");
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Parse a queue type list and store it in the queue
 *
 * An empty or missing value clears the type, it is not an error.
 *
 * @param[in,out] this_elem the queue instance to change
 * @param[out] answer_list receives the message naming the bad type
 * @param value the type names, as an administrator wrote them
 * @return true when the value was understood
 */
bool
qinstance_parse_qtype_from_string(lListElem *this_elem, lList **answer_list,
                                  const char *value)
{
   bool ret = true;
   uint32_t type = 0;

   DENTER(QINSTANCE_TYPE_LAYER);
   SGE_CHECK_POINTER_FALSE(this_elem, answer_list);
   if (value != nullptr && *value != 0) {
      if (!sge_parse_bitfield_str(value, queue_types, &type,
                                  "queue type", nullptr, true)) {
         ret = false;
      }
   }

   lSetUlong(this_elem, QU_qtype, type);
   DRETURN(ret);
}

/**
 * @brief Is this a batch queue?
 *
 * @param this_elem the queue instance to check
 * @return true when the #BQ bit is set
 */
bool qinstance_is_batch_queue(const lListElem *this_elem)
{
   return qinstance_has_type(this_elem, BQ);
}

/**
 * @brief Is this an interactive queue?
 *
 * @param this_elem the queue instance to check
 * @return true when the #IQ bit is set
 */
bool qinstance_is_interactive_queue(const lListElem *this_elem)
{
   return qinstance_has_type(this_elem, IQ);
}

/**
 * @brief Is this a checkpointing queue?
 *
 * @param this_elem the queue instance to check
 * @return true when it references a checkpointing environment
 */
bool qinstance_is_checkpointing_queue(const lListElem *this_elem)
{
   return qinstance_is_a_ckpt_referenced(this_elem);
}

/**
 * @brief Is this a parallel queue?
 *
 * @param this_elem the queue instance to check
 * @return true when it references a parallel environment
 */
bool qinstance_is_parallel_queue(const lListElem *this_elem)
{
   return qinstance_is_a_pe_referenced(this_elem);
}

