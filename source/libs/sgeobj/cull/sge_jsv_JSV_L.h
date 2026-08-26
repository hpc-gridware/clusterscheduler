#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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

/*
 * This code was generated from file source/libs/sgeobj/json/JSV.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Job Submission Verifier
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Job Submission Verifier
*
* An external script that inspects, and may modify or reject, every job as it is submitted.
* One element per configured verifier. The element holds both the configuration (where the script is and how to run it) and the live state of the running script process, because a verifier is kept alive across jobs rather than started per job.
*
*    SGE_STRING(JSV_name) - Name
*    The verifier's name, unique within its context.
*
*    SGE_STRING(JSV_context) - Context
*    Where this verifier runs: in the submit client, or in qmaster. The list is searched by context.
*
*    SGE_STRING(JSV_url) - Script URL
*    The configured location of the script, as written in the JSV configuration.
*
*    SGE_STRING(JSV_type) - Script Type
*    How the script is to be started, derived from the URL.
*
*    SGE_STRING(JSV_user) - Run As User
*    The user the script runs as, when the configuration asks for one other than the caller.
*
*    SGE_STRING(JSV_command) - Script Path
*    The resolved path of the script to execute.
*
*    SGE_STRING(JSV_pid) - Process Id
*    Process id of the running script, as a string, for logging and for killing it.
*
*    SGE_REF(JSV_in) - Input Stream
*    `FILE *` writing to the script's stdin - how the job is sent to it.
*
*    SGE_REF(JSV_out) - Output Stream
*    `FILE *` reading the script's stdout - how its verdict comes back.
*
*    SGE_REF(JSV_err) - Error Stream
*    `FILE *` reading the script's stderr, logged as JSV output.
*
*    SGE_BOOL(JSV_has_to_restart) - Restart Pending
*    Set when the configuration changed under a running script, so it is restarted before the next job.
*
*    SGE_ULONG(JSV_last_mod) - Script Timestamp
*    Modification time of the script file when it was started. Compared on each use, so an edited script is picked up without a reconfiguration.
*
*    SGE_BOOL(JSV_send_env) - Send Environment
*    The script asked for the job environment as well as the job itself. Off by default because the environment can be large.
*
*    SGE_REF(JSV_old_job) - Job Before
*    `lListElem *` to the job as submitted, so the changes the script asks for can be applied to a copy.
*
*    SGE_REF(JSV_new_job) - Job After
*    `lListElem *` to the job the script may modify. This is what is submitted if the verifier accepts.
*
*    SGE_BOOL(JSV_restart) - Restart Requested
*    The script asked to be restarted. Ends the current conversation.
*
*    SGE_BOOL(JSV_accept) - Verdict
*    The script accepted the job. Read together with JSV_done to tell acceptance from a script that died mid-conversation.
*
*    SGE_BOOL(JSV_done) - Conversation Finished
*    The script sent its closing command. The read loop runs until this is set.
*
*    SGE_BOOL(JSV_soft_shutdown) - Soft Shutdown
*    The script asked to be shut down cleanly after this job rather than killed.
*
*    SGE_BOOL(JSV_test) - Test Mode
*    Verify the script's own answers against an expected sequence, used by the JSV tests rather than in production.
*
*    SGE_ULONG(JSV_test_pos) - Test Position
*    How far through the expected sequence the test has got.
*
*    SGE_STRING(JSV_result) - Last Result
*    The last result string the script sent, kept for the test comparison and for the error message.
*
*/

enum {
   JSV_name = JSV_LOWERBOUND,   ///< Name
   JSV_context,   ///< Context
   JSV_url,   ///< Script URL
   JSV_type,   ///< Script Type
   JSV_user,   ///< Run As User
   JSV_command,   ///< Script Path
   JSV_pid,   ///< Process Id
   JSV_in,   ///< Input Stream
   JSV_out,   ///< Output Stream
   JSV_err,   ///< Error Stream
   JSV_has_to_restart,   ///< Restart Pending
   JSV_last_mod,   ///< Script Timestamp
   JSV_send_env,   ///< Send Environment
   JSV_old_job,   ///< Job Before
   JSV_new_job,   ///< Job After
   JSV_restart,   ///< Restart Requested
   JSV_accept,   ///< Verdict
   JSV_done,   ///< Conversation Finished
   JSV_soft_shutdown,   ///< Soft Shutdown
   JSV_test,   ///< Test Mode
   JSV_test_pos,   ///< Test Position
   JSV_result   ///< Last Result
};

LISTDEF(JSV_Type)
   SGE_STRING(JSV_name, CULL_DEFAULT)
   SGE_STRING(JSV_context, CULL_DEFAULT)
   SGE_STRING(JSV_url, CULL_DEFAULT)
   SGE_STRING(JSV_type, CULL_DEFAULT)
   SGE_STRING(JSV_user, CULL_DEFAULT)
   SGE_STRING(JSV_command, CULL_DEFAULT)
   SGE_STRING(JSV_pid, CULL_DEFAULT)
   SGE_REF(JSV_in, CULL_ANY_SUBTYPE, CULL_NO_TRANSFER)
   SGE_REF(JSV_out, CULL_ANY_SUBTYPE, CULL_NO_TRANSFER)
   SGE_REF(JSV_err, CULL_ANY_SUBTYPE, CULL_NO_TRANSFER)
   SGE_BOOL(JSV_has_to_restart, CULL_DEFAULT)
   SGE_ULONG(JSV_last_mod, CULL_DEFAULT)
   SGE_BOOL(JSV_send_env, CULL_DEFAULT)
   SGE_REF(JSV_old_job, CULL_ANY_SUBTYPE, CULL_NO_TRANSFER)
   SGE_REF(JSV_new_job, CULL_ANY_SUBTYPE, CULL_NO_TRANSFER)
   SGE_BOOL(JSV_restart, CULL_DEFAULT)
   SGE_BOOL(JSV_accept, CULL_DEFAULT)
   SGE_BOOL(JSV_done, CULL_DEFAULT)
   SGE_BOOL(JSV_soft_shutdown, CULL_DEFAULT)
   SGE_BOOL(JSV_test, CULL_DEFAULT)
   SGE_ULONG(JSV_test_pos, CULL_DEFAULT)
   SGE_STRING(JSV_result, CULL_DEFAULT)
LISTEND

NAMEDEF(JSVN)
   NAME("JSV_name")
   NAME("JSV_context")
   NAME("JSV_url")
   NAME("JSV_type")
   NAME("JSV_user")
   NAME("JSV_command")
   NAME("JSV_pid")
   NAME("JSV_in")
   NAME("JSV_out")
   NAME("JSV_err")
   NAME("JSV_has_to_restart")
   NAME("JSV_last_mod")
   NAME("JSV_send_env")
   NAME("JSV_old_job")
   NAME("JSV_new_job")
   NAME("JSV_restart")
   NAME("JSV_accept")
   NAME("JSV_done")
   NAME("JSV_soft_shutdown")
   NAME("JSV_test")
   NAME("JSV_test_pos")
   NAME("JSV_result")
NAMEEND

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define JSV_SIZE sizeof(JSVN)/sizeof(char *)


