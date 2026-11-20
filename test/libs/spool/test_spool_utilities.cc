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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Unit tests for utilities in `libs/spool`
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_dstring.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/sge_object.h"

#include "spool/sge_spooling_utilities.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_qinstance.h"

#include "tom/ocs_Model.h"
#include "tom/ocs_Raw.h"

static int s_fail = 0;

/** @def CHECK
 * @brief Assert one condition and record the result
 *
 * Prints `PASS`/`FAIL` with the test's id and label and counts the failure, so
 * a run reports every problem rather than stopping at the first.
 *
 * @param id the test number, printed as `[Tnn]`
 * @param label what the check is about, printed on failure
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

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_spool_utilities");
   component_set_daemonized(true);
   ocs::tom::install_model(ocs::tom::gcs_model());

   int id = 1;
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   // --- spool_get_fields_to_spool ---
   printf("\n--- spool_get_fields_to_spool ---\n");

   // T01: valid call returns a non-null field array
   spooling_field *fields = spool_get_fields_to_spool(&alp, ocs::tom::QU::descriptor.e, &spool_config_instr);
   alp.reset();
   CHECK(id, "spool_get_fields_to_spool(QU) returns non-null", fields != nullptr); id++;

   // T02: the returned array contains at least one field
   int field_count = 0;
   if (fields != nullptr) {
      while (fields[field_count].nm != NoName) {
         field_count++;
      }
   }
   CHECK(id, "spool_get_fields_to_spool(QU) returns at least one field", field_count > 0); id++;
   fields = spool_free_spooling_fields(fields);

   // T03: null descriptor is rejected, returns null
   spooling_field *null_fields = spool_get_fields_to_spool(&alp, nullptr, &spool_config_instr);
   alp.reset();
   CHECK(id, "spool_get_fields_to_spool(null descriptor) returns null", null_fields == nullptr); id++;

   // --- field-value round-trip ---
   printf("\n--- field-value round-trip ---\n");

   // T04: for every field in QU, serialize to string and parse back gives identical string
   {
      const lDescr *descr = ocs::tom::QU::descriptor.e;
      const auto elem = ocs::tom::create_raw_elem(descr);
      const auto copy = ocs::tom::create_raw_elem(descr);
      dstring elem_str = DSTRING_INIT;
      dstring copy_str = DSTRING_INIT;
      int bad_count = 0;

      for (int i = 0; mt_get_type(descr[i].mt) != lEndT; i++) {
         int nm = descr[i].nm;
         sge_dstring_clear(&elem_str);
         sge_dstring_clear(&copy_str);

         // the value is converted through the field's spooling record, as the flatfile walkers convert it
         const ocs_spool_field_ops *ops = ocs_spool_field_ops_of(nm);
         const char *value = ops != nullptr ? ocs_spool_append_value(*ops, elem.get(), &alp, &elem_str) : nullptr;
         alp.reset();
         const char *reread_value = nullptr;

         if (value != nullptr) {
            ocs_spool_parse_value(*ops, copy.get(), &alp, value);
            alp.reset();
            reread_value = ocs_spool_append_value(*ops, copy.get(), &alp, &copy_str);
            alp.reset();
         }

         if (sge_strnullcmp(value, reread_value) != 0) {
            printf("   mismatch for field %s: \"%s\" != \"%s\"\n", ocs::tom::field_name(nm),
                   value != nullptr ? value : "<null>",
                   reread_value != nullptr ? reread_value : "<null>");
            bad_count++;
         }
      }

      CHECK(id, "all QU fields: serialize+parse round-trip is lossless on a default element",
            bad_count == 0); id++;

      sge_dstring_free(&elem_str);
      sge_dstring_free(&copy_str);
   }

   // --- the contract of ocs_spool_append_value / ocs_spool_parse_value ---
   printf("\n--- append/parse contract ---\n");
   {
      const auto qinstance = ocs::tom::create_raw_elem(ocs::tom::QU::descriptor.e);
      dstring buffer = DSTRING_INIT;

      // T05: an empty element is reported, nothing is rendered
      const char *value = ocs_spool_append_value(*ocs_spool_field_ops_of(QU_qname), nullptr, &alp, &buffer);
      CHECK(id, "append_value: empty element gives nullptr and an error",
            value == nullptr && answer_list_has_error(&alp)); id++;
      alp.reset();

      // T06: a field of another type is reported, nothing is rendered (no exception)
      sge_dstring_clear(&buffer);
      value = ocs_spool_append_value(*ocs_spool_field_ops_of(CQ_name), qinstance.get(), &alp, &buffer);
      CHECK(id, "append_value: field the element does not hold gives nullptr and an error",
            value == nullptr && answer_list_has_error(&alp)); id++;
      alp.reset();

      // T07: parsing into a field of another type is reported and stores nothing (no exception)
      bool ok = ocs_spool_parse_value(*ocs_spool_field_ops_of(CQ_name), qinstance.get(), &alp, "all.q");
      CHECK(id, "parse_value: field the element does not hold gives false and an error",
            !ok && answer_list_has_error(&alp)); id++;
      alp.reset();

      // T08: a list field has no plain value: nothing rendered, parsing it is no error and stores nothing
      sge_dstring_clear(&buffer);
      value = ocs_spool_append_value(*ocs_spool_field_ops_of(QU_load_thresholds), qinstance.get(), &alp, &buffer);
      ok = ocs_spool_parse_value(*ocs_spool_field_ops_of(QU_load_thresholds), qinstance.get(), &alp, "NONE");
      CHECK(id, "list field: append gives nullptr, parse gives true and leaves the list unset",
            value == nullptr && ok && !answer_list_has_error(&alp) &&
            ocs::tom::get<const lList *>(qinstance.get(), QU_load_thresholds) == nullptr); id++;
      alp.reset();

      // T09: a host field: unset is written as NONE, a parsed name is stored as a host and written back
      sge_dstring_clear(&buffer);
      value = ocs_spool_append_value(*ocs_spool_field_ops_of(QU_qhostname), qinstance.get(), &alp, &buffer);
      const bool unset_is_none = value != nullptr && strcmp(value, NONE_STR) == 0;
      ok = ocs_spool_parse_value(*ocs_spool_field_ops_of(QU_qhostname), qinstance.get(), &alp, "host1");
      sge_dstring_clear(&buffer);
      value = ocs_spool_append_value(*ocs_spool_field_ops_of(QU_qhostname), qinstance.get(), &alp, &buffer);
      CHECK(id, "host field: unset written as NONE, parsed host stored and written back",
            unset_is_none && ok && value != nullptr && strcmp(value, "host1") == 0 &&
            strcmp(ocs::tom::get<ocs::tom::Host>(qinstance.get(), QU_qhostname).c_str(), "host1") == 0); id++;
      alp.reset();

      // T10: a field of a symbolic kind is parsed from and written as its word
      ok = ocs_spool_parse_value(*ocs_spool_field_ops_of(QU_qtype), qinstance.get(), &alp, "BATCH");
      sge_dstring_clear(&buffer);
      value = ocs_spool_append_value(*ocs_spool_field_ops_of(QU_qtype), qinstance.get(), &alp, &buffer);
      // written as ocs_object_append_symbolic() writes a queue type: each name followed by a blank
      DSTRING_STATIC(expected, 64);
      const char *const expected_word = ocs_object_append_symbolic(&expected, ocs_object_symbolic::qinstance_type, BQ);
      CHECK(id, "symbolic field: BATCH parsed into the BQ bit and written back as BATCH",
            ok && ocs::tom::get<std::uint32_t>(qinstance.get(), QU_qtype) == static_cast<std::uint32_t>(BQ) &&
            value != nullptr && expected_word != nullptr && strcmp(value, expected_word) == 0); id++;
      alp.reset();

      // T11: a number that does not parse is refused and leaves the field as it was
      ocs::tom::set(qinstance.get(), QU_seq_no, std::uint32_t{7});
      ok = ocs_spool_parse_value(*ocs_spool_field_ops_of(QU_seq_no), qinstance.get(), &alp, "seven");
      CHECK(id, "number field: unparsable text gives false and keeps the old value",
            !ok && ocs::tom::get<std::uint32_t>(qinstance.get(), QU_seq_no) == 7U); id++;
      alp.reset();

      sge_dstring_free(&buffer);
   }

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
