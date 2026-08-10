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
 * @brief Deciding which attributes of an object get spooled, and how
 */

#include "uti/sge_dstring.h"

#include "cull/cull.h"

#include "sgeobj/sge_object.h"

/** @defgroup spool_utilities Spooling utilities
 * @brief Which attributes of an object are spooled, and under what name
 *
 * A cull object has more attributes than belong in a spool file: runtime
 * state, cached values, things derived on load. Which ones survive is not
 * decided here but in the object's own definition - every field carries a
 * `mt` bitmask, and #CULL_SPOOL is the bit that says "write me".
 *
 * This module turns that bitmask into something the backends can iterate.
 * #spool_get_fields_to_spool walks a `lDescr` under a #spool_instr_t and
 * returns the matching fields as a #spooling_field array, which is what the
 * flatfile writer formats and the reader parses back.
 * @{
 */


/** @brief How to select the fields of one object type for spooling */
typedef struct spool_instr {
   int selection;             ///< Bitmask ANDed with each field's `mt`, e.g. #CULL_SPOOL - a non-zero result means the field is spooled
   bool copy_field_names;     ///< Copy the attribute name into the #spooling_field instead of pointing into the descriptor
   bool strip_field_prefix;   ///< Drop the `XX_` type prefix from the name, so `QU_qname` is spooled as `qname`
   const struct spool_instr *sub_instr;   ///< The instruction to apply to elements of sublist fields
   const void *clientdata;    ///< Free for the caller; unused by the framework itself
} spool_instr_t;

/** @name The ready made selections
 *
 * Each `*_instr` is the top level instruction for one family of objects and
 * points at the matching `*_subinstr` for whatever sublists it contains.
 * @{
 */
extern const spool_instr_t spool_config_instr;      ///< The default: #CULL_SPOOL fields, names copied and un-prefixed
extern const spool_instr_t spool_config_subinstr;   ///< Sublists of the above: #CULL_SUBLIST fields, names left alone

extern const spool_instr_t spool_complex_instr;     ///< Complex entries: #CULL_SPOOL fields, names kept verbatim
extern const spool_instr_t spool_complex_subinstr;  ///< Sublists of a complex entry

extern const spool_instr_t spool_user_instr;        ///< Users and projects: #CULL_SPOOL plus #CULL_SPOOL_USER
extern const spool_instr_t spool_userprj_subinstr;  ///< Sublists of a user or project - it names *itself* as its own `sub_instr`, so nesting to any depth uses the same selection
/** @} */

/** @brief One attribute to spool, as the backends see it
 *
 * #spool_get_fields_to_spool returns these as an array terminated by an entry
 * with `nm == NoName`. The `free_*` flags say which pointers that array owns,
 * so that #spool_free_spooling_fields can release exactly those.
 */
typedef struct spooling_field {
   int nm;                    ///< The cull attribute number, `NoName` in the terminating entry
   int width;                 ///< Field width for formatted output, 0 for unformatted
   const char *name;          ///< The name to write into the spool file
   bool free_name;            ///< `name` was copied and must be freed with the array
   struct spooling_field *sub_fields;   ///< For a list attribute, the fields to spool per element
   bool free_sub_fields;      ///< `sub_fields` was built here and must be freed with the array
   const void *clientdata;    ///< Free for the backend; the flatfile writer keeps its format info here
   int (*read_func) (lListElem *ep, int nm, const char *buffer, lList **alp);   ///< Backend hook parsing this attribute out of `buffer`, instead of the default conversion
   int (*write_func) (const lListElem *ep, int nm, dstring *buffer, lList **alp);   ///< Backend hook rendering this attribute into `buffer`
} spooling_field;

spooling_field *
spool_get_fields_to_spool(lList **answer_list, const lDescr *descr,
                          const spool_instr_t *instr);

spooling_field *
spool_free_spooling_fields(spooling_field *fields);

bool
spool_default_validate_func(lList **answer_list,
                          const lListElem *type,
                          const lListElem *rule,
                          lListElem *object,
                          const sge_object_type object_type);

bool
spool_default_validate_list_func(lList **answer_list,
                          const lListElem *type, const lListElem *rule,
                          const sge_object_type object_type);

lList *
spool_exechost_strip_dynamic_load(const lListElem *object);

void
spool_exechost_restore_load_list(const lListElem *object, lList **backup_load_list);

/** @} */
