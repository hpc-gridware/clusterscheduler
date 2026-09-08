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

/*
 * The amount of a resource request is parsed once, when the request is built, and stored in
 * CE_doubleval. The scheduler used to parse the string form a second time while matching, which
 * was redundant and made the string form the effective source of truth. These tests pin the
 * current contract: for a numeric type the request amount comes from CE_doubleval, and the string
 * form is carried for diagnostics only.
 *
 * Several tests deliberately set CE_stringval and CE_doubleval to values that disagree. That
 * never happens in a real request - it is the only way to prove which of the two is read.
 */

#include <cstdio>
#include <cstring>

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_centry.h"
#include "sgeobj/cull/sge_all_listsL.h"

#include "sge_complex_schedd.h"

static int failures = 0;

static void
check(const char *id, const char *what, int got, int expected) {
   if (got == expected) {
      printf("ok    [%s] %s\n", id, what);
   } else {
      printf("FAIL  [%s] %s: got %d, expected %d\n", id, what, got, expected);
      failures++;
   }
}

/**
 * Build the request side of a match: what the job asked for. A real request always has both
 * fields consistent; the tests below vary them on purpose.
 */
static lListElem *
make_request(const char *name, u_long32 type, const char *stringval, double doubleval) {
   lListElem *ep = lCreateElem(CE_Type);
   lSetString(ep, CE_name, name);
   lSetUlong(ep, CE_valtype, type);
   lSetString(ep, CE_stringval, stringval);
   lSetDouble(ep, CE_doubleval, doubleval);
   return ep;
}

/**
 * Build the offer side of a match: what a host or queue provides. compare_complexes() takes the
 * name, the type and the relation operator from this element. Leaving both dominant fields at 0
 * makes the per job and the per slot comparison run against the same value, so with one slot the
 * outcome is simply the relation operator applied to request and offer.
 */
static lListElem *
make_offer(const char *name, u_long32 type, u_long32 relop, const char *stringval, double doubleval) {
   lListElem *ep = lCreateElem(CE_Type);
   lSetString(ep, CE_name, name);
   lSetUlong(ep, CE_valtype, type);
   lSetUlong(ep, CE_relop, relop);
   lSetString(ep, CE_stringval, stringval);
   lSetDouble(ep, CE_doubleval, doubleval);
   lSetDouble(ep, CE_pj_doubleval, doubleval);
   return ep;
}

static int
match(lListElem *request, lListElem *offer) {
   char availability_text[2048];
   return compare_complexes(1, request, offer, availability_text, false, false);
}

static void
test_numeric_amount_comes_from_doubleval() {
   lListElem *req, *off;

   /* the ordinary cases, both fields agreeing */
   req = make_request("nc", TYPE_INT, "4", 4.0);
   off = make_offer("nc", TYPE_INT, CMPLXLE_OP, "8", 8.0);
   check("T01", "request 4 of 8 matches", match(req, off), 1);
   lFreeElem(&req);
   lFreeElem(&off);

   req = make_request("nc", TYPE_INT, "16", 16.0);
   off = make_offer("nc", TYPE_INT, CMPLXLE_OP, "8", 8.0);
   check("T02", "request 16 of 8 does not match", match(req, off), 0);
   lFreeElem(&req);
   lFreeElem(&off);

   /* the discriminating case: the two fields disagree and CE_doubleval wins */
   req = make_request("nc", TYPE_INT, "16", 4.0);
   off = make_offer("nc", TYPE_INT, CMPLXLE_OP, "8", 8.0);
   check("T03", "amount is read from CE_doubleval, not from CE_stringval", match(req, off), 1);
   lFreeElem(&req);
   lFreeElem(&off);

   /* an unparsable string used to be silently treated as a request for 0, which matched
      anything under <= and nothing under >=; the amount now decides */
   req = make_request("nc", TYPE_INT, "not-a-number", 4.0);
   off = make_offer("nc", TYPE_INT, CMPLXGE_OP, "2", 2.0);
   check("T04", "an unparsable string is not silently read as a request for 0", match(req, off), 1);
   lFreeElem(&req);
   lFreeElem(&off);

   /* memory multipliers are applied once, when the request is built */
   req = make_request("mem", TYPE_MEM, "2G", 2147483648.0);
   off = make_offer("mem", TYPE_MEM, CMPLXLE_OP, "4G", 4294967296.0);
   check("T05", "a memory request of 2G fits into 4G", match(req, off), 1);
   lFreeElem(&req);
   lFreeElem(&off);

   req = make_request("mem", TYPE_MEM, "8G", 8589934592.0);
   off = make_offer("mem", TYPE_MEM, CMPLXLE_OP, "4G", 4294967296.0);
   check("T06", "a memory request of 8G does not fit into 4G", match(req, off), 0);
   lFreeElem(&req);
   lFreeElem(&off);

   /* the boundary is inclusive for <= */
   req = make_request("nc", TYPE_INT, "8", 8.0);
   off = make_offer("nc", TYPE_INT, CMPLXLE_OP, "8", 8.0);
   check("T07", "request equal to the offer matches under <=", match(req, off), 1);
   lFreeElem(&req);
   lFreeElem(&off);

   /* a boolean request travels as 1 or 0 in CE_doubleval */
   req = make_request("bo", TYPE_BOO, "true", 1.0);
   off = make_offer("bo", TYPE_BOO, CMPLXEQ_OP, "true", 1.0);
   check("T08", "a boolean request matches an equal offer", match(req, off), 1);
   lFreeElem(&req);
   lFreeElem(&off);

   req = make_request("bo", TYPE_BOO, "true", 1.0);
   off = make_offer("bo", TYPE_BOO, CMPLXEQ_OP, "false", 0.0);
   check("T09", "a boolean request does not match an unequal offer", match(req, off), 0);
   lFreeElem(&req);
   lFreeElem(&off);
}

static void
test_string_types_still_use_the_string() {
   lListElem *req, *off;

   /* string types have no numeric form at all; CE_doubleval is meaningless for them and must
      not be consulted */
   req = make_request("ar", TYPE_STR, "lx-amd64", 0.0);
   off = make_offer("ar", TYPE_STR, CMPLXEQ_OP, "lx-amd64", 0.0);
   check("T10", "a string request still matches on CE_stringval", match(req, off), 1);
   lFreeElem(&req);
   lFreeElem(&off);

   req = make_request("ar", TYPE_STR, "lx-amd64", 0.0);
   off = make_offer("ar", TYPE_STR, CMPLXEQ_OP, "sol-sparc", 0.0);
   check("T11", "a string request does not match a different value", match(req, off), 0);
   lFreeElem(&req);
   lFreeElem(&off);

   /* for a RESTRING it is the request that carries the pattern and the offer that is matched
      against it - string_base_cmp() documents s1, the request, as the pattern. This is a
      property of the type and is unaffected by how the amount of a numeric request is obtained */
   req = make_request("ar", TYPE_RESTR, "lx*", 0.0);
   off = make_offer("ar", TYPE_RESTR, CMPLXEQ_OP, "lx-amd64", 0.0);
   check("T12", "a RESTRING request is the pattern and matches the offer", match(req, off), 1);
   lFreeElem(&req);
   lFreeElem(&off);

   req = make_request("ar", TYPE_RESTR, "sol*", 0.0);
   off = make_offer("ar", TYPE_RESTR, CMPLXEQ_OP, "lx-amd64", 0.0);
   check("T13", "a RESTRING pattern that does not match the offer is rejected", match(req, off), 0);
   lFreeElem(&req);
   lFreeElem(&off);
}

int
main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_sched_request_amount");

   lInit(nmv);

   test_numeric_amount_comes_from_doubleval();
   test_string_types_still_use_the_string();

   if (failures == 0) {
      printf("\nPASS - 0 failure(s)\n");
   } else {
      printf("\nFAIL - %d failure(s)\n", failures);
   }

   DRETURN(failures == 0 ? 0 : 1);
}
