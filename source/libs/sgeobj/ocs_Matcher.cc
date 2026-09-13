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
 * @brief Host group members that describe a set of hosts instead of naming one
 *
 * The stub of the open source edition. The real implementation is selected at
 * build time from the extension sources; this file carries the same interface
 * and answers so that everything downstream is inert without a case distinction
 * anywhere in the core.
 *
 * The classification in `uti/ocs_Pattern.h` answers the same in both editions -
 * a member *is* recognised as a matcher here, because a member can be refused
 * *as a matcher* only by something that recognises one. What differs is that
 * #ocs::Matcher::prepare refuses, so no host group ever comes to carry one; from
 * there on nothing downstream has anything to do.
 */

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/ocs_Matcher.h"

#include "msg_qmaster.h"
#include "msg_common.h"

/** @brief Refuses a matcher, because this edition does not carry the feature
 *
 * The two-message form the product uses elsewhere for a feature of the
 * commercial edition: the specific refusal, then where to get it.
 *
 * @param member the member as it was written
 * @param hgroup the host group the member is destined for
 * @param[out] out unused; nothing is stored
 * @param[out] answer_list receives the refusal
 *
 * @return always false
 */
bool
ocs::Matcher::prepare(const char *member, const lListElem *hgroup, dstring *out, lList **answer_list) {
   DENTER(TOP_LAYER);

   answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_MATCHER_NOTSUPPORTED);
   answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_CONTACT_HPC_GRIDWARE);

   DRETURN(false);
}

/** @brief Contributes no hosts, because no member here is ever a matcher
 *
 * The caller unites this with the literal members and the members reached
 * through group references, so an empty contribution leaves the enumeration
 * exactly as it was before the feature existed.
 *
 * @param member_list the members to expand
 * @param candidates the hosts a matcher would be resolved against
 * @param[out] hosts receives nothing
 * @param[out] answer_list receives nothing
 *
 * @return always true
 */
bool
ocs::Matcher::expand(const lList *member_list, const lList *candidates, lList **hosts,
                     lList **answer_list) {
   DENTER(TOP_LAYER);
   DRETURN(true);
}

/** @brief Answers no, because no host group here can carry a matcher
 *
 * The caller asks this only after the enumerated membership has already said
 * no, so answering no leaves the permission decision exactly what it was before
 * the feature existed.
 *
 * @param hgroup the group to ask
 * @param hostname the host in question
 * @param master_hgroup_list the groups its references resolve against
 *
 * @return always false
 */
bool
ocs::Matcher::admits(const lListElem *hgroup, const char *hostname, const lList *master_hgroup_list) {
   DENTER(TOP_LAYER);
   DRETURN(false);
}

/** @brief Reports nothing, because no host group can carry a matcher here
 *
 * @param master_hgroup_list the host groups to examine
 * @param[out] answer_list receives nothing
 *
 * @return always true
 */
bool
ocs::Matcher::report_ineffective(const lList *master_hgroup_list, lList **answer_list) {
   DENTER(TOP_LAYER);
   DRETURN(true);
}
