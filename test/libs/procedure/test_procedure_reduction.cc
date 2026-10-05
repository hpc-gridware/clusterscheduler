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
 * @brief Unit tests for the RBAC reduction channel of the procedure response (CS-2821)
 */

/*
 * The channel qmaster uses to tell a client what RBAC removed from a response.
 * Nothing reduces yet, so what is tested here is the carrier rather than any
 * reduction: that the entry survives the bundle in both directions, that an
 * absent entry reads as "not reduced" - which is what a response from an older
 * qmaster looks like - and that the bits combine.
 *
 * The bundle's value sublist is declared CULL_ANY_SUBTYPE, which is why a ULNG
 * entry can sit next to the ST_Type response text at all. If that ever changes,
 * this test is what notices.
 */

#include <cstdint>
#include <cstdio>
#include <string>

#include "uti/sge_rmon_macros.h"

#include "sgeobj/cull/sge_param_SPP_L.h"
#include "sgeobj/cull/sge_ulong_ULNG_L.h"
#include "sgeobj/cull/sge_str_ST_L.h"

#include "ocs_ProcedureModel.h"
#include "ocs_ProcedureParameter.h"

static int s_fail = 0;

/** @def CHECK
 * @brief Assert one condition and record the result
 *
 * Prints `ok`/`FAIL` with the test's id and label and counts the failure, so a
 * run reports every problem rather than stopping at the first.
 *
 * @param id the test number, printed as `[Tnn]`
 * @param label what the check is about
 * @param expr the condition that must hold
 */
#define CHECK(id, label, expr) \
   do { \
      if (!(expr)) { \
         printf("FAIL  [T%02d] %s\n", (id), (label)); \
         ++s_fail; \
      } else { \
         printf("ok    [T%02d] %s\n", (id), (label)); \
      } \
   } while (0)

namespace {
   /** @brief A model that reads a response the test built instead of calling GDI
    *
    * `ProcedureModel::make_snapshot()` is the only other way to fill the
    * response, and it talks to a qmaster. This hands one in directly.
    */
   class TestModel : public ocs::ProcedureModel {
   public:
      /** @brief Adopt a response bundle
       * @param bundle the bundle; the model takes ownership
       */
      void adopt(lList *bundle) {
         lFreeList(&procedure_response);
         procedure_response = bundle;
      }
   };

   /** @brief Build a response bundle the way prepare_response() does
    *
    * @param text      the rendered output, or nullptr to leave the entry out
    * @param reduction the reduction mask
    * @param with_reduction false to leave the reduction entry out entirely,
    *                       which is what a response from an older qmaster is
    * @return the bundle, which the caller owns
    */
   lList *make_response(const char *text, uint32_t reduction, bool with_reduction) {
      /* add_parameter_bundle() asserts a non-null bundle, so it starts as an
       * empty list rather than as nullptr */
      lList *bundle = lCreateList("", SPP_Type);

      if (text != nullptr) {
         lList *output_list = nullptr;
         lAddElemStr(&output_list, ST_name, text, ST_Type);
         ocs::ProcedureParameter::add_parameter_bundle(bundle, ocs::ProcedureParameter::RESPONSE,
                                                       output_list);
      }
      if (with_reduction) {
         lList *reduction_list = nullptr;
         lAddElemUlong(&reduction_list, ULNG_value, reduction, ULNG_Type);
         ocs::ProcedureParameter::add_parameter_bundle(bundle, ocs::ProcedureParameter::REDUCTION,
                                                       reduction_list);
      }
      return bundle;
   }
}

int main(int, char *[]) {
   DENTER_MAIN(TOP_LAYER, "test_procedure_reduction");

   using P = ocs::ProcedureParameter;

   {
      TestModel model;
      model.adopt(make_response("all.q\n", P::REDUCTION_NONE, true));
      CHECK(1, "an explicit zero reads as not reduced", model.get_reduction() == P::REDUCTION_NONE);
      CHECK(2, "the response text is unaffected by the new entry",
            std::string(model.get_output_text()) == "all.q\n");
   }
   {
      TestModel model;
      model.adopt(make_response("all.q\n", P::REDUCTION_MASKED, true));
      CHECK(3, "the masked bit survives the bundle", model.get_reduction() == P::REDUCTION_MASKED);
   }
   {
      TestModel model;
      model.adopt(make_response("all.q\n", P::REDUCTION_HIDDEN, true));
      CHECK(4, "the hidden bit survives the bundle", model.get_reduction() == P::REDUCTION_HIDDEN);
   }
   {
      TestModel model;
      model.adopt(make_response("all.q\n", P::REDUCTION_MASKED | P::REDUCTION_HIDDEN, true));
      const uint32_t both = P::REDUCTION_MASKED | P::REDUCTION_HIDDEN;
      CHECK(5, "the two bits combine", model.get_reduction() == both);
      CHECK(6, "and each is recognisable in the combination",
            (model.get_reduction() & P::REDUCTION_MASKED) != 0 &&
            (model.get_reduction() & P::REDUCTION_HIDDEN) != 0);
   }
   {
      /* a response from a qmaster that does not know the entry: it must read as
       * not reduced, never as reduced - an old daemon does not reduce */
      TestModel model;
      model.adopt(make_response("all.q\n", P::REDUCTION_MASKED, false));
      CHECK(7, "an absent entry reads as not reduced", model.get_reduction() == P::REDUCTION_NONE);
      CHECK(8, "and the response text still arrives",
            std::string(model.get_output_text()) == "all.q\n");
   }
   {
      /* the bits must not collide: a write guard asks for "anything at all" */
      CHECK(9, "the bits are distinct", (P::REDUCTION_MASKED & P::REDUCTION_HIDDEN) == 0);
      CHECK(10, "and none of them is zero",
            P::REDUCTION_MASKED != P::REDUCTION_NONE && P::REDUCTION_HIDDEN != P::REDUCTION_NONE);
   }

   printf("%s\n", (s_fail == 0) ? "all tests passed" : "THERE WERE FAILURES");
   DRETURN(s_fail == 0 ? 0 : 1);
}
