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
#include "sgeobj/cull/sge_all_listsL.h"

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
   check_int("T20", "a request with a parameter list is accepted",
             centry_list_fill_request(lp, &answer_list, centries, true, false, true), 0);
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

   /* accepted: reserved names, a characteristic name, any order, and no list at all */
   check_int("T30", "a reserved parameter is accepted", fill("gpu=4[same=id]", centries), 0);
   check_int("T31", "several reserved parameters are accepted",
             fill("gpu=4[same=id,scope=host,bind=no]", centries), 0);
   check_int("T32", "a characteristic name is accepted",
             fill("gpu=1[gpu_memory=40G]", centries), 0);
   check_int("T33", "order does not matter",
             fill("gpu=1[gpu_memory=40G,same=id]", centries), 0);
   check_int("T34", "an empty parameter list is accepted", fill("gpu=4[]", centries), 0);
   check_int("T35", "no parameter list at all is accepted", fill("gpu=4", centries), 0);

   /* a value may itself contain a bracket */
   check_int("T36", "a character class in a parameter value is accepted",
             fill("gpu=1[id=gpu[01]*]", centries), 0);

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
             fill("gpu=4[same=id,,scope=host]", centries), -1);
   check_int("T44", "an unclosed parameter list is rejected",
             fill("gpu=4[same=id", centries), -1);
   check_int("T45", "text after the parameter list is rejected",
             fill("gpu=4[same=id]x", centries), -1);

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

int
main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_sgeobj_request_parse");

   lInit(nmv);

   test_splitting();
   test_amount_split();
   test_parameter_validation();
   test_reserved_names();

   if (failures == 0) {
      printf("\nPASS - 0 failure(s)\n");
   } else {
      printf("\nFAIL - %d failure(s)\n", failures);
   }

   DRETURN(failures == 0 ? 0 : 1);
}
