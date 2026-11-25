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
 * @brief Unit tests for the generic converters of `libs/sgeobj`: ocs_parse_util.cc and the XML writer of
 *        ocs_object_xml.cc
 *
 * The converters take their fields as an `int` table or walk the descriptor of
 * an element of any type. The expected results are those of the former
 * implementation on the raw list interface: the same lists, return codes and
 * output bytes.
 *
 * Also: repeated -M options keep every address (CS-3188), and sge_parse_bitfield_str() can be called from
 * several threads at once (CS-3171).
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "basis_types.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/ocs_parse_util.h"
#include "sgeobj/ocs_object_xml.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_mailrec.h"
#include "sgeobj/sge_qinstance_type.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_MR.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_PN.h"
#include "sgeobj/tom/ocs_tom_RDE.h"
#include "sgeobj/tom/ocs_tom_SPA.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_TEST.h"
#include "sgeobj/tom/ocs_tom_XMLA.h"
#include "sgeobj/tom/ocs_tom_XMLE.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Raw.h"
#include "tom/ocs_Select.h"
#include "uti/sge_dstring.h"
#include "uti/sge_time.h"

#include "ocs_module_test.h"

/**
 * @brief Are two strings set and equal?
 * @param a the first string, or nullptr
 * @param b the second string, or nullptr
 * @return true if both are set and strcmp() finds them equal
 */
static bool same(const char *a, const char *b) {
   return a != nullptr && b != nullptr && strcmp(a, b) == 0;
}

/**
 * @brief Parse words with ocs_parse_string_list()
 * @param words the words, without the terminating nullptr
 * @param descr the descriptor of the elements
 * @param rule the interpretation rule, terminated by 0
 * @param[out] lpp receives the list on success
 * @return the result of ocs_parse_string_list()
 */
static int parse_words(const std::initializer_list<const char *> words, const ocs::tom::FieldDescr *descr, int *rule,
                       ocs::tom::ListData **lpp) {
   std::vector<std::string> storage(words.begin(), words.end());
   std::vector<char *> argv;
   for (auto &word : storage) {
      argv.push_back(word.data());
   }
   argv.push_back(nullptr);
   return ocs_parse_string_list(argv.data(), "parsed", descr, rule, lpp);
}

/**
 * @brief Append an ST element
 * @param list the list
 * @param name the name, or nullptr
 * @param id the id
 */
static void add_st(const ocs::tom::List<ocs::tom::ST> list, const char *name, const std::uint32_t id) {
   auto ep = ocs::tom::ElementPtr<ocs::tom::ST>::create();
   ep->set<ST_name>(name);
   ep->set<ST_id>(id);
   list.push_back(std::move(ep));
}

/**
 * @brief Append an SPA element with a switch and its argument
 * @param list the list
 * @param switch_val the switch
 * @param switch_arg the argument
 */
static void add_spa(const ocs::tom::List<ocs::tom::SPA> list, const char *switch_val, const char *switch_arg) {
   auto ep = ocs::tom::ElementPtr<ocs::tom::SPA>::create();
   ep->set<SPA_switch_val>(switch_val);
   ep->set<SPA_switch_arg>(switch_arg);
   list.push_back(std::move(ep));
}

/**
 * @brief Append a PN element
 * @param list the list
 * @param path the path
 * @param host the host, or nullptr
 * @param file_host the file host, or nullptr
 */
static void add_pn(const ocs::tom::List<ocs::tom::PN> list, const char *path, const char *host,
                   const char *file_host) {
   auto ep = ocs::tom::ElementPtr<ocs::tom::PN>::create();
   ep->set<PN_path>(path);
   if (host != nullptr) {
      ep->set<PN_host>(ocs::tom::Host(host));
   }
   if (file_host != nullptr) {
      ep->set<PN_file_host>(ocs::tom::Host(file_host));
   }
   list.push_back(std::move(ep));
}

/**
 * @brief Append a CE element
 * @param list the list
 * @param name the name
 * @param stringval the string value, or nullptr
 * @param valtype the value type
 * @param doubleval the double value
 */
static void add_ce(const ocs::tom::List<ocs::tom::CE> list, const char *name, const char *stringval,
                   const std::uint32_t valtype, const double doubleval) {
   auto ep = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep->set<CE_name>(name);
   if (stringval != nullptr) {
      ep->set<CE_stringval>(stringval);
   }
   ep->set<CE_valtype>(valtype);
   ep->set<CE_doubleval>(doubleval);
   list.push_back(std::move(ep));
}

/**
 * @brief The string field of every element of a list, joined by blanks
 * @param list the list, of any type
 * @param nm the string field
 * @return the values, "-" for an unset one
 */
static std::string join_strings(const ocs::tom::ConstRawList list, const int nm) {
   std::string out;
   for (const auto ep : list) {
      const char *value = ep.get<const char *>(nm);
      out += (out.empty() ? "" : " ") + std::string(value != nullptr ? value : "-");
   }
   return out;
}

// ---------------------------------------------------------------------------

/**
 * @brief ocs_parse_string_list() and ocs_parse_definition_list() [T01-T12]
 * @return nothing; CHECK records the results
 */
static void test_parse_string_list() {
   int spa_rule[] = {SPA_switch_val, SPA_argval_lUlongT, SPA_argval_lDoubleT, SPA_argval_lLongT, SPA_argval_lIntT, 0};

   {
      ocs::tom::ListData *lp = nullptr;
      const int ret = parse_words({"-x", "7", "1.5", "-3", "-4"}, ocs::tom::SPA::descriptor.e, spa_rule, &lp);
      const ocs::tom::ListPtr<ocs::tom::SPA> list(lp);
      const auto ep = list ? list->front() : ocs::tom::Element<ocs::tom::SPA>();
      CHECK(1, "one element of five typed words: string, ulong, double, long and int set",
            ret == 0 && list->size() == 1 && same(ep.get<SPA_switch_val>(), "-x") &&
               ep.get<SPA_argval_lUlongT>() == 7 && ep.get<SPA_argval_lDoubleT>() == 1.5 &&
               ep.get<SPA_argval_lLongT>() == -3 && ep.get<SPA_argval_lIntT>() == -4);
      CHECK(11, "the list carries the given name", list && same(list->name(), "parsed"));
   }
   {
      ocs::tom::ListData *lp = nullptr;
      const int ret = parse_words({"-x", "7", "1.5", "-3", "-4", "-y", "8", "2", "3", "4"}, ocs::tom::SPA::descriptor.e,
                                  spa_rule, &lp);
      const ocs::tom::ListPtr<ocs::tom::SPA> list(lp);
      CHECK(2, "a used up rule starts a new element", ret == 0 && list->size() == 2 &&
                                                          join_strings(list.get(), SPA_switch_val) == "-x -y");
   }
   {
      ocs::tom::ListData *lp = nullptr;
      const int ret = parse_words({"none", "7", "1.5", "-3", "-4"}, ocs::tom::SPA::descriptor.e, spa_rule, &lp);
      const ocs::tom::ListPtr<ocs::tom::SPA> list(lp);
      CHECK(3, "a string word NONE in any case leaves the field unset",
            ret == 0 && join_strings(list.get(), SPA_switch_val) == "-");
   }
   {
      ocs::tom::ListData *lp = nullptr;
      const bool ulong_bad = parse_words({"-x", "x", "1.5", "-3", "-4"}, ocs::tom::SPA::descriptor.e, spa_rule,
                                         &lp) == -8;
      const bool double_bad = parse_words({"-x", "7", "y", "-3", "-4"}, ocs::tom::SPA::descriptor.e, spa_rule,
                                          &lp) == -7;
      const bool long_bad = parse_words({"-x", "7", "1.5", "z", "-4"}, ocs::tom::SPA::descriptor.e, spa_rule,
                                        &lp) == -9;
      const bool int_bad = parse_words({"-x", "7", "1.5", "-3", "w"}, ocs::tom::SPA::descriptor.e, spa_rule,
                                       &lp) == -11;
      CHECK(4, "a word that is no ulong, double, long or int gives -8, -7, -9, -11 and no list",
            ulong_bad && double_bad && long_bad && int_bad && lp == nullptr);
   }
   {
      ocs::tom::ListData *lp = nullptr;
      CHECK(5, "words that do not fill the last element give -13",
            parse_words({"-x", "7"}, ocs::tom::SPA::descriptor.e, spa_rule, &lp) == -13 && lp == nullptr);
   }
   {
      ocs::tom::ListData *lp = nullptr;
      int u64_rule[] = {SPA_argval_lUlong64T, 0};
      int bool_rule[] = {SPA_argval_lBoolT, 0};
      int foreign_rule[] = {ST_name, 0};
      CHECK(6, "a 64 bit unsigned, a bool and a foreign field give -12",
            parse_words({"1"}, ocs::tom::SPA::descriptor.e, u64_rule, &lp) == -12 &&
            parse_words({"1"}, ocs::tom::SPA::descriptor.e, bool_rule, &lp) == -12 &&
               parse_words({"1"}, ocs::tom::SPA::descriptor.e, foreign_rule, &lp) == -12 && lp == nullptr);
   }
   {
      ocs::tom::ListData *lp = nullptr;
      int list_rule[] = {SPA_switch_val, SPA_argval_lListT, 0};
      const int ret = parse_words({"-x", "ignored"}, ocs::tom::SPA::descriptor.e, list_rule, &lp);
      const ocs::tom::ListPtr<ocs::tom::SPA> list(lp);
      CHECK(7, "a list field of the rule is skipped",
            ret == 0 && list->size() == 1 && !list->front().get<SPA_argval_lListT>());
   }
   {
      ocs::tom::ListData *lp = nullptr;
      int pn_rule[] = {PN_path, PN_host, 0};
      const int ret = parse_words({"/p", "h1", "/q", "NONE"}, ocs::tom::PN::descriptor.e, pn_rule, &lp);
      const ocs::tom::ListPtr<ocs::tom::PN> list(lp);
      auto it = list->begin();
      const ocs::tom::Element<ocs::tom::PN> first = it != list->end() ? *it++ : ocs::tom::Element<ocs::tom::PN>();
      const ocs::tom::Element<ocs::tom::PN> second = it != list->end() ? *it : ocs::tom::Element<ocs::tom::PN>();
      CHECK(8, "a host word is set, a host word NONE leaves the field unset",
            ret == 0 && same(first.get<PN_host>().c_str(), "h1") && second.get<PN_host>().c_str() == nullptr);
   }
   {
      ocs::tom::ListData *lp = nullptr;
      int empty_rule[] = {0};
      CHECK(9, "a nullptr descriptor gives -1, an empty rule -2",
            parse_words({"a"}, nullptr, spa_rule, &lp) == -1 &&
            parse_words({"a"}, ocs::tom::SPA::descriptor.e, empty_rule, &lp) == -2);
   }
   {
      ocs::tom::ListData *lp = nullptr;
      CHECK(10, "no words give -13, the first element is never filled",
            parse_words({}, ocs::tom::SPA::descriptor.e, spa_rule, &lp) == -13 && lp == nullptr);
   }
   {
      int ce_rule[] = {CE_name, CE_stringval, 0};
      char text[] = "a=1,b=2";
      char none[] = "NONE";
      ocs::tom::ListData *lp = nullptr;
      const int ret = ocs_parse_definition_list(text, &lp, "defs", ocs::tom::CE::descriptor.e, ce_rule);
      const ocs::tom::ListPtr<ocs::tom::CE> list(lp);
      ocs::tom::ListData *none_lp = reinterpret_cast<ocs::tom::ListData *>(1);
      const int none_ret = ocs_parse_definition_list(none, &none_lp, "defs", ocs::tom::CE::descriptor.e, ce_rule);
      CHECK(12, "a definition list gives name and value pairs, NONE gives no list",
            ret == 0 && join_strings(list.get(), CE_name) == "a b" &&
               join_strings(list.get(), CE_stringval) == "1 2" && none_ret == 0 && none_lp == nullptr);
   }
}

/**
 * @brief ocs_compress_definition_list() [T13-T17, T33]
 * @return nothing; CHECK records the results
 */
static void test_compress() {
   {
      auto list = ocs::tom::ListPtr<ocs::tom::ST>::create("st");
      add_st(*list, "a", 1);
      add_st(*list, "b", 2);
      add_st(*list, "a", 3);
      const int ret = ocs_compress_definition_list(*list, ST_name, ST_id, 0);
      CHECK(13, "a string key held twice keeps the later element",
            ret == 0 && join_strings(*list, ST_name) == "b a" && list->front().get<ST_id>() == 2);
   }
   {
      auto list = ocs::tom::ListPtr<ocs::tom::ST>::create("st");
      add_st(*list, "x", 5);
      add_st(*list, "y", 6);
      add_st(*list, "z", 5);
      const int ret = ocs_compress_definition_list(list->raw(), ST_id, ST_name, 0);
      CHECK(14, "an unsigned key held twice keeps the later element",
            ret == 0 && join_strings(*list, ST_name) == "y z");
   }
   {
      auto list = ocs::tom::ListPtr<ocs::tom::SPA>::create("spa");
      add_spa(*list, "a", "1");
      add_spa(*list, "a", "2");
      add_spa(*list, "a", "1");
      const int ret = ocs_compress_definition_list(*list, SPA_switch_val, SPA_switch_arg, 1);
      CHECK(15, "double keys remove only an element with the same name and value",
            ret == 0 && join_strings(*list, SPA_switch_arg) == "2 1");
   }
   {
      auto list = ocs::tom::ListPtr<ocs::tom::PN>::create("pn");
      add_pn(*list, "p0", nullptr, nullptr);
      add_pn(*list, "p1", "h1", "f1");
      add_pn(*list, "p2", "h2", "f2");
      add_pn(*list, "p3", "h1", "f1");
      add_pn(*list, "p4", nullptr, nullptr);
      const int ret = ocs_compress_definition_list(*list, PN_host, PN_file_host, 0);
      auto double_list = ocs::tom::ListPtr<ocs::tom::PN>::create("pn");
      add_pn(*double_list, "q1", "h1", "f1");
      add_pn(*double_list, "q2", "h1", "f2");
      add_pn(*double_list, "q3", "h1", "f1");
      const int double_ret = ocs_compress_definition_list(*double_list, PN_host, PN_file_host, 1);
      CHECK(16, "host keys compare as strings, an unset host is no key; double keys compare both hosts",
            ret == 0 && join_strings(*list, PN_path) == "p0 p2 p3 p4" && double_ret == 0 &&
               join_strings(*double_list, PN_path) == "q2 q3");
   }
   {
      auto list = ocs::tom::ListPtr<ocs::tom::SPA>::create("spa");
      add_spa(*list, "a", "1");
      auto single = ocs::tom::ListPtr<ocs::tom::SPA>::create("spa");
      add_spa(*single, "a", "1");
      add_spa(*list, "b", "2");
      CHECK(17, "a list key gives -4, a 64 bit unsigned or foreign key -5, both only from two elements on",
            ocs_compress_definition_list(*list, SPA_argval_lListT, SPA_switch_arg, 0) == -4 &&
               ocs_compress_definition_list(*list, SPA_argval_lUlong64T, SPA_switch_arg, 0) == -5 &&
               ocs_compress_definition_list(*list, ST_name, SPA_switch_arg, 0) == -5 &&
               ocs_compress_definition_list(*single, ST_name, SPA_switch_arg, 0) == 0 &&
               ocs_compress_definition_list(nullptr, ST_name, ST_id, 0) == 0 && list->size() == 2);
   }
   {
      // the marker in SPA_argval_lStringT tells the elements apart, it is no part of the key
      const auto add = [](const ocs::tom::List<ocs::tom::SPA> list, const char *name, const char *value,
                          const char *marker) {
         auto ep = ocs::tom::ElementPtr<ocs::tom::SPA>::create();
         ep->set<SPA_switch_val>(name);
         ep->set<SPA_switch_arg>(value);
         ep->set<SPA_argval_lStringT>(marker);
         list.push_back(std::move(ep));
      };
      // double keys compare name and value with sge_strnullcmp(), which takes two nullptr as equal: an unset
      // value, and even an unset name and value, is a key like any other (p1 goes for p2, p4 for p5)
      auto double_list = ocs::tom::ListPtr<ocs::tom::SPA>::create("spa");
      add(*double_list, "a", nullptr, "p1");
      add(*double_list, "a", nullptr, "p2");
      add(*double_list, "a", "1", "p3");
      add(*double_list, nullptr, nullptr, "p4");
      add(*double_list, nullptr, nullptr, "p5");
      const int double_ret = ocs_compress_definition_list(*double_list, SPA_switch_val, SPA_switch_arg, 1);
      // a single key needs a name in the later element: two unset names stay, a repeated set name does not
      auto single_list = ocs::tom::ListPtr<ocs::tom::SPA>::create("spa");
      add(*single_list, nullptr, "x", "q1");
      add(*single_list, nullptr, "y", "q2");
      add(*single_list, "a", "z", "q3");
      add(*single_list, "a", "w", "q4");
      const int single_ret = ocs_compress_definition_list(*single_list, SPA_switch_val, SPA_switch_arg, 0);
      CHECK(33, "string keys with unset values: double keys take two nullptr as equal, a single key needs a name",
            double_ret == 0 && join_strings(*double_list, SPA_argval_lStringT) == "p2 p3 p5" && single_ret == 0 &&
               join_strings(*single_list, SPA_argval_lStringT) == "q1 q2 q4");
   }
}

/**
 * @brief uni_print_list() into a stream and into a buffer [T18-T27]
 * @return nothing; CHECK records the results
 */
static void test_uni_print_list() {
   auto list = ocs::tom::ListPtr<ocs::tom::CE>::create("ce");
   add_ce(*list, "a", "1", 0, 1.5);
   add_ce(*list, "b", nullptr, 3, 0.0);
   int fields[] = {CE_name, CE_stringval, 0};
   const char *delis[] = {"=", ",", "\n"};

   {
      std::ostringstream os;
      const int ret = uni_print_list(os, *list, fields, delis, 0);
      CHECK(18, "a typed list: name=value pairs, an unset string as NONE", ret == 0 && os.str() == "a=1,b=NONE\n");
   }
   {
      std::ostringstream os;
      const int ret = uni_print_list(os, list->raw(), fields, delis, FLG_NO_VALUE_AS_EMPTY);
      CHECK(19, "a raw list: an unset string as nothing with FLG_NO_VALUE_AS_EMPTY",
            ret == 0 && os.str() == "a=1,b=\n");
   }
   {
      std::ostringstream os;
      const int ret = uni_print_list(os, nullptr, fields, delis, 0);
      CHECK(20, "no list prints NONE and the final delimiter", ret == 0 && os.str() == "NONE\n");
   }
   {
      auto empty_name = ocs::tom::ListPtr<ocs::tom::CE>::create("ce");
      add_ce(*empty_name, "", "1", 0, 0.0);
      std::ostringstream os;
      const int ret = uni_print_list(os, *empty_name, fields, delis, FLG_NO_DELIS_STRINGS);
      CHECK(21, "FLG_NO_DELIS_STRINGS leaves out the delimiter after an empty string", ret == 0 && os.str() == "1\n");
   }
   {
      int num_fields[] = {CE_name, CE_valtype, 0};
      std::ostringstream os;
      const int ret = uni_print_list(os, *list, num_fields, delis, FLG_NO_DELIS_NUMBERS);
      CHECK(22, "FLG_NO_DELIS_NUMBERS leaves out the delimiter before a zero", ret == 0 && os.str() == "a0,b=3\n");
   }
   {
      int double_fields[] = {CE_name, CE_doubleval, 0};
      std::ostringstream os;
      const int ret = uni_print_list(os, *list, double_fields, delis, 0);
      CHECK(23, "a double is printed with up to ten significant digits", ret == 0 && os.str() == "a=1.5,b=0\n");
   }
   {
      int foreign_fields[] = {CE_name, ST_name, 0};
      int empty_rule[] = {0};
      std::ostringstream os;
      CHECK(24, "a foreign field gives -19, an empty rule -2, no rule -1",
            uni_print_list(os, *list, foreign_fields, delis, 0) == -19 &&
               uni_print_list(os, *list, empty_rule, delis, 0) == -2 &&
               uni_print_list(os, *list, nullptr, delis, 0) == -1);
   }
   {
      char buffer[64];
      char small[4];
      const int ret = uni_print_list(nullptr, buffer, sizeof(buffer), *list, fields, delis, 0);
      CHECK(25, "the buffer form gives the stream's text, -1 for a buffer too small",
            ret == 0 && strcmp(buffer, "a=1,b=NONE\n") == 0 &&
               uni_print_list(nullptr, small, sizeof(small), *list, fields, delis, 0) == -1);
   }
   {
      auto pn = ocs::tom::ListPtr<ocs::tom::PN>::create("pn");
      add_pn(*pn, "/p", "h1", nullptr);
      add_pn(*pn, "/q", nullptr, nullptr);
      int pn_fields[] = {PN_path, PN_host, 0};
      std::ostringstream os;
      const int ret = uni_print_list(os, *pn, pn_fields, delis, 0);
      CHECK(26, "a host is printed, an unset host as NONE", ret == 0 && os.str() == "/p=h1,/q=NONE\n");
   }
   {
      FILE *fp = tmpfile();
      const int ret = fp != nullptr ? uni_print_list(fp, nullptr, 0, *list, fields, delis, 0) : -100;
      std::string text;
      if (fp != nullptr) {
         rewind(fp);
         char line[64];
         while (fgets(line, sizeof(line), fp) != nullptr) {
            text += line;
         }
         fclose(fp);
      }
      CHECK(27, "the file form writes the stream's text", ret == 0 && text == "a=1,b=NONE\n");
   }
}

/**
 * @brief lWriteElemXMLTo() on an element that is no XML element: the generic field walk [T28-T31]
 * @return nothing; CHECK records the results
 */
static void test_xml_field_walk() {
   auto spa = ocs::tom::ElementPtr<ocs::tom::SPA>::create();
   spa->set<SPA_number>(1U);
   spa->set<SPA_argtype>(2U);
   spa->set<SPA_switch_val>("-l");
   spa->set<SPA_argval_lDoubleT>(1.5);
   spa->set<SPA_argval_lUlongT>(7U);
   spa->set<SPA_argval_lUlong64T>(std::uint64_t{9});
   spa->set<SPA_argval_lLongT>(std::int64_t{-3});
   spa->set<SPA_argval_lBoolT>(true);
   spa->set<SPA_argval_lIntT>(std::int32_t{-4});
   spa->set<SPA_argval_lStringT>("a<b");
   auto st_list = ocs::tom::ListPtr<ocs::tom::ST>::create("st_list");
   add_st(*st_list, "x", 5);
   spa->set<SPA_argval_lListT>(std::move(st_list));

   const std::string spa_expected = "<SPA_number>1</SPA_number>\n"
                                    "<SPA_argtype>2</SPA_argtype>\n"
                                    "<SPA_switch_val>-l</SPA_switch_val>\n"
                                    "<SPA_occurrence>0</SPA_occurrence>\n"
                                    "<SPA_argval_lDoubleT>1.5</SPA_argval_lDoubleT>\n"
                                    "<SPA_argval_lUlongT>7</SPA_argval_lUlongT>\n"
                                    "<SPA_argval_lUlong64T>9</SPA_argval_lUlong64T>\n"
                                    "<SPA_argval_lLongT>-3</SPA_argval_lLongT>\n"
                                    "<SPA_argval_lBoolT>true</SPA_argval_lBoolT>\n"
                                    "<SPA_argval_lIntT>-4</SPA_argval_lIntT>\n"
                                    "<SPA_argval_lStringT>a&lt;b</SPA_argval_lStringT>\n"
                                    "<SPA_argval_lListT>\n"
                                    "  <st_list>\n"
                                    "    <ST_name>x</ST_name>\n"
                                    "    <ST_id>5</ST_id>\n"
                                    "  </st_list>\n"
                                    "</SPA_argval_lListT>\n";
   {
      std::ostringstream os;
      lWriteElemXMLTo(*spa, os);
      CHECK(28, "every field type in descriptor order, an unset string left out, a sub-list wrapped in its name",
            os.str() == spa_expected);
   }
   {
      auto pn = ocs::tom::ListPtr<ocs::tom::PN>::create("pn");
      add_pn(*pn, "/p", "h1", nullptr);
      std::ostringstream os;
      lWriteElemXMLTo(pn->front(), os);
      CHECK(29, "a host is escaped like a string, an unset host is written as an empty tag",
            os.str() == "<PN_path>/p</PN_path>\n"
                        "<PN_host>h1</PN_host>\n"
                        "<PN_file_host></PN_file_host>\n"
                        "<PN_file_staging>false</PN_file_staging>\n");
   }
   {
      FILE *fp = tmpfile();
      std::string text;
      if (fp != nullptr) {
         lWriteElemXMLTo(*spa, fp);
         rewind(fp);
         char line[128];
         while (fgets(line, sizeof(line), fp) != nullptr) {
            text += line;
         }
         fclose(fp);
      }
      CHECK(30, "the file form writes the stream's text", text == spa_expected);
   }
   {
      std::ostringstream os;
      lWriteElemXMLTo(nullptr, os);
      CHECK(31, "no element writes nothing", os.str().empty());
   }
}

/**
 * @brief Fill the plain fields of a TEST element that the XML tests set
 * @param ep the element
 * @param uint32 the value of TEST_uint32
 * @param uint64 the value of TEST_uint64
 */
static void set_test_numbers(const ocs::tom::Element<ocs::tom::TEST> ep, const std::uint32_t uint32,
                             const std::uint64_t uint64) {
   ep.set<TEST_uint32>(uint32);
   ep.set<TEST_uint64>(uint64);
}

/**
 * @brief lWriteElemXMLTo() on time stamps, object fields and a reduced element [T34-T37]
 * @return nothing; CHECK records the results
 */
static void test_xml_values() {
   {
      // a Uint64 field whose name contains "time" is written as a time stamp: as whole seconds (value / 1000000)
      // if SGE_QSTAT_SGE_COMPATIBILITY is set, else as sge_ctime64_xml() formats it. The writer reads the variable
      // once per process, at the first such field; the test does not change the environment, so getenv() here
      // decides the same way. Either text differs from the plain number, which a field without "time" gets.
      const std::uint64_t stamp = 1700000000123456;
      std::string stamp_text;
      if (getenv("SGE_QSTAT_SGE_COMPATIBILITY") != nullptr) {
         stamp_text = std::to_string(stamp / 1000000);
      } else {
         dstring text = DSTRING_INIT;
         stamp_text = sge_ctime64_xml(stamp, &text);
         sge_dstring_free(&text);
      }
      auto rde = ocs::tom::ElementPtr<ocs::tom::RDE>::create();
      rde->set<RDE_time>(stamp);
      rde->set<RDE_amount>(2.5);
      std::ostringstream os;
      lWriteElemXMLTo(*rde, os);
      CHECK(34, "a Uint64 field named *time* is written as a time stamp, not as the number",
            os.str() == "<RDE_time>" + stamp_text + "</RDE_time>\n<RDE_amount>2.5</RDE_amount>\n" &&
               stamp_text != std::to_string(stamp));
   }

   // an object field is written even when unset, as an empty tag (only lists, strings and references are
   // left out); a set object follows on a new line at the same indentation, nested objects alike. A time
   // stamp 0 is "0" in both forms of the time branch.
   auto rde = ocs::tom::ElementPtr<ocs::tom::RDE>::create();
   rde->set<RDE_amount>(0.5);
   auto inner = ocs::tom::ElementPtr<ocs::tom::TEST>::create();
   set_test_numbers(*inner, 2, 0);
   inner->set<TEST_obj_any>(std::move(rde));
   auto outer = ocs::tom::ElementPtr<ocs::tom::TEST>::create();
   set_test_numbers(*outer, 1, 9);
   outer->set<TEST_obj_any>(std::move(inner));

   const std::string inner_text = "<TEST_bool>false</TEST_bool>\n"
                                  "<TEST_uint32>2</TEST_uint32>\n"
                                  "<TEST_uint64>0</TEST_uint64>\n"
                                  "<TEST_int>0</TEST_int>\n"
                                  "<TEST_long>0</TEST_long>\n"
                                  "<TEST_double>0</TEST_double>\n"
                                  "<TEST_host></TEST_host>\n"
                                  "<TEST_obj_jb></TEST_obj_jb>\n"
                                  "<TEST_obj_any>\n"
                                  "<RDE_time>0</RDE_time>\n"
                                  "<RDE_amount>0.5</RDE_amount>\n"
                                  "</TEST_obj_any>\n";
   {
      std::ostringstream os;
      lWriteElemXMLTo((*outer).get<TEST_obj_any>(), os);
      CHECK(35, "an unset object is an empty tag, a set one its fields; an unset host is an empty tag, a reference "
                "and an unset string are left out",
            os.str() == inner_text);
   }
   {
      std::ostringstream os;
      lWriteElemXMLTo(*outer, os);
      CHECK(36, "a nested object is written inside the object field holding it",
            os.str() == "<TEST_bool>false</TEST_bool>\n"
                        "<TEST_uint32>1</TEST_uint32>\n"
                        "<TEST_uint64>9</TEST_uint64>\n"
                        "<TEST_int>0</TEST_int>\n"
                        "<TEST_long>0</TEST_long>\n"
                        "<TEST_double>0</TEST_double>\n"
                        "<TEST_host></TEST_host>\n"
                        "<TEST_obj_jb></TEST_obj_jb>\n"
                        "<TEST_obj_any>\n" +
                           inner_text + "</TEST_obj_any>\n");
   }
   {
      // the walk goes over the fields the reduced descriptor holds; the absent ones are not written at all
      auto plain = ocs::tom::ElementPtr<ocs::tom::TEST>::create();
      set_test_numbers(*plain, 3, 4);
      plain->set<TEST_string>("s<");
      plain->set<TEST_double>(1.5);
      const auto reduced = (*plain).select(ocs::tom::what<TEST_uint32, TEST_string>());
      std::ostringstream os;
      lWriteElemXMLTo(*reduced, os);
      CHECK(37, "a reduced element writes only the fields it holds",
            os.str() == "<TEST_uint32>3</TEST_uint32>\n<TEST_string>s&lt;</TEST_string>\n");
   }
}

/**
 * @brief lWriteElemXMLTo() on an XML document: head, attributes and printed elements [T32]
 * @return nothing; CHECK records the results
 */
static void test_xml_document() {
   auto elements = ocs::tom::ListPtr<ocs::tom::XMLE>::create("elements");
   xml_append_Attr_S(*elements, "name", "a&b");
   xml_append_Attr_I(*elements, "n", 5);
   auto head = xml_getHead("root", std::move(elements), ocs::tom::ListPtr<ocs::tom::XMLA>());

   std::ostringstream os;
   lWriteElemXMLTo(*head, os);
   CHECK(32, "the version, the root tag with the schema attribute, the escaped elements indented",
         os.str() == "<?xml version='1.0'?>\n"
                     "<root  xmlns:xsd=\"https://github.com/hpc-gridware/clusterscheduler/raw/master/source/dist/"
                     "util/resources/schemas/qstat/qstat.xsd\">\n"
                     "  <name>a&amp;b</name>\n"
                     "  <n>5</n>\n"
                     "</root>\n");
}

/**
 * @brief Append a -M switch with its parsed recipients to a command line
 * @param cmdline the parsed option list
 * @param recipients the argument of -M, e.g. `alice@example.com`
 */
static void add_mail_option(const ocs::tom::List<ocs::tom::SPA> cmdline, const char *recipients) {
   auto [ret, mail_list] = mailrec_parse(recipients);
   auto ep = ocs::tom::ElementPtr<ocs::tom::SPA>::create();
   ep->set<SPA_switch_val>("-M");
   ep->set<SPA_switch_arg>(recipients);
   ep->set<SPA_argval_lListT>(std::move(mail_list));
   cmdline.push_back(std::move(ep));
}

/**
 * @brief The mail list of a job as qstat -j shows it
 * @param job the job
 * @return the recipients, comma separated
 */
static std::string mail_list_text(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   char text[1024];
   mailrec_unparse(job.get<JB_mail_list>(), text, sizeof(text));
   return text;
}

/**
 * @brief Repeated -M options keep every address, also of the same mail domain (CS-3188) [T38-T41, T43]
 * @return nothing; CHECK records the results
 */
static void test_mail_list_merge() {
   {
      auto cmdline = ocs::tom::ListPtr<ocs::tom::SPA>::create("cmdline");
      add_mail_option(*cmdline, "alice@example.com");
      add_mail_option(*cmdline, "bob@example.com");
      auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
      ocs_parse_mail_list_merge<JB_mail_list>(*cmdline, "-M", *job);
      CHECK(38, "two recipients of the same domain are both kept, in their order",
            mail_list_text(*job) == "alice@example.com,bob@example.com" && cmdline->empty());
   }
   {
      auto cmdline = ocs::tom::ListPtr<ocs::tom::SPA>::create("cmdline");
      add_mail_option(*cmdline, "alice@example.com,carol");
      add_mail_option(*cmdline, "alice@example.com");
      add_mail_option(*cmdline, "carol");
      auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
      ocs_parse_mail_list_merge<JB_mail_list>(*cmdline, "-M", *job);
      CHECK(39, "an address given again is kept once, a user without a domain as well",
            mail_list_text(*job) == "alice@example.com,carol");
   }
   {
      auto cmdline = ocs::tom::ListPtr<ocs::tom::SPA>::create("cmdline");
      add_mail_option(*cmdline, "bob@example.com");
      auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
      job->set<JB_mail_list>(mailrec_parse("alice@example.com").second);
      ocs_parse_mail_list_merge<JB_mail_list>(*cmdline, "-M", *job);
      CHECK(40, "a recipient of the domain of one the job holds already is added",
            mail_list_text(*job) == "alice@example.com,bob@example.com");
   }
   {
      const auto [ret, mail_list] = mailrec_parse("alice@a.com,alice@b.com,alice@b.com");
      CHECK(41, "one -M with a user of two domains, one of them twice, keeps both domains once",
            ret == 0 && mail_list && mail_list->size() == 2);
   }
   {
      auto cmdline = ocs::tom::ListPtr<ocs::tom::SPA>::create("cmdline");
      add_mail_option(*cmdline, "alice@Example.com,Alice@example.com");
      add_mail_option(*cmdline, "alice@example.COM");
      auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
      ocs_parse_mail_list_merge<JB_mail_list>(*cmdline, "-M", *job);
      CHECK(43, "addresses differing only in the case of the domain are merged, in the case of the user not",
            mail_list_text(*job) == "alice@Example.com,Alice@example.com");
   }
}

/**
 * @brief sge_parse_bitfield_str() gives the right bits when it runs in several threads at once (CS-3171) [T42]
 * @return nothing; CHECK records the results
 */
static void test_bitfield_parse_threads() {
   struct Case {
      const char *text;
      uint32_t expected;
   };
   static constexpr Case cases[] = {{"BATCH INTERACTIVE", 3}, {"INTERACTIVE", 2}, {"BATCH", 1},
                                    {"INTERACTIVE,BATCH", 3}};
   constexpr int rounds = 20000;
   int wrong[std::size(cases)] = {};

   std::vector<std::thread> threads;
   for (std::size_t i = 0; i < std::size(cases); i++) {
      threads.emplace_back([&wrong, i] {
         for (int r = 0; r < rounds; r++) {
            uint32_t value = 0;
            ocs::tom::ListPtr<ocs::tom::AN> alp;
            if (!sge_parse_bitfield_str(cases[i].text, queue_types, &value, "qtype", &alp, true) ||
                value != cases[i].expected) {
               wrong[i]++;
            }
         }
      });
   }
   for (auto &thread : threads) {
      thread.join();
   }

   int wrong_sum = 0;
   for (const int w : wrong) {
      wrong_sum += w;
   }
   if (wrong_sum > 0) {
      printf("      %d of %d parses gave a wrong result\n", wrong_sum, static_cast<int>(std::size(cases)) * rounds);
   }
   CHECK(42, "four threads parsing queue types at once all get their own bits", wrong_sum == 0);
}

// ---------------------------------------------------------------------------

/**
 * @brief Module-test entry point; runs the scenarios through the shared frame.
 * @param argc argument count, see ocs_module_test.h for the frame's options
 * @param argv argument vector
 * @return 0 if all checks pass, 1 otherwise, 2 on a usage error
 */
int main(int argc, char *argv[]) {
   ocs::tom::install_model(ocs::tom::gcs_model());

   return ocs::module_test::run(argc, argv, {
      {"PARSE-01", "parsing words into a list by an interpretation rule",
       "ocs_parse_string_list() and ocs_parse_definition_list() fill the fields of the rule by their type and "
       "give the former return codes for every error",
       {"parse typed words into one and into two elements", "parse NONE for a string and a host",
        "parse words of a wrong form, too few words, no words",
        "parse into fields of an unsupported type, of another type and into a list field",
        "parse a definition list and NONE"},
       "the lists hold the parsed values, the errors give -1, -2, -7, -8, -9, -11, -12 and -13 and no list",
       {}, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}, test_parse_string_list},
      {"PARSE-02", "collapsing repeated keys of a definition list",
       "ocs_compress_definition_list() keeps the last element of a key for string, unsigned and host keys, with "
       "single and double keys",
       {"compress string, unsigned and host keys", "compress with double keys",
        "compress by a list, 64 bit unsigned and foreign key, and an empty handle"},
       "the earlier elements of a repeated key are removed, the errors give -4 and -5 from two elements on",
       {}, {13, 14, 15, 16, 17, 33}, test_compress},
      {"PARSE-03", "printing a list by a field table",
       "uni_print_list() gives the former text for strings, hosts and numbers with every flag, into a stream, a "
       "buffer and a file",
       {"print a typed and a raw list", "print with every flag", "print fields of another type",
        "print into a buffer and into a file"},
       "the texts are the former ones, the errors give -1, -2 and -19",
       {}, {18, 19, 20, 21, 22, 23, 24, 25, 26, 27}, test_uni_print_list},
      {"PARSE-04", "writing an element of any type as XML",
       "the generic field walk of lWriteElemXMLTo() writes every field type in descriptor order byte for byte as "
       "before",
       {"write an element with every field type and a sub-list", "write an element with hosts",
        "write into a file", "write no element"},
       "the output equals the former output", {}, {28, 29, 30, 31}, test_xml_field_walk},
      {"PARSE-05", "writing an XML document",
       "lWriteElemXMLTo() writes the head, its attributes and the printed elements of an XML document",
       {"build a document with two elements and write it"}, "the output equals the former output", {}, {32},
       test_xml_document},
      {"PARSE-06", "writing time stamps, object fields and reduced elements as XML",
       "lWriteElemXMLTo() writes a Uint64 field named *time* as a time stamp, unset, set and nested objects, and "
       "only the fields a reduced element holds",
       {"write a time stamp", "write an element with an unset and a set object field",
        "write an object nesting another object", "write a reduced element"},
       "the output equals the output the former code derives", {}, {34, 35, 36, 37}, test_xml_values},
      {"PARSE-07", "merging repeated -M options",
       "ocs_parse_mail_list_merge() keeps every address of repeated -M options, also of the same mail domain, and "
       "drops only an address given again (CS-3188)",
       {"merge two recipients of one domain", "merge an address given twice", "merge into the list of a job",
        "parse one -M with a user of two domains", "merge addresses differing in the case of the domain or user"},
       "every address is kept once, in the order given, the domain compared without regard to case", {},
       {38, 39, 40, 41, 43}, test_mail_list_merge},
      {"PARSE-08", "parsing bit fields in several threads",
       "sge_parse_bitfield_str() uses the reentrant tokenizer, threads parsing at the same time do not see each "
       "other's tokens (CS-3171)",
       {"parse four different queue type texts in four threads, 20000 times each"},
       "every parse succeeds with the bits of its own text", {}, {42}, test_bitfield_parse_threads},
   });
}
