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
 * AH, SH, MGR and OP are not object types either (CS-2820): each names one
 * *reserved* object whose member sublist is what the switch reports - the host
 * groups @admin_hosts and @submit_hosts for -sh and -ss, the access lists
 * manager and operator for -sm and -so. They share the descriptor of their
 * owner type and differ only in which object to read and which sublist to
 * render, so they are rows here rather than columns in the read table; that
 * keeps the views taking nothing but a qconf_object_desc.
 *
 * Values are **appended, never inserted**: qconf_desc() is a plain index into
 * qconf_object_table, and qconf_master_list in ocs_QConfModelServer.cc is a
 * second array on the same index. Neither carries a name to grep for, so an
 * insertion shifts every row below it in silence. */
enum class QConfObj {
   CAL, CK, CE, EH, HGRP, PE, PR, CQ, RL, RQS, US, UU, STN, SC, CONF, EV, CAT,
   AH, SH, MGR, OP
};

/** @brief Where a member name list comes from (CS-2820)
 *
 * The four reserved lists are read as "one named object, then its sublist",
 * which no other read kind expresses: NAME_LIST reads every object of a type,
 * SINGLE_OBJECT renders one in full. What the output looks like is a name list
 * all the same, so QConfReadKind::MEMBER_LIST reuses the whole rendering and
 * empty-list machinery and only the source differs - this struct says how. */
struct qconf_member_desc {
   const char *owner;        ///< name of the reserved object, e.g. "@admin_hosts"
   int list_nm;              ///< its sublist field, e.g. HGRP_host_list
   int key_nm;               ///< the name field of a sublist element, e.g. HR_name
   const lDescr *descr;      ///< the sublist element type, for the empty case
   bool split_matchers;      ///< plain output only: host names and @group references
                             ///< first, matchers after them, each block still sorted.
                             ///< A property of the reserved host groups (CS-2680) and
                             ///< deliberately not of the JSON form, whose array order
                             ///< must stay the plain sort order
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
   lListElem *(*create_template)(lList **, const char *);
                                   ///< SINGLE_OBJECT: build the type's template, for a read
                                   ///< that feeds an editor and found no object of that name
                                   ///< (CS-2823). It is what makes "-m<obj> <unknown>" offer
                                   ///< a template and add on exit, and "-a<obj>" the same
                                   ///< operation as "-m<obj>" - see
                                   ///< QConfModelBase::make_snapshot(). Built here rather
                                   ///< than in the client so that the text an editor sees
                                   ///< comes out of the same renderer and the same field
                                   ///< list as a stored object's: two renderers for one text
                                   ///< format is how the exec host's read/write mismatch
                                   ///< arose, see QConfParameter::get_for_edit().
                                   ///< nullptr for a type that is not created from a name -
                                   ///< the exec host, whose template is an entry of its own
                                   ///< list, and every type whose write is not on the
                                   ///< procedure yet
   const char *empty_msg;          ///< message of a singleton read that finds nothing,
                                   ///< takes no argument; nullptr for a type that is not one
   void (*prepare)(lListElem *, const lList *);   ///< fixup before rendering with the
                                                 ///< master centry list, nullptr for none
   const spool_flatfile_instr *list_sfi;  ///< the aligned table form of -s<obj>l, used by
                                         ///< the table sub-procedures; nullptr for a type
                                         ///< that has no such form (CS-2819)
   const char *json_list_id;       ///< the table form's JSON schema id when it differs from
                                   ///< the single record's - the category list is written
                                   ///< as "category-list" holding "category" objects, so
                                   ///< the two do not collide; nullptr uses the plain
                                   ///< flatfile writer, as the complex entries do
   const char *not_found_noun;     ///< OBJECT_LIST: what a name that matched nothing is
                                   ///< called in "denied: <noun> \"x\" does not exist";
                                   ///< nullptr reports the whole read as empty instead
   bool name_header;               ///< SINGLE_OBJECT: write a "#<name>:" comment line before
                                   ///< the object in a text format. Only the configuration
                                   ///< does this, and only because -sconf has always done
                                   ///< it: the header is what tells the blocks of
                                   ///< "-sconf host1,host2" apart, and omitting it in JSON
                                   ///< is what CS-2313a decided (a comment is not JSON)
   bool write_as_list;             ///< OBJECT_LIST: render the whole list through the
                                   ///< spooling instruction in one go, rather than one
                                   ///< object at a time separated by blank lines. It is a
                                   ///< property of the type, not of the switch: -srqs
                                   ///< wraps every record in braces (qconf_rqs_sfi),
                                   ///< -su prints plain blocks (CS-2819)
   const char *json_name_id;       ///< name list JSON: the envelope stem, for a list whose
                                   ///< elements are not a registered object type and would
                                   ///< otherwise be reported as "unknown"; nullptr derives
                                   ///< it from the first element, as the object name lists
                                   ///< do (CS-2820)
   bool json_name_id_when_empty;   ///< name list JSON: whether json_name_id is still given
                                   ///< for an empty list. It decides between a bare
                                   ///< {"names": []} and the full "<stem>-list" envelope,
                                   ///< and the two reserved pairs really do differ: -sh and
                                   ///< -ss passed the type name only for a non-empty list,
                                   ///< -sm and -so passed it always (CS-2820)
   qconf_member_desc member;       ///< MEMBER_LIST: which object and which sublist the
                                   ///< names come from; unused by every other kind
};

/** @brief What the write path needs on top of the common descriptor (CS-2823)
 *
 * Both members exist because an item's text can hold a *list*, and a list has
 * properties a single object has not: elements that contradict each other, and
 * elements the text does not mention. A type whose text is one object leaves
 * the whole struct empty.
 */
struct qconf_write_desc {
   int (*verify_list)(const lList *, lList **);
                             ///< MODE_LIST: the checks that need the whole list in
                             ///< hand, run in the reading phase so that a text
                             ///< failing them is rejected as a whole. Per-element
                             ///< checks belong in qconf_object_desc::validate, or
                             ///< in the type's own modifier where they already are -
                             ///< this is only for what one element cannot answer:
                             ///< the duplicate name of a rule set text, the
                             ///< duplicate name or shortcut of a complex list.
                             ///< nullptr for a type with nothing to check
   bool (*retain_on_replace)(const lListElem *);
                             ///< MODE_REPLACE: true for an object the replace must
                             ///< *not* delete although the text left it out. The
                             ///< complex list is the only user: a built-in cannot be
                             ///< deleted at all, so asking would abort the load of
                             ///< an old backup that has no row for it (CS-2523).
                             ///< nullptr deletes everything the text omits
   const char *retain_msg;   ///< the one line that names what was retained, taking
                             ///< the comma separated names; required with
                             ///< retain_on_replace, because silently keeping an
                             ///< object would hide an edit that was not carried out
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
   bool parse_values;                                ///< parse attribute values when reading a file
   bool is_root;                                     ///< singleton export: object is the root
   const char *exclude1;                             ///< export: skip this name
   const char *exclude2;                             ///< export: skip this name too
   qconf_read_desc read;                             ///< the read path additions (CS-2796)
   qconf_write_desc write;                           ///< the write path additions (CS-2823)
};

/** @brief The descriptor of one object type.
 * @param o the object type
 * @return its row in the object table
 */
const qconf_object_desc &qconf_desc(QConfObj o);

bool qconf_name_is_host(const lDescr *descr, int name_nm);

const char *qconf_get_name(const lListElem *ep, int name_nm);

bool qconf_name_in_list(const lList *names, const lDescr *descr, int name_nm, const char *name);

lList *qconf_parse_list(lList **answer_list, const lDescr *descr, spooling_field *fields,
                        spool_flatfile_format format, const char *filename, const char *content,
                        int (*validate)(const lListElem *, lList **),
                        const spool_flatfile_instr *sfi,
                        int (*prepare)(lListElem *, lList **),
                        bool parse_values);

lListElem *qconf_parse_object(lList **answer_list, const lDescr *descr, spooling_field *fields,
                              spool_flatfile_format format, const char *filename, const char *content,
                              int (*validate)(const lListElem *, lList **),
                              const spool_flatfile_instr *sfi,
                              int (*prepare)(lListElem *, lList **),
                              bool parse_values);

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
