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
 * @brief Job Submission Verifier: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of JSV, Job Submission Verifier
 *
 * The enumeration is the type of every field number of JSV, so a parameter
 * of this type takes the field numbers of JSV only.
 */
enum JSV_Field {
   /**
    * @brief Name
    *
    * The verifier's name, unique within its context.
    */
   JSV_name = JSV_LOWERBOUND,
   /**
    * @brief Context
    *
    * Where this verifier runs: in the submit client, or in qmaster. The list is searched by context.
    */
   JSV_context,
   /**
    * @brief Script URL
    *
    * The configured location of the script, as written in the JSV configuration.
    */
   JSV_url,
   /**
    * @brief Script Type
    *
    * How the script is to be started, derived from the URL.
    */
   JSV_type,
   /**
    * @brief Run As User
    *
    * The user the script runs as, when the configuration asks for one other than the caller.
    */
   JSV_user,
   /**
    * @brief Script Path
    *
    * The resolved path of the script to execute.
    */
   JSV_command,
   /**
    * @brief Process Id
    *
    * Process id of the running script, as a string, for logging and for killing it.
    */
   JSV_pid,
   /**
    * @brief Input Stream
    *
    * `FILE *` writing to the script's stdin - how the job is sent to it.
    */
   JSV_in,
   /**
    * @brief Output Stream
    *
    * `FILE *` reading the script's stdout - how its verdict comes back.
    */
   JSV_out,
   /**
    * @brief Error Stream
    *
    * `FILE *` reading the script's stderr, logged as JSV output.
    */
   JSV_err,
   /**
    * @brief Restart Pending
    *
    * Set when the configuration changed under a running script, so it is restarted before the next job.
    */
   JSV_has_to_restart,
   /**
    * @brief Script Timestamp
    *
    * Modification time of the script file when it was started. Compared on each use, so an edited script is picked up without a reconfiguration.
    */
   JSV_last_mod,
   /**
    * @brief Send Environment
    *
    * The script asked for the job environment as well as the job itself. Off by default because the environment can be large.
    */
   JSV_send_env,
   /**
    * @brief Job Before
    *
    * `lListElem *` to the job as submitted, so the changes the script asks for can be applied to a copy.
    */
   JSV_old_job,
   /**
    * @brief Job After
    *
    * `lListElem *` to the job the script may modify. This is what is submitted if the verifier accepts.
    */
   JSV_new_job,
   /**
    * @brief Restart Requested
    *
    * The script asked to be restarted. Ends the current conversation.
    */
   JSV_restart,
   /**
    * @brief Verdict
    *
    * The script accepted the job. Read together with JSV_done to tell acceptance from a script that died mid-conversation.
    */
   JSV_accept,
   /**
    * @brief Conversation Finished
    *
    * The script sent its closing command. The read loop runs until this is set.
    */
   JSV_done,
   /**
    * @brief Soft Shutdown
    *
    * The script asked to be shut down cleanly after this job rather than killed.
    */
   JSV_soft_shutdown,
   /**
    * @brief Test Mode
    *
    * Verify the script's own answers against an expected sequence, used by the JSV tests rather than in production.
    */
   JSV_test,
   /**
    * @brief Test Position
    *
    * How far through the expected sequence the test has got.
    */
   JSV_test_pos,
   /**
    * @brief Last Result
    *
    * The last result string the script sent, kept for the test comparison and for the error message.
    */
   JSV_result,
};

namespace ocs::tom {

   /**
    * @brief Job Submission Verifier
    *
    * An external script that inspects, and may modify or reject, every job as it is submitted.
    * One element per configured verifier. The element holds both the configuration (where the script is and how to run it) and the live state of the running script process, because a verifier is kept alive across jobs rather than started per job.
    */
   struct JSV {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {JSV_name, "JSV_name", FieldType::String, Flag::None},
         {JSV_context, "JSV_context", FieldType::String, Flag::None},
         {JSV_url, "JSV_url", FieldType::String, Flag::None},
         {JSV_type, "JSV_type", FieldType::String, Flag::None},
         {JSV_user, "JSV_user", FieldType::String, Flag::None},
         {JSV_command, "JSV_command", FieldType::String, Flag::None},
         {JSV_pid, "JSV_pid", FieldType::String, Flag::None},
         {JSV_in, "JSV_in", FieldType::Ref, Flag::NoTransfer},
         {JSV_out, "JSV_out", FieldType::Ref, Flag::NoTransfer},
         {JSV_err, "JSV_err", FieldType::Ref, Flag::NoTransfer},
         {JSV_has_to_restart, "JSV_has_to_restart", FieldType::Bool, Flag::None},
         {JSV_last_mod, "JSV_last_mod", FieldType::Uint32, Flag::None},
         {JSV_send_env, "JSV_send_env", FieldType::Bool, Flag::None},
         {JSV_old_job, "JSV_old_job", FieldType::Ref, Flag::NoTransfer},
         {JSV_new_job, "JSV_new_job", FieldType::Ref, Flag::NoTransfer},
         {JSV_restart, "JSV_restart", FieldType::Bool, Flag::None},
         {JSV_accept, "JSV_accept", FieldType::Bool, Flag::None},
         {JSV_done, "JSV_done", FieldType::Bool, Flag::None},
         {JSV_soft_shutdown, "JSV_soft_shutdown", FieldType::Bool, Flag::None},
         {JSV_test, "JSV_test", FieldType::Bool, Flag::None},
         {JSV_test_pos, "JSV_test_pos", FieldType::Uint32, Flag::None},
         {JSV_result, "JSV_result", FieldType::String, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of JSV belongs to JSV */
   template <>
   struct TypeOf<JSV_Field> {
      using type = JSV;   ///< the object type
   };

} // namespace ocs::tom

/** @brief The descriptor of JSV for the list library: a synonym for ocs::tom::JSV::descriptor */
inline constexpr const lDescr *JSV_Type = ocs::tom::JSV::descriptor.e;
