#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

/** @file
 * @brief The `qconf` object descriptor table
 */

/** @defgroup qconf_object The qconf object types
 * @brief One row per configuration object, read by the generic engines
 *
 * The table used to live in `clients/qconf/ocs_qconf_parse.cc`. It moved here
 * with CS-2799, because the read path renders inside qmaster from now on and
 * the table has to be reachable from both the client and the daemon. Nothing
 * about it changed in the move.
 *
 * @{
 */

#include "cull/cull.h"

#include "gdi/ocs_gdi_Target.h"
#include "gdi/ocs_gdi_Command.h"

#include "spool/sge_spooling_utilities.h"
#include "spool/flatfile/sge_flatfile.h"

typedef spooling_field *(*qconf_build_fields_fn)(spool_flatfile_format format, bool *owned);

/** @brief The configuration object types the generic engines handle.
 *
 * CONF and EV carry read-only rows (CS-2808). The cluster configuration is keyed
 * on the host basename rather than on a field, so its write path keeps its own
 * qconf_conf_export()/qconf_conf_apply_path() and the write hooks of its row
 * stay empty; only -sconfl uses it. EV is not a configuration object at all -
 * event clients live in the event master, not in a master list - and only
 * -sss uses it.
 *
 * Values are **appended, never inserted**: qconf_desc() is a plain index into
 * qconf_object_table, and qconf_master_list in ocs_QConfModelServer.cc is a
 * second array on the same index. Neither carries a name to grep for, so an
 * insertion shifts every row below it in silence. */
enum class QConfObj {
   CAL, CK, CE, EH, HGRP, PE, PR, CQ, RL, RQS, US, UU, STN, SC, CONF, EV
};

/** @brief What the read path needs on top of the common descriptor (CS-2796).
 *
 * There is one deliberate difference between showing an object and writing it
 * to a file, and it is in @c fields: -se builds its field list with
 * to_stdout=true, so it shows load_values and processors, which -Se leaves out
 * on purpose - the export has to round-trip. Pinned by exechost.show.plain vs
 * exechost.export.plain (CS-2794). nullptr means "the same as the write path".
 *
 * The layout itself is now the same for both: -suser used to render with
 * qconf_param_sfi where -Suser uses qconf_sfi, which never reached the output
 * (every UU field has width 0, so align_names does nothing, and the sublist
 * fields only exist with spool=true, which qconf does not use). -Suser is new
 * in 9.2, so there was no compatibility reason to keep the two apart. */
struct qconf_read_desc {
   qconf_build_fields_fn fields;   ///< field list for -s<obj>, nullptr to use build_fields
   const char *list_name;          ///< object name in the "no X defined" message of -s<obj>l
   const char *not_found_msg;      ///< format string of the "X is not a Y" message of -s<obj>
   const char *empty_msg;          ///< message of a singleton read that finds nothing,
                                   ///< takes no argument; nullptr for a type that is not one
   void (*prepare)(lListElem *, const lList *);   ///< fixup before rendering with the
                                                 ///< master centry list, nullptr for none
};

/** @brief Everything the generic engines need to know about one object type. */
struct qconf_object_desc {
   ocs::gdi::Target target;                          ///< GDI target list
   const lDescr *descr;                              ///< CULL descriptor
   int name_nm;                                      ///< name field, NoName for the singletons
   qconf_build_fields_fn build_fields;               ///< field list provider (may allocate)
   const spool_flatfile_instr *sfi;                  ///< flatfile spooling instruction
   const char *json_type_name;                       ///< explicit JSON $id, nullptr derives it
   int (*validate)(const lListElem *, lList **);     ///< semantic check, nullptr for none
   int (*prepare)(lListElem *, lList **);            ///< pre-send fixup, nullptr for none
   ocs::gdi::SubCommand sub_cmd;                     ///< GDI sub-command of an upsert
   bool as_list;                                     ///< file holds a list, not one object
   bool del_parse_values;                            ///< parse values when reading for a delete
   bool is_root;                                     ///< singleton export: object is the root
   const char *exclude1;                             ///< export: skip this name
   const char *exclude2;                             ///< export: skip this name too
   qconf_read_desc read;                             ///< the read path additions (CS-2796)
};

/** @brief The descriptor of one object type.
 * @param o the object type
 * @return its row in the object table
 */
const qconf_object_desc &qconf_desc(QConfObj o);

/** @brief A private copy of a field list, for a renderer that mutates widths
 *
 * `spool_flatfile_align_list()` computes the column widths of a table and writes
 * them back into the `spooling_field` array it is given. Most providers hand out
 * a shared static table (`CE_fields`, `CAT_fields`, ...), which is harmless in a
 * client - one process, one call - and a data race inside qmaster, where reader
 * threads run procedures in parallel. A renderer that aligns therefore works on
 * its own copy (CS-2817).
 *
 * The copy is shallow: `align_list` only writes `width` of the top level, and
 * everything else in the struct is a pointer to static data.
 *
 * @param fields the list to copy, terminated by `nm == NoName`
 * @return the copy, which the caller frees with sge_free()
 */
spooling_field *qconf_copy_fields(const spooling_field *fields);

void qconf_json_fill_complex(lListElem *obj, const int *ce_fields, int n_fields,
                             const lList *master_centry_list);

/** @} */
