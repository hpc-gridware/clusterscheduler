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
 * @brief Unit tests for single fields of the flatfile reader and writer in `libs/spool/flatfile`
 *
 * - The load formula of the scheduler configuration is validated as it is stored, without blanks (CS-3166).
 * - A field the writer cannot render is left out instead of crashing the writer (CS-3167).
 * - A configuration entry admin_user followed only by blanks is refused, not read through a null pointer (CS-3165).
 * - A configuration entry without value is refused with a message naming it, for every kind of entry (CS-3215);
 *   the classic reader of a configuration file checks every value it gets in the same field reader.
 * - The JSON reader rejects an unknown word of a field written by name instead of storing 0 (CS-3170), also of a
 *   bit field and of an enumeration.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <utility>
#include <unistd.h>

#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_unistd.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_AQTLIST.h"
#include "sgeobj/tom/ocs_tom_CONF.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/tom/ocs_tom_STN.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_SC.h"
#include "tom/ocs_Element.h"

#include "spool/sge_spooling_utilities.h"
#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"
#include "spool/flatfile/ocs_spool_json.h"

static int failures = 0;

/**
 * @brief Record the outcome of one check
 * @param id the id of the check, printed as `[Tnn]`
 * @param what what the check is about
 * @param ok whether the check held
 */
static void
check(const char *id, const char *what, const bool ok) {
   if (ok) {
      printf("ok    [%s] %s\n", id, what);
   } else {
      printf("FAIL  [%s] %s\n", id, what);
      failures++;
   }
   fflush(stdout);
}

/**
 * @brief Write a string into a new temporary file
 * @param content the complete file content
 * @param[out] path receives the file name, at least 64 bytes
 * @return true if the file was written
 */
static bool
write_tmp_file(const char *content, char *path) {
   strcpy(path, "/tmp/test_spool_ff_fields_XXXXXX");
   const int fd = mkstemp(path);
   if (fd < 0) {
      return false;
   }
   const size_t len = strlen(content);
   const bool ret = write(fd, content, len) == static_cast<ssize_t>(len);
   close(fd);
   return ret;
}

/**
 * @brief Print the messages of an answer list, so a failing check shows its reason
 * @param alp the answer list
 */
static void
print_answers(const ocs::tom::ListPtr<ocs::tom::AN> &alp) {
   if (alp) {
      for (const auto aep : *alp) {
         const char *text = aep.get<AN_text>();
         if (text != nullptr) {
            const size_t len = strlen(text);
            printf("      %s%s", text, len > 0 && text[len - 1] == '\n' ? "" : "\n");
         }
      }
   }
}

/**
 * @brief Read a scheduler configuration from a text as qconf -Msconf does
 * @param text the content of the file
 * @param[out] alp receives the messages
 * @return the scheduler configuration, empty if it was refused
 */
static ocs::tom::ElementPtr<ocs::tom::SC>
read_sconf(const char *text, ocs::tom::ListPtr<ocs::tom::AN> &alp) {
   char path[64];
   ocs::tom::ElementPtr<ocs::tom::SC> sc;
   if (write_tmp_file(text, path)) {
      sc = ocs::tom::ElementPtr<ocs::tom::SC>::from_typed(
         spool_flatfile_read_object(&alp, ocs::tom::SC::descriptor.e, nullptr, SC_fields, nullptr, true,
                                    &qconf_comma_sfi, SP_FORM_ASCII, nullptr, path));
      unlink(path);
   }
   print_answers(alp);
   return sc;
}

/**
 * @brief The load formula is validated as it is stored, without blanks (CS-3166)
 *
 * `no ne` is no keyword, but with the blank stripped it is stored as `none`, which is not allowed.
 */
static void
test_sconf_load_formula_validated_as_stored() {
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   auto sc = read_sconf("load_formula   no ne\n", alp);
   check("T01", "a load formula that is none without its blanks is refused", !sc);

   alp.reset();
   sc = read_sconf("load_formula   np_load_avg + 2\n", alp);
   check("T02", "a valid load formula is stored without blanks",
         sc && sc->get<SC_load_formula>() != nullptr && strcmp(sc->get<SC_load_formula>(), "np_load_avg+2") == 0);
}

/**
 * @brief A field the writer cannot render is left out (CS-3167)
 *
 * A field list with the object field JG_queue: there is no text for an object field, the writer
 * has to skip it and write the other fields.
 */
static void
test_write_unrenderable_field() {
   const auto *type_ops = ocs_spool_type_ops_of(ocs::tom::JG::descriptor.e);
   check("T11", "the spooling records of JG exist", type_ops != nullptr);
   if (type_ops == nullptr) {
      return;
   }

   spooling_field fields[3] = {};
   const int nms[2] = {JG_qname, JG_queue};
   for (int i = 0; i < 2; i++) {
      fields[i].nm = nms[i];
      fields[i].name = ocs::tom::field_name(nms[i]);
      fields[i].ops = &type_ops->ops[nms[i] - type_ops->base];
   }
   fields[2].nm = NoName;

   auto gdil = ocs::tom::ElementPtr<ocs::tom::JG>::create();
   gdil->set<JG_qname>("all.q@host1");

   std::ostringstream os;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   const char *name = spool_flatfile_write_object(&alp, gdil.get(), true, fields, &qconf_sfi, SP_DEST_STREAM,
                                                  SP_FORM_ASCII, nullptr, false, nullptr, &os, SP_LINE_BREAKS_OFF);
   print_answers(alp);
   const std::string text = os.str();
   check("T12", "an object field is left out, the writer does not crash", name != nullptr);
   check("T13", "the other fields are written", text.find("all.q@host1") != std::string::npos);
   check("T14", "nothing is written for the object field", text.find("JG_queue") == std::string::npos);
   check("T15", "leaving out a field without a text form is no error", !answer_list_has_error(&alp));
   sge_free(&name);
}

/**
 * @brief A configuration entry admin_user with only blanks as its value is refused (CS-3165)
 *
 * The JSON reader hands the text of a configuration value to read_CF_value() as it is, so `"   "` reaches it;
 * the tokenizer gives no token for it, and the value was dereferenced without a null check. The classic
 * (ASCII) reader calls read_CF_value() with the rest of the line whether it parses values or not, but only when
 * the line has a value after the name; a name followed by blanks only is stored without value and refused by
 * qmaster's check_config() (see test_conf_entry_classic()).
 */
static void
test_conf_admin_user_without_value() {
   spooling_field *fields = sge_build_CONF_field_list(true);
   check("T31", "the spooling fields of CONF exist", fields != nullptr);
   if (fields == nullptr) {
      return;
   }

   auto conf = ocs::tom::ElementPtr<ocs::tom::CONF>::create();
   conf->set<CONF_name>(ocs::tom::Host("global"));
   auto entries = ocs::tom::ListPtr<ocs::tom::CF>::create("entries");
   const auto cf = entries->emplace_back<CF_name>("admin_user");
   cf.set<CF_value>("test_admin");
   conf->set<CONF_entries>(std::move(entries));

   ocs::tom::ListPtr<ocs::tom::AN> alp;
   dstring out = DSTRING_INIT;
   const bool written = spool_json_write_object(&alp, conf.get(), fields, &out);
   std::string json = written ? sge_dstring_get_string(&out) : "";
   sge_dstring_free(&out);
   print_answers(alp);

   const std::string value = "\"test_admin\"";
   const std::size_t pos = json.find(value);
   check("T32", "admin_user is written as JSON", pos != std::string::npos);
   if (pos != std::string::npos) {
      json.replace(pos, value.size(), "\"   \"");
      alp.reset();
      auto ep = spool_json_read_object(&alp, ocs::tom::CONF::descriptor.e, fields, nullptr, json.c_str());
      print_answers(alp);
      check("T33", "admin_user with only blanks is refused with a message, the reader does not crash",
            !ep && answer_list_has_error(&alp));
   }
   sge_free(&fields);
}

/**
 * @brief Whether an answer list holds an error whose text names a word
 * @param alp the answer list
 * @param word the word
 * @return true if an error message contains the word
 */
static bool
has_error_naming(const ocs::tom::ListPtr<ocs::tom::AN> &alp, const char *word) {
   if (alp) {
      for (const auto aep : *alp) {
         const char *text = aep.get<AN_text>();
         if (aep.get<AN_quality>() == ANSWER_QUALITY_ERROR && text != nullptr && strstr(text, word) != nullptr) {
            return true;
         }
      }
   }
   return false;
}

/**
 * @brief Read a cluster configuration from JSON whose only entry has a given text as value
 * @param name the name of the entry
 * @param value the value, written into the JSON as it is
 * @param[out] alp receives the messages
 * @return true if the configuration was read, false if it was refused
 */
static bool
read_conf_json(const char *name, const char *value, ocs::tom::ListPtr<ocs::tom::AN> &alp) {
   bool read = false;
   spooling_field *fields = sge_build_CONF_field_list(true);
   if (fields != nullptr) {
      const std::string json = std::string("{\"") + name + "\": \"" + value + "\"}";
      auto ep = spool_json_read_object(&alp, ocs::tom::CONF::descriptor.e, fields, nullptr, json.c_str());
      read = static_cast<bool>(ep);
   }
   sge_free(&fields);
   print_answers(alp);
   return read;
}

/**
 * @brief A configuration entry without value is refused the same way for every kind of entry (CS-3215)
 *
 * read_CF_value() reads the values of the JSON reader. gid_range with blanks only was accepted with a null
 * value, an entry that takes the rest of the line (prolog) with blanks only was stored as an empty text, and
 * the other rejections only logged a warning: the caller saw the read fail without a reason. Every kind of
 * entry has to be refused with a message naming the entry. The classic (ASCII) reader of a configuration file
 * calls read_CF_value() too, for every line with a value; test_conf_entry_classic() covers that path.
 */
static void
test_conf_entry_without_value() {
   // one entry of each branch of read_CF_value(): its own, the rest of the line, a single word
   const char *names[] = {"gid_range", "admin_user", "prolog", "load_report_time"};
   int id = 36;
   for (const char *name : names) {
      ocs::tom::ListPtr<ocs::tom::AN> alp;
      const bool read = read_conf_json(name, "   ", alp);
      const std::string label = std::string(name) + " with blanks only is refused naming the entry";
      char tid[16];
      snprintf(tid, sizeof(tid), "T%02d", id++);
      check(tid, label.c_str(), !read && has_error_naming(alp, name));
   }

   // a second value of a single word entry and a gid_range that is no range or too low are refused with a message
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   check("T40", "two values of load_report_time are refused naming the entry",
         !read_conf_json("load_report_time", "00:00:40 00:00:50", alp) && has_error_naming(alp, "load_report_time"));
   alp.reset();
   check("T41", "a gid_range that is no range is refused naming the entry",
         !read_conf_json("gid_range", "x-y", alp) && has_error_naming(alp, "gid_range"));
   alp.reset();
   check("T42", "a gid_range below the allowed minimum is refused with a message",
         !read_conf_json("gid_range", "50-200", alp) && answer_list_has_error(&alp));

   // values that are fine are still read
   for (const auto &[name, value] : {std::pair{"gid_range", "20000-20100"}, std::pair{"prolog", "/bin/true arg"},
                                     std::pair{"load_report_time", "00:00:40"}}) {
      alp.reset();
      const std::string label = std::string("a valid ") + name + " is read";
      char tid[16];
      snprintf(tid, sizeof(tid), "T%02d", id + 3);
      id++;
      check(tid, label.c_str(), read_conf_json(name, value, alp) && !answer_list_has_error(&alp));
   }
}

/**
 * @brief Read a cluster configuration from a text as the classic (ASCII) reader of a configuration file does
 *
 * The same call as spooldefaults makes for the global configuration: values are not parsed by the reader, the
 * field reader read_CF_value() gets the rest of the line all the same.
 *
 * @param text the content of the file
 * @param[out] alp receives the messages
 * @return the configuration, empty if it was refused
 */
static ocs::tom::ElementPtr<ocs::tom::CONF>
read_conf_classic(const char *text, ocs::tom::ListPtr<ocs::tom::AN> &alp) {
   char path[64];
   ocs::tom::ElementPtr<ocs::tom::CONF> conf;
   spooling_field *fields = sge_build_CONF_field_list(true);
   if (fields != nullptr && write_tmp_file(text, path)) {
      conf = ocs::tom::ElementPtr<ocs::tom::CONF>::from_typed(
         spool_flatfile_read_object(&alp, ocs::tom::CONF::descriptor.e, nullptr, fields, nullptr, false,
                                    &qconf_sfi, SP_FORM_ASCII, nullptr, path));
      unlink(path);
   }
   sge_free(&fields);
   print_answers(alp);
   return conf;
}

/**
 * @brief The value of a configuration entry as the classic reader stored it
 * @param conf the configuration
 * @param name the name of the entry
 * @return the value, nullptr if there is no such entry or it has no value
 */
static const char *
conf_value_of(const ocs::tom::ElementPtr<ocs::tom::CONF> &conf, const char *name) {
   if (conf) {
      const auto cf = conf->get<CONF_entries>().find_first<CF_name>(name);
      if (cf) {
         return cf.get<CF_value>();
      }
   }
   return nullptr;
}

/**
 * @brief The classic reader of a configuration file checks its values in read_CF_value() too (CS-3215)
 *
 * The flatfile reader hands the rest of the line to the field reader read_CF_value() also when it does not
 * parse values, so a refused value of a configuration file is refused with a message naming the entry, as from
 * JSON. A name followed by blanks only is different: the line ends before the value field, the field reader is
 * not called and the entry is stored without value; qmaster's check_config() refuses such an entry naming it.
 * Valid entries are read with their values.
 */
static void
test_conf_entry_classic() {
   const char *names[] = {"gid_range", "prolog", "load_report_time"};
   int id = 50;
   for (const char *name : names) {
      ocs::tom::ListPtr<ocs::tom::AN> alp;
      const std::string text = std::string(name) + "   \n";
      const auto conf = read_conf_classic(text.c_str(), alp);
      const std::string label = std::string("classic reader: ") + name + " with blanks only is stored without value";
      char tid[16];
      snprintf(tid, sizeof(tid), "T%02d", id++);
      check(tid, label.c_str(), conf && conf->get<CONF_entries>().find_first<CF_name>(name) &&
            conf_value_of(conf, name) == nullptr);
   }

   ocs::tom::ListPtr<ocs::tom::AN> alp;
   check("T53", "classic reader: a gid_range that is no range is refused naming the entry",
         !read_conf_classic("gid_range   x-y\n", alp) && has_error_naming(alp, "gid_range"));
   alp.reset();
   check("T57", "classic reader: two values of load_report_time are refused naming the entry",
         !read_conf_classic("load_report_time   00:00:40 00:00:50\n", alp) &&
         has_error_naming(alp, "load_report_time"));
   alp.reset();
   check("T58", "classic reader: a gid_range below the allowed minimum is refused with a message",
         !read_conf_classic("gid_range   50-200\n", alp) && answer_list_has_error(&alp));

   alp.reset();
   const auto conf = read_conf_classic("gid_range   20000-20100\nprolog   /bin/true arg\n"
                                       "load_report_time   00:00:40\n", alp);
   check("T54", "classic reader: valid entries are read", conf && !answer_list_has_error(&alp));
   const char *gid_range = conf_value_of(conf, "gid_range");
   const char *prolog = conf_value_of(conf, "prolog");
   check("T55", "classic reader: gid_range keeps its value", gid_range != nullptr &&
         strcmp(gid_range, "20000-20100") == 0);
   check("T56", "classic reader: prolog keeps the rest of the line", prolog != nullptr &&
         strcmp(prolog, "/bin/true arg") == 0);
}

/**
 * @brief The JSON reader rejects an unknown word of a field written by name (CS-3170)
 *
 * A complex entry is written as JSON, its requestable word `FORCED` is replaced by an unknown one. The
 * reader used to ignore the failure of the parser and left the field at 0, which the complex code treats
 * like `YES`: a resource that must not be requested became requestable. The read has to fail with a message.
 */
static void
test_json_by_name_unknown_word() {
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   spooling_field *fields = spool_get_fields_to_spool(&alp, ocs::tom::CE::descriptor.e, &spool_config_instr);
   check("T21", "the spooling fields of CE exist", fields != nullptr);
   if (fields == nullptr) {
      return;
   }

   auto ce = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ce->set<CE_name>("test_ce");
   ce->set<CE_shortcut>("tc");
   ce->set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::INT));
   ce->set<CE_relop>(CMPLXLE_OP);
   ce->set<CE_requestable>(REQU_FORCED);
   ce->set<CE_consumable>(CONSUMABLE_NO);
   ce->set<CE_defaultval>("0");
   ce->set<CE_urgency_weight>("0");

   dstring out = DSTRING_INIT;
   const bool written = spool_json_write_object(&alp, ce.get(), fields, &out);
   const std::string json = written ? sge_dstring_get_string(&out) : "";
   sge_dstring_free(&out);
   print_answers(alp);

   const std::string word = "\"FORCED\"";
   const std::size_t pos = json.find(word);
   check("T22", "the requestable word is written as FORCED", pos != std::string::npos);
   if (pos != std::string::npos) {
      alp.reset();
      auto ep = spool_json_read_object(&alp, ocs::tom::CE::descriptor.e, fields, nullptr, json.c_str());
      check("T23", "the unchanged JSON reads back as FORCED",
            ep && ep->get<std::uint32_t>(CE_requestable) == REQU_FORCED);

      std::string bad = json;
      bad.replace(pos, word.size(), "\"BOGUS\"");
      alp.reset();
      ep = spool_json_read_object(&alp, ocs::tom::CE::descriptor.e, fields, nullptr, bad.c_str());
      print_answers(alp);
      check("T24", "an unknown requestable word fails the read with a message",
            !ep && answer_list_has_error(&alp));
   }

   spool_free_spooling_fields(fields);
}

/**
 * @brief Write an element as JSON, replace one word and read it back
 * @param ep the element
 * @param descr the descriptor of the element
 * @param word the word as written, with its quotes; it has to occur exactly once
 * @param replacement the replacement, with its quotes
 * @param[out] written true if the element was written and the word found
 * @param[out] error true if the read failed with an error in the answer list
 * @param object_fields the spooling fields of the object, nullptr for those derived from @p descr
 * @return true if the read gave an element
 */
static bool
json_read_with_replaced_word(const ocs::tom::ConstRawElement ep, const ocs::tom::FieldDescr *descr,
                             const std::string &word, const std::string &replacement, bool *written, bool *error,
                             spooling_field *object_fields = nullptr) {
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   spooling_field *fields = object_fields != nullptr ? object_fields
                                                     : spool_get_fields_to_spool(&alp, descr, &spool_config_instr);
   *written = false;
   *error = false;
   if (fields == nullptr) {
      return false;
   }
   dstring out = DSTRING_INIT;
   std::string json = spool_json_write_object(&alp, ep, fields, &out) ? sge_dstring_get_string(&out) : "";
   sge_dstring_free(&out);
   const std::size_t pos = json.find(word);
   bool read = false;
   if (pos != std::string::npos && json.find(word, pos + 1) == std::string::npos) {
      *written = true;
      json.replace(pos, word.size(), replacement);
      alp.reset();
      auto rp = spool_json_read_object(&alp, descr, fields, nullptr, json.c_str());
      print_answers(alp);
      *error = answer_list_has_error(&alp);
      read = static_cast<bool>(rp);
   } else {
      printf("      %s not found once in:\n%s\n", word.c_str(), json.c_str());
   }
   if (object_fields == nullptr) {
      spool_free_spooling_fields(fields);
   }
   return read;
}

/**
 * @brief The JSON reader rejects an unknown word of a bit field or of an enumeration (CS-3170)
 *
 * A userset type and a share tree node type are written as JSON, the word is replaced by an unknown one. The
 * reader dropped an unknown bit and read an unknown enumeration word as 0; it has to fail with a message.
 */
static void
test_json_bitfield_enum_unknown_word() {
   auto us = ocs::tom::ElementPtr<ocs::tom::US>::create();
   us->set<US_name>("test_us");
   us->set<US_type>(US_ACL);
   bool written = false;
   bool error = false;
   bool read = json_read_with_replaced_word(us.get(), ocs::tom::US::descriptor.e, "\"ACL\"", "\"ACL\"", &written,
                                            &error);
   check("T25", "the userset type reads back unchanged", written && read && !error);
   read = json_read_with_replaced_word(us.get(), ocs::tom::US::descriptor.e, "\"ACL\"", "\"BOGUS\"", &written,
                                       &error);
   check("T26", "an unknown userset type word fails the read with a message", written && !read && error);

   auto stn = ocs::tom::ElementPtr<ocs::tom::STN>::create();
   stn->set<STN_name>("test_node");
   stn->set<STN_type>(1);
   read = json_read_with_replaced_word(stn.get(), ocs::tom::STN::descriptor.e, "\"PROJECT\"", "\"PROJECT\"",
                                       &written, &error);
   check("T27", "the share tree node type reads back unchanged", written && read && !error);
   read = json_read_with_replaced_word(stn.get(), ocs::tom::STN::descriptor.e, "\"PROJECT\"", "\"BOGUS\"",
                                       &written, &error);
   check("T28", "an unknown share tree node type word fails the read with a message", written && !read && error);
}

/**
 * @brief The JSON reader rejects an unknown word of a bit field inside a sub-list (CS-3170)
 *
 * The queue type of a cluster queue is a list of host specific values, each a bit field. The word of one value
 * is replaced by an unknown one; the reader has to fail with a message instead of dropping the bit.
 */
static void
test_json_sublist_bitfield_unknown_word() {
   auto cq = ocs::tom::ElementPtr<ocs::tom::CQ>::create();
   cq->set<CQ_name>("test.q");
   const auto qtype = cq->emplace_back_in<CQ_qtype, AQTLIST_href>("@/");
   qtype.set<AQTLIST_value>(BQ);
   bool written = false;
   bool error = false;
   bool read = json_read_with_replaced_word(cq.get(), ocs::tom::CQ::descriptor.e, "\"BATCH\"", "\"BATCH\"",
                                            &written, &error, CQ_fields);
   check("T34", "the queue type in the sub-list reads back unchanged", written && read && !error);
   read = json_read_with_replaced_word(cq.get(), ocs::tom::CQ::descriptor.e, "\"BATCH\"", "\"BOGUS\"",
                                       &written, &error, CQ_fields);
   check("T35", "an unknown queue type word in the sub-list fails the read with a message",
         written && !read && error);
}

/**
 * @brief Run the tests
 * @return 0 if all checks held, 1 otherwise
 */
int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_spool_flatfile_fields");

   component_set_daemonized(true);
   ocs::tom::install_model(ocs::tom::gcs_model());

   test_sconf_load_formula_validated_as_stored();
   test_write_unrenderable_field();
   test_json_by_name_unknown_word();
   test_json_bitfield_enum_unknown_word();
   test_json_sublist_bitfield_unknown_word();
   test_conf_admin_user_without_value();
   test_conf_entry_without_value();
   test_conf_entry_classic();

   printf("\n%s: %d failure(s)\n", failures == 0 ? "PASSED" : "FAILED", failures);
   DRETURN(failures == 0 ? 0 : 1);
}
