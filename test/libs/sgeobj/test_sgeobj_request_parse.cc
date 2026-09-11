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
 * centry_list_parse_from_string() is the single entry point behind "-l" for qsub, qstat, qhost,
 * qquota, qacct and JSV. It splits the request string into "name=value" tokens, and since a
 * resource map request carries a bracketed parameter list after its amount, a separator inside
 * brackets has to be payload rather than a separator.
 *
 * The tests below fix that behaviour, and the behaviour of the amount split that goes with it:
 * the whole string stays in CE_stringval, and CE_doubleval holds only the amount in front of the
 * bracket.
 */

#include <cstdio>
#include <cstring>

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_centry_rsmap.h"
#include "sgeobj/cull/sge_all_listsL.h"

/*
 * A request which carries a parameter list is refused outright on a build without the
 * extensions - centry_rsmap_check_request_params() gates the whole syntax there. So what
 * a case which the full build accepts is expected to return depends on the build.
 *
 * The cases are kept rather than left out, because on such a build they assert the gate
 * itself, which nothing else covers: the gate is compiled out of every build which has
 * the extensions, so this is the only place it is ever executed.
 */
#if defined(WITH_EXTENSIONS)
#  define PARAM_LIST_RC    0
#  define PARAM_LIST_NOTE  ""
#else
#  define PARAM_LIST_RC    -1
#  define PARAM_LIST_NOTE  " (refused, this build has no extensions)"
#endif

static int failures = 0;

static void
check_str(const char *id, const char *what, const char *got, const char *expected) {
   if (got != nullptr && expected != nullptr && strcmp(got, expected) == 0) {
      printf("ok    [%s] %s\n", id, what);
   } else {
      printf("FAIL  [%s] %s: got \"%s\", expected \"%s\"\n", id, what,
             got == nullptr ? "(null)" : got, expected == nullptr ? "(null)" : expected);
      failures++;
   }
}

static void
check_int(const char *id, const char *what, int got, int expected) {
   if (got == expected) {
      printf("ok    [%s] %s\n", id, what);
   } else {
      printf("FAIL  [%s] %s: got %d, expected %d\n", id, what, got, expected);
      failures++;
   }
}

/**
 * Parse a request string and report how many elements it produced, plus the name and value of
 * each, so a test can assert the split without reaching into the list itself.
 */
static lList *
parse(const char *request) {
   return centry_list_parse_from_string(nullptr, request, false);
}

static const char *
value_of(const lList *lp, const char *name) {
   const lListElem *ep = lGetElemStr(lp, CE_name, name);
   return ep == nullptr ? nullptr : lGetString(ep, CE_stringval);
}

static void
test_splitting() {
   lList *lp;

   /* the ordinary cases have to keep working */
   lp = parse("mem=2G");
   check_int("T01", "one request yields one element", (int)lGetNumberOfElem(lp), 1);
   check_str("T01b", "its value is kept", value_of(lp, "mem"), "2G");
   lFreeList(&lp);

   lp = parse("mem=2G,arch=lx-amd64");
   check_int("T02", "a comma separates two requests", (int)lGetNumberOfElem(lp), 2);
   lFreeList(&lp);

   lp = parse("mem=2G arch=lx-amd64");
   check_int("T03", "a space separates two requests", (int)lGetNumberOfElem(lp), 2);
   lFreeList(&lp);

   lp = parse("mem=2G,,arch=lx-amd64");
   check_int("T04", "repeated separators do not produce empty elements",
             (int)lGetNumberOfElem(lp), 2);
   lFreeList(&lp);

   /* the bracket list is one value, whatever it contains */
   lp = parse("gpu=4[same=id]");
   check_int("T05", "a bracket list does not split the request", (int)lGetNumberOfElem(lp), 1);
   check_str("T05b", "the whole string is kept in CE_stringval",
             value_of(lp, "gpu"), "4[same=id]");
   lFreeList(&lp);

   lp = parse("gpu=4[id=gpu1*,same=id],mem=2G");
   check_int("T06", "a comma inside brackets does not split, the one outside does",
             (int)lGetNumberOfElem(lp), 2);
   check_str("T06b", "the bracket list is intact", value_of(lp, "gpu"), "4[id=gpu1*,same=id]");
   check_str("T06c", "the following request is intact", value_of(lp, "mem"), "2G");
   lFreeList(&lp);

   lp = parse("gpu=4[id=a b]");
   check_int("T07", "a space inside brackets does not split", (int)lGetNumberOfElem(lp), 1);
   check_str("T07b", "including the space", value_of(lp, "gpu"), "4[id=a b]");
   lFreeList(&lp);

   /* a nested bracket, as in a character class inside a pattern */
   lp = parse("gpu=1[id=gpu[01]*],mem=2G");
   check_int("T08", "a nested bracket is counted, the list ends at the matching one",
             (int)lGetNumberOfElem(lp), 2);
   check_str("T08b", "the nested class survives", value_of(lp, "gpu"), "1[id=gpu[01]*]");
   lFreeList(&lp);

   /* the same rule makes a character class in a string request work, which it did not before */
   lp = parse("h=node[1,2]");
   check_int("T09", "a character class in a string request is one request",
             (int)lGetNumberOfElem(lp), 1);
   check_str("T09b", "and keeps its comma", value_of(lp, "h"), "node[1,2]");
   lFreeList(&lp);

   /* an unbalanced bracket swallows the rest rather than splitting silently */
   lp = parse("gpu=4[same=id,mem=2G");
   check_int("T10", "an unbalanced bracket yields one element, not two",
             (int)lGetNumberOfElem(lp), 1);
   lFreeList(&lp);

   /* a bare boolean request still becomes TRUE when values are not required */
   lp = parse("some_bool");
   check_str("T11", "a bare request becomes TRUE", value_of(lp, "some_bool"), "TRUE");
   lFreeList(&lp);

   /* the last of two requests for one complex wins, as before */
   lp = parse("gpu=1,gpu=2");
   check_int("T12", "a repeated complex yields one element", (int)lGetNumberOfElem(lp), 1);
   check_str("T12b", "and keeps the last value", value_of(lp, "gpu"), "2");
   lFreeList(&lp);
}

/**
 * Build the master complex list the amount split is resolved against.
 */
static lList *
make_centry_list() {
   lList *lp = lCreateList("complexes", CE_Type);

   lListElem *ep = lCreateElem(CE_Type);
   lSetString(ep, CE_name, "gpu");
   lSetUlong(ep, CE_valtype, TYPE_RSMAP);
   lSetUlong(ep, CE_relop, CMPLXLE_OP);
   lSetUlong(ep, CE_consumable, CONSUMABLE_HOST);
   lSetUlong(ep, CE_requestable, REQU_YES);
   lAppendElem(lp, ep);

   ep = lCreateElem(CE_Type);
   lSetString(ep, CE_name, "gpu_memory");
   lSetUlong(ep, CE_valtype, TYPE_MEM);
   lSetUlong(ep, CE_relop, CMPLXLE_OP);
   lSetUlong(ep, CE_consumable, CONSUMABLE_NO);
   lSetUlong(ep, CE_requestable, REQU_YES);
   lAppendElem(lp, ep);

   // a string characteristic, so that a parameter value may be a pattern
   ep = lCreateElem(CE_Type);
   lSetString(ep, CE_name, "gpu_model");
   lSetUlong(ep, CE_valtype, TYPE_STR);
   lSetUlong(ep, CE_relop, CMPLXEQ_OP);
   lSetUlong(ep, CE_consumable, CONSUMABLE_NO);
   lSetUlong(ep, CE_requestable, REQU_YES);
   lAppendElem(lp, ep);

   ep = lCreateElem(CE_Type);
   lSetString(ep, CE_name, "mem");
   lSetUlong(ep, CE_valtype, TYPE_MEM);
   lSetUlong(ep, CE_relop, CMPLXLE_OP);
   lSetUlong(ep, CE_consumable, CONSUMABLE_YES);
   lSetUlong(ep, CE_requestable, REQU_YES);
   lAppendElem(lp, ep);

   return lp;
}

static void
test_amount_split() {
   lList *centries = make_centry_list();
   lList *lp;
   lList *answer_list;
   const lListElem *ep;

   /* the amount in front of the bracket is what CE_doubleval holds */
   lp = parse("gpu=4[same=id]");
   answer_list = nullptr;
   check_int("T20", "a request with a parameter list is accepted" PARAM_LIST_NOTE,
             centry_list_fill_request(lp, &answer_list, centries, true, false, true),
             PARAM_LIST_RC);
   ep = lGetElemStr(lp, CE_name, "gpu");
   check_int("T20b", "CE_doubleval is the amount", (int)lGetDouble(ep, CE_doubleval), 4);
   check_str("T20c", "CE_stringval still carries the parameters",
             lGetString(ep, CE_stringval), "4[same=id]");
   lFreeList(&answer_list);
   lFreeList(&lp);

   /* no bracket, unchanged behaviour */
   lp = parse("gpu=4");
   answer_list = nullptr;
   check_int("T21", "a plain amount is accepted",
             centry_list_fill_request(lp, &answer_list, centries, true, false, true), 0);
   ep = lGetElemStr(lp, CE_name, "gpu");
   check_int("T21b", "CE_doubleval is the amount", (int)lGetDouble(ep, CE_doubleval), 4);
   lFreeList(&answer_list);
   lFreeList(&lp);

   /* a bracket with no amount in front of it is not a number */
   lp = parse("gpu=[same=id]");
   answer_list = nullptr;
   check_int("T22", "a parameter list without an amount is rejected",
             centry_list_fill_request(lp, &answer_list, centries, true, false, true), -1);
   lFreeList(&answer_list);
   lFreeList(&lp);

   /* the split is for resource maps only; another type keeps rejecting a bracket */
   lp = parse("mem=2G[x=y]");
   answer_list = nullptr;
   check_int("T23", "a parameter list on a non resource map is rejected",
             centry_list_fill_request(lp, &answer_list, centries, true, false, true), -1);
   lFreeList(&answer_list);
   lFreeList(&lp);

   /* The checks that follow the parse apply to a resource map like to any other numeric
      type. An earlier version of the split returned early for a bracketed request and
      skipped them, so a negative amount was accepted with a parameter list and rejected
      without one. allow_neg_consumable is false here, which is what a submit does. */
   lp = parse("gpu=-1");
   answer_list = nullptr;
   check_int("T25", "a negative amount is rejected",
             centry_list_fill_request(lp, &answer_list, centries, true, false, false), -1);
   lFreeList(&answer_list);
   lFreeList(&lp);

   lp = parse("gpu=-1[same=id]");
   answer_list = nullptr;
   check_int("T26", "a negative amount is rejected with a parameter list too",
             centry_list_fill_request(lp, &answer_list, centries, true, false, false), -1);
   lFreeList(&answer_list);
   lFreeList(&lp);

   /* a memory amount is still parsed with its multiplier */
   lp = parse("mem=2G");
   answer_list = nullptr;
   check_int("T24", "a memory request is accepted",
             centry_list_fill_request(lp, &answer_list, centries, true, false, true), 0);
   ep = lGetElemStr(lp, CE_name, "mem");
   check_int("T24b", "the multiplier is applied",
             (int)(lGetDouble(ep, CE_doubleval) / (1024 * 1024)), 2048);
   lFreeList(&answer_list);
   lFreeList(&lp);

   lFreeList(&centries);
}

/**
 * The parameter list is validated by centry_list_fill_request(), so these go through the same
 * entry point as a real submit. A return of 0 means accepted, -1 rejected.
 */
static int
fill(const char *request, lList *centries) {
   lList *lp = parse(request);
   lList *answer_list = nullptr;
   int ret = centry_list_fill_request(lp, &answer_list, centries, true, false, true);
   lFreeList(&answer_list);
   lFreeList(&lp);
   return ret;
}

static void
test_parameter_validation() {
   lList *centries = make_centry_list();

   /* accepted: the reserved names which are implemented, in any order, and no list at all */
   check_int("T30", "a reserved parameter is accepted" PARAM_LIST_NOTE,
             fill("gpu=4[same=id]", centries), PARAM_LIST_RC);
   check_int("T31", "several reserved parameters are accepted" PARAM_LIST_NOTE,
             fill("gpu=4[same=id,bind=no]", centries), PARAM_LIST_RC);
   check_int("T33", "order does not matter" PARAM_LIST_NOTE,
             fill("gpu=4[bind=no,same=id]", centries), PARAM_LIST_RC);
   check_int("T34", "an empty parameter list is accepted" PARAM_LIST_NOTE,
             fill("gpu=4[]", centries), PARAM_LIST_RC);
   check_int("T35", "no parameter list at all is accepted", fill("gpu=4", centries), 0);

   /* rejected */
   check_int("T37", "a parameter list on a non resource map is rejected",
             fill("mem=2G[same=id]", centries), -1);
   check_int("T38", "an unknown parameter name is rejected",
             fill("gpu=4[nosuchthing=1]", centries), -1);
   check_int("T39", "a repeated parameter is rejected",
             fill("gpu=4[same=id,same=id]", centries), -1);
   check_int("T40", "a repeated characteristic is rejected",
             fill("gpu=4[gpu_memory=1G,gpu_memory=2G]", centries), -1);
   check_int("T41", "a parameter without a value is rejected",
             fill("gpu=4[same]", centries), -1);
   check_int("T42", "a parameter without a name is rejected",
             fill("gpu=4[=id]", centries), -1);
   check_int("T43", "an empty parameter between two commas is rejected",
             fill("gpu=4[same=id,,bind=no]", centries), -1);
   check_int("T44", "an unclosed parameter list is rejected",
             fill("gpu=4[same=id", centries), -1);
   check_int("T45", "text after the parameter list is rejected",
             fill("gpu=4[same=id]x", centries), -1);

   /* reserved, but nothing reads them yet - see centry_rsmap_check_request_params() */
   check_int("T46", "id is refused until it is implemented",
             fill("gpu=4[id=gpu0]", centries), -1);
   check_int("T47", "scope is refused until it is implemented, even with its default value",
             fill("gpu=4[scope=host]", centries), -1);
   check_int("T48", "distinct is refused until it is implemented",
             fill("gpu=4[distinct=id]", centries), -1);
   check_int("T49", "a refused reserved parameter is refused next to an accepted one",
             fill("gpu=4[same=id,scope=job]", centries), -1);

   /* same= is honoured, but only with the value "id" - CS-2735 takes a characteristic name */
   check_int("T49c", "same naming a characteristic is refused until it is implemented",
             fill("gpu=4[same=gpu_memory]", centries), -1);
   check_int("T49d", "same with a value which names nothing at all is refused",
             fill("gpu=4[same=banana]", centries), -1);

   /* a characteristic name resolves, but nothing matches it against an instance yet */
   check_int("T32", "a characteristic name is refused until it is implemented",
             fill("gpu=1[gpu_memory=40G]", centries), -1);
   check_int("T36", "a character class in a characteristic value does not derail the walk",
             fill("gpu=1[gpu_model=tesla[AB]]", centries), -1);

   lFreeList(&centries);
}

/**
 * A complex may not be created or modified under one of the reserved parameter names, but one
 * already in the spool keeps working - refusing it there would stop qmaster from starting.
 */
static lListElem *
make_complex(const char *name, const char *shortcut) {
   lListElem *ep = lCreateElem(CE_Type);
   lSetString(ep, CE_name, name);
   lSetString(ep, CE_shortcut, shortcut);
   lSetUlong(ep, CE_valtype, TYPE_INT);
   lSetUlong(ep, CE_relop, CMPLXLE_OP);
   lSetUlong(ep, CE_consumable, CONSUMABLE_NO);
   lSetUlong(ep, CE_requestable, REQU_YES);
   return ep;
}

static void
test_reserved_names() {
   lListElem *ep;
   lList *answer_list;

   /* every reserved name is refused, by name and by shortcut */
   const char *reserved[] = {"id", "same", "scope", "distinct", "bind"};
   for (int i = 0; i < 5; i++) {
      char id[8];
      snprintf(id, sizeof(id), "T5%d", i);

      ep = make_complex(reserved[i], "sc");
      answer_list = nullptr;
      check_int(id, "a reserved name is refused when a complex is created",
                centry_elem_validate(ep, nullptr, &answer_list, false) ? 1 : 0, 0);
      lFreeList(&answer_list);
      lFreeElem(&ep);
   }

   ep = make_complex("something", "same");
   answer_list = nullptr;
   check_int("T55", "a reserved shortcut is refused too",
             centry_elem_validate(ep, nullptr, &answer_list, false) ? 1 : 0, 0);
   lFreeList(&answer_list);
   lFreeElem(&ep);

   /* an ordinary complex is unaffected */
   ep = make_complex("gpu_temp", "gt");
   answer_list = nullptr;
   check_int("T56", "an ordinary complex is accepted",
             centry_elem_validate(ep, nullptr, &answer_list, false) ? 1 : 0, 1);
   lFreeList(&answer_list);
   lFreeElem(&ep);

   /* read from the spool it is kept, so that a qmaster which has one still starts */
   ep = make_complex("same", "sc");
   answer_list = nullptr;
   check_int("T57", "a reserved name found in the spool is kept",
             centry_elem_validate(ep, nullptr, &answer_list, true) ? 1 : 0, 1);
   check_int("T57b", "and is reported", (int)lGetNumberOfElem(answer_list), 1);
   lFreeList(&answer_list);
   lFreeElem(&ep);
}

static void
test_parameter_values_and_defaults() {
   lList *centries = make_centry_list();
   lListElem *ep;
   lList *answer_list;

   /* bind= is reserved and validated now, although nothing reads it until CS-2706 */
   check_int("T60", "bind=no is accepted" PARAM_LIST_NOTE,
             fill("gpu=4[bind=no]", centries), PARAM_LIST_RC);
   check_int("T61", "any other value of bind is rejected", fill("gpu=4[bind=yes]", centries), -1);
   check_int("T62", "an empty value of bind is rejected", fill("gpu=4[bind=]", centries), -1);

   /* distinct= is reserved so the grammar has room for it, and refused until it exists */
   check_int("T63", "distinct is refused as not yet supported",
             fill("gpu=4[distinct=id]", centries), -1);

   /* a default value must be an amount. requestable YES is the case with no other validation,
      so it is the one worth pinning */
   ep = make_complex("gpu2", "g2");
   lSetUlong(ep, CE_valtype, TYPE_RSMAP);
   lSetUlong(ep, CE_consumable, CONSUMABLE_HOST);
   lSetUlong(ep, CE_requestable, REQU_YES);
   lSetString(ep, CE_defaultval, "4");
   answer_list = nullptr;
   check_int("T64", "a numeric default is accepted",
             centry_elem_validate(ep, nullptr, &answer_list, false) ? 1 : 0, 1);
   lFreeList(&answer_list);
   lFreeElem(&ep);

   ep = make_complex("gpu2", "g2");
   lSetUlong(ep, CE_valtype, TYPE_RSMAP);
   lSetUlong(ep, CE_consumable, CONSUMABLE_HOST);
   lSetUlong(ep, CE_requestable, REQU_YES);
   lSetString(ep, CE_defaultval, "4[same=id]");
   answer_list = nullptr;
   check_int("T65", "a default carrying a parameter list is refused",
             centry_elem_validate(ep, nullptr, &answer_list, false) ? 1 : 0, 0);
   lFreeList(&answer_list);
   lFreeElem(&ep);

   lFreeList(&centries);
}

/**
 * Build the resource map of a host: "gpu=8(0 0 0 0 1 1 1 1)" as the reader leaves it, two
 * elements carrying a count rather than eight elements.
 */
static lListElem *
make_resource_definition() {
   lListElem *ep = lCreateElem(CE_Type);
   lSetString(ep, CE_name, "gpu");
   lSetUlong(ep, CE_valtype, TYPE_RSMAP);
   lSetDouble(ep, CE_doubleval, 8);

   lListElem *resl = lAddSubStr(ep, RESL_value, "0", CE_resource_map_list, RESL_Type);
   lSetUlong(resl, RESL_amount, 4);
   resl = lAddSubStr(ep, RESL_value, "1", CE_resource_map_list, RESL_Type);
   lSetUlong(resl, RESL_amount, 4);

   return ep;
}

/**
 * Build a map whose instances carry a characteristic, four identifiers on two NUMA nodes:
 *
 *    gpu=8(0[numa_node=0] 0[..] 1[numa_node=0] 1[..] 2[numa_node=1] 2[..] 3[numa_node=1] 3[..])
 *
 * Two instances per identifier, two identifiers per node, so a node holds four and no single
 * identifier does. That is the shape which tells same=<characteristic> apart from same=id: a
 * request for three can be served by a node and by no identifier.
 */
static lListElem *
make_resource_definition_numa() {
   lListElem *ep = lCreateElem(CE_Type);
   lSetString(ep, CE_name, "gpu");
   lSetUlong(ep, CE_valtype, TYPE_RSMAP);
   lSetDouble(ep, CE_doubleval, 8);

   static const char *const node_of[] = {"0", "0", "1", "1"};
   for (int i = 0; i < 4; i++) {
      char id[2] = {static_cast<char>('0' + i), '\0'};
      lListElem *resl = lAddSubStr(ep, RESL_value, id, CE_resource_map_list, RESL_Type);
      lSetUlong(resl, RESL_amount, 2);

      lListElem *property = lAddSubStr(resl, CE_name, "numa_node", RESL_properties, CE_Type);
      lSetString(property, CE_stringval, node_of[i]);
   }

   return ep;
}

/**
 * Book "used" instances of one id, the way the host's utilization records them.
 */
static lListElem *
make_utilization(const char *id, u_long32 used) {
   lListElem *ep = lCreateElem(RUE_Type);
   lSetString(ep, RUE_name, "gpu");
   if (id != nullptr) {
      lListElem *resl = lAddSubStr(ep, RESL_value, id, RUE_utilized_now_resource_map_list,
                                   RESL_Type);
      lSetUlong(resl, RESL_amount, used);
   }
   return ep;
}

static void
test_best_free_id() {
   lListElem *def = make_resource_definition();
   lListElem *use;
   u_long32 free = 0;

   check_str("T90", "an unused map reports its first id", centry_rsmap_best_free_id(def, nullptr, &free), "0");
   check_int("T90b", "with its full count", (int)free, 4);

   /* one share of card 0 booked, so card 1 is now the emptiest */
   use = make_utilization("0", 1);
   check_str("T91", "the emptiest id is reported", centry_rsmap_best_free_id(def, use, &free), "1");
   check_int("T91b", "with its count", (int)free, 4);
   lFreeElem(&use);

   /* both cards half used: four free in the map, two on the best id */
   use = make_utilization("0", 2);
   lListElem *resl = lAddSubStr(use, RESL_value, "1", RUE_utilized_now_resource_map_list, RESL_Type);
   lSetUlong(resl, RESL_amount, 2);
   check_int("T92", "the count is of one id, not of the map", (int)(centry_rsmap_best_free_id(def, use, &free), free), 2);
   lFreeElem(&use);

   /* a full map reports zero rather than nothing, so a caller can tell "no room" from
      "no such resource map" */
   use = make_utilization("0", 4);
   resl = lAddSubStr(use, RESL_value, "1", RUE_utilized_now_resource_map_list, RESL_Type);
   lSetUlong(resl, RESL_amount, 4);
   check_int("T93", "a full map reports an id", centry_rsmap_best_free_id(def, use, &free) != nullptr, 1);
   check_int("T93b", "with a free count of zero", (int)free, 0);
   lFreeElem(&use);

   check_int("T94", "a missing definition reports nothing",
             centry_rsmap_best_free_id(nullptr, nullptr, &free) == nullptr, 1);
   check_int("T94b", "and a free count of zero", (int)free, 0);

   lFreeElem(&def);
}

/**
 * Grouping by a characteristic rather than by the identifier. The map has four identifiers of
 * two on two NUMA nodes, so a node holds four instances and no identifier holds more than two.
 */
static void
test_best_free_group() {
   lListElem *def = make_resource_definition_numa();
   u_long32 free = 0;

   /* keying on the identifier has to reach exactly what the id function reaches, since that is
    * the case every request in the field takes today */
   const char *key = centry_rsmap_best_free_group(def, nullptr, "id", &free);
   check_str("T95", "keying on id returns an id", key, "0");
   check_int("T95b", "with the free count of that one id", (int)free, 2);
   check_int("T95c", "and a nullptr key means the same thing",
             centry_rsmap_best_free_group(def, nullptr, nullptr, &free) != nullptr, 1);

   /* the point of the generalization: a node has more than any one id it is made of */
   key = centry_rsmap_best_free_group(def, nullptr, "numa_node", &free);
   check_int("T96", "keying on a characteristic finds a group", key != nullptr, 1);
   check_int("T96b", "which holds the instances of both its ids", (int)free, 4);

   /* booking one id shrinks its node but not the other */
   lListElem *use = make_utilization("0", 2);
   key = centry_rsmap_best_free_group(def, use, "numa_node", &free);
   check_str("T97", "the emptier node is returned", key, "1");
   check_int("T97b", "with its full count", (int)free, 4);
   lFreeElem(&use);

   /* an instance which does not carry the characteristic cannot agree with anything, so it is
    * left out rather than forming a group of its own */
   lListElem *def_partial = make_resource_definition_numa();
   lListElem *stray = lAddSubStr(def_partial, RESL_value, "9", CE_resource_map_list, RESL_Type);
   lSetUlong(stray, RESL_amount, 99);
   key = centry_rsmap_best_free_group(def_partial, nullptr, "numa_node", &free);
   check_int("T98", "an instance without the characteristic is left out", (int)free, 4);
   check_int("T98b", "so the group is still a node", key != nullptr && strcmp(key, "9") != 0, 1);

   /* a characteristic no instance carries leaves nothing to group by */
   key = centry_rsmap_best_free_group(def, nullptr, "no_such_characteristic", &free);
   check_int("T99", "a characteristic nothing carries finds no group", key == nullptr, 1);
   check_int("T99b", "and a free count of zero", (int)free, 0);

   lFreeElem(&def_partial);
   lFreeElem(&def);
}

/**
 * Taking the instances, rather than only asking what a group could serve.
 */
static void
test_select_group_instances() {
   lList *selected = nullptr;

   /* keying on the identifier: one entry, because a group has one member there */
   lListElem *def = make_resource_definition();
   check_int("T100", "four taken from the plain map",
             centry_rsmap_select_group_instances(def, nullptr, nullptr, "id", 4, &selected), 1);
   check_int("T100b", "come from a single id", (int)lGetNumberOfElem(selected), 1);
   check_int("T100c", "with the whole amount on it",
             (int)lGetUlong(lFirst(selected), RESL_amount), 4);
   lFreeList(&selected);

   check_int("T101", "more than any id holds is refused",
             centry_rsmap_select_group_instances(def, nullptr, nullptr, "id", 5, &selected), 0);
   check_int("T101b", "and nothing is handed back", selected == nullptr, 1);
   lFreeElem(&def);

   /* keying on a characteristic: a node of two cards of two serves three, no card does */
   lListElem *numa = make_resource_definition_numa();
   check_int("T102", "three taken from one node",
             centry_rsmap_select_group_instances(numa, nullptr, nullptr, "numa_node", 3,
                                                 &selected), 1);
   check_int("T102b", "spread over the two cards of it", (int)lGetNumberOfElem(selected), 2);
   check_int("T102c", "filling the first before touching the second",
             (int)lGetUlong(lFirst(selected), RESL_amount), 2);
   check_int("T102d", "and taking the remainder from the second",
             (int)lGetUlong(lLast(selected), RESL_amount), 1);
   lFreeList(&selected);

   check_int("T103", "the same three are refused by the identifier keying",
             centry_rsmap_select_group_instances(numa, nullptr, nullptr, "id", 3, &selected), 0);
   lFreeList(&selected);

   check_int("T104", "more than a node holds is refused",
             centry_rsmap_select_group_instances(numa, nullptr, nullptr, "numa_node", 5,
                                                 &selected), 0);
   lFreeList(&selected);

   /* a second request scope: the group is read back from what was granted already, and what
    * this job holds is subtracted along with what other jobs hold */
   lList *already = nullptr;
   lListElem *held = lAddElemStr(&already, RESL_value, "0", RESL_Type);
   lSetUlong(held, RESL_amount, 2);

   check_int("T105", "a later scope takes from the group the first one took from",
             centry_rsmap_select_group_instances(numa, nullptr, already, "numa_node", 2,
                                                 &selected), 1);
   check_str("T105b", "from the other card of that node, the first being spent",
             lGetString(lFirst(selected), RESL_value), "1");
   check_int("T105c", "and only what is left of it",
             (int)lGetUlong(lFirst(selected), RESL_amount), 2);
   lFreeList(&selected);

   check_int("T106", "what this job already holds is not free for it again",
             centry_rsmap_select_group_instances(numa, nullptr, already, "numa_node", 3,
                                                 &selected), 0);
   lFreeList(&selected);
   lFreeList(&already);

   /* what other jobs hold counts against the group too */
   lListElem *use = make_utilization("0", 2);
   check_int("T107", "a node booked down to two cannot serve three",
             centry_rsmap_select_group_instances(numa, use, nullptr, "numa_node", 3,
                                                 &selected), 1);
   check_str("T107b", "it moves to the node which can",
             lGetString(lFirst(selected), RESL_value), "2");
   lFreeList(&selected);
   lFreeElem(&use);

   lFreeElem(&numa);
}

static void
test_get_request_param() {
   lList *lp;
   DSTRING_STATIC(value, 256);

   lp = parse("gpu=4[same=id]");
   check_int("T81", "a single parameter is found",
             centry_rsmap_get_request_param(lFirst(lp), "same", &value) ? 1 : 0, 1);
   check_str("T81b", "with its value", sge_dstring_get_string(&value), "id");
   lFreeList(&lp);

   lp = parse("gpu=4[scope=host,same=id,memory=40G]");
   check_int("T82", "a parameter in the middle is found",
             centry_rsmap_get_request_param(lFirst(lp), "same", &value) ? 1 : 0, 1);
   check_str("T82b", "with its value", sge_dstring_get_string(&value), "id");
   check_int("T83", "the last parameter is found",
             centry_rsmap_get_request_param(lFirst(lp), "memory", &value) ? 1 : 0, 1);
   check_str("T83b", "with its value", sge_dstring_get_string(&value), "40G");
   lFreeList(&lp);

   /* a name which is a prefix of another must not match it */
   lp = parse("gpu=4[same_thing=x]");
   check_int("T84", "a longer name is not matched by a shorter one",
             centry_rsmap_get_request_param(lFirst(lp), "same", &value) ? 1 : 0, 0);
   lFreeList(&lp);

   /* a value containing a bracket does not confuse the scan */
   lp = parse("gpu=4[id=gpu[01]*,same=id]");
   check_int("T85", "a parameter after a bracketed value is found",
             centry_rsmap_get_request_param(lFirst(lp), "same", &value) ? 1 : 0, 1);
   check_str("T85b", "with its value", sge_dstring_get_string(&value), "id");
   check_int("T86", "and the bracketed value itself is returned whole",
             centry_rsmap_get_request_param(lFirst(lp), "id", &value) ? 1 : 0, 1);
   check_str("T86b", "including its brackets", sge_dstring_get_string(&value), "gpu[01]*");
   lFreeList(&lp);

   lp = parse("gpu=4");
   check_int("T87", "a request without a parameter list has no parameters",
             centry_rsmap_get_request_param(lFirst(lp), "same", &value) ? 1 : 0, 0);
   lFreeList(&lp);

   lp = parse("gpu=4[same=id]");
   check_int("T88", "a parameter which is not there is not found",
             centry_rsmap_get_request_param(lFirst(lp), "scope", &value) ? 1 : 0, 0);
   lFreeList(&lp);
}

int
main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_sgeobj_request_parse");

   lInit(nmv);

   test_splitting();
   test_amount_split();
   test_parameter_validation();
   test_reserved_names();
   test_parameter_values_and_defaults();
   test_best_free_id();
   test_best_free_group();
   test_select_group_instances();
   test_get_request_param();

   if (failures == 0) {
      printf("\nPASS - 0 failure(s)\n");
   } else {
      printf("\nFAIL - %d failure(s)\n", failures);
   }

   DRETURN(failures == 0 ? 0 : 1);
}
