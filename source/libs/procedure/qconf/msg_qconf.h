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

// clang-format on
