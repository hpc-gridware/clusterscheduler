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
 * @brief Unit tests for attr in `libs/sgeobj`
 */

#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/sge_attr.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/tom/ocs_tom_ASTR.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "tom/ocs_Element.h"

// ---------------------------------------------------------------------------
// Test infrastructure
// ---------------------------------------------------------------------------

static int s_fail = 0;

// id    — scenario number (T01, T02, …); all assertions within one scenario
//         share the same id so a failing line can be mapped back instantly.
// label — short description of the specific assertion.
// expr  — boolean expression that must be true for the assertion to pass.
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

// ---------------------------------------------------------------------------
// Builder helpers
// ---------------------------------------------------------------------------

/**
 * @brief Create a standalone ASTR element mapping a hostname or hostgroup ref to a string value
 *
 * Use lAppendElem() to add it to an attr list.
 * @param href the host name or host group reference
 * @param value the string value
 * @return the element, owned by the caller
 */
static ocs::tom::ElementPtr<ocs::tom::ASTR> make_astr(const char *href, const char *value) {
   auto ep = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep->set<ASTR_href>(ocs::tom::Host(href));
   ep->set<ASTR_value>(value);
   return ep;
}

/**
 * @brief Create an HGRP element whose host_list contains the given names
 *
 * Names beginning with '@' are treated as nested hostgroup references;
 * plain names are leaf hostnames.
 *
 * @param name the group name
 * @param members the member names
 * @return the new host group, or an empty handle if it could not be created
 */
static ocs::tom::ElementPtr<ocs::tom::HGRP> make_hgroup(const char *name,
                                                        std::initializer_list<const char *> members) {
   ocs::tom::ListPtr<ocs::tom::HR> href_list;
   for (const char *m : members) {
      href_list_add(href_list, nullptr, m);
   }
   return hgroup_create(nullptr, name, std::move(href_list), true);
}

/**
 * @brief ocs_attr_list_find() on a string attribute list, in the shape the scenarios check
 * @param attrs the attribute list, may be an empty handle
 * @param al receives the errors
 * @param hostname the host, may be nullptr
 * @param[out] value receives the value of the entry that was read, if any
 * @param[out] match_hog see ocs_attr_list_find()
 * @param[out] match_grp see ocs_attr_list_find()
 * @param[out] ambiguous see ocs_attr_list_find()
 * @param hgroups the host groups
 * @return whether a value was found
 */
static bool find_str_value(const ocs::tom::ConstList<ocs::tom::ASTR> attrs, ocs::tom::ListPtr<ocs::tom::AN> *al,
                           const char *hostname, const char **value, const char **match_hog, const char **match_grp,
                           bool *ambiguous, const ocs::tom::ConstList<ocs::tom::HGRP> hgroups) {
   const auto lookup = ocs_attr_list_find(attrs, al, hostname, match_hog, match_grp, ambiguous, hgroups);
   if (lookup.entry) {
      *value = lookup.entry.get<ASTR_value>();
   }
   return lookup.found;
}

/**
 * @brief ocs_attr_list_find_href() on a string attribute list, in the shape the scenarios check
 * @param attrs the attribute list, may be an empty handle
 * @param al receives the errors
 * @param hostname the host
 * @param[out] value receives the value of the entry found, if any
 * @param[out] found see ocs_attr_list_find_href()
 * @return whether an entry was found
 */
static bool find_str_value_href(const ocs::tom::ConstList<ocs::tom::ASTR> attrs, ocs::tom::ListPtr<ocs::tom::AN> *al,
                                const char *hostname, const char **value, bool *found) {
   const auto entry = ocs_attr_list_find_href(attrs, al, hostname, found);
   if (entry) {
      *value = entry.get<ASTR_value>();
   }
   return static_cast<bool>(entry);
}

// ---------------------------------------------------------------------------
// ocs_attr_list_find
//
// Looks up the effective string attribute value for a given hostname.
// Resolution proceeds in priority order:
//   1. Direct hostname entry in the attribute list (highest priority).
//   2. Hostgroup entries, with full transitive expansion via the master
//      hgroup list. If the host belongs to exactly one group that has an
//      entry, that group's value is returned.
//   3. HOSTREF_DEFAULT ("@/") catch-all entry (lowest priority).
//
// Ambiguity: if the host belongs to two or more groups that each carry a
// distinct value, is_ambiguous is set and the default value is returned.
// The return value being true in the ambiguous case indicates that a fallback
// value (the default) was found — callers must check is_ambiguous before
// trusting the value.
//
// Null guards: passing a null attr_list or null hostname causes the function
// to return false immediately without touching any output parameters.
//
// Scenarios T01–T10
// ---------------------------------------------------------------------------

/**
 * @brief Scenarios of ocs_attr_list_find() [T01-T10]
 */
static void test_ocs_attr_list_find() {
   printf("\n--- ocs_attr_list_find ---\n");

   // Master hgroup topology shared by all tests in this section:
   //   @grpA     → {host1, host2}
   //   @grpB     → {host1, host3}      host1 is in both grpA and grpB
   //   @grpC     → {host4}
   //   @grpOuter → {@grpInner}         nested: outer contains inner as a group
   //   @grpInner → {host5}
   auto hgroups = ocs::tom::ListPtr<ocs::tom::HGRP>::create("hgroups");
   hgroups->push_back(make_hgroup("@grpA",     {"host1", "host2"}));
   hgroups->push_back(make_hgroup("@grpB",     {"host1", "host3"}));
   hgroups->push_back(make_hgroup("@grpC",     {"host4"}));
   hgroups->push_back(make_hgroup("@grpInner", {"host5"}));
   hgroups->push_back(make_hgroup("@grpOuter", {"@grpInner"}));

   ocs::tom::ListPtr<ocs::tom::AN> al;
   const char *value = nullptr;
   const char *match_hog = nullptr;
   const char *match_grp = nullptr;
   bool ambiguous = false;
   bool ok = false;

   // T01 — direct hostname match
   // When the attr list contains an entry keyed to the exact hostname,
   // that value is returned without consulting any hostgroup.
   {
      auto attrs = ocs::tom::ListPtr<ocs::tom::ASTR>::create("attrs");
      attrs->push_back(make_astr("host1",         "direct"));
      attrs->push_back(make_astr(HOSTREF_DEFAULT, "default"));

      value = nullptr; ambiguous = false;
      ok = find_str_value(*attrs, &al, "host1",
                          &value, &match_hog, &match_grp,
                          &ambiguous, *hgroups);
      CHECK(1, "direct match: returns true",   ok);
      CHECK(1, "direct match: correct value",  ok && strcmp(value, "direct") == 0);
      CHECK(1, "direct match: not ambiguous",  !ambiguous);
      al.reset(); 
   }

   // T02 — default fallback
   // When the host has no direct entry and does not belong to any group that
   // has an entry, the HOSTREF_DEFAULT ("@/") value is used.
   {
      auto attrs = ocs::tom::ListPtr<ocs::tom::ASTR>::create("attrs");
      attrs->push_back(make_astr(HOSTREF_DEFAULT, "default"));

      value = nullptr; ambiguous = false;
      ok = find_str_value(*attrs, &al, "unknownhost",
                          &value, &match_hog, &match_grp,
                          &ambiguous, *hgroups);
      CHECK(2, "default fallback: returns true",    ok);
      CHECK(2, "default fallback: correct value",   ok && strcmp(value, "default") == 0);
      CHECK(2, "default fallback: not ambiguous",   !ambiguous);
      al.reset(); 
   }

   // T03 — hostgroup match (one level)
   // host2 belongs only to @grpA. When the attr list has an entry for @grpA
   // the function expands @grpA, finds host2, and returns the group value.
   // The matching_host_or_group output is set to the matched group name.
   {
      auto attrs = ocs::tom::ListPtr<ocs::tom::ASTR>::create("attrs");
      attrs->push_back(make_astr("@grpA",         "group_value"));
      attrs->push_back(make_astr(HOSTREF_DEFAULT, "default"));

      value = nullptr; match_hog = nullptr; ambiguous = false;
      ok = find_str_value(*attrs, &al, "host2",
                          &value, &match_hog, &match_grp,
                          &ambiguous, *hgroups);
      CHECK(3, "hostgroup match: returns true",             ok);
      CHECK(3, "hostgroup match: correct value",            ok && strcmp(value, "group_value") == 0);
      CHECK(3, "hostgroup match: matching group is @grpA",  match_hog && strcmp(match_hog, "@grpA") == 0);
      CHECK(3, "hostgroup match: not ambiguous",            !ambiguous);
      al.reset(); 
   }

   // T04 — transitive (nested) hostgroup expansion
   // host5 is a leaf member of @grpInner, which is itself a member of @grpOuter.
   // The attr list has an entry for @grpOuter only. The function must expand
   // @grpOuter transitively to discover host5 and return the outer group's value.
   {
      auto attrs = ocs::tom::ListPtr<ocs::tom::ASTR>::create("attrs");
      attrs->push_back(make_astr("@grpOuter",     "outer_value"));
      attrs->push_back(make_astr(HOSTREF_DEFAULT, "default"));

      value = nullptr; match_hog = nullptr; ambiguous = false;
      ok = find_str_value(*attrs, &al, "host5",
                          &value, &match_hog, &match_grp,
                          &ambiguous, *hgroups);
      CHECK(4, "nested group: returns true",                  ok);
      CHECK(4, "nested group: correct value from outer",      ok && strcmp(value, "outer_value") == 0);
      CHECK(4, "nested group: matching group is @grpOuter",   match_hog && strcmp(match_hog, "@grpOuter") == 0);
      CHECK(4, "nested group: not ambiguous",                 !ambiguous);
      al.reset(); 
   }

   // T05 — group-without-attr fallback
   // host4 belongs to @grpC but the attr list has no entry for @grpC.
   // Because no group match is found, the default value is used.
   {
      auto attrs = ocs::tom::ListPtr<ocs::tom::ASTR>::create("attrs");
      attrs->push_back(make_astr("@grpA",         "group_value"));
      attrs->push_back(make_astr(HOSTREF_DEFAULT, "default"));

      value = nullptr; ambiguous = false;
      ok = find_str_value(*attrs, &al, "host4",
                          &value, &match_hog, &match_grp,
                          &ambiguous, *hgroups);
      CHECK(5, "group-without-attr: returns true",    ok);
      CHECK(5, "group-without-attr: correct value",   ok && strcmp(value, "default") == 0);
      CHECK(5, "group-without-attr: not ambiguous",   !ambiguous);
      al.reset(); 
   }

   // T06 — ambiguous with default
   // host1 belongs to both @grpA and @grpB, each with a different value.
   // The function detects ambiguity, sets is_ambiguous, and falls back to
   // HOSTREF_DEFAULT. The return value is true (a fallback was found) but
   // callers must check is_ambiguous before using the value.
   {
      auto attrs = ocs::tom::ListPtr<ocs::tom::ASTR>::create("attrs");
      attrs->push_back(make_astr("@grpA",         "value_a"));
      attrs->push_back(make_astr("@grpB",         "value_b"));
      attrs->push_back(make_astr(HOSTREF_DEFAULT, "default"));

      value = nullptr; ambiguous = false;
      ok = find_str_value(*attrs, &al, "host1",
                          &value, &match_hog, &match_grp,
                          &ambiguous, *hgroups);
      CHECK(6, "ambiguous+default: returns true (default used)",  ok);
      CHECK(6, "ambiguous+default: is_ambiguous flag is set",     ambiguous);
      CHECK(6, "ambiguous+default: value is the default",         ok && strcmp(value, "default") == 0);
      al.reset(); 
   }

   // T07 — ambiguous without default
   // Same ambiguous scenario as T06, but without a HOSTREF_DEFAULT entry.
   // The function cannot fall back to a default, so it returns false and adds
   // an error to the answer list. is_ambiguous remains set.
   {
      auto attrs = ocs::tom::ListPtr<ocs::tom::ASTR>::create("attrs");
      attrs->push_back(make_astr("@grpA", "value_a"));
      attrs->push_back(make_astr("@grpB", "value_b"));

      value = nullptr; ambiguous = false;
      ok = find_str_value(*attrs, &al, "host1",
                          &value, &match_hog, &match_grp,
                          &ambiguous, *hgroups);
      CHECK(7, "ambiguous+no-default: returns false",          !ok);
      CHECK(7, "ambiguous+no-default: is_ambiguous flag set",  ambiguous);
      al.reset(); 
   }

   // T08 — direct hostname entry overrides hostgroup entry
   // host2 belongs to @grpA which has a group-level value, but the attr list
   // also contains a direct entry for host2. The direct entry wins because
   // the function looks for an exact hostname match before expanding groups.
   {
      auto attrs = ocs::tom::ListPtr<ocs::tom::ASTR>::create("attrs");
      attrs->push_back(make_astr("host2",         "direct_value"));
      attrs->push_back(make_astr("@grpA",         "group_value"));
      attrs->push_back(make_astr(HOSTREF_DEFAULT, "default"));

      value = nullptr; ambiguous = false;
      ok = find_str_value(*attrs, &al, "host2",
                          &value, &match_hog, &match_grp,
                          &ambiguous, *hgroups);
      CHECK(8, "direct overrides group: returns true",   ok);
      CHECK(8, "direct overrides group: correct value",  ok && strcmp(value, "direct_value") == 0);
      CHECK(8, "direct overrides group: not ambiguous",  !ambiguous);
      al.reset(); 
   }

   // T09 — null attr list
   // Passing a null attr list is a valid call — the function returns false
   // immediately without touching any output parameter.
   {
      value = nullptr; ambiguous = false;
      ok = find_str_value(nullptr, &al, "host1",
                          &value, &match_hog, &match_grp,
                          &ambiguous, *hgroups);
      CHECK(9, "null attr list: returns false",   !ok);
      al.reset();
   }

   // T10 — null hostname
   // Passing a null hostname is handled symmetrically — the function returns
   // false without touching any output parameter.
   {
      auto attrs = ocs::tom::ListPtr<ocs::tom::ASTR>::create("attrs");
      attrs->push_back(make_astr(HOSTREF_DEFAULT, "default"));

      value = nullptr; ambiguous = false;
      ok = find_str_value(*attrs, &al, nullptr,
                          &value, &match_hog, &match_grp,
                          &ambiguous, *hgroups);
      CHECK(10, "null hostname: returns false",   !ok);
      al.reset(); 
   }

}

// ---------------------------------------------------------------------------
// ocs_attr_list_find_href
//
// Simplified lookup variant that does NOT expand hostgroups. Only two entry
// types are considered:
//   1. A direct entry keyed to the exact hostname.
//   2. The HOSTREF_DEFAULT ("@/") catch-all entry.
//
// The `found` output parameter distinguishes the two cases:
//   true  — the hostname had its own direct entry.
//   false — the default was used (no direct entry exists).
//
// Hostgroup entries in the attr list are silently ignored. This variant is
// used when group-level inheritance is not desired, e.g. when the caller
// wants to know whether a host has been explicitly configured.
//
// Scenarios T11–T14
// ---------------------------------------------------------------------------

/**
 * @brief Scenarios of ocs_attr_list_find_href() [T11-T14]
 */
static void test_ocs_attr_list_find_href() {
   printf("\n--- ocs_attr_list_find_href ---\n");

   ocs::tom::ListPtr<ocs::tom::AN> al;
   const char *value = nullptr;
   bool found = false;
   bool ok = false;

   // T11 — direct hostname match
   // An entry keyed to the exact hostname is returned. The `found` output is
   // set to true to indicate that this was a direct (non-default) match.
   {
      auto attrs = ocs::tom::ListPtr<ocs::tom::ASTR>::create("attrs");
      attrs->push_back(make_astr("host1",         "direct"));
      attrs->push_back(make_astr(HOSTREF_DEFAULT, "default"));

      value = nullptr; found = false;
      ok = find_str_value_href(*attrs, &al, "host1", &value, &found);
      CHECK(11, "href/direct match: returns true",   ok);
      CHECK(11, "href/direct match: correct value",  ok && strcmp(value, "direct") == 0);
      CHECK(11, "href/direct match: found is true",  found);
      al.reset(); 
   }

   // T12 — default fallback (found = false)
   // When no direct entry exists for the hostname, the HOSTREF_DEFAULT value
   // is used. The `found` output is set to false to signal that no explicit
   // configuration for this host was present.
   {
      auto attrs = ocs::tom::ListPtr<ocs::tom::ASTR>::create("attrs");
      attrs->push_back(make_astr(HOSTREF_DEFAULT, "default"));

      value = nullptr; found = true;
      ok = find_str_value_href(*attrs, &al, "unknownhost", &value, &found);
      CHECK(12, "href/default fallback: returns true",    ok);
      CHECK(12, "href/default fallback: correct value",   ok && strcmp(value, "default") == 0);
      CHECK(12, "href/default fallback: found is false",  !found);
      al.reset(); 
   }

   // T13 — hostgroup entries are ignored
   // The href variant skips hostgroup expansion entirely. A host that would
   // be reached via a hostgroup in the full variant is not found here; the
   // default value is used instead and found is set to false.
   {
      auto attrs = ocs::tom::ListPtr<ocs::tom::ASTR>::create("attrs");
      attrs->push_back(make_astr("@grpA",         "group_value"));
      attrs->push_back(make_astr(HOSTREF_DEFAULT, "default"));

      value = nullptr; found = true;
      ok = find_str_value_href(*attrs, &al, "host2", &value, &found);
      CHECK(13, "href/group ignored: returns true (default used)",  ok);
      CHECK(13, "href/group ignored: returns default not group",    ok && strcmp(value, "default") == 0);
      CHECK(13, "href/group ignored: found is false",               !found);
      al.reset(); 
   }

   // T14 — null attr list
   // A null attr list causes an immediate return of false without touching
   // any output parameter.
   {
      value = nullptr; found = false;
      ok = find_str_value_href(nullptr, &al, "host1", &value, &found);
      CHECK(14, "href/null attr list: returns false",  !ok);
      al.reset();
   }
}

// ---------------------------------------------------------------------------

int main(int /*argc*/, char * /*argv*/[]) {
   ocs::tom::install_model(ocs::tom::gcs_model());

   test_ocs_attr_list_find();
   test_ocs_attr_list_find_href();

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
