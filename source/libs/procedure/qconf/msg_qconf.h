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
 * @brief Messages of the `qconf` stored procedure
 *
 * **This file owns the id block 12000-12999.** Append at the bottom and take the
 * next free number; never reuse one that has been given out, because the id is
 * the catalogue key and a number that comes back with another text would hand
 * out the wrong translation.
 *
 * Do not write the message macro's name followed by an opening parenthesis
 * anywhere in this file except in a real definition. The testsuite's
 * update_macro_messages_list() scans every line for that exact string with no
 * regard for comments, and a line it cannot parse as a definition aborts the
 * build - which is how this note came to be written this way.
 *
 * The messages came from `clients/qconf/msg_qconf.h` when the code moved out of
 * the client - `MSG_ROLE_DOESNOTEXIST_S` with the object descriptor table
 * (CS-2799), the rest with the host group views (CS-2816). They kept the
 * client's ids at first, which was wrong twice over: this file had no block of
 * its own, unlike its siblings `qhost` (9000), `qquota` (10000) and `qstat`
 * (19010), and `MSG_QCONF_WHY_LITERAL_S` carried 5255, which
 * `MSG_CENTRY_KEPT_BUILTIN_S` in the client had been given independently by
 * CS-2523. Splitting the two across two files had hidden that collision from a
 * per-file check. Renumbering into this block settles both.
 *
 * With CS-2823 the complex list is written by the procedure as well, so the
 * client's three NULL messages and its retained-built-ins line followed the
 * code here and were renumbered in turn. The collision described above is
 * therefore gone twice: the two names now live in one file, where a per-file
 * check can see them.
 *
 * The texts are unchanged throughout. The ids the client gave up stay empty
 * there rather than being handed out again.
 */

#include <cinttypes>

// clang-format off

#define MSG_HGROUP_NOTEXIST_S                         _MESSAGE(12000, _("Host group " SFQ " does not exist"))
#define MSG_ROLE_DOESNOTEXIST_S                       _MESSAGE(12001, _(SFQ " is not a role"))

/* the routes -shgrp_why reports (CS-2680, N-I-7) */
#define MSG_QCONF_WHY_LITERAL_S                       _MESSAGE(12002, _(SFN " is a member as a literal entry"))
#define MSG_QCONF_WHY_GROUP_SS                        _MESSAGE(12003, _(SFN " is a member through the group " SFN " (literal entry)"))
#define MSG_QCONF_WHY_MATCHER_SS                      _MESSAGE(12004, _(SFN " is a member through the matcher " SFQ))
#define MSG_QCONF_WHY_MATCHER_GROUP_SSS               _MESSAGE(12005, _(SFN " is a member through the matcher " SFQ " of the group " SFN))
#define MSG_QCONF_WHY_NOT_S                           _MESSAGE(12006, _(SFN " is not a member"))

#define MSG_CENTRY_DOESNOTEXIST_S                     _MESSAGE(12007, _("Complex attribute " SFQ " does not exist"))

/* moved out of the client with the cluster queue read (CS-2818); its only user
 * is the model now, and a library cannot include a client header */
#define MSG_CQUEUE_NAMENOTCORRECT_SS                  _MESSAGE(12008, _("The queue name " SFQ " is not correct.  Queue names may not begin with @.  Perhaps you mean \"*" SFN "\"?"))

/* moved out of the client with the category read (CS-2819); it formats a
 * number, which is why the read table reports it with the parsed id rather
 * than with the text the user typed */
#define MSG_CAT_DOESNOTEXIST_U                        _MESSAGE(12009, _("Category " sge_u32 " does not exist"))
#define MSG_QCONF_NOWRITEPROVIDER                     _MESSAGE(12010, _("no write provider installed - a write procedure can only run inside qmaster"))
#define MSG_QCONF_NOWRITEOBJECT                       _MESSAGE(12011, _("the write procedure was not told which object type to write"))
#define MSG_QCONF_EMPTYOBJECTSET                      _MESSAGE(12012, _("no object was given to write"))

/* CS-2823: the complex list is written by the procedure now, so the checks that
 * need the whole list and the report of what a replace retained happen here.
 * The three NULL messages and the retained-built-ins line moved over from
 * clients/qconf/msg_qconf.h (5103-5105 and 5255) and are renumbered into this
 * file's band, as a moved message has to be. */
#define MSG_CENTRY_NULL_NAME                          _MESSAGE(12013, _("Invalid complex attribute definition"))
#define MSG_CENTRY_NULL_SHORTCUT_S                    _MESSAGE(12014, _("Complex attribute " SFQ " has no shortcut defined"))
#define MSG_CENTRY_NULL_URGENCY                       _MESSAGE(12015, _("Complex urgency definition is missing"))
#define MSG_CENTRY_KEPT_BUILTIN_S                     _MESSAGE(12016, _("kept built-in complex attribute(s) " SFNMAX " that the file does not define - built-ins cannot be deleted"))

/* The qconf actions (CS-2962): "-purge" (CS-2948) and the access list
 * membership switches "-au"/"-du"/"-am"/"-ao"/"-dm"/"-do" (CS-2959) run in
 * qmaster. The messages of their old client paths that the actions produce moved
 * over from clients/qconf/msg_qconf.h (5004, 5150 and 5050-5056) and are
 * renumbered into this file's band; names and texts are unchanged, because the
 * testsuite matches them. */
#define MSG_QCONF_NOACTIONPROVIDER_S                  _MESSAGE(12017, _("no function for the qconf action " SFQ " is installed - an action can only run inside qmaster"))
#define MSG_QCONF_PURGEINCOMPLETE                     _MESSAGE(12018, _("a purge needs a cluster queue, an attribute list and a host or host group"))
#define MSG_QCONF_ATTR_ARGS_NOT_FOUND                 _MESSAGE(12019, _("Attribute name (" SFQ ") and/or value (" SFQ ") not found"))
#define MSG_CQUEUE_DOESNOTEXIST_S                     _MESSAGE(12020, _("Cluster queue entry " SFQ " does not exist"))
#define MSG_QCONF_MEMBERINCOMPLETE                    _MESSAGE(12021, _("a change of access list entries needs at least one user and one access list"))
#define MSG_ACL_USERINACL_SS                          _MESSAGE(12022, _(SFQ " is already in access list " SFQ))
#define MSG_ACL_CANTADDTOACL_SS                       _MESSAGE(12023, _("can't add " SFQ " to access list " SFQ))
#define MSG_ACL_ADDTOACL_SS                           _MESSAGE(12024, _("added " SFQ " to access list " SFQ))
#define MSG_ACL_ACLDOESNOTEXIST_S                     _MESSAGE(12025, _("access list " SFQ " doesn't exist"))
#define MSG_ACL_USERNOTINACL_SS                       _MESSAGE(12026, _("user " SFQ " is not in access list " SFQ))
#define MSG_ACL_CANTDELFROMACL_SS                     _MESSAGE(12027, _("can't delete user " SFQ " from access list " SFQ))
#define MSG_ACL_DELFROMACL_SS                         _MESSAGE(12028, _("deleted user " SFQ " from access list " SFQ))
/* CS-2966: the share tree node switches -astnode/-mstnode/-dstnode run in qmaster;
 * the one message of their old client path that lived in clients/qconf/msg_qconf.h
 * (5009) moved over, name and text unchanged. */
#define MSG_QCONF_STNODEINCOMPLETE                    _MESSAGE(12029, _("a change of share tree nodes needs a node list and an operation"))
#define MSG_QCONF_POSITIVE_SHARE_VALUE                _MESSAGE(12030, _("share value must be positive"))
/* CS-2968: a read that lacks an argument it needs is refused in qmaster, in the
 * words the client uses for a command line that ends too early */
#define MSG_QCONF_MISSINGARGUMENT_S                   _MESSAGE(12031, _("no option argument provided to " SFQ))

/* CS-2970: used only by this procedure, moved here from the general catalogues */
#define MSG_ANSWER_CONFIGXNOTDEFINED_S                _MESSAGE(12032, _("configuration " SFN " not defined"))
#define MSG_CALENDAR_XISNOTACALENDAR_S                _MESSAGE(12033, _(SFN " is not a calendar"))
#define MSG_CKPT_XISNOTCHKPINTERFACEDEF_S             _MESSAGE(12034, _(SFN " is not a checkpointing interface definition"))
#define MSG_EXEC_XISNOTANEXECUTIONHOST_S              _MESSAGE(12035, _(SFN " is not an execution host"))
#define MSG_PARALLEL_XNOTAPARALLELEVIRONMENT_S        _MESSAGE(12036, _(SFN " is not a parallel environment"))
#define MSG_PROJECT_XISNOKNWOWNPROJECT_S              _MESSAGE(12037, _(SFN " is not known as project"))
#define MSG_QCONF_NOXDEFINED_S                        _MESSAGE(12038, _("no " SFN " defined"))
#define MSG_QCONF_NOEXECUTIONHOSTSDEFINED             _MESSAGE(12039, _("no execution hosts defined"))
#define MSG_QCONF_NOEVENTCLIENTSREGISTERED            _MESSAGE(12040, _("no event clients registered"))
#define MSG_NORQSFOUND                                _MESSAGE(12041, _("No resource quota set found"))
#define MSG_RQS_REQUEST_DUPLICATE_NAME_S              _MESSAGE(12042, _("Resource quota replacement request contains rqs name " SFQ " multiple times"))
#define MSG_TREE_NOSHARETREE                          _MESSAGE(12043, _("No sharetree"))
#define MSG_TREE_CANTADDNODEXISNONUNIQUE_S            _MESSAGE(12044, _("Could not add node " SFN " to sharetree due to non-unique path"))
#define MSG_TREE_SETTING                              _MESSAGE(12045, _("setting "))
#define MSG_TREE_REMOVING                             _MESSAGE(12046, _("removing "))
#define MSG_ANSWER_XISNOTVALIDSEENODESHARESLIST_S     _MESSAGE(12047, _(SFN " is not a valid argument, see node_shares_list format"))
#define MSG_TREE_MODIFIEDSHARETREE                    _MESSAGE(12048, _("modified sharetree"))
#define MSG_TREE_NOMIDIFIEDSHARETREE                  _MESSAGE(12049, _("no modifications to sharetree"))
#define MSG_TREE_CANTDELROOTNODE                      _MESSAGE(12050, _("can't delete root node"))
#define MSG_TREE_CANTDELNODESWITHCHILD                _MESSAGE(12051, _("can't delete nodes with children"))
#define MSG_TREE_UNABLETOLACATEXINSHARETREE_S         _MESSAGE(12052, _("Unable to locate " SFN " in sharetree"))
#define MSG_TABLE_HOST                                "HOST"
#define MSG_TABLE_PROCESSORS                          "PROCESSORS"
#define MSG_TABLE_SOCKETS                             "NSOC"
#define MSG_TABLE_CORES                               "NCOR"
#define MSG_TABLE_ARCH                                "ARCH"
#define MSG_TABLE_SUM_F                               "SUM"
#define MSG_TABLE_EV_ID                               "ID"
#define MSG_TABLE_EV_NAME                             "NAME"
#define MSG_TABLE_EV_POOL                             "POOL"
#define MSG_TABLE_SIZE                                "SIZE"

/* CS-2822: the qconf write procedure, which applies a set of objects. Moved here
 * from msg_qmaster.h with the procedure's other messages (CS-2970).
 *
 * The two dry-run lines repeat the wording of MSG_QCONF_DRYRUNWOULDADD_S and
 * MSG_QCONF_DRYRUNWOULDMODIFY_S in the qconf client on purpose. Add against
 * modify is decided here, in qmaster, because only this side holds the master
 * list - so this is the side that has to say which it would be. The client keeps
 * its own copies for the paths that are not procedures yet, the bulk delete and
 * the configuration, and the two catalogues cannot include each other: both
 * files are called msg_qconf.h and the bands must not overlap. */
#define MSG_QCONF_WRITEWOULDADD_S                  _MESSAGE(12053, _("[dry-run] would add " SFQ))
#define MSG_QCONF_WRITEWOULDMODIFY_S               _MESSAGE(12054, _("[dry-run] would modify " SFQ))
#define MSG_QCONF_WRITEWRONGTARGET_SS              _MESSAGE(12055, _("the write procedure was addressed to " SFQ " but writes " SFQ))
#define MSG_QCONF_WRITEEMPTYTEXT_S                 _MESSAGE(12056, _(PFNMAX ": no object text was sent"))
#define MSG_QCONF_WRITEEMPTYTEXT                   _MESSAGE(12057, _("no object text was sent"))
/* moved here from clients/qconf/msg_qconf.h (5251): the names of "-Mrqs <file>
 * <names>" are selected where the file is parsed, which is qmaster (CS-2823) */
#define MSG_RQS_NOTFOUNDINFILE_SS                  _MESSAGE(12058, _("resource quota set " SFQ " not found in file " PFQMAX))
/* Only a type whose text holds a list can produce this, and of those only the
 * resource quota sets: the complex list replaces by design, so what its text
 * omits is deleted on purpose (CS-2523). Hence the switch in the wording. */
#define MSG_QCONF_WRITENOTREMOVED_S                _MESSAGE(12059, _(SFQ " is still there: a modify writes what the text holds and removes nothing - delete it with \"qconf -drqs\""))

/* CS-2823: the complex list is a replacing write - what its text does not hold
 * is deleted. The dry run says so in the same shape as the add and the modify
 * above. */
#define MSG_QCONF_WRITEWOULDDELETE_S               _MESSAGE(12060, _("[dry-run] would delete " SFQ))

/* CS-2823: the three ways a replacing write is refused before anything is
 * touched. All three are programming errors rather than user mistakes - no
 * switch produces them - which is why they are refusals and not warnings: a
 * replace that goes ahead on a wrong premise deletes. */
#define MSG_QCONF_WRITEREPLACENOTLIST              _MESSAGE(12061, _("a replacing write needs a text that holds a list of objects"))
#define MSG_QCONF_WRITENOREPLACE_S                 _MESSAGE(12062, _("objects of type " SFQ " cannot be deleted by a replacing write"))
#define MSG_QCONF_WRITEREPLACEWITHSELECT           _MESSAGE(12063, _("a replacing write cannot be restricted to selected object names"))
#define MSG_QCONF_WRITENAMEWITHLIST                _MESSAGE(12064, _("a named write cannot be a list: the name would be given to every object the text holds"))
#define MSG_QCONF_WRITEWOULDLEAVE_S                _MESSAGE(12065, _("[dry-run] would leave " SFQ " unchanged"))
#define MSG_QCONF_WRITENOCHANGE                    _MESSAGE(12066, _("nothing was changed: what the text describes is already stored"))

/* CS-2973: a read for the editor is checked as the write it prepares; one whose
 * object type cannot be told is refused rather than let through unchecked */
#define MSG_QCONF_EDITREADUNKNOWN_S                _MESSAGE(12067, _("the read " SFQ " names no object type that could be edited"))

// clang-format on
