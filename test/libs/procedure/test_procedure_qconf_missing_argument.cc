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
 * @brief Unit tests: a qconf read that lacks an argument it needs is refused, not crashed on (CS-2968)
 */

/*
 * The qmaster side of the qconf read procedure takes its arguments from the
 * request. A request that names no object for a read of one object - which a
 * qconf without the argument check of CS-2969 sent for "qconf -mcal" - used to
 * reach lWhere() with a null pointer and took qmaster down with SIGSEGV.
 *
 * The check sits in QConfModelBase::make_snapshot(), which the qmaster model and
 * the client model share. The client model is driven here, because the qmaster
 * one links against the daemon; the refusal comes before the model fetches
 * anything, so neither GDI nor a data store is reached. Every case must come
 * back refused with an answer instead of crashing; a crash is what makes this
 * test fail.
 */

#include <cstdio>
#include <string>

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_answer.h"

#include "procedure/qconf/ocs_QConfModelClient.h"
#include "procedure/qconf/ocs_QConfParameter.h"

static int s_fail = 0;

/** @def CHECK
 * @brief Assert one condition and record the result
 * @param id the test number, printed as `[Tnn]`
 * @param label what the check is about
 * @param expr the condition that must hold
 */
#define CHECK(id, label, expr)                                                        \
   do {                                                                               \
      const bool ok_ = (expr);                                                        \
      printf("[T%02d] %-70s %s\n", (id), (label), ok_ ? "ok" : "FAIL");               \
      if (!ok_) {                                                                     \
         s_fail++;                                                                    \
      }                                                                               \
   } while (0)

/** @brief Run one read the way qmaster runs it and report whether it was refused
 * @param token the switch the operation names
 * @param sub_procedure the sub-procedure, empty for a table read
 * @param args how many of the required arguments to give
 * @return true when the read failed and said why
 */
static bool
refused(const char *token, const char *sub_procedure, const int args) {
   DENTER(TOP_LAYER);

   ocs::QConfParameter param("qconf");
   qconf_op op;
   op.token = token;
   for (int i = 0; i < args; i++) {
      op.args.emplace_back("x");
   }
   param.set_operation(op);
   param.set_sub_procedure_name(sub_procedure);

   ocs::QConfModelClient model;
   lList *alp = nullptr;
   const bool ok = model.make_snapshot(&alp, param);
   const bool said_why = answer_list_has_error(&alp);
   lFreeList(&alp);

   DRETURN(!ok && said_why);
}

int main(int, char *[]) {
   DENTER_MAIN(TOP_LAYER, "test_procedure_qconf_missing_argument");

   using P = ocs::QConfParameter;

   CHECK(1, "-scal without a name is refused", refused("-scal", "", 0));
   CHECK(2, "-sprj without a name is refused", refused("-sprj", "", 0));
   CHECK(3, "-scat without an id is refused", refused("-scat", "", 0));
   CHECK(4, "-sobjl without arguments is refused", refused("-sobjl", P::OBJECT_FILTER, 0));
   CHECK(5, "-sobjl with two of three arguments is refused", refused("-sobjl", P::OBJECT_FILTER, 2));
   CHECK(6, "-sstnode without a path is refused", refused("-sstnode", P::SHARETREE_NODES, 0));
   CHECK(7, "-shgrp without a group is refused", refused("-shgrp", P::HGROUP_SHOW, 0));
   CHECK(8, "-shgrp_tree without a group is refused", refused("-shgrp_tree", P::HGROUP_TREE, 0));
   CHECK(9, "-shgrp_resolved without a group is refused", refused("-shgrp_resolved", P::HGROUP_RESOLVED, 0));
   CHECK(10, "-shgrp_why with one of two arguments is refused", refused("-shgrp_why", P::HGROUP_WHY, 1));

   printf("%s\n", (s_fail == 0) ? "all tests passed" : "THERE WERE FAILURES");
   DRETURN(s_fail == 0 ? 0 : 1);
}
