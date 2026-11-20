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
#include <initializer_list>

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_centry_rsmap.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_RESL.h"
#include "tom/ocs_Element.h"

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
 * @brief Parse a request string
 *
 * Report how many elements it produced, plus the name and value of each, so a test can assert
 * the split without reaching into the list itself.
 *
 * @param request the request string
 * @return the parsed request list, empty if nothing was parsed
 */
static ocs::tom::ListPtr<ocs::tom::CE>
parse(const char *request) {
   return centry_list_parse_from_string(ocs::tom::ListPtr<ocs::tom::CE>(), request, false);
}

/**
 * @brief The string value of a request
 * @param lp the request list
 * @param name the name of the request
 * @return its CE_stringval, or nullptr if there is no such request
 */
static const char *
value_of(const ocs::tom::ConstList<ocs::tom::CE> lp, const char *name) {
   ocs::tom::ConstElement<ocs::tom::CE> ep = lp.find_first<CE_name>(name);
   return ep == nullptr ? nullptr : ep.get<CE_stringval>();
}

/// The empty instance list, for a call that books or holds nothing
static const ocs::tom::ConstList<ocs::tom::RESL> no_instances;

/**
 * @brief A request string is split into requests, a bracket list kept whole
 */
static void
test_splitting() {
   ocs::tom::ListPtr<ocs::tom::CE> lp;

   /* the ordinary cases have to keep working */
   lp = parse("mem=2G");
   check_int("T01", "one request yields one element", (int)lp->size(), 1);
   check_str("T01b", "its value is kept", value_of(lp, "mem"), "2G");
   lp.reset();

   lp = parse("mem=2G,arch=lx-amd64");
   check_int("T02", "a comma separates two requests", (int)lp->size(), 2);
   lp.reset();

   lp = parse("mem=2G arch=lx-amd64");
   check_int("T03", "a space separates two requests", (int)lp->size(), 2);
   lp.reset();

   lp = parse("mem=2G,,arch=lx-amd64");
   check_int("T04", "repeated separators do not produce empty elements",
             (int)lp->size(), 2);
   lp.reset();

   /* the bracket list is one value, whatever it contains */
   lp = parse("gpu=4[same=id]");
   check_int("T05", "a bracket list does not split the request", (int)lp->size(), 1);
   check_str("T05b", "the whole string is kept in CE_stringval",
             value_of(lp, "gpu"), "4[same=id]");
   lp.reset();

   lp = parse("gpu=4[id=gpu1*,same=id],mem=2G");
   check_int("T06", "a comma inside brackets does not split, the one outside does",
             (int)lp->size(), 2);
   check_str("T06b", "the bracket list is intact", value_of(lp, "gpu"), "4[id=gpu1*,same=id]");
   check_str("T06c", "the following request is intact", value_of(lp, "mem"), "2G");
   lp.reset();

   lp = parse("gpu=4[id=a b]");
   check_int("T07", "a space inside brackets does not split", (int)lp->size(), 1);
   check_str("T07b", "including the space", value_of(lp, "gpu"), "4[id=a b]");
   lp.reset();

   /* a nested bracket, as in a character class inside a pattern */
   lp = parse("gpu=1[id=gpu[01]*],mem=2G");
   check_int("T08", "a nested bracket is counted, the list ends at the matching one",
             (int)lp->size(), 2);
   check_str("T08b", "the nested class survives", value_of(lp, "gpu"), "1[id=gpu[01]*]");
   lp.reset();

   /* the same rule makes a character class in a string request work, which it did not before */
   lp = parse("h=node[1,2]");
   check_int("T09", "a character class in a string request is one request",
             (int)lp->size(), 1);
   check_str("T09b", "and keeps its comma", value_of(lp, "h"), "node[1,2]");
   lp.reset();

   /* an unbalanced bracket swallows the rest rather than splitting silently */
   lp = parse("gpu=4[same=id,mem=2G");
   check_int("T10", "an unbalanced bracket yields one element, not two",
             (int)lp->size(), 1);
   lp.reset();

   /* a bare boolean request still becomes TRUE when values are not required */
   lp = parse("some_bool");
   check_str("T11", "a bare request becomes TRUE", value_of(lp, "some_bool"), "TRUE");
   lp.reset();

   /* the last of two requests for one complex wins, as before */
   lp = parse("gpu=1,gpu=2");
   check_int("T12", "a repeated complex yields one element", (int)lp->size(), 1);
   check_str("T12b", "and keeps the last value", value_of(lp, "gpu"), "2");
   lp.reset();
}

/**
 * @brief Build the master complex list the amount split is resolved against
 * @return the complex list
 */
static ocs::tom::ListPtr<ocs::tom::CE>
make_centry_list() {
   auto lp = ocs::tom::ListPtr<ocs::tom::CE>::create("complexes");

   auto ep = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep->set<CE_name>("gpu");
   ep->set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::RSMAP));
   ep->set<CE_relop>(CMPLXLE_OP);
   ep->set<CE_consumable>(CONSUMABLE_HOST);
   ep->set<CE_requestable>(REQU_YES);
   lp->push_back(std::move(ep));

   auto ep_ce = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep_ce->set<CE_name>("gpu_memory");
   ep_ce->set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::MEM));
   ep_ce->set<CE_relop>(CMPLXLE_OP);
   ep_ce->set<CE_consumable>(CONSUMABLE_NO);
   ep_ce->set<CE_requestable>(REQU_YES);
   lp->push_back(std::move(ep_ce));

   // a string characteristic, so that a parameter value may be a pattern
   auto ep_ce_2 = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep_ce_2->set<CE_name>("gpu_model");
   ep_ce_2->set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::STR));
   ep_ce_2->set<CE_relop>(CMPLXEQ_OP);
   ep_ce_2->set<CE_consumable>(CONSUMABLE_NO);
   ep_ce_2->set<CE_requestable>(REQU_YES);
   lp->push_back(std::move(ep_ce_2));

   // may not be requested, so it may not be matched against either
   auto ep_ce_3 = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep_ce_3->set<CE_name>("gpu_serial");
   ep_ce_3->set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::STR));
   ep_ce_3->set<CE_relop>(CMPLXEQ_OP);
   ep_ce_3->set<CE_consumable>(CONSUMABLE_NO);
   ep_ce_3->set<CE_requestable>(REQU_NO);
   lp->push_back(std::move(ep_ce_3));

   auto ep_ce_4 = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep_ce_4->set<CE_name>("mem");
   ep_ce_4->set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::MEM));
   ep_ce_4->set<CE_relop>(CMPLXLE_OP);
   ep_ce_4->set<CE_consumable>(CONSUMABLE_YES);
   ep_ce_4->set<CE_requestable>(REQU_YES);
   lp->push_back(std::move(ep_ce_4));

   return lp;
}

/**
 * @brief The amount in front of a parameter list is what CE_doubleval holds
 */
static void
test_amount_split() {
   auto centries = make_centry_list();
   ocs::tom::ListPtr<ocs::tom::CE> lp;
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;

   /* the amount in front of the bracket is what CE_doubleval holds */
   lp = parse("gpu=4[same=id]");
   answer_list = nullptr;
   check_int("T20", "a request with a parameter list is accepted" PARAM_LIST_NOTE,
             centry_list_fill_request(lp, &answer_list, centries, true, false, true),
             PARAM_LIST_RC);
   ocs::tom::ConstElement<ocs::tom::CE> ep = ocs::tom::find_first<CE_name>(*lp, "gpu");
   check_int("T20b", "CE_doubleval is the amount", (int)ep.get<CE_doubleval>(), 4);
   check_str("T20c", "CE_stringval still carries the parameters",
             ep.get<CE_stringval>(), "4[same=id]");
   answer_list.reset();
   lp.reset();

   /* no bracket, unchanged behaviour */
   lp = parse("gpu=4");
   answer_list = nullptr;
   check_int("T21", "a plain amount is accepted",
             centry_list_fill_request(lp, &answer_list, centries, true, false, true), 0);
   ocs::tom::ConstElement<ocs::tom::CE> ep_ce = ocs::tom::find_first<CE_name>(*lp, "gpu");
   check_int("T21b", "CE_doubleval is the amount", (int)ep_ce.get<CE_doubleval>(), 4);
   answer_list.reset();
   lp.reset();

   /* a bracket with no amount in front of it is not a number */
   lp = parse("gpu=[same=id]");
   answer_list = nullptr;
   check_int("T22", "a parameter list without an amount is rejected",
             centry_list_fill_request(lp, &answer_list, centries, true, false, true), -1);
   answer_list.reset();
   lp.reset();

   /* the split is for resource maps only; another type keeps rejecting a bracket */
   lp = parse("mem=2G[x=y]");
   answer_list = nullptr;
   check_int("T23", "a parameter list on a non resource map is rejected",
             centry_list_fill_request(lp, &answer_list, centries, true, false, true), -1);
   answer_list.reset();
   lp.reset();

   /* The checks that follow the parse apply to a resource map like to any other numeric
      type. An earlier version of the split returned early for a bracketed request and
      skipped them, so a negative amount was accepted with a parameter list and rejected
      without one. allow_neg_consumable is false here, which is what a submit does. */
   lp = parse("gpu=-1");
   answer_list = nullptr;
   check_int("T25", "a negative amount is rejected",
             centry_list_fill_request(lp, &answer_list, centries, true, false, false), -1);
   answer_list.reset();
   lp.reset();

   lp = parse("gpu=-1[same=id]");
   answer_list = nullptr;
   check_int("T26", "a negative amount is rejected with a parameter list too",
             centry_list_fill_request(lp, &answer_list, centries, true, false, false), -1);
   answer_list.reset();
   lp.reset();

   /* a memory amount is still parsed with its multiplier */
   lp = parse("mem=2G");
   answer_list = nullptr;
   check_int("T24", "a memory request is accepted",
             centry_list_fill_request(lp, &answer_list, centries, true, false, true), 0);
   ocs::tom::ConstElement<ocs::tom::CE> ep_ce_2 = ocs::tom::find_first<CE_name>(*lp, "mem");
   check_int("T24b", "the multiplier is applied",
             (int)(ep_ce_2.get<CE_doubleval>() / (1024 * 1024)), 2048);
   answer_list.reset();
   lp.reset();

   centries.reset();
}

/**
 * @brief Fill a request against the master complex list
 *
 * The parameter list is validated by centry_list_fill_request(), so these go through the same
 * entry point as a real submit.
 *
 * @param request the request string
 * @param centries the master complex list
 * @return 0 if accepted, -1 if rejected
 */
static int
fill(const char *request, const ocs::tom::ConstList<ocs::tom::CE> centries) {
   auto lp = parse(request);
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   int ret = centry_list_fill_request(lp, &answer_list, centries, true, false, true);
   answer_list.reset();
   lp.reset();
   return ret;
}

/**
 * @brief The parameter list of a resource map request is validated
 */
static void
test_parameter_validation() {
   auto centries = make_centry_list();

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

   /* same= takes the name of a characteristic as well as "id". Whether an instance carries it
    * is a question for the scheduler, not for the submission. */
   check_int("T49c", "same naming a characteristic is accepted" PARAM_LIST_NOTE,
             fill("gpu=4[same=gpu_memory]", centries), PARAM_LIST_RC);

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

   /* A bracket in the value of anything which is not a resource map is part of the value. A
      string valued resource is matched with a pattern, so a character class is how a request
      writes one, wherever in the value it falls - "-l h=[r]ocky-8-amd64-1" is a host name and
      not a malformed parameter list (CS-2783). */
   check_int("T58", "a character class at the start of a string value is accepted",
             fill("gpu_model=[A]100", centries), 0);
   check_int("T58b", "one at the end of it too",
             fill("gpu_model=A10[01]", centries), 0);
   check_int("T58c", "and one in the middle",
             fill("gpu_model=A[01]00", centries), 0);
   check_int("T58d", "a class holding a comma is accepted, the request not being split there",
             fill("gpu_model=[A,B]100", centries), 0);

   /* id= selects instances by an expression over their identifiers. Only the parse is checked
      at submission: whether anything matches depends on the host the job is scheduled to. */
   check_int("T46", "a plain identifier is accepted",
             fill("gpu=4[id=gpu0]", centries), 0);
   check_int("T46b", "a wildcard is accepted",
             fill("gpu=4[id=gpu1*]", centries), 0);
   check_int("T46c", "an alternation is accepted",
             fill("gpu=4[id=gpu0|gpu1]", centries), 0);
   check_int("T46d", "a negation is accepted",
             fill("gpu=4[id=!gpu0]", centries), 0);
   check_int("T46e", "grouping and negation combined are accepted",
             fill("gpu=4[id=(gpu1*|gpu2*)&!*b]", centries), 0);
   check_int("T46f", "id together with same is accepted",
             fill("gpu=4[id=gpu1*,same=id]", centries), 0);
   check_int("T46g", "an expression with an unbalanced bracket is refused",
             fill("gpu=4[id=(gpu0|gpu1]", centries), -1);
   check_int("T46h", "an empty expression is refused",
             fill("gpu=4[id=]", centries), -1);

   /* scope= says how far a same= constraint reaches. It widens same= and says nothing on its
      own, so a request which carries no same= has nothing for it to apply to. */
   check_int("T47", "scope without same is refused",
             fill("gpu=4[scope=host]", centries), -1);
   check_int("T47b", "scope=host next to same is accepted" PARAM_LIST_NOTE,
             fill("gpu=4[same=id,scope=host]", centries), PARAM_LIST_RC);
   check_int("T47c", "and so is the job wide reading" PARAM_LIST_NOTE,
             fill("gpu=4[same=id,scope=job]", centries), PARAM_LIST_RC);
   check_int("T47d", "a scope which is neither is refused",
             fill("gpu=4[same=id,scope=cluster]", centries), -1);
   check_int("T47e", "an empty scope is refused",
             fill("gpu=4[same=id,scope=]", centries), -1);

   /* reserved, but nothing reads it yet - see centry_rsmap_check_request_params() */
   check_int("T48", "distinct is refused until it is implemented",
             fill("gpu=4[distinct=id]", centries), -1);
   check_int("T49", "a refused reserved parameter is refused next to an accepted one",
             fill("gpu=4[same=id,distinct=id]", centries), -1);

   check_int("T49d", "same with a value which names nothing at all is refused",
             fill("gpu=4[same=banana]", centries), -1);

   /* a parameter naming a complex is matched against the characteristic of that name on an
      instance, so the name has to resolve and the complex has to be one which can be matched */
   check_int("T32", "a characteristic name is accepted",
             fill("gpu=1[gpu_memory=40G]", centries), 0);
   check_int("T36", "a character class in a characteristic value does not derail the walk",
             fill("gpu=1[gpu_model=tesla[AB]]", centries), 0);
   check_int("T32b", "a characteristic composes with the reserved parameters",
             fill("gpu=4[gpu_memory=40G,id=gpu1*,same=id]", centries), 0);
   check_int("T32c", "a consumable complex cannot be a characteristic",
             fill("gpu=1[mem=1G]", centries), -1);
   check_int("T32d", "a complex which is not requestable cannot be matched",
             fill("gpu=1[gpu_serial=X1]", centries), -1);
   check_int("T32e", "a resource map cannot be a characteristic",
             fill("gpu=1[gpu=1]", centries), -1);

   centries.reset();
}

/**
 * @brief Build an ordinary integer complex
 *
 * A complex may not be created or modified under one of the reserved parameter names, but one
 * already in the spool keeps working - refusing it there would stop qmaster from starting.
 *
 * @param name the complex name
 * @param shortcut its shortcut
 * @return the complex
 */
static ocs::tom::ElementPtr<ocs::tom::CE>
make_complex(const char *name, const char *shortcut) {
   auto ep = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep->set<CE_name>(name);
   ep->set<CE_shortcut>(shortcut);
   ep->set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::INT));
   ep->set<CE_relop>(CMPLXLE_OP);
   ep->set<CE_consumable>(CONSUMABLE_NO);
   ep->set<CE_requestable>(REQU_YES);
   return ep;
}

/**
 * @brief A complex may not be named after a reserved parameter
 */
static void
test_reserved_names() {
   ocs::tom::ElementPtr<ocs::tom::CE> ep;
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;

   /* every reserved name is refused, by name and by shortcut */
   const char *reserved[] = {"id", "same", "scope", "distinct", "bind"};
   for (int i = 0; i < 5; i++) {
      char id[8];
      snprintf(id, sizeof(id), "T5%d", i);

      ep = make_complex(reserved[i], "sc");
      answer_list = nullptr;
      check_int(id, "a reserved name is refused when a complex is created",
                centry_elem_validate(*ep, ocs::tom::ConstList<ocs::tom::CE>(), &answer_list, false) ? 1 : 0, 0);
      answer_list.reset();
      ep.reset();
   }

   ep = make_complex("something", "same");
   answer_list = nullptr;
   check_int("T55", "a reserved shortcut is refused too",
             centry_elem_validate(*ep, ocs::tom::ConstList<ocs::tom::CE>(), &answer_list, false) ? 1 : 0, 0);
   answer_list.reset();
   ep.reset();

   /* an ordinary complex is unaffected */
   ep = make_complex("gpu_temp", "gt");
   answer_list = nullptr;
   check_int("T56", "an ordinary complex is accepted",
             centry_elem_validate(*ep, ocs::tom::ConstList<ocs::tom::CE>(), &answer_list, false) ? 1 : 0, 1);
   answer_list.reset();
   ep.reset();

   /* read from the spool it is kept, so that a qmaster which has one still starts */
   ep = make_complex("same", "sc");
   answer_list = nullptr;
   check_int("T57", "a reserved name found in the spool is kept",
             centry_elem_validate(*ep, ocs::tom::ConstList<ocs::tom::CE>(), &answer_list, true) ? 1 : 0, 1);
   check_int("T57b", "and is reported", (int)answer_list->size(), 1);
   answer_list.reset();
   ep.reset();
}

/**
 * @brief The values of bind and distinct, and the default value of a resource map
 */
static void
test_parameter_values_and_defaults() {
   auto centries = make_centry_list();
   ocs::tom::ElementPtr<ocs::tom::CE> ep;
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;

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
   ep->set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::RSMAP));
   ep->set<CE_consumable>(CONSUMABLE_HOST);
   ep->set<CE_requestable>(REQU_YES);
   ep->set<CE_defaultval>("4");
   answer_list = nullptr;
   check_int("T64", "a numeric default is accepted",
             centry_elem_validate(*ep, ocs::tom::ConstList<ocs::tom::CE>(), &answer_list, false) ? 1 : 0, 1);
   answer_list.reset();
   ep.reset();

   ep = make_complex("gpu2", "g2");
   ep->set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::RSMAP));
   ep->set<CE_consumable>(CONSUMABLE_HOST);
   ep->set<CE_requestable>(REQU_YES);
   ep->set<CE_defaultval>("4[same=id]");
   answer_list = nullptr;
   check_int("T65", "a default carrying a parameter list is refused",
             centry_elem_validate(*ep, ocs::tom::ConstList<ocs::tom::CE>(), &answer_list, false) ? 1 : 0, 0);
   answer_list.reset();
   ep.reset();

   centries.reset();
}

/**
 * @brief Build the resource map of a host
 *
 * "gpu=8(0 0 0 0 1 1 1 1)" as the reader leaves it, two elements carrying a count rather than
 * eight elements.
 *
 * @return the resource map
 */
static ocs::tom::ElementPtr<ocs::tom::CE>
make_resource_definition() {
   auto ep = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep->set<CE_name>("gpu");
   ep->set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::RSMAP));
   ep->set<CE_doubleval>(8);

   ocs::tom::Element<ocs::tom::RESL> resl = ep->emplace_back_in<CE_resource_map_list, RESL_value>("0");
   resl.set<RESL_amount>(4);
   ocs::tom::Element<ocs::tom::RESL> resl_resl = ep->emplace_back_in<CE_resource_map_list, RESL_value>("1");
   resl_resl.set<RESL_amount>(4);

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
 *
 * @return the resource map
 */
static ocs::tom::ElementPtr<ocs::tom::CE>
make_resource_definition_numa() {
   auto ep = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep->set<CE_name>("gpu");
   ep->set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::RSMAP));
   ep->set<CE_doubleval>(8);

   static const char *const node_of[] = {"0", "0", "1", "1"};
   for (int i = 0; i < 4; i++) {
      char id[2] = {static_cast<char>('0' + i), '\0'};
      ocs::tom::Element<ocs::tom::RESL> resl = ep->emplace_back_in<CE_resource_map_list, RESL_value>(id);
      resl.set<RESL_amount>(2);

      ocs::tom::Element<ocs::tom::CE> property = resl.emplace_back_in<RESL_properties, CE_name>("numa_node");
      property.set<CE_stringval>(node_of[i]);
   }

   return ep;
}

/**
 * @brief Book "used" instances of one id, the way the host's utilization records them
 * @param id the id, or nullptr for no booking at all
 * @param used the number of instances booked
 * @return the utilization, empty if @p id is nullptr
 */
static ocs::tom::ListPtr<ocs::tom::RESL>
make_utilization(const char *id, const uint32_t used) {
   ocs::tom::ListPtr<ocs::tom::RESL> taken;
   if (id != nullptr) {
      taken = ocs::tom::ListPtr<ocs::tom::RESL>::create("taken");
      ocs::tom::Element<ocs::tom::RESL> resl = taken->emplace_back<RESL_value>(id);
      resl.set<RESL_amount>(used);
   }
   return taken;
}

/**
 * @brief Keep a selection and report whether there was one
 * @param selected receives the selection, empty if there was none
 * @param chosen the selection a select function returned
 * @return 1 if there was a selection, 0 otherwise
 */
static int
took(ocs::tom::ListPtr<ocs::tom::RESL> &selected, ocs::tom::ListPtr<ocs::tom::RESL> &&chosen) {
   selected = std::move(chosen);
   return selected ? 1 : 0;
}

/**
 * @brief centry_rsmap_best_free_id() reports the emptiest identifier
 */
static void
test_best_free_id() {
   auto def = make_resource_definition();
   ocs::tom::ListPtr<ocs::tom::RESL> use;
   uint32_t free = 0;

   check_str("T90", "an unused map reports its first id", centry_rsmap_best_free_id(*def, no_instances, &free), "0");
   check_int("T90b", "with its full count", (int)free, 4);

   /* one share of card 0 booked, so card 1 is now the emptiest */
   use = make_utilization("0", 1);
   check_str("T91", "the emptiest id is reported", centry_rsmap_best_free_id(*def, use, &free), "1");
   check_int("T91b", "with its count", (int)free, 4);
   use.reset();

   /* both cards half used: four free in the map, two on the best id */
   use = make_utilization("0", 2);
   ocs::tom::Element<ocs::tom::RESL> resl = use->emplace_back<RESL_value>("1");
   resl.set<RESL_amount>(2);
   check_int("T92", "the count is of one id, not of the map",
             (int)(centry_rsmap_best_free_id(*def, use, &free), free), 2);
   use.reset();

   /* a full map reports zero rather than nothing, so a caller can tell "no room" from
      "no such resource map" */
   use = make_utilization("0", 4);
   ocs::tom::Element<ocs::tom::RESL> resl_resl = use->emplace_back<RESL_value>("1");
   resl_resl.set<RESL_amount>(4);
   check_int("T93", "a full map reports an id", centry_rsmap_best_free_id(*def, use, &free) != nullptr, 1);
   check_int("T93b", "with a free count of zero", (int)free, 0);
   use.reset();

   check_int("T94", "a missing definition reports nothing",
             centry_rsmap_best_free_id(ocs::tom::ConstElement<ocs::tom::CE>(), no_instances, &free) == nullptr, 1);
   check_int("T94b", "and a free count of zero", (int)free, 0);

   def.reset();
}

/**
 * @brief Grouping by a characteristic rather than by the identifier. The map has four identifiers of
 * two on two NUMA nodes, so a node holds four instances and no identifier holds more than two.
 * @return nothing; CHECK records the results
 */
static void
test_best_free_group() {
   auto def = make_resource_definition_numa();
   uint32_t free = 0;

   /* keying on the identifier has to reach exactly what the id function reaches, since that is
    * the case every request in the field takes today */
   const char *key = centry_rsmap_best_free_group(*def, no_instances, "id", &free);
   check_str("T95", "keying on id returns an id", key, "0");
   check_int("T95b", "with the free count of that one id", (int)free, 2);
   check_int("T95c", "and a nullptr key means the same thing",
             centry_rsmap_best_free_group(*def, no_instances, nullptr, &free) != nullptr, 1);

   /* the point of the generalization: a node has more than any one id it is made of */
   key = centry_rsmap_best_free_group(*def, no_instances, "numa_node", &free);
   check_int("T96", "keying on a characteristic finds a group", key != nullptr, 1);
   check_int("T96b", "which holds the instances of both its ids", (int)free, 4);

   /* booking one id shrinks its node but not the other */
   auto use = make_utilization("0", 2);
   key = centry_rsmap_best_free_group(*def, use, "numa_node", &free);
   check_str("T97", "the emptier node is returned", key, "1");
   check_int("T97b", "with its full count", (int)free, 4);
   use.reset();

   /* an instance which does not carry the characteristic cannot agree with anything, so it is
    * left out rather than forming a group of its own */
   auto def_partial = make_resource_definition_numa();
   ocs::tom::Element<ocs::tom::RESL> stray =
      ocs::tom::emplace_back_in<CE_resource_map_list, RESL_value>(def_partial, "9");
   stray.set<RESL_amount>(99);
   key = centry_rsmap_best_free_group(*def_partial, no_instances, "numa_node", &free);
   check_int("T98", "an instance without the characteristic is left out", (int)free, 4);
   check_int("T98b", "so the group is still a node", key != nullptr && strcmp(key, "9") != 0, 1);

   /* a characteristic no instance carries leaves nothing to group by */
   key = centry_rsmap_best_free_group(*def, no_instances, "no_such_characteristic", &free);
   check_int("T99", "a characteristic nothing carries finds no group", key == nullptr, 1);
   check_int("T99b", "and a free count of zero", (int)free, 0);

   def_partial.reset();
   def.reset();
}

/**
 * @brief Taking the instances, rather than only asking what a group could serve.
 * @return nothing; CHECK records the results
 */
static void
test_select_group_instances() {
   ocs::tom::ListPtr<ocs::tom::RESL> selected;

   /* keying on the identifier: one entry, because a group has one member there */
   auto def = make_resource_definition();
   check_int("T100", "four taken from the plain map",
             took(selected, centry_rsmap_select_group_instances(*def, nullptr, nullptr, "id", 4)), 1);
   check_int("T100b", "come from a single id", (int)selected->size(), 1);
   check_int("T100c", "with the whole amount on it",
             (int)ocs::tom::get<RESL_amount>(selected->front()), 4);
   selected.reset();

   check_int("T101", "more than any id holds is refused",
             took(selected, centry_rsmap_select_group_instances(*def, nullptr, nullptr, "id", 5)), 0);
   check_int("T101b", "and nothing is handed back", selected == nullptr, 1);
   def.reset();

   /* keying on a characteristic: a node of two cards of two serves three, no card does */
   auto numa = make_resource_definition_numa();
   check_int("T102", "three taken from one node",
             took(selected, centry_rsmap_select_group_instances(*numa, nullptr, nullptr, "numa_node", 3)), 1);
   check_int("T102b", "spread over the two cards of it", (int)selected->size(), 2);
   check_int("T102c", "filling the first before touching the second",
             (int)ocs::tom::get<RESL_amount>(selected->front()), 2);
   check_int("T102d", "and taking the remainder from the second",
             (int)ocs::tom::get<RESL_amount>(selected->back()), 1);
   selected.reset();

   check_int("T103", "the same three are refused by the identifier keying",
             took(selected, centry_rsmap_select_group_instances(*numa, nullptr, nullptr, "id", 3)), 0);
   selected.reset();

   check_int("T104", "more than a node holds is refused",
             took(selected, centry_rsmap_select_group_instances(*numa, nullptr, nullptr, "numa_node", 5)), 0);
   selected.reset();

   /* a second request scope: the group is read back from what was granted already, and what
    * this job holds is subtracted along with what other jobs hold */
   auto already = ocs::tom::ListPtr<ocs::tom::RESL>::create("already");
   ocs::tom::Element<ocs::tom::RESL> held = already->emplace_back<RESL_value>("0");
   held.set<RESL_amount>(2);

   check_int("T105", "a later scope takes from the group the first one took from",
             took(selected, centry_rsmap_select_group_instances(*numa, nullptr, already, "numa_node", 2)), 1);
   check_str("T105b", "from the other card of that node, the first being spent",
             ocs::tom::get<RESL_value>(selected->front()), "1");
   check_int("T105c", "and only what is left of it",
             (int)ocs::tom::get<RESL_amount>(selected->front()), 2);
   selected.reset();

   check_int("T106", "what this job already holds is not free for it again",
             took(selected, centry_rsmap_select_group_instances(*numa, nullptr, already, "numa_node", 3)), 0);
   selected.reset();
   already.reset();

   /* what other jobs hold counts against the group too */
   auto use = make_utilization("0", 2);
   check_int("T107", "a node booked down to two cannot serve three",
             took(selected, centry_rsmap_select_group_instances(*numa, use, nullptr, "numa_node", 3)), 1);
   check_str("T107b", "it moves to the node which can",
             ocs::tom::get<RESL_value>(selected->front()), "2");
   selected.reset();
   use.reset();

   /* scope=job hands the group down rather than letting the host pick one: the whole job is
    * bound to it, so a host takes from that group or from none - see rsmap_job_scope_key() */
   check_int("T108", "a required group is taken from although another is no worse",
             took(selected, centry_rsmap_select_group_instances(*numa, nullptr, nullptr, "numa_node", 3, nullptr,
                                                                nullptr, "1")), 1);
   check_str("T108b", "so the cards of that node are used",
             ocs::tom::get<RESL_value>(selected->front()), "2");
   check_str("T108c", "both of them", ocs::tom::get<RESL_value>(selected->back()), "3");
   selected.reset();

   check_int("T109", "more than the required group holds is refused, not moved elsewhere",
             took(selected, centry_rsmap_select_group_instances(*numa, nullptr, nullptr, "numa_node", 5, nullptr,
                                                                nullptr, "1")), 0);
   selected.reset();

   check_int("T109b", "a required group the map does not have is refused",
             took(selected, centry_rsmap_select_group_instances(*numa, nullptr, nullptr, "numa_node", 1, nullptr,
                                                                nullptr, "9")), 0);
   selected.reset();

   /* what an earlier request scope was granted on this host comes first: it has already been
    * handed out and cannot be exchanged for the group the rest of the job uses */
   auto granted = ocs::tom::ListPtr<ocs::tom::RESL>::create("granted");
   ocs::tom::Element<ocs::tom::RESL> granted_ep = granted->emplace_back<RESL_value>("0");
   granted_ep.set<RESL_amount>(2);

   check_int("T109c", "a group already granted here wins over the required one",
             took(selected, centry_rsmap_select_group_instances(*numa, nullptr, granted, "numa_node", 2, nullptr,
                                                                nullptr, "1")), 1);
   check_str("T109d", "so the rest comes from the node that scope used",
             ocs::tom::get<RESL_value>(selected->front()), "1");
   selected.reset();
   granted.reset();

   numa.reset();
}

/**
 * @brief Taking instances without a constraint between them.
 * @return nothing; CHECK records the results
 */
static void
test_select_instances() {
   auto def = make_resource_definition();   /* two ids of four */
   ocs::tom::ListPtr<ocs::tom::RESL> selected;

   check_int("T110", "four are taken from one id",
             took(selected, centry_rsmap_select_instances(*def, nullptr, nullptr, 4)), 1);
   check_int("T110b", "so a single entry is returned", (int)selected->size(), 1);
   selected.reset();

   check_int("T111", "six spread over both ids",
             took(selected, centry_rsmap_select_instances(*def, nullptr, nullptr, 6)), 1);
   check_int("T111b", "two entries", (int)selected->size(), 2);
   check_int("T111c", "the first id filled before the second is touched",
             (int)ocs::tom::get<RESL_amount>(selected->front()), 4);
   check_int("T111d", "and the remainder from the second",
             (int)ocs::tom::get<RESL_amount>(selected->back()), 2);
   selected.reset();

   check_int("T112", "more than the map holds is refused",
             took(selected, centry_rsmap_select_instances(*def, nullptr, nullptr, 9)), 0);
   selected.reset();

   /* What this job already holds is not free for it a second time. A map requested in more
    * than one request scope reaches the selection twice, and the first grant is not in the
    * utilization yet - nothing is debited until the assignment is complete. */
   auto already = ocs::tom::ListPtr<ocs::tom::RESL>::create("already");
   ocs::tom::Element<ocs::tom::RESL> held = already->emplace_back<RESL_value>("0");
   held.set<RESL_amount>(4);

   check_int("T113", "a later scope skips the id the first one used up",
             took(selected, centry_rsmap_select_instances(*def, nullptr, already, 4)), 1);
   check_str("T113b", "and takes from the other", ocs::tom::get<RESL_value>(selected->front()), "1");
   check_int("T113c", "in one entry", (int)selected->size(), 1);
   selected.reset();

   check_int("T114", "and cannot take more than the rest of the map holds",
             took(selected, centry_rsmap_select_instances(*def, nullptr, already, 5)), 0);
   selected.reset();
   already.reset();

   /* what other jobs hold counts too */
   auto use = make_utilization("0", 4);
   check_int("T115", "an id used up by other jobs is skipped",
             took(selected, centry_rsmap_select_instances(*def, use, nullptr, 4)), 1);
   check_str("T115b", "so the free one is taken", ocs::tom::get<RESL_value>(selected->front()), "1");
   selected.reset();
   use.reset();

   def.reset();
}

/**
 * @brief centry_rsmap_get_request_param() and centry_rsmap_request_is_job_scope()
 */
static void
test_get_request_param() {
   ocs::tom::ListPtr<ocs::tom::CE> lp;
   DSTRING_STATIC(value, 256);

   lp = parse("gpu=4[same=id]");
   check_int("T81", "a single parameter is found",
             centry_rsmap_get_request_param(lp->front(), "same", &value) ? 1 : 0, 1);
   check_str("T81b", "with its value", sge_dstring_get_string(&value), "id");
   lp.reset();

   lp = parse("gpu=4[scope=host,same=id,memory=40G]");
   check_int("T82", "a parameter in the middle is found",
             centry_rsmap_get_request_param(lp->front(), "same", &value) ? 1 : 0, 1);
   check_str("T82b", "with its value", sge_dstring_get_string(&value), "id");
   check_int("T83", "the last parameter is found",
             centry_rsmap_get_request_param(lp->front(), "memory", &value) ? 1 : 0, 1);
   check_str("T83b", "with its value", sge_dstring_get_string(&value), "40G");
   lp.reset();

   /* a name which is a prefix of another must not match it */
   lp = parse("gpu=4[same_thing=x]");
   check_int("T84", "a longer name is not matched by a shorter one",
             centry_rsmap_get_request_param(lp->front(), "same", &value) ? 1 : 0, 0);
   lp.reset();

   /* a value containing a bracket does not confuse the scan */
   lp = parse("gpu=4[id=gpu[01]*,same=id]");
   check_int("T85", "a parameter after a bracketed value is found",
             centry_rsmap_get_request_param(lp->front(), "same", &value) ? 1 : 0, 1);
   check_str("T85b", "with its value", sge_dstring_get_string(&value), "id");
   check_int("T86", "and the bracketed value itself is returned whole",
             centry_rsmap_get_request_param(lp->front(), "id", &value) ? 1 : 0, 1);
   check_str("T86b", "including its brackets", sge_dstring_get_string(&value), "gpu[01]*");
   lp.reset();

   lp = parse("gpu=4");
   check_int("T87", "a request without a parameter list has no parameters",
             centry_rsmap_get_request_param(lp->front(), "same", &value) ? 1 : 0, 0);
   lp.reset();

   lp = parse("gpu=4[same=id]");
   check_int("T88", "a parameter which is not there is not found",
             centry_rsmap_get_request_param(lp->front(), "scope", &value) ? 1 : 0, 0);
   check_int("T89", "and a request without scope reads as the per host default",
             centry_rsmap_request_is_job_scope(lp->front()) ? 1 : 0, 0);
   lp.reset();

   lp = parse("gpu=4[same=id,scope=host]");
   check_int("T89b", "as does one which writes that default out",
             centry_rsmap_request_is_job_scope(lp->front()) ? 1 : 0, 0);
   lp.reset();

   lp = parse("gpu=4[same=id,scope=job]");
   check_int("T89c", "the job wide reading is recognised",
             centry_rsmap_request_is_job_scope(lp->front()) ? 1 : 0, 1);
   lp.reset();
}

/**
 * @brief Build a map whose identifiers say which card a share belongs to
 *
 * As in the CS-2732 example: "gpu=4(gpu1a gpu1b gpu2a gpu2b)". One instance each, so a count
 * is a count of identifiers.
 *
 * @return the resource map
 */
static ocs::tom::ElementPtr<ocs::tom::CE>
make_named_map() {
   static const char *ids[] = {"gpu1a", "gpu1b", "gpu2a", "gpu2b"};

   auto ep = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep->set<CE_name>("gpu");
   ep->set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::RSMAP));

   for (const char *id : ids) {
      ocs::tom::Element<ocs::tom::RESL> resl = ep->emplace_back_in<CE_resource_map_list, RESL_value>(id);
      resl.set<RESL_amount>(1);
   }

   return ep;
}

/**
 * @brief id= narrows which instances a request may use. The expression is asked of the identifier of
 * each instance, at every point which looks at one, so that what is counted and what is taken
 * are the same set.
 * @return nothing; CHECK records the results
 */
static void
test_id_expression() {
   auto def = make_named_map();
   ocs::tom::ListPtr<ocs::tom::RESL> selected;
   uint32_t free = 0;

   /* counting */
   check_int("T120", "without an expression the whole map is available",
             (int)centry_rsmap_free(*def, no_instances, nullptr), 4);
   check_int("T121", "a wildcard counts only the instances it accepts",
             (int)centry_rsmap_free(*def, no_instances, "gpu1*"), 2);
   check_int("T122", "an exact identifier counts one",
             (int)centry_rsmap_free(*def, no_instances, "gpu2a"), 1);
   check_int("T123", "an alternation counts both sides",
             (int)centry_rsmap_free(*def, no_instances, "gpu1a|gpu2b"), 2);
   check_int("T124", "a negation counts the rest",
             (int)centry_rsmap_free(*def, no_instances, "!gpu1a"), 3);
   check_int("T125", "grouping and negation combined",
             (int)centry_rsmap_free(*def, no_instances, "(gpu1*|gpu2*)&!*b"), 2);
   check_int("T126", "an expression matching nothing counts nothing",
             (int)centry_rsmap_free(*def, no_instances, "gpu3*"), 0);

   /* what other jobs hold is still subtracted */
   auto use = make_utilization("gpu1a", 1);
   check_int("T127", "an instance in use is not free even when it matches",
             (int)centry_rsmap_free(*def, use, "gpu1*"), 1);
   use.reset();

   /* selecting */
   check_int("T130", "one instance is taken from those the expression accepts",
             took(selected, centry_rsmap_select_instances(*def, nullptr, nullptr, 1, "gpu1*")), 1);
   check_str("T130b", "and it is one of them",
             ocs::tom::get<RESL_value>(selected->front()), "gpu1a");
   selected.reset();

   check_int("T131", "more matches than are asked for is not a problem",
             took(selected, centry_rsmap_select_instances(*def, nullptr, nullptr, 2, "!gpu1a")), 1);
   check_int("T131b", "the amount is taken", (int)selected->size(), 2);
   check_str("T131c", "from the accepted set only",
             ocs::tom::get<RESL_value>(selected->front()), "gpu1b");
   selected.reset();

   check_int("T132", "more than the expression leaves is refused",
             took(selected, centry_rsmap_select_instances(*def, nullptr, nullptr, 3, "gpu1*")), 0);
   selected.reset();

   check_int("T133", "an expression matching nothing is refused",
             took(selected, centry_rsmap_select_instances(*def, nullptr, nullptr, 1, "gpu3*")), 0);
   selected.reset();

   /* the best free identifier is the best of those accepted */
   check_str("T134", "the expression narrows which identifier is reported",
             centry_rsmap_best_free_id(*def, no_instances, &free, "gpu2*"), "gpu2a");
   check_int("T135", "no accepted identifier reports none",
             centry_rsmap_best_free_id(*def, no_instances, &free, "gpu3*") == nullptr, 1);

   def.reset();

   /* id= and same= together: the instances agree in the identifier and match the expression */
   auto shared = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   shared->set<CE_name>("gpu");
   shared->set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::RSMAP));
   for (const char *id : {"gpu1", "gpu2"}) {
      ocs::tom::Element<ocs::tom::RESL> resl = shared->emplace_back_in<CE_resource_map_list, RESL_value>(id);
      resl.set<RESL_amount>(4);          /* four shares of each card */
   }

   check_int("T136", "a whole card is taken from the one the expression names",
             took(selected, centry_rsmap_select_group_instances(*shared, nullptr, nullptr, "id", 4,
                                                                "gpu2*")), 1);
   check_str("T136b", "and it is that card",
             ocs::tom::get<RESL_value>(selected->front()), "gpu2");
   check_int("T136c", "in one entry", (int)selected->size(), 1);
   selected.reset();

   check_int("T137", "a card the expression excludes cannot serve the request",
             took(selected, centry_rsmap_select_group_instances(*shared, nullptr, nullptr, "id", 5,
                                                                "gpu2*")), 0);
   selected.reset();

}

/**
 * @brief Build a complex which can be a characteristic
 *
 * So that the type and operator matrix can be exercised: the comparison uses the type and the
 * relop the complex declares.
 *
 * @param name the complex name
 * @param type its type
 * @param relop its relational operator
 * @return the complex
 */
static ocs::tom::ElementPtr<ocs::tom::CE>
make_characteristic(const char *name, ocs::CEntry::Type type, uint32_t relop) {
   auto ep = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep->set<CE_name>(name);
   ep->set<CE_valtype>(static_cast<uint32_t>(type));
   ep->set<CE_relop>(relop);
   ep->set<CE_consumable>(CONSUMABLE_NO);
   ep->set<CE_requestable>(REQU_YES);
   return ep;
}

/**
 * @brief Give one instance of a map a characteristic with a value
 * @param def the resource map
 * @param id the identifier of the instance
 * @param pname the name of the characteristic
 * @param type its type
 * @param value its value
 */
static void
set_characteristic(const ocs::tom::Element<ocs::tom::CE> def, const char *id, const char *pname,
                   const ocs::CEntry::Type type, const char *value) {
   ocs::tom::Element<ocs::tom::RESL> resl = def.find_first_in_rw<CE_resource_map_list, RESL_value>(id);
   ocs::tom::Element<ocs::tom::CE> prop = resl.emplace_back_in<RESL_properties, CE_name>(pname);
   prop.set<CE_valtype>(static_cast<uint32_t>(type));
   prop.set<CE_stringval>(value);
   ocs::tom::ListPtr<ocs::tom::AN> answers;
   centry_fill_and_check(prop, &answers, false, false);
   answers.reset();
}

/**
 * @brief A map of two instances, each carrying the characteristics the cases below ask about
 * @return the resource map
 */
static ocs::tom::ElementPtr<ocs::tom::CE>
make_map_with_characteristics() {
   auto ep = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep->set<CE_name>("gpu");
   ep->set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::RSMAP));

   for (const char *id : {"gpu0", "gpu1"}) {
      ocs::tom::Element<ocs::tom::RESL> resl = ep->emplace_back_in<CE_resource_map_list, RESL_value>(id);
      resl.set<RESL_amount>(1);
   }

   /* gpu0 is the cool, small, A100; gpu1 the hot, large, H100 */
   set_characteristic(*ep, "gpu0", "gpu_temp", ocs::CEntry::Type::INT, "35");
   set_characteristic(*ep, "gpu1", "gpu_temp", ocs::CEntry::Type::INT, "55");
   set_characteristic(*ep, "gpu0", "gpu_mem", ocs::CEntry::Type::MEM, "40G");
   set_characteristic(*ep, "gpu1", "gpu_mem", ocs::CEntry::Type::MEM, "80G");
   set_characteristic(*ep, "gpu0", "gpu_name", ocs::CEntry::Type::STR, "A100");
   set_characteristic(*ep, "gpu1", "gpu_name", ocs::CEntry::Type::STR, "H100");
   set_characteristic(*ep, "gpu0", "gpu_re", ocs::CEntry::Type::RESTR, "A100");
   set_characteristic(*ep, "gpu1", "gpu_re", ocs::CEntry::Type::RESTR, "H100");
   set_characteristic(*ep, "gpu0", "gpu_ci", ocs::CEntry::Type::CSTR, "A100");
   set_characteristic(*ep, "gpu1", "gpu_ci", ocs::CEntry::Type::CSTR, "H100");

   return ep;
}

/**
 * @brief The type and operator matrix. The comparison is "requested relop configured" with the
 * operator the characteristic's own complex declares, so the same request reads differently
 * depending on how the complex was defined - which is the point of it.
 * @return nothing; CHECK records the results
 */
static void
test_characteristic_matching() {
   auto centries = ocs::tom::ListPtr<ocs::tom::CE>::create("complexes");

   auto gpu = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   gpu->set<CE_name>("gpu");
   gpu->set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::RSMAP));
   gpu->set<CE_consumable>(CONSUMABLE_HOST);
   gpu->set<CE_requestable>(REQU_YES);
   centries->push_back(std::move(gpu));

   /* >= : the request is an upper bound, "no hotter than" */
   centries->push_back(make_characteristic("gpu_temp", ocs::CEntry::Type::INT, CMPLXGE_OP));
   /* <= : the request is a lower bound, "at least" */
   centries->push_back(make_characteristic("gpu_mem", ocs::CEntry::Type::MEM, CMPLXLE_OP));
   /* == on a string: matched exactly */
   centries->push_back(make_characteristic("gpu_name", ocs::CEntry::Type::STR, CMPLXEQ_OP));
   /* defined, but no instance of the map carries it */
   centries->push_back(make_characteristic("gpu_clock", ocs::CEntry::Type::INT, CMPLXLE_OP));
   /* the string types differ in whether a value may be a pattern, and in case */
   centries->push_back(make_characteristic("gpu_re", ocs::CEntry::Type::RESTR, CMPLXEQ_OP));
   centries->push_back(make_characteristic("gpu_ci", ocs::CEntry::Type::CSTR, CMPLXEQ_OP));

   auto def = make_map_with_characteristics();

   struct {
      const char *id;
      const char *request;
      uint32_t    expected;
      const char *what;
   } cases[] = {
      {"T140", "gpu=1[gpu_temp=40]", 1,
       "a >= characteristic reads as an upper bound: only the instance at 35 matches"},
      {"T141", "gpu=1[gpu_temp=60]", 2,
       "both instances are below 60"},
      {"T142", "gpu=1[gpu_temp=30]", 0,
       "neither instance is that cool"},
      {"T143", "gpu=1[gpu_mem=40G]", 2,
       "a <= characteristic reads as a lower bound: both carry at least 40G"},
      {"T144", "gpu=1[gpu_mem=80G]", 1,
       "only the larger one carries 80G"},
      {"T145", "gpu=1[gpu_mem=100G]", 0,
       "neither carries 100G"},
      {"T146", "gpu=1[gpu_name=A100]", 1,
       "a string characteristic matched exactly"},
      {"T147", "gpu=1[gpu_name=V100]", 0,
       "a name no instance carries matches none"},
      {"T148", "gpu=1[gpu_temp=40,gpu_mem=40G]", 1,
       "two characteristics compose, both have to hold"},
      {"T149", "gpu=1[gpu_temp=40,gpu_mem=80G]", 0,
       "the cool one is too small and the large one too hot"},
      {"T150", "gpu=1[gpu_clock=1000]", 0,
       "a characteristic which is defined but which no instance carries matches none"},

      /* whether a value may be a pattern follows from the type, which is what the three
         string types are there to distinguish */
      {"T152", "gpu=1[gpu_re=A*]", 1,
       "a RESTRING characteristic takes a pattern"},
      {"T153", "gpu=1[gpu_re=(A100|H100)]", 2,
       "and a whole expression"},
      {"T154", "gpu=1[gpu_name=A*]", 0,
       "a STRING characteristic does not take a pattern, it is matched exactly"},
      {"T155", "gpu=1[gpu_name=A100]", 1,
       "and matches the value it carries"},
      {"T156", "gpu=1[gpu_ci=a100]", 1,
       "a CSTRING characteristic ignores case"},
      {"T157", "gpu=1[gpu_ci=a*]", 0,
       "but does not take a pattern either"},
      {nullptr, nullptr, 0, nullptr}
   };

   for (int i = 0; cases[i].id != nullptr; i++) {
      auto request = centry_list_parse_from_string(ocs::tom::ListPtr<ocs::tom::CE>(), cases[i].request, false);
      ocs::tom::Element<ocs::tom::CE> req = request->front();
      req.set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::RSMAP));

      auto required = centry_rsmap_resolve_request_properties(req, centries);

      const uint32_t free = centry_rsmap_free(*def, no_instances, nullptr, required);
      check_int(cases[i].id, cases[i].what, (int)free, (int)cases[i].expected);
   }

   /* an instance which does not carry the characteristic at all never matches, even when the
      operator would let anything through */
   auto bare = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   bare->set<CE_name>("gpu");
   bare->set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::RSMAP));
   ocs::tom::Element<ocs::tom::RESL> resl = bare->emplace_back_in<CE_resource_map_list, RESL_value>("gpu9");
   resl.set<RESL_amount>(1);

   auto request = centry_list_parse_from_string(ocs::tom::ListPtr<ocs::tom::CE>(), "gpu=1[gpu_temp=99]", false);
   ocs::tom::Element<ocs::tom::CE> req = request->front();
   req.set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::RSMAP));
   auto required = centry_rsmap_resolve_request_properties(req, centries);
   check_int("T151", "an instance carrying no characteristics matches none of them",
             (int)centry_rsmap_free(*bare, no_instances, nullptr, required), 0);
   required.reset();
   request.reset();
   bare.reset();

   def.reset();
}

int
main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_sgeobj_request_parse");

   ocs::tom::install_model(ocs::tom::gcs_model());

   test_splitting();
   test_amount_split();
   test_parameter_validation();
   test_reserved_names();
   test_parameter_values_and_defaults();
   test_best_free_id();
   test_best_free_group();
   test_select_group_instances();
   test_select_instances();
   test_get_request_param();
   test_id_expression();
   test_characteristic_matching();

   if (failures == 0) {
      printf("\nPASS - 0 failure(s)\n");
   } else {
      printf("\nFAIL - %d failure(s)\n", failures);
   }

   DRETURN(failures == 0 ? 0 : 1);
}
