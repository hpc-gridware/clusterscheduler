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
 *  Portions of this software are Copyright (c) 2024-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Interface of the qconf switch handlers
 */

#include "spool/sge_spooling_utilities.h"
#include "spool/flatfile/sge_flatfile.h"   /* CS-2313a: spool_flatfile_format */
#include "sgeobj/sge_daemonize.h"

/** @brief The output format `-fmt` selected: plain or JSON (CS-2313a)
 *
 * A global because the `show` paths in `ocs_qconf_cqueue.cc`,
 * `ocs_qconf_centry.cc` and `ocs_qconf_rqs.cc` format their output themselves
 * rather than going through the generic writer, and all of them have to agree.
 */
extern spool_flatfile_format qconf_opt_format;

/* CS-2313a: fill the CE_valtype/CE_doubleval of an object's complex-value sublists
 * (e.g. QU_consumable_config_list, QU_load_thresholds) from the centry definitions
 * so the JSON writer can emit native numbers. No-op unless -fmt json. */
void qconf_json_fill_complex(lListElem *obj, const int *ce_fields, int n_fields);

/** @brief One row of the table that drives the generic `qconf` switches
 *
 * Most `qconf` switches do the same thing to a different object: fetch it, hand
 * it to the editor, send it back. Rather than one function per object type,
 * there is one table entry per object type and one implementation that reads
 * this. Adding a configurable object means adding a row, not a switch.
 */
typedef struct object_info_entry {
   ocs::gdi::Target target;                  ///< which master list the object lives in
   const char *object_name;                  ///< the name used in messages and in the `-s*`/`-m*` switch
   lDescr *cull_descriptor;                  ///< the CULL type of the object
   const char *attribute_name;               ///< the attribute the `-*attr` switches operate on
   int nm_name;                              ///< the CULL field holding the object's name, its key
   spooling_field *fields;                   ///< which fields are written out, and in what order
   const struct spool_flatfile_instr *instr; ///< how to lay them out as text
   bool (*pre_gdi_function)(lList *list, lList **answer_list);   ///< validation to run before sending, or `nullptr`
} object_info_entry;

/** @brief Work through the `qconf` command line, one switch at a time
 *
 * See the definition in `ocs_qconf_parse.cc` for what it does.
 *
 * @param argv the arguments, not including `argv[0]`
 * @return 0 when every switch succeeded
 *
 * @note Declared as `char **argv` and defined as `char *argv[]`. The two are
 *       the same type to the compiler but not to doxygen, so this declaration
 *       has to carry its own block rather than picking up the definition's.
 */
int sge_parse_qconf(char **argv);
