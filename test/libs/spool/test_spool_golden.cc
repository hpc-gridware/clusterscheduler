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
 * @brief Golden output of the flatfile and JSON spooling of every spooled object type
 *
 * Writes one file per object type, variant, field list ("layout") and format, built from
 * objects in which every field the field table writes holds a distinctive value. Two runs,
 * one before and one after a change of the spooling field tables (`spooling_field[]` in
 * sge_flatfile_obj.cc, the writers in sge_flatfile.cc and ocs_spool_json.cc, the field
 * selection in sge_spooling_utilities.cc), are compared with `diff -r`; an empty diff
 * proves the change byte identical.
 *
 * Usage: `test_spool_golden [--no-roundtrip] [<outdir>]`
 *
 * - `<outdir>` receives the files. Without it they go to a temporary directory that is
 *   removed at the end, which is what a bare ctest run does.
 * - Unless `--no-roundtrip` is given, every text is read back with the reader the product
 *   uses for it and written again; the second text has to equal the first. A mismatch is
 *   reported, the second text is left next to the first as `<file>.roundtrip`, and the exit
 *   code is 1.
 *
 * Determinism: TZ is set to UTC (the JSON writer renders UU_delete_time and qconf -se with
 * more info renders EH_auto_added_time in local time); the `# Version:` line of the spool
 * header, which carries the version and the build id, is replaced by a fixed text; host
 * names are fixed strings and never resolved (a throwaway bootstrap with ignore_fqdn is
 * installed, as test_sgeobj_hgroup does, because hashing a host field needs it). The
 * classic job and advance reservation files are packed CULL, written below `classic/`.
 */

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <map>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <unistd.h>

#include "uti/ocs_Bootstrap.h"
#include "uti/sge.h"
#include "uti/sge_component.h"
#include "uti/sge_dstring.h"
#include "uti/sge_io.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_spool.h"
#include "uti/sge_stdlib.h"

#include "symbols.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/ocs_CEntry.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_attr.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_userset.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "spool/sge_spooling_utilities.h"
#include "spool/classic/read_write_ar.h"
#include "spool/classic/read_write_job.h"
#include "spool/flatfile/ocs_spool_json.h"
#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"

namespace tom = ocs::tom;

namespace {

// ---------------------------------------------------------------------------------------------------------------
// run state

std::string g_outdir;          ///< absolute directory the golden files go to
bool g_roundtrip = true;       ///< read every text back and write it again
int g_failures = 0;            ///< number of failed checks
int g_files = 0;               ///< number of golden files written
int g_known = 0;               ///< number of failed checks that are known deviations of the product

/**
 * @brief A read-back the product is known to fail today, recorded so the test pins the rest
 */
struct KnownDeviation {
   const char *what;   ///< the file the check is about, as fail() names it
   const char *why;    ///< why the product fails it
   bool seen;          ///< the check failed in this run
};

/// read-backs that fail with the product as it is; a deviation that no longer fails is reported, so the entry goes
KnownDeviation g_known_deviations[] = {
   {"CQ.empty.qconf.qconf", "an empty (not absent) hostlist is written without a value, the reader rejects it", false},
   {"CQ.empty.qconf.qconf-1line", "as CQ.empty.qconf.qconf, in the one-line form", false},
   {"classic/jobs/00/0000/4711/common", "read from disk and spooled again the job file is 12 bytes shorter", false},
};

constexpr uint64_t T0 = 1767225600000000;   ///< 2026-01-01T00:00:00Z in microseconds, the base of every time stamp
constexpr const char *GRP = "@ggrp1";        ///< the host group the per host overrides name
constexpr const char *HOST = "gexec02";      ///< the host the per host overrides name

/** @brief Which shape of an object's sublists a builder produces */
enum class Variant {
   FULL,    ///< every optional sublist holds distinctive elements
   NONE,    ///< every optional sublist is unset (nullptr), written as NONE
   EMPTY    ///< every optional sublist is an empty list
};

/** @brief One output format a field list is written in */
enum class Form {
   SPOOL,         ///< the classic spool file: ASCII with the version header, as SP_DEST_SPOOL writes it
   QCONF,         ///< ASCII as qconf shows it, long lines broken (SGE_SINGLE_LINE unset)
   QCONF_1LINE,   ///< ASCII as qconf shows it with SGE_SINGLE_LINE set
   JSON,          ///< JSON, TIME and MEM values in the compact form (qconf -fmt json)
   JSON_NUM       ///< JSON, TIME and MEM values as numbers (qconf -fmt json -fmtval numeric)
};

constexpr unsigned F_SPOOL = 1;   ///< layout is written as Form::SPOOL
constexpr unsigned F_QCONF = 2;   ///< layout is written as Form::QCONF and Form::QCONF_1LINE
constexpr unsigned F_JSON = 4;    ///< layout is written as Form::JSON and Form::JSON_NUM

/** @brief How a field list is obtained: the static tables and the builders of sge_flatfile_obj.cc */
enum class FieldKind {
   STATIC,     ///< a static table, FieldSource::table
   PR,         ///< sge_build_PR_field_list(a)
   UU,         ///< sge_build_UU_field_list(a)
   STN,        ///< sge_build_STN_field_list(a, b)
   STN_JSON,   ///< sge_build_STN_json_field_list()
   EH,         ///< sge_build_EH_field_list(a, b, c, d)
   CONF,       ///< sge_build_CONF_field_list(a)
   CQ,         ///< sge_build_CQ_field_list(a)
   QU,         ///< sge_build_QU_field_list(a, b)
   SC_SPOOL    ///< spool_get_fields_to_spool() of SC with spool_config_instr, what the classic backend uses
};

/** @brief The field list of a layout, as the product obtains it */
struct FieldSource {
   FieldKind kind;                    ///< which builder
   spooling_field *table = nullptr;   ///< the static table for FieldKind::STATIC
   bool a = false;                    ///< first flag of the builder
   bool b = false;                    ///< second flag of the builder
   bool c = false;                    ///< third flag of the builder
   bool d = false;                    ///< fourth flag of the builder
};

/** @brief One way the product writes an object type: field list, instruction and formats */
struct Layout {
   const char *label;                                  ///< part of the file name
   FieldSource source;                                 ///< the field list
   const spool_flatfile_instr *instr;                  ///< the flatfile instruction
   unsigned forms;                                     ///< F_SPOOL, F_QCONF, F_JSON
   bool is_root;                                       ///< the is_root argument of the writer
   bool parse_values;                                  ///< the parse_values argument of the reader
   const char *json_type_name;                         ///< the explicit JSON type, nullptr derives it
   bool roundtrip;                                     ///< the product reads this text back
   lListElem *(*prepare)(const lListElem *) = nullptr; ///< turns the object into what is written, or nullptr
};

/** @brief One spooled object type with every way it is written */
struct ObjectCase {
   const char *name;                    ///< file name stem
   const lDescr *descr;                 ///< the descriptor the reader is given
   lListElem *(*build)(Variant);        ///< builds the object; the caller frees it
   bool variants;                       ///< also write the NONE and EMPTY variants
   void (*fixup)(lListElem *);          ///< applied to an object read back before it is written again, or nullptr
   std::vector<Layout> layouts;         ///< the ways the object is written
};

/** @brief How a list is written as JSON */
enum class ListJson {
   NONE,       ///< not at all
   PLAIN,      ///< spool_flatfile_write_list(SP_FORM_JSON) -> spool_json_write_list()
   TYPED,      ///< spool_json_write_typed_list(json_id)
   TYPED_EX,   ///< spool_json_write_typed_list_ex(json_id, json_envelope)
   NAMES       ///< spool_json_write_name_list_ex(name_nm, json_id)
};

/** @brief One list written as a whole (spool_flatfile_write_list() and the JSON list writers) */
struct ListCase {
   const char *name;                      ///< file name stem
   const lDescr *descr;                   ///< the descriptor of the elements
   lList *(*build)();                     ///< builds the list; the caller frees it
   FieldSource source;                    ///< the field list
   const spool_flatfile_instr *instr;     ///< the ASCII instruction, nullptr when there is no ASCII form
   bool table;                            ///< fields copied and aligned with padding 3, as QConfTableView does
   bool parse_values;                     ///< the parse_values argument of the reader
   ListJson json;                         ///< the JSON writer
   const char *json_id;                   ///< type name for TYPED, TYPED_EX and NAMES
   const char *json_envelope;             ///< envelope name for TYPED_EX
   int name_nm;                           ///< key field for NAMES
   bool roundtrip;                        ///< the product reads this text back
};

// ---------------------------------------------------------------------------------------------------------------
// output, normalisation, reporting

/**
 * @brief Record a failed check
 * @param what the file or object the check was about
 * @param why what went wrong
 */
void fail(const std::string &what, const std::string &why) {
   for (auto &known : g_known_deviations) {
      if (what == known.what) {
         printf("KNOWN %s: %s (%s)\n", what.c_str(), why.c_str(), known.why);
         known.seen = true;
         ++g_known;
         return;
      }
   }
   printf("FAIL  %s: %s\n", what.c_str(), why.c_str());
   ++g_failures;
}

/**
 * @brief Print the messages of an answer list and tell whether it holds an error
 * @param alp the answer list, emptied afterwards
 * @param what the file or object the call was about, for the messages
 * @return true when the list holds no error
 */
bool answers_ok(tom::ListPtr<tom::AN> &alp, const std::string &what) {
   const bool ok = !answer_list_has_error(&alp);
   if (!ok) {
      for (const auto an : *alp) {
         const char *text = an.get<AN_text>();
         printf("      %s: %s\n", what.c_str(), text != nullptr ? text : "");
      }
   }
   alp.reset();
   return ok;
}

/**
 * @brief Replace what differs between two builds of the same source: the `# Version:` line of the spool header
 *
 * The header carries the product version and the build id (feature_get_product_name()), so it
 * would differ between a build before and one after a change.
 *
 * @param text the text as written
 * @return the text with every `# Version: ...` line replaced by `# Version: <normalised>`
 */
std::string normalise(const std::string &text) {
   static const std::string marker = "# Version: ";
   std::string out;
   std::size_t pos = 0;
   while (pos < text.size()) {
      std::size_t end = text.find('\n', pos);
      if (end == std::string::npos) {
         end = text.size();
      }
      const std::string line = text.substr(pos, end - pos);
      out += (line.compare(0, marker.size(), marker) == 0) ? marker + "<normalised>" : line;
      if (end < text.size()) {
         out += '\n';
      }
      pos = end + 1;
   }
   return out;
}

/**
 * @brief Write a golden file
 * @param name the file name below the output directory
 * @param text the content, normalised here
 */
void emit(const std::string &name, const std::string &text) {
   std::ofstream out(g_outdir + "/" + name, std::ios::binary | std::ios::trunc);
   out << normalise(text);
   if (!out.good()) {
      fail(name, "cannot write the file");
   }
   ++g_files;
}

/**
 * @brief Compare a text with the one written again after reading it back
 * @param name the golden file name
 * @param first the text as written from the built object
 * @param second the text written from the object read back
 */
void compare(const std::string &name, const std::string &first, const std::string &second) {
   if (normalise(first) != normalise(second)) {
      emit(name + ".roundtrip", second);
      fail(name, "read back and written again it differs, see " + name + ".roundtrip");
   }
}

/**
 * @brief The file name extension of a form
 * @param form the form
 * @return the extension, without the dot
 */
const char *form_ext(const Form form) {
   switch (form) {
      case Form::SPOOL:
         return "spool";
      case Form::QCONF:
         return "qconf";
      case Form::QCONF_1LINE:
         return "qconf-1line";
      case Form::JSON:
         return "json";
      case Form::JSON_NUM:
         return "num.json";
   }
   return "?";
}

/**
 * @brief The forms a mask of F_SPOOL, F_QCONF and F_JSON stands for
 * @param mask the mask
 * @return the forms, in a fixed order
 */
std::vector<Form> forms_of(const unsigned mask) {
   std::vector<Form> forms;
   if ((mask & F_SPOOL) != 0) {
      forms.push_back(Form::SPOOL);
   }
   if ((mask & F_QCONF) != 0) {
      forms.push_back(Form::QCONF);
      forms.push_back(Form::QCONF_1LINE);
   }
   if ((mask & F_JSON) != 0) {
      forms.push_back(Form::JSON);
      forms.push_back(Form::JSON_NUM);
   }
   return forms;
}

/**
 * @brief Whether a form is JSON; sets the JSON value format the form stands for
 * @param form the form
 * @return true for Form::JSON and Form::JSON_NUM
 */
bool select_format(const Form form) {
   ocs_json_value_format_opt = (form == Form::JSON_NUM) ? OCS_JSON_VALUES_NUMERIC : OCS_JSON_VALUES_COMPACT;
   return form == Form::JSON || form == Form::JSON_NUM;
}

/**
 * @brief The line break mode of a form
 * @param form the form
 * @return SP_LINE_BREAKS_OFF for Form::QCONF_1LINE, SP_LINE_BREAKS_ON otherwise
 */
spool_flatfile_line_breaks line_breaks_of(const Form form) {
   return (form == Form::QCONF_1LINE) ? SP_LINE_BREAKS_OFF : SP_LINE_BREAKS_ON;
}

/**
 * @brief The name of a variant, part of the file name
 * @param variant the variant
 * @return "full", "none" or "empty"
 */
const char *variant_name(const Variant variant) {
   switch (variant) {
      case Variant::FULL:
         return "full";
      case Variant::NONE:
         return "none";
      case Variant::EMPTY:
         return "empty";
   }
   return "?";
}

// ---------------------------------------------------------------------------------------------------------------
// field lists

/**
 * @brief Obtain a field list the way the product does
 * @param source which list
 * @param owned set to true when the list was allocated and has to be freed with spool_free_spooling_fields()
 * @return the field list
 */
spooling_field *build_fields(const FieldSource &source, bool *owned) {
   *owned = true;
   switch (source.kind) {
      case FieldKind::STATIC:
         *owned = false;
         return source.table;
      case FieldKind::PR:
         return sge_build_PR_field_list(source.a);
      case FieldKind::UU:
         return sge_build_UU_field_list(source.a);
      case FieldKind::STN:
         return sge_build_STN_field_list(source.a, source.b);
      case FieldKind::STN_JSON:
         return sge_build_STN_json_field_list();
      case FieldKind::EH:
         return sge_build_EH_field_list(source.a, source.b, source.c, source.d);
      case FieldKind::CONF:
         return sge_build_CONF_field_list(source.a);
      case FieldKind::CQ:
         return sge_build_CQ_field_list(source.a);
      case FieldKind::QU:
         return sge_build_QU_field_list(source.a, source.b);
      case FieldKind::SC_SPOOL: {
         tom::ListPtr<tom::AN> alp;
         spooling_field *fields = spool_get_fields_to_spool(&alp, ocs::tom::SC::descriptor.e, &spool_config_instr);
         answers_ok(alp, "spool_get_fields_to_spool(SC)");
         return fields;
      }
   }
   *owned = false;
   return nullptr;
}

/**
 * @brief Release a field list obtained from build_fields()
 * @param fields the list
 * @param owned what build_fields() reported
 */
void release_fields(spooling_field *fields, const bool owned) {
   if (owned && fields != nullptr) {
      spool_free_spooling_fields(fields);
   }
}

/**
 * @brief Copy a field list, as qconf_copy_fields() does before QConfTableView aligns it
 * @param fields the list
 * @return the copy, to be freed with sge_free()
 */
spooling_field *copy_fields(const spooling_field *fields) {
   int n = 0;
   while (fields[n].nm != NoName) {
      n++;
   }
   auto *copy = reinterpret_cast<spooling_field *>(sge_malloc(sizeof(spooling_field) * (n + 1)));
   memcpy(copy, fields, sizeof(spooling_field) * (n + 1));
   return copy;
}

// ---------------------------------------------------------------------------------------------------------------
// writing and reading through the product functions

/**
 * @brief Write one object as the product does
 * @param ep the object
 * @param layout field list, instruction and flags
 * @param fields the field list of the layout
 * @param form the form
 * @param what the file name, for the messages
 * @return the text, empty on an error
 */
std::string render_object(const lListElem *ep, const Layout &layout, const spooling_field *fields, const Form form,
                          const std::string &what) {
   const bool json = select_format(form);
   std::ostringstream os;
   tom::ListPtr<tom::AN> alp;
   const char *name = spool_flatfile_write_object(&alp, ep, layout.is_root, fields, layout.instr, SP_DEST_STREAM,
                                                  json ? SP_FORM_JSON : SP_FORM_ASCII, nullptr, form == Form::SPOOL,
                                                  layout.json_type_name, &os, line_breaks_of(form));
   sge_free(&name);
   if (!answers_ok(alp, what)) {
      return "";
   }
   return os.str();
}

/**
 * @brief Read one object back with the reader the product uses for the text
 * @param text the text
 * @param descr the descriptor of the object
 * @param layout field list, instruction and flags
 * @param fields the field list of the layout
 * @param form the form the text was written in
 * @param what the file name, for the messages
 * @return the object, empty if it cannot be read
 */
tom::RawElementPtr parse_object(const std::string &text, const lDescr *descr, const Layout &layout,
                                const spooling_field *fields, const Form form, const std::string &what) {
   const bool json = select_format(form);
   tom::ListPtr<tom::AN> alp;
   tom::RawElementPtr ep(spool_flatfile_read_object(&alp, descr, nullptr, fields, nullptr, layout.parse_values,
                                                    layout.instr, json ? SP_FORM_JSON : SP_FORM_ASCII, nullptr,
                                                    nullptr, text.c_str()));
   if (!answers_ok(alp, what)) {
      ep.reset();
   }
   return ep;
}

/**
 * @brief Write one list as the product does
 * @param lp the list
 * @param c the list case
 * @param fields the field list
 * @param form the form
 * @param what the file name, for the messages
 * @return the text, empty on an error
 */
std::string render_list(const lList *lp, const ListCase &c, const spooling_field *fields, const Form form,
                        const std::string &what) {
   const bool json = select_format(form);
   tom::ListPtr<tom::AN> alp;
   spooling_field *aligned = nullptr;
   if (c.table) {
      aligned = copy_fields(fields);
      spool_flatfile_align_list(&alp, lp, aligned, 3);
      fields = aligned;
   }

   std::string text;
   if (!json || c.json == ListJson::PLAIN) {
      std::ostringstream os;
      const char *name = spool_flatfile_write_list(&alp, lp, fields, c.instr, SP_DEST_STREAM,
                                                   json ? SP_FORM_JSON : SP_FORM_ASCII, nullptr, false, &os,
                                                   line_breaks_of(form));
      sge_free(&name);
      text = os.str();
   } else {
      dstring out = DSTRING_INIT;
      switch (c.json) {
         case ListJson::TYPED:
            spool_json_write_typed_list(&alp, lp, fields, c.json_id, &out);
            break;
         case ListJson::TYPED_EX:
            spool_json_write_typed_list_ex(&alp, lp, fields, c.json_id, c.json_envelope, &out);
            break;
         case ListJson::NAMES:
            spool_json_write_name_list_ex(&alp, lp, c.name_nm, c.json_id, &out);
            break;
         case ListJson::NONE:
         case ListJson::PLAIN:
            break;
      }
      const char *s = sge_dstring_get_string(&out);
      text = (s != nullptr) ? s : "";
      sge_dstring_free(&out);
   }
   sge_free(&aligned);
   if (!answers_ok(alp, what)) {
      return "";
   }
   return text;
}

/**
 * @brief Read a list back with the reader the product uses for the text
 * @param text the text
 * @param c the list case
 * @param fields the field list
 * @param form the form the text was written in
 * @param what the file name, for the messages
 * @return the list, empty if it cannot be read
 */
tom::RawListPtr parse_list(const std::string &text, const ListCase &c, const spooling_field *fields,
                           const Form form, const std::string &what) {
   const bool json = select_format(form);
   tom::ListPtr<tom::AN> alp;
   tom::RawListPtr lp(spool_flatfile_read_list(&alp, c.descr, fields, nullptr, c.parse_values,
                                               c.instr != nullptr ? c.instr : &qconf_sfi,
                                               json ? SP_FORM_JSON : SP_FORM_ASCII, nullptr, nullptr, text.c_str()));
   if (!answers_ok(alp, what)) {
      lp.reset();
   }
   return lp;
}

// ---------------------------------------------------------------------------------------------------------------
// building blocks for the objects

/**
 * @brief The list a builder stores for an optional sublist in a variant
 * @tparam T the element type
 * @param variant the variant
 * @param full the list of Variant::FULL
 * @return @p full, an unset list or an empty list
 */
template <tom::ObjectType T>
tom::ListPtr<T> pick(const Variant variant, tom::ListPtr<T> &&full) {
   switch (variant) {
      case Variant::NONE:
         return {};
      case Variant::EMPTY:
         return tom::ListPtr<T>::create("empty");
      case Variant::FULL:
         break;
   }
   return std::move(full);
}

/**
 * @brief A value in the form the member set<F>() of a field takes: a host name becomes a Host
 * @tparam F the field the value is meant for
 * @tparam V the value type
 * @param value the value
 * @return a Host for a host field, otherwise the value unchanged
 */
template <auto F, typename V>
auto field_value(const V &value) {
   if constexpr (tom::FieldTraits<F>::type == tom::FieldType::Host && std::is_same_v<V, const char *>) {
      return tom::Host(value);
   } else {
      return value;
   }
}

/**
 * @brief A list of elements that hold one name each
 * @tparam F the name field, a string or host field
 * @param values the names
 * @return the list
 */
template <auto F>
tom::ListPtr<typename tom::FieldTraits<F>::Type> names(const std::initializer_list<const char *> values) {
   using T = typename tom::FieldTraits<F>::Type;
   auto lp = tom::ListPtr<T>::create("names");
   for (const char *value : values) {
      auto ep = tom::ElementPtr<T>::create();
      ep->template set<F>(field_value<F>(value));
      lp->push_back(std::move(ep));
   }
   return lp;
}

/**
 * @brief A list of name/value elements
 * @tparam NAME the name field
 * @tparam VALUE the value field
 * @tparam V the value type
 * @param values the pairs
 * @return the list
 */
template <auto NAME, auto VALUE, typename V>
tom::ListPtr<typename tom::FieldTraits<NAME>::Type> pairs(
      const std::initializer_list<std::pair<const char *, V>> values) {
   using T = typename tom::FieldTraits<NAME>::Type;
   auto lp = tom::ListPtr<T>::create("pairs");
   for (const auto &[name, value] : values) {
      auto ep = tom::ElementPtr<T>::create();
      ep->template set<NAME>(field_value<NAME>(name));
      ep->template set<VALUE>(field_value<VALUE>(value));
      lp->push_back(std::move(ep));
   }
   return lp;
}

/**
 * @brief A cluster queue attribute with a default, a host group override and a host override
 * @tparam HREF the host reference field of the attribute type
 * @tparam VALUE the value field
 * @tparam V the value type
 * @param def the default value, for HOSTREF_DEFAULT
 * @param grp the value for the host group GRP
 * @param host the value for the host HOST
 * @return the attribute list
 */
template <auto HREF, auto VALUE, typename V>
tom::ListPtr<typename tom::FieldTraits<HREF>::Type> attr(const V def, const V grp, const V host) {
   using A = typename tom::FieldTraits<HREF>::Type;
   auto lp = tom::ListPtr<A>::create("attr");
   const char *hrefs[] = {HOSTREF_DEFAULT, GRP, HOST};
   const V values[] = {def, grp, host};
   for (int i = 0; i < 3; i++) {
      auto ep = tom::ElementPtr<A>::create();
      ep->template set<HREF>(tom::Host(hrefs[i]));
      ep->template set<VALUE>(values[i]);
      lp->push_back(std::move(ep));
   }
   return lp;
}

/**
 * @brief A cluster queue attribute whose values are lists, with a default and two overrides
 *
 * The default value follows the variant, so that NONE and EMPTY reach the attribute values too.
 *
 * @tparam HREF the host reference field of the attribute type
 * @tparam VALUE the list value field
 * @tparam Make callable taking the index 0 (default), 1 (GRP) or 2 (HOST), returning the value list
 * @param variant the variant
 * @param make builds the value lists
 * @return the attribute list
 */
template <auto HREF, auto VALUE, typename Make>
tom::ListPtr<typename tom::FieldTraits<HREF>::Type> attr_lists(const Variant variant, Make make) {
   using A = typename tom::FieldTraits<HREF>::Type;
   auto lp = tom::ListPtr<A>::create("attr");
   const char *hrefs[] = {HOSTREF_DEFAULT, GRP, HOST};
   for (int i = 0; i < 3; i++) {
      auto ep = tom::ElementPtr<A>::create();
      ep->template set<HREF>(tom::Host(hrefs[i]));
      ep->template set<VALUE>(i == 0 ? pick(variant, make(i)) : make(i));
      lp->push_back(std::move(ep));
   }
   return lp;
}

/**
 * @brief A complex entry with every field of CE_fields set
 * @param name name
 * @param shortcut shortcut
 * @param type value type
 * @param relop relational operator
 * @param requestable REQU_NO, REQU_YES or REQU_FORCED
 * @param consumable CONSUMABLE_NO, _YES, _JOB or _HOST
 * @param defaultval default value
 * @param urgency urgency weight
 * @return the complex entry
 */
tom::ElementPtr<tom::CE> centry(const char *name, const char *shortcut, const ocs::CEntry::Type type,
                                const uint32_t relop, const uint32_t requestable, const uint32_t consumable,
                                const char *defaultval, const char *urgency) {
   auto ce = tom::ElementPtr<tom::CE>::create();
   ce->set<CE_name>(name);
   ce->set<CE_shortcut>(shortcut);
   ce->set<CE_valtype>(static_cast<uint32_t>(type));
   ce->set<CE_relop>(relop);
   ce->set<CE_requestable>(requestable);
   ce->set<CE_consumable>(consumable);
   ce->set<CE_defaultval>(defaultval);
   ce->set<CE_urgency_weight>(urgency);
   return ce;
}

/**
 * @brief A usage list, as projects, users and the scheduler configuration hold it
 * @param scale multiplies every value, so that several lists differ
 * @return the usage list
 */
tom::ListPtr<tom::UA> usage(const double scale) {
   return pairs<UA_name, UA_value, double>({{"cpu", 12.5 * scale}, {"mem", 3.25 * scale}, {"io", 0.5 * scale}});
}

/**
 * @brief The per project usage of a user or project
 * @return the list, two projects with usage and long term usage
 */
tom::ListPtr<tom::UPP> project_usage() {
   auto lp = tom::ListPtr<tom::UPP>::create("project usage");
   const char *projects[] = {"golden_prj", "other_prj"};
   for (int i = 0; i < 2; i++) {
      auto upp = tom::ElementPtr<tom::UPP>::create();
      upp->set<UPP_name>(projects[i]);
      upp->set<UPP_usage>(usage(1.0 + i));
      upp->set<UPP_long_term_usage>(usage(10.0 + i));
      lp->push_back(std::move(upp));
   }
   return lp;
}

/**
 * @brief The debited job usage of a user or project
 * @return the list, two jobs with their old usage
 */
tom::ListPtr<tom::UPU> debited_usage() {
   auto lp = tom::ListPtr<tom::UPU>::create("debited usage");
   const uint32_t jobs[] = {4711, 4712};
   for (int i = 0; i < 2; i++) {
      auto upu = tom::ElementPtr<tom::UPU>::create();
      upu->set<UPU_job_number>(jobs[i]);
      upu->set<UPU_old_usage_list>(usage(0.5 + i));
      lp->push_back(std::move(upu));
   }
   return lp;
}

/**
 * @brief A load value of an execution host
 * @param name name
 * @param value value
 * @param is_static whether it is spooled (ocs_spool_exechost_static_copy() keeps only static ones)
 * @return the load value
 */
tom::ElementPtr<tom::HL> load_value(const char *name, const char *value, const bool is_static) {
   auto hl = tom::ElementPtr<tom::HL>::create();
   hl->set<HL_name>(name);
   hl->set<HL_value>(value);
   hl->set<HL_is_static>(is_static);
   return hl;
}

/**
 * @brief An entry of a resource map
 * @param id the id
 * @param amount how often the id occurs
 * @return the entry
 */
tom::ElementPtr<tom::RESL> resource(const char *id, const uint32_t amount) {
   auto resl = tom::ElementPtr<tom::RESL>::create();
   resl->set<RESL_value>(id);
   resl->set<RESL_amount>(amount);
   return resl;
}

/**
 * @brief A share tree node
 * @param name name
 * @param type 0 for a user node, 1 for a project node
 * @param id node id
 * @param shares shares
 * @return the node
 */
tom::ElementPtr<tom::STN> share_node(const char *name, const uint32_t type, const uint32_t id,
                                     const uint32_t shares) {
   auto stn = tom::ElementPtr<tom::STN>::create();
   stn->set<STN_name>(name);
   stn->set<STN_type>(type);
   stn->set<STN_id>(id);
   stn->set<STN_shares>(shares);
   return stn;
}

/**
 * @brief A resource quota filter
 * @param expand whether the filter is written in braces
 * @param scope the names in scope
 * @param xscope the excluded names, none when empty
 * @return the filter
 */
tom::ElementPtr<tom::RQRF> rqs_filter(const bool expand, const std::initializer_list<const char *> scope,
                                      const std::initializer_list<const char *> xscope) {
   auto filter = tom::ElementPtr<tom::RQRF>::create();
   filter->set<RQRF_expand>(expand);
   filter->set<RQRF_scope>(names<ST_name>(scope));
   if (xscope.size() > 0) {
      filter->set<RQRF_xscope>(names<ST_name>(xscope));
   }
   return filter;
}

/**
 * @brief A granted queue of a job or advance reservation
 * @param qname queue instance name
 * @param host host name
 * @param slots slots
 * @return the granted queue
 */
tom::ElementPtr<tom::JG> granted(const char *qname, const char *host, const uint32_t slots) {
   auto jg = tom::ElementPtr<tom::JG>::create();
   jg->set<JG_qname>(qname);
   jg->set<JG_qhostname>(ocs::tom::Host(host));
   jg->set<JG_slots>(slots);
   return jg;
}

/**
 * @brief A range list with one range
 * @param min first id
 * @param max last id
 * @param step step
 * @return the list
 */
tom::ListPtr<tom::RN> range(const uint32_t min, const uint32_t max, const uint32_t step) {
   auto lp = tom::ListPtr<tom::RN>::create("range");
   auto rn = tom::ElementPtr<tom::RN>::create();
   rn->set<RN_min>(min);
   rn->set<RN_max>(max);
   rn->set<RN_step>(step);
   lp->push_back(std::move(rn));
   return lp;
}

// ---------------------------------------------------------------------------------------------------------------
// the objects

/**
 * @brief A calendar
 * @param variant unused, a calendar has no sublists
 * @return the object, owned by the caller
 */
lListElem *build_CAL([[maybe_unused]] const Variant variant) {
   auto cal = tom::ElementPtr<tom::CAL>::create();
   cal->set<CAL_name>("golden_cal");
   cal->set<CAL_year_calendar>("1.1.2026=off 24.12.2026-26.12.2026=off");
   cal->set<CAL_week_calendar>("mon-fri=8-18=on sat-sun=off");
   return cal.release();
}

/**
 * @brief A checkpointing environment
 * @param variant unused, a checkpointing environment has no sublists
 * @return the object, owned by the caller
 */
lListElem *build_CK([[maybe_unused]] const Variant variant) {
   auto ck = tom::ElementPtr<tom::CK>::create();
   ck->set<CK_name>("golden_ckpt");
   ck->set<CK_interface>("userdefined");
   ck->set<CK_ckpt_command>("/opt/ckpt/ckpt.sh $job_id");
   ck->set<CK_migr_command>("/opt/ckpt/migrate.sh $job_id");
   ck->set<CK_rest_command>("/opt/ckpt/restart.sh $job_id");
   ck->set<CK_clean_command>("/opt/ckpt/clean.sh");
   ck->set<CK_ckpt_dir>("/var/spool/ckpt");
   ck->set<CK_signal>("USR2");
   ck->set<CK_when>("smxr");
   return ck.release();
}

/**
 * @brief A complex entry
 * @param variant unused, the fields of a complex entry that are written hold no sublist
 * @return the object, owned by the caller
 */
lListElem *build_CE([[maybe_unused]] const Variant variant) {
   return centry("golden_mem", "gm", ocs::CEntry::Type::MEM, CMPLXLE_OP, REQU_FORCED, CONSUMABLE_JOB, "1G", "1000")
      .release();
}

/**
 * @brief A host group with hosts and a nested group
 * @param variant shape of the host list
 * @return the object, owned by the caller
 */
lListElem *build_HGRP(const Variant variant) {
   auto hgrp = tom::ElementPtr<tom::HGRP>::create();
   hgrp->set<HGRP_name>(ocs::tom::Host(GRP));
   hgrp->set<HGRP_host_list>(pick(variant, names<HR_name>({"gexec01", HOST, "@ggrp2"})));
   return hgrp.release();
}

/**
 * @brief The reserved host group of the administrative hosts, how `qconf -ah` is spooled
 * @param variant shape of the host list
 * @return the object, owned by the caller
 */
lListElem *build_HGRP_admin(const Variant variant) {
   auto hgrp = tom::ElementPtr<tom::HGRP>::create();
   hgrp->set<HGRP_name>(ocs::tom::Host(ADMIN_HOSTGROUP));
   hgrp->set<HGRP_host_list>(pick(variant, names<HR_name>({"gmaster", "gexec01"})));
   return hgrp.release();
}

/**
 * @brief An access list that is also a department
 * @param variant shape of the entries
 * @return the object, owned by the caller
 */
lListElem *build_US(const Variant variant) {
   auto us = tom::ElementPtr<tom::US>::create();
   us->set<US_name>("golden_acl");
   us->set<US_type>(US_ACL | US_DEPT);
   us->set<US_fshare>(1234);
   us->set<US_oticket>(5678);
   us->set<US_entries>(pick(variant, names<UE_name>({"alice", "bob", "@staff"})));
   return us.release();
}

/**
 * @brief The reserved userset of the managers, how `qconf -am` is spooled
 * @param variant shape of the entries
 * @return the object, owned by the caller
 */
lListElem *build_US_manager(const Variant variant) {
   auto us = tom::ElementPtr<tom::US>::create();
   us->set<US_name>(MANAGER_USERSET);
   us->set<US_type>(US_ACL);
   us->set<US_entries>(pick(variant, names<UE_name>({"root", "gadmin"})));
   return us.release();
}

/**
 * @brief A parallel environment
 * @param variant shape of the access lists
 * @return the object, owned by the caller
 */
lListElem *build_PE(const Variant variant) {
   auto pe = tom::ElementPtr<tom::PE>::create();
   pe->set<PE_name>("golden_pe");
   pe->set<PE_slots>(4711);
   pe->set<PE_user_list>(pick(variant, names<US_name>({"pe_acl_a", "pe_acl_b"})));
   pe->set<PE_xuser_list>(pick(variant, names<US_name>({"pe_xacl"})));
   pe->set<PE_start_proc_args>("/opt/pe/start.sh -catch_rsh $pe_hostfile");
   pe->set<PE_stop_proc_args>("/opt/pe/stop.sh");
   pe->set<PE_allocation_rule>("$round_robin");
   pe->set<PE_control_slaves>(true);
   pe->set<PE_job_is_first_task>(true);
   pe->set<PE_urgency_slots>("avg");
   pe->set<PE_accounting_summary>(true);
   pe->set<PE_ignore_slave_requests_on_master_host>(true);
   pe->set<PE_master_forks_slaves>(true);
   pe->set<PE_daemon_forks_slaves>(true);
   return pe.release();
}

/**
 * @brief An RBAC role
 * @param variant shape of the user list and the parent roles
 * @return the object, owned by the caller
 */
lListElem *build_RL(const Variant variant) {
   auto rl = tom::ElementPtr<tom::RL>::create();
   rl->set<RL_name>("golden_role");
   rl->set<RL_enabled>(true);
   rl->set<RL_user_list>(pick(variant, names<US_name>({"golden_acl", "role_acl"})));
   rl->set<RL_parent_role_list>(pick(variant, names<ST_name>({"operator", "viewer"})));
   rl->set<RL_perm_list>("client:*:read:queue:*:*,client:*:modify:job:*:owner");
   return rl.release();
}

/**
 * @brief A job category, as `qconf -scat` shows it
 * @param variant unused, the fields written hold no sublist
 * @return the object, owned by the caller
 */
lListElem *build_CAT([[maybe_unused]] const Variant variant) {
   auto ct = tom::ElementPtr<tom::CT>::create();
   ct->set<CT_id>(17);
   ct->set<CT_refcount>(3);
   ct->set<CT_str>("-l h_vmem=1G -q golden.q");
   return ct.release();
}

/**
 * @brief A project with accumulated usage
 * @param variant shape of the usage and access lists
 * @return the object, owned by the caller
 */
lListElem *build_PR(const Variant variant) {
   auto pr = tom::ElementPtr<tom::PR>::create();
   pr->set<PR_name>("golden_prj");
   pr->set<PR_oticket>(111);
   pr->set<PR_fshare>(222);
   pr->set<PR_usage>(pick(variant, usage(1.0)));
   pr->set<PR_usage_time_stamp>(T0 + 1);
   pr->set<PR_long_term_usage>(pick(variant, usage(100.0)));
   pr->set<PR_project>(pick(variant, project_usage()));
   pr->set<PR_acl>(pick(variant, names<US_name>({"golden_acl"})));
   pr->set<PR_xacl>(pick(variant, names<US_name>({"bad_acl", "worse_acl"})));
   pr->set<PR_debited_job_usage>(pick(variant, debited_usage()));
   return pr.release();
}

/**
 * @brief A user with accumulated usage
 * @param variant shape of the usage lists
 * @return the object, owned by the caller
 */
lListElem *build_UU(const Variant variant) {
   auto uu = tom::ElementPtr<tom::UU>::create();
   uu->set<UU_name>("alice");
   uu->set<UU_oticket>(333);
   uu->set<UU_fshare>(444);
   uu->set<UU_delete_time>(T0 + 86400000000);
   uu->set<UU_usage>(pick(variant, usage(2.0)));
   uu->set<UU_usage_time_stamp>(T0 + 2);
   uu->set<UU_long_term_usage>(pick(variant, usage(200.0)));
   uu->set<UU_project>(pick(variant, project_usage()));
   uu->set<UU_default_project>("golden_prj");
   uu->set<UU_debited_job_usage>(pick(variant, debited_usage()));
   return uu.release();
}

/**
 * @brief A share tree of three levels
 * @param variant shape of the children of the root
 * @return the root node, owned by the caller
 */
lListElem *build_STN(const Variant variant) {
   auto root = share_node("Root", 0, 0, 1);
   root->set<STN_version>(7);

   auto project = share_node("golden_prj", 1, 1, 100);
   auto users = tom::ListPtr<tom::STN>::create("children");
   users->push_back(share_node("alice", 0, 2, 50));
   users->push_back(share_node("default", 0, 3, 10));
   project->set<STN_children>(std::move(users));

   auto children = tom::ListPtr<tom::STN>::create("children");
   children->push_back(std::move(project));
   children->push_back(share_node("other_prj", 1, 4, 20));
   root->set<STN_children>(pick(variant, std::move(children)));
   return root.release();
}

/**
 * @brief A scheduler configuration
 * @param variant shape of the load adjustments and the usage weights
 * @return the object, owned by the caller
 */
lListElem *build_SC(const Variant variant) {
   auto sc = tom::ElementPtr<tom::SC>::create();
   sc->set<SC_algorithm>("default");
   sc->set<SC_schedule_interval>("0:0:15");
   sc->set<SC_maxujobs>(1024);
   sc->set<SC_queue_sort_method>(static_cast<uint32_t>(QSM_SEQNUM));
   sc->set<SC_job_load_adjustments>(pick(variant, pairs<CE_name, CE_stringval, const char *>(
                                                          {{"np_load_avg", "0.50"}, {"mem_free", "128M"}})));
   sc->set<SC_load_adjustment_decay_time>("0:7:30");
   sc->set<SC_load_formula>("np_load_avg+mem_used*2");
   sc->set<SC_schedd_job_info>("false");
   sc->set<SC_flush_submit_sec>(3);
   sc->set<SC_flush_finish_sec>(5);
   sc->set<SC_params>("MONITOR=true,PROFILE=1");
   sc->set<SC_reprioritize_interval>("0:2:0");
   sc->set<SC_halftime>(168);
   sc->set<SC_usage_weight_list>(pick(variant, pairs<UA_name, UA_value, double>(
                                                       {{"cpu", 0.5}, {"mem", 0.25}, {"io", 0.25}})));
   sc->set<SC_compensation_factor>(5.5);
   sc->set<SC_weight_user>(0.125);
   sc->set<SC_weight_project>(0.25);
   sc->set<SC_weight_department>(0.375);
   sc->set<SC_weight_job>(0.25);
   sc->set<SC_weight_tickets_functional>(10000);
   sc->set<SC_weight_tickets_share>(20000);
   sc->set<SC_share_override_tickets>(true);
   sc->set<SC_share_functional_shares>(true);
   sc->set<SC_max_functional_jobs_to_schedule>(300);
   sc->set<SC_report_pjob_tickets>(true);
   sc->set<SC_max_pending_tasks_per_job>(70);
   sc->set<SC_halflife_decay_list>("cpu=168:mem=0:io=-1");
   sc->set<SC_policy_hierarchy>("OSF");
   sc->set<SC_weight_ticket>(0.5);
   sc->set<SC_weight_waiting_time>(0.125);
   sc->set<SC_weight_deadline>(3600.5);
   sc->set<SC_weight_urgency>(0.25);
   sc->set<SC_weight_priority>(2.5);
   sc->set<SC_max_reservation>(42);
   sc->set<SC_default_duration>("8:0:0");
   return sc.release();
}

/**
 * @brief The global configuration
 * @param variant unused, the entries are the object
 * @return the object, owned by the caller
 */
lListElem *build_CONF([[maybe_unused]] const Variant variant) {
   auto conf = tom::ElementPtr<tom::CONF>::create();
   conf->set<CONF_name>(ocs::tom::Host("global"));
   conf->set<CONF_version>(3);
   conf->set<CONF_entries>(pairs<CF_name, CF_value, const char *>({
      {"execd_spool_dir", "/var/spool/gcs"},
      {"mailer", "/usr/bin/mail"},
      {"load_sensor", "none"},
      {"prolog", "root@/opt/gcs/prolog.sh $job_id"},
      {"epilog", "none"},
      {"shell_start_mode", "posix_compliant"},
      {"login_shells", "sh,bash,ksh,csh,tcsh"},
      {"min_uid", "100"},
      {"min_gid", "100"},
      {"user_lists", "golden_acl role_acl"},
      {"xuser_lists", "none"},
      {"projects", "golden_prj"},
      {"enforce_project", "false"},
      {"load_report_time", "00:00:40"},
      {"max_unheard", "00:05:00"},
      {"reschedule_unknown", "00:00:00"},
      {"loglevel", "log_warning"},
      {"administrator_mail", "admin@example.com"},
      {"qmaster_params", "ENABLE_FORCED_QDEL=true MONITOR_TIME=0:0:10"},
      {"execd_params", "KEEP_ACTIVE=false,ENABLE_BINDING=true"},
      {"reporting_params", "accounting=true reporting=false flush_time=00:00:15 joblog=false"},
      {"finished_jobs", "100"},
      {"gid_range", "20000-20100"},
      {"qlogin_command", "builtin"},
      {"rsh_daemon", "builtin"},
      {"max_aj_tasks", "75000"},
      {"max_u_jobs", "0"},
      {"auto_user_oticket", "0"},
      {"auto_user_fshare", "10"},
      {"auto_user_default_project", "none"},
      {"jsv_url", "none"},
      {"jsv_allowed_mod", "ac,h,i,e,o,j,M,N,p,w"}
   }));
   return conf.release();
}

/**
 * @brief An execution host, with a resource map among its complex values
 * @param variant shape of the sublists
 * @return the object, owned by the caller
 */
lListElem *build_EH(const Variant variant) {
   auto eh = tom::ElementPtr<tom::EH>::create();
   eh->set<EH_name>(ocs::tom::Host("gexec01"));
   eh->set<EH_scaling_list>(pick(variant, pairs<HS_name, HS_value, double>(
                                                  {{"np_load_avg", 1.5}, {"mem_free", 0.75}})));

   auto complex_values = pairs<CE_name, CE_stringval, const char *>({{"mem_free", "4G"}, {"slots", "8"}});
   auto gpu = tom::ElementPtr<tom::CE>::create();
   gpu->set<CE_name>("gpu");
   gpu->set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::RSMAP));
   auto map = tom::ListPtr<tom::RESL>::create("resource map");
   map->push_back(resource("0", 1));
   map->push_back(resource("1", 1));
   map->push_back(resource("A", 2));
#if defined(WITH_EXTENSIONS)
   // an id with per-instance characteristics (CS-1338), written as id[k=v;...]
   auto characteristic = resource("gpu7", 1);
   characteristic->set<RESL_properties>(pairs<CE_name, CE_stringval, const char *>(
                                                {{"memory", "80G"}, {"device", "/dev/nvidia7"}}));
   map->push_back(std::move(characteristic));
   gpu->set<CE_stringval>("5");
#else
   tom::set<CE_stringval>(gpu, "4");
#endif
   gpu->set<CE_resource_map_list>(std::move(map));
   complex_values->push_back(std::move(gpu));
   eh->set<EH_consumable_config_list>(pick(variant, std::move(complex_values)));

   eh->set<EH_usage_scaling_list>(pick(variant, pairs<HS_name, HS_value, double>(
                                                        {{"cpu", 1.25}, {"mem", 2.0}, {"io", 0.5}})));
   auto loads = tom::ListPtr<tom::HL>::create("load values");
   loads->push_back(load_value("arch", "lx-amd64", true));
   loads->push_back(load_value("num_proc", "16", true));
   loads->push_back(load_value("load_avg", "0.250000", false));
   loads->push_back(load_value("mem_free", "12.5G", false));
   eh->set<EH_load_list>(pick(variant, std::move(loads)));
   eh->set<EH_processors>(16);
   eh->set<EH_acl>(pick(variant, names<US_name>({"golden_acl"})));
   eh->set<EH_xacl>(pick(variant, names<US_name>({"bad_acl"})));
   eh->set<EH_prj>(pick(variant, names<PR_name>({"golden_prj"})));
   eh->set<EH_xprj>(pick(variant, names<PR_name>({"other_prj"})));
   eh->set<EH_resource_utilization>(pick(variant, pairs<RUE_name, RUE_utilized_now, double>(
                                                          {{"slots", 3.0}, {"mem_free", 1073741824.0}})));
   auto unknown = tom::ListPtr<tom::RU>::create("reschedule unknown");
   const uint32_t ru[][3] = {{4711, 1, 2}, {4712, 3, 1}};
   for (const auto &entry : ru) {
      auto ep = tom::ElementPtr<tom::RU>::create();
      ep->set<RU_job_number>(entry[0]);
      ep->set<RU_task_number>(entry[1]);
      ep->set<RU_state>(entry[2]);
      unknown->push_back(std::move(ep));
   }
   eh->set<EH_reschedule_unknown_list>(pick(variant, std::move(unknown)));
   eh->set<EH_report_variables>(pick(variant, names<STU_name>({"cpu", "mem_free"})));
   eh->set<EH_auto_added_time>(T0 + 3000000);
   return eh.release();
}

/**
 * @brief A queue instance on a given host
 * @param variant shape of the sublists
 * @param host the host
 * @return the object, owned by the caller
 */
lListElem *build_QU_on(const Variant variant, const char *host) {
   auto qu = tom::ElementPtr<tom::QU>::create();
   qu->set<QU_qname>("golden.q");
   qu->set<QU_qhostname>(ocs::tom::Host(host));
   qu->set<QU_seq_no>(7);
   qu->set<QU_load_thresholds>(pick(variant, pairs<CE_name, CE_stringval, const char *>(
                                                     {{"np_load_avg", "1.75"}})));
   qu->set<QU_suspend_thresholds>(pick(variant, pairs<CE_name, CE_stringval, const char *>(
                                                        {{"load_avg", "3"}})));
   qu->set<QU_nsuspend>(2);
   qu->set<QU_suspend_interval>("0:10:0");
   qu->set<QU_priority>("5");
   qu->set<QU_min_cpu_interval>("0:5:0");
   qu->set<QU_processors>("UNDEFINED");
   qu->set<QU_qtype>(static_cast<uint32_t>(BQ | IQ));
   qu->set<QU_ckpt_list>(pick(variant, names<ST_name>({"golden_ckpt"})));
   qu->set<QU_pe_list>(pick(variant, names<ST_name>({"golden_pe", "make"})));
   qu->set<QU_rerun>(true);
   qu->set<QU_job_slots>(8);
   qu->set<QU_tmpdir>("/scratch");
   qu->set<QU_shell>("/bin/bash");
   qu->set<QU_prolog>("/opt/gcs/qprolog.sh");
   qu->set<QU_epilog>("/opt/gcs/qepilog.sh");
   qu->set<QU_shell_start_mode>("unix_behavior");
   qu->set<QU_starter_method>("/opt/gcs/starter.sh");
   qu->set<QU_suspend_method>("SIGTSTP");
   qu->set<QU_resume_method>("SIGCONT");
   qu->set<QU_terminate_method>("/opt/gcs/kill.sh $job_pid");
   qu->set<QU_notify>("0:0:60");
   qu->set<QU_owner_list>(pick(variant, names<US_name>({"qowner"})));
   qu->set<QU_acl>(pick(variant, names<US_name>({"golden_acl"})));
   qu->set<QU_xacl>(pick(variant, names<US_name>({"bad_acl"})));
   auto subordinates = tom::ListPtr<tom::SO>::create("subordinates");
   auto so = tom::ElementPtr<tom::SO>::create();
   so->set<SO_name>("sub.q");
   so->set<SO_threshold>(3);
   subordinates->push_back(std::move(so));
   qu->set<QU_subordinate_list>(pick(variant, std::move(subordinates)));
   qu->set<QU_consumable_config_list>(pick(variant, pairs<CE_name, CE_stringval, const char *>(
                                                            {{"slots", "8"}, {"h_vmem", "16G"}})));
   qu->set<QU_projects>(pick(variant, names<PR_name>({"golden_prj"})));
   qu->set<QU_xprojects>(pick(variant, names<PR_name>({"other_prj"})));
   qu->set<QU_calendar>("golden_cal");
   qu->set<QU_initial_state>("enabled");
   qu->set<QU_s_rt>("1:0:0");
   qu->set<QU_h_rt>("2:0:0");
   qu->set<QU_s_cpu>("0:30:0");
   qu->set<QU_h_cpu>("1:0:0");
   qu->set<QU_s_fsize>("1G");
   qu->set<QU_h_fsize>("2G");
   qu->set<QU_s_data>("3G");
   qu->set<QU_h_data>("4G");
   qu->set<QU_s_stack>("8M");
   qu->set<QU_h_stack>("16M");
   qu->set<QU_s_core>("0");
   qu->set<QU_h_core>("1M");
   qu->set<QU_s_rss>("5G");
   qu->set<QU_h_rss>("6G");
   qu->set<QU_s_vmem>("7G");
   qu->set<QU_h_vmem>("INFINITY");
   qu->set<QU_state>(QI_DISABLED | QI_SUSPENDED);
   qu->set<QU_pending_signal>(15);
   qu->set<QU_pending_signal_delivery_time>(T0 + 4000000);
   qu->set<QU_version>(42);
   return qu.release();
}

/**
 * @brief A queue instance on gexec01
 * @param variant shape of the sublists
 * @return the object, owned by the caller
 */
lListElem *build_QU(const Variant variant) {
   return build_QU_on(variant, "gexec01");
}

/**
 * @brief A cluster queue with a host group and a host override for every attribute
 * @param variant shape of the host list and of the default of every list valued attribute
 * @return the object, owned by the caller
 */
lListElem *build_CQ(const Variant variant) {
   auto cq = tom::ElementPtr<tom::CQ>::create();
   cq->set<CQ_name>("golden.q");
   cq->set<CQ_hostlist>(pick(variant, names<HR_name>({GRP, HOST, "gexec03"})));

   cq->set<CQ_seq_no>(attr<AULNG_href, AULNG_value, uint32_t>(0, 10, 20));
   cq->set<CQ_nsuspend>(attr<AULNG_href, AULNG_value, uint32_t>(1, 2, 3));
   cq->set<CQ_job_slots>(attr<AULNG_href, AULNG_value, uint32_t>(8, 16, 32));
   cq->set<CQ_rerun>(attr<ABOOL_href, ABOOL_value, bool>(false, true, false));
   cq->set<CQ_qtype>(attr<AQTLIST_href, AQTLIST_value, uint32_t>(BQ | IQ, BQ, IQ));

   using S = const char *;
   cq->set<CQ_suspend_interval>(attr<AINTER_href, AINTER_value, S>("00:05:00", "00:10:00", "00:15:00"));
   cq->set<CQ_min_cpu_interval>(attr<AINTER_href, AINTER_value, S>("00:05:00", "00:01:00", "00:02:00"));
   cq->set<CQ_notify>(attr<AINTER_href, AINTER_value, S>("00:00:60", "00:00:30", "00:01:30"));
   cq->set<CQ_priority>(attr<ASTR_href, ASTR_value, S>("0", "5", "-5"));
   cq->set<CQ_processors>(attr<ASTR_href, ASTR_value, S>("UNDEFINED", "0-3", "4-7"));
   cq->set<CQ_tmpdir>(attr<ASTR_href, ASTR_value, S>("/tmp", "/scratch", "/local"));
   cq->set<CQ_shell>(attr<ASTR_href, ASTR_value, S>("/bin/sh", "/bin/bash", "/bin/zsh"));
   cq->set<CQ_calendar>(attr<ASTR_href, ASTR_value, S>("NONE", "golden_cal", "night_cal"));
   cq->set<CQ_prolog>(attr<ASTR_href, ASTR_value, S>("NONE", "/opt/p1.sh", "/opt/p2.sh"));
   cq->set<CQ_epilog>(attr<ASTR_href, ASTR_value, S>("NONE", "/opt/e1.sh", "/opt/e2.sh"));
   cq->set<CQ_shell_start_mode>(attr<ASTR_href, ASTR_value, S>("unix_behavior", "posix_compliant",
                                                                    "script_from_stdin"));
   cq->set<CQ_starter_method>(attr<ASTR_href, ASTR_value, S>("NONE", "/opt/s1.sh", "/opt/s2.sh"));
   cq->set<CQ_suspend_method>(attr<ASTR_href, ASTR_value, S>("NONE", "SIGTSTP", "/opt/susp.sh"));
   cq->set<CQ_resume_method>(attr<ASTR_href, ASTR_value, S>("NONE", "SIGCONT", "/opt/res.sh"));
   cq->set<CQ_terminate_method>(attr<ASTR_href, ASTR_value, S>("NONE", "SIGKILL", "/opt/term.sh"));
   cq->set<CQ_initial_state>(attr<ASTR_href, ASTR_value, S>("default", "enabled", "disabled"));
   cq->set<CQ_s_rt>(attr<ATIME_href, ATIME_value, S>("INFINITY", "1:0:0", "0:30:0"));
   cq->set<CQ_h_rt>(attr<ATIME_href, ATIME_value, S>("INFINITY", "2:0:0", "1:0:0"));
   cq->set<CQ_s_cpu>(attr<ATIME_href, ATIME_value, S>("INFINITY", "3:0:0", "0:10:0"));
   cq->set<CQ_h_cpu>(attr<ATIME_href, ATIME_value, S>("INFINITY", "4:0:0", "0:20:0"));
   cq->set<CQ_s_fsize>(attr<AMEM_href, AMEM_value, S>("INFINITY", "1G", "512M"));
   cq->set<CQ_h_fsize>(attr<AMEM_href, AMEM_value, S>("INFINITY", "2G", "1G"));
   cq->set<CQ_s_data>(attr<AMEM_href, AMEM_value, S>("INFINITY", "3G", "2G"));
   cq->set<CQ_h_data>(attr<AMEM_href, AMEM_value, S>("INFINITY", "4G", "3G"));
   cq->set<CQ_s_stack>(attr<AMEM_href, AMEM_value, S>("INFINITY", "8M", "4M"));
   cq->set<CQ_h_stack>(attr<AMEM_href, AMEM_value, S>("INFINITY", "16M", "8M"));
   cq->set<CQ_s_core>(attr<AMEM_href, AMEM_value, S>("INFINITY", "0", "1K"));
   cq->set<CQ_h_core>(attr<AMEM_href, AMEM_value, S>("INFINITY", "1M", "2K"));
   cq->set<CQ_s_rss>(attr<AMEM_href, AMEM_value, S>("INFINITY", "5G", "4G"));
   cq->set<CQ_h_rss>(attr<AMEM_href, AMEM_value, S>("INFINITY", "6G", "5G"));
   cq->set<CQ_s_vmem>(attr<AMEM_href, AMEM_value, S>("INFINITY", "7G", "6G"));
   cq->set<CQ_h_vmem>(attr<AMEM_href, AMEM_value, S>("INFINITY", "8G", "7G"));

   cq->set<CQ_pe_list>(attr_lists<ASTRLIST_href, ASTRLIST_value>(variant, [](const int i) {
      return names<ST_name>({i == 0 ? "make" : i == 1 ? "golden_pe" : "mpi"});
   }));
   cq->set<CQ_ckpt_list>(attr_lists<ASTRLIST_href, ASTRLIST_value>(variant, [](const int i) {
      return names<ST_name>({i == 0 ? "golden_ckpt" : i == 1 ? "ckpt_a" : "ckpt_b"});
   }));
   cq->set<CQ_owner_list>(attr_lists<AUSRLIST_href, AUSRLIST_value>(variant, [](const int i) {
      return names<US_name>({i == 0 ? "qowner" : i == 1 ? "grp_owner" : "host_owner"});
   }));
   cq->set<CQ_acl>(attr_lists<AUSRLIST_href, AUSRLIST_value>(variant, [](const int i) {
      return names<US_name>({i == 0 ? "golden_acl" : i == 1 ? "grp_acl" : "host_acl"});
   }));
   cq->set<CQ_xacl>(attr_lists<AUSRLIST_href, AUSRLIST_value>(variant, [](const int i) {
      return names<US_name>({i == 0 ? "bad_acl" : i == 1 ? "grp_xacl" : "host_xacl"});
   }));
   cq->set<CQ_projects>(attr_lists<APRJLIST_href, APRJLIST_value>(variant, [](const int i) {
      return names<PR_name>({i == 0 ? "golden_prj" : i == 1 ? "grp_prj" : "host_prj"});
   }));
   cq->set<CQ_xprojects>(attr_lists<APRJLIST_href, APRJLIST_value>(variant, [](const int i) {
      return names<PR_name>({i == 0 ? "other_prj" : i == 1 ? "grp_xprj" : "host_xprj"});
   }));
   cq->set<CQ_load_thresholds>(attr_lists<ACELIST_href, ACELIST_value>(variant, [](const int i) {
      return pairs<CE_name, CE_stringval, const char *>({{"np_load_avg", i == 0 ? "1.75" : i == 1 ? "2.5" : "4"}});
   }));
   cq->set<CQ_suspend_thresholds>(attr_lists<ACELIST_href, ACELIST_value>(variant, [](const int i) {
      return pairs<CE_name, CE_stringval, const char *>({{"load_avg", i == 0 ? "3" : i == 1 ? "6" : "9"}});
   }));
   cq->set<CQ_consumable_config_list>(attr_lists<ACELIST_href, ACELIST_value>(variant, [](const int i) {
      return pairs<CE_name, CE_stringval, const char *>(
         {{"slots", i == 0 ? "8" : i == 1 ? "16" : "32"}, {"h_vmem", i == 0 ? "16G" : "64G"}});
   }));
   cq->set<CQ_subordinate_list>(attr_lists<ASOLIST_href, ASOLIST_value>(variant, [](const int i) {
      auto lp = tom::ListPtr<tom::SO>::create("subordinates");
      auto so = tom::ElementPtr<tom::SO>::create();
      so->set<SO_name>(i == 0 ? "sub.q" : i == 1 ? "grp_sub.q" : "host_sub.q");
      so->set<SO_threshold>(static_cast<uint32_t>(2 + i));
      lp->push_back(std::move(so));
      return lp;
   }));
   return cq.release();
}

/**
 * @brief A resource quota set with three rules over every kind of filter
 * @param name the name of the set
 * @param enabled whether the set is enabled
 * @return the set, as an owning handle
 */
tom::ElementPtr<tom::RQS> rqs(const char *name, const bool enabled) {
   auto rule_set = tom::ElementPtr<tom::RQS>::create();
   rule_set->set<RQS_name>(name);
   rule_set->set<RQS_description>("golden limits");
   rule_set->set<RQS_enabled>(enabled);

   auto rules = tom::ListPtr<tom::RQR>::create("rules");
   auto rule = tom::ElementPtr<tom::RQR>::create();
   rule->set<RQR_name>("per_user");
   rule->set<RQR_filter_users>(rqs_filter(true, {"@golden_acl"}, {"baduser"}));
   rule->set<RQR_filter_projects>(rqs_filter(false, {"golden_prj"}, {}));
   rule->set<RQR_filter_pes>(rqs_filter(false, {"golden_pe"}, {"other_pe"}));
   rule->set<RQR_filter_queues>(rqs_filter(false, {"golden.q"}, {}));
   rule->set<RQR_filter_hosts>(rqs_filter(true, {GRP}, {HOST}));
   rule->set<RQR_limit>(pairs<RQRL_name, RQRL_value, const char *>({{"slots", "10"}, {"h_vmem", "4G"}}));
   rules->push_back(std::move(rule));

   rule = tom::ElementPtr<tom::RQR>::create();
   rule->set<RQR_filter_hosts>(rqs_filter(true, {"*"}, {}));
   rule->set<RQR_limit>(pairs<RQRL_name, RQRL_value, const char *>({{"slots", "$num_proc*2"}}));
   rules->push_back(std::move(rule));

   rule = tom::ElementPtr<tom::RQR>::create();
   rule->set<RQR_name>("arch_limit");
   rule->set<RQR_filter_users>(rqs_filter(false, {"*"}, {}));
   rule->set<RQR_limit>(pairs<RQRL_name, RQRL_value, const char *>({{"arch", "lx-amd64"}}));
   rules->push_back(std::move(rule));

   rule_set->set<RQS_rule>(std::move(rules));
   return rule_set;
}

/**
 * @brief A resource quota set
 * @param variant unused, the rules are the object
 * @return the object, owned by the caller
 */
lListElem *build_RQS([[maybe_unused]] const Variant variant) {
   return rqs("golden_rqs", true).release();
}

/**
 * @brief An advance reservation
 * @param variant shape of the sublists
 * @return the object, owned by the caller
 */
lListElem *build_AR(const Variant variant) {
   auto ar = tom::ElementPtr<tom::AR>::create();
   ar->set<AR_id>(815);
   ar->set<AR_name>("golden_ar");
   ar->set<AR_account>("golden_account");
   ar->set<AR_project>("golden_prj");
   ar->set<AR_owner>("alice");
   ar->set<AR_group>("staff");
   ar->set<AR_submission_time>(T0 + 5000000);
   ar->set<AR_start_time>(T0 + 3600000000);
   ar->set<AR_end_time>(T0 + 7200000000);
   ar->set<AR_duration>(uint64_t{3600000000});
   ar->set<AR_verify>(static_cast<uint32_t>(AR_JUST_VERIFY));
   ar->set<AR_error_handling>(1);
   ar->set<AR_state>(static_cast<uint32_t>(AR_WAITING));
   ar->set<AR_checkpoint_name>("golden_ckpt");
   ar->set<AR_resource_list>(pick(variant, pairs<CE_name, CE_stringval, const char *>(
                                                   {{"h_vmem", "2G"}, {"arch", "lx-amd64"}})));
   ar->set<AR_queue_list>(pick(variant, names<QR_name>({"golden.q", "other.q@gexec01"})));
   auto slots = tom::ListPtr<tom::JG>::create("granted slots");
   slots->push_back(granted("golden.q@gexec01", "gexec01", 4));
   slots->push_back(granted("golden.q@gexec02", HOST, 2));
   ar->set<AR_granted_slots>(pick(variant, std::move(slots)));
   ar->set<AR_mail_options>(MAIL_AT_BEGINNING | MAIL_AT_EXIT);
   auto mail = tom::ListPtr<tom::MR>::create("mail");
   auto mr = tom::ElementPtr<tom::MR>::create();
   mr->set<MR_user>("alice");
   mr->set<MR_host>("mail.example.com");
   mail->push_back(std::move(mr));
   mr = tom::ElementPtr<tom::MR>::create();
   mr->set<MR_user>("bob");
   mail->push_back(std::move(mr));
   ar->set<AR_mail_list>(pick(variant, std::move(mail)));
   ar->set<AR_pe>("golden_pe");
   ar->set<AR_pe_range>(pick(variant, range(2, 8, 2)));
   ar->set<AR_granted_pe>("golden_pe");
   ar->set<AR_master_queue_list>(pick(variant, names<QR_name>({"golden.q@gexec01"})));
   auto acl = tom::ListPtr<tom::ARA>::create("acl");
   auto ara = tom::ElementPtr<tom::ARA>::create();
   ara->set<ARA_name>("alice");
   ara->set<ARA_group>("staff");
   acl->push_back(std::move(ara));
   ara = tom::ElementPtr<tom::ARA>::create();
   ara->set<ARA_name>("@golden_acl");
   acl->push_back(std::move(ara));
   ar->set<AR_acl_list>(pick(variant, std::move(acl)));
   auto xacl = tom::ListPtr<tom::ARA>::create("xacl");
   ara = tom::ElementPtr<tom::ARA>::create();
   ara->set<ARA_name>("baduser");
   ara->set<ARA_group>("badgroup");
   xacl->push_back(std::move(ara));
   ar->set<AR_xacl_list>(pick(variant, std::move(xacl)));
   ar->set<AR_type>(1);
   return ar.release();
}

// ---------------------------------------------------------------------------------------------------------------
// the lists

/**
 * @brief The resource quota sets, as `qconf -srqs` writes them
 * @return the list, owned by the caller
 */
lList *build_RQS_list() {
   auto lp = tom::ListPtr<tom::RQS>::create("rqs");
   lp->push_back(rqs("golden_rqs", true));
   lp->push_back(rqs("disabled_rqs", false));
   return lp.release();
}

/**
 * @brief A complex with an entry of every value type, as `qconf -sc` writes it
 * @return the list, owned by the caller
 */
lList *build_CE_list() {
   using T = ocs::CEntry::Type;
   auto lp = tom::ListPtr<tom::CE>::create("centries");
   lp->push_back(centry("arch", "a", T::RESTR, CMPLXEQ_OP, REQU_YES, CONSUMABLE_NO, "NONE", "0"));
   lp->push_back(centry("calendar", "c", T::STR, CMPLXEQ_OP, REQU_YES, CONSUMABLE_NO, "NONE", "0"));
   lp->push_back(centry("cpu", "cpu", T::DOUBLE, CMPLXGE_OP, REQU_YES, CONSUMABLE_NO, "0", "0"));
   lp->push_back(centry("display_win_gui", "dwg", T::BOOL, CMPLXEQ_OP, REQU_YES, CONSUMABLE_NO, "0", "0"));
   lp->push_back(centry("exclusive", "excl", T::BOOL, CMPLXEXCL_OP, REQU_YES, CONSUMABLE_YES, "0", "1000"));
   lp->push_back(centry("gpu", "gpu", T::RSMAP, CMPLXLE_OP, REQU_YES, CONSUMABLE_HOST, "0", "500"));
   lp->push_back(centry("h_rt", "h_rt", T::TIME, CMPLXLE_OP, REQU_FORCED, CONSUMABLE_NO, "0:0:0", "0"));
   lp->push_back(centry("h_vmem", "h_vmem", T::MEM, CMPLXLE_OP, REQU_YES, CONSUMABLE_JOB, "1G", "100"));
   lp->push_back(centry("hostname", "h", T::HOST, CMPLXEQ_OP, REQU_YES, CONSUMABLE_NO, "NONE", "0"));
   lp->push_back(centry("np_load_avg", "nla", T::DOUBLE, CMPLXGT_OP, REQU_NO, CONSUMABLE_NO, "0", "0"));
   lp->push_back(centry("os", "os", T::CSTR, CMPLXNE_OP, REQU_YES, CONSUMABLE_NO, "NONE", "0"));
   lp->push_back(centry("slots", "s", T::INT, CMPLXLT_OP, REQU_YES, CONSUMABLE_YES, "1", "1000"));
   return lp.release();
}

/**
 * @brief Two job categories, as `qconf -scatl` writes them
 * @return the list, owned by the caller
 */
lList *build_CAT_list() {
   auto lp = tom::ListPtr<tom::CT>::create("categories");
   lp->push_back(tom::ElementPtr<tom::CT>::from_typed(build_CAT(Variant::FULL)));
   auto ct = tom::ElementPtr<tom::CT>::create();
   ct->set<CT_id>(18);
   ct->set<CT_refcount>(1);
   ct->set<CT_str>("-U alice -pe golden_pe 4");
   lp->push_back(std::move(ct));
   return lp.release();
}

/**
 * @brief Two queue instances, as `qconf -sq golden.q@*` writes them in JSON
 * @return the list, owned by the caller
 */
lList *build_QU_list() {
   auto lp = tom::ListPtr<tom::QU>::create("qinstances");
   lp->push_back(tom::ElementPtr<tom::QU>::from_typed(build_QU_on(Variant::FULL, "gexec01")));
   lp->push_back(tom::ElementPtr<tom::QU>::from_typed(build_QU_on(Variant::FULL, HOST)));
   return lp.release();
}

/**
 * @brief The managers, as `qconf -sm -fmt json` writes them
 * @return the list, owned by the caller
 */
lList *build_manager_list() {
   return names<UE_name>({"gadmin", "root"}).release();
}

/**
 * @brief The administrative hosts, as `qconf -sh -fmt json` writes them
 * @return the list, owned by the caller
 */
lList *build_adminhost_list() {
   return names<HR_name>({"gexec01", "gmaster"}).release();
}

// ---------------------------------------------------------------------------------------------------------------
// hooks

/**
 * @brief What the classic backend writes for an execution host: the copy without the dynamic load values
 * @param ep the execution host
 * @return the copy, owned by the caller
 */
lListElem *prepare_EH_spool(const lListElem *ep) {
   return ocs_spool_exechost_static_copy(tom::ConstElement<tom::EH>::from_typed(ep)).release();
}

/**
 * @brief Restore the value type of the resource maps of an execution host read back
 *
 * The reader keeps the ids of a resource map but not its type, which qmaster fills in later
 * from the complex; the writer writes the ids only for a resource map. This does what qmaster
 * does, for the complex values that hold a resource map.
 *
 * @param ep the execution host read back
 */
void fixup_EH(lListElem *ep) {
   const auto eh = tom::Element<tom::EH>::from_typed(ep);
   for (const auto ce : eh.get<EH_consumable_config_list>()) {
      if (!ce.get<CE_resource_map_list>().empty()) {
         ce.set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::RSMAP));
      }
   }
}

// ---------------------------------------------------------------------------------------------------------------
// the tables

/**
 * @brief The layouts of a type that qconf and the classic backend write with one static table
 * @param table the field table
 * @param instr the instruction both use
 * @param parse_values the parse_values argument of the qconf reader
 * @return a layout for qconf (ASCII and JSON) and one for the spool file
 */
std::vector<Layout> static_layouts(spooling_field *table, const spool_flatfile_instr *instr,
                                   const bool parse_values) {
   return {
      {"qconf", {FieldKind::STATIC, table}, instr, F_QCONF | F_JSON, false, parse_values, nullptr, true},
      {"spool", {FieldKind::STATIC, table}, instr, F_SPOOL, false, true, nullptr, true},
   };
}

/**
 * @brief Every object type and every way the product writes it
 *
 * Taken from the field_info table of spool_classic_create_context() (spool files) and from
 * ocs_qconf_objects (qconf show, modify and JSON).
 *
 * @return the cases
 */
std::vector<ObjectCase> object_cases() {
   using K = FieldKind;
   return {
      {"CAL", ocs::tom::CAL::descriptor.e, build_CAL, false, nullptr, static_layouts(CAL_fields, &qconf_sfi, true)},
      {"CK", ocs::tom::CK::descriptor.e, build_CK, false, nullptr, static_layouts(CK_fields, &qconf_sfi, true)},
      {"CE", ocs::tom::CE::descriptor.e, build_CE, false, nullptr, {
         {"qconf", {K::STATIC, CE_fields}, &qconf_ce_sfi, F_QCONF | F_JSON, false, true, nullptr, true},
         {"spool", {K::STATIC, CE_fields}, &qconf_sfi, F_SPOOL, false, true, nullptr, true},
      }},
      {"HGRP", ocs::tom::HGRP::descriptor.e, build_HGRP, true, nullptr, static_layouts(HGRP_fields, &qconf_sfi, true)},
      {"HGRP-adminhosts", ocs::tom::HGRP::descriptor.e, build_HGRP_admin, false, nullptr,
       static_layouts(HGRP_fields, &qconf_sfi, true)},
      {"US", ocs::tom::US::descriptor.e, build_US, true, nullptr, static_layouts(US_fields, &qconf_param_sfi, true)},
      {"US-manager", ocs::tom::US::descriptor.e, build_US_manager, false, nullptr,
       static_layouts(US_fields, &qconf_param_sfi, true)},
      {"PE", ocs::tom::PE::descriptor.e, build_PE, true, nullptr, static_layouts(PE_fields, &qconf_sfi, true)},
      {"RL", ocs::tom::RL::descriptor.e, build_RL, true, nullptr, static_layouts(RL_fields, &qconf_sfi, true)},
      {"CAT", ocs::tom::CT::descriptor.e, build_CAT, false, nullptr, {
         {"qconf", {K::STATIC, CAT_fields}, &qconf_cat_sfi, F_QCONF | F_JSON, false, false, "category", true},
      }},
      // AR_fields is not used by the product (the classic backend packs the reservation), but it is a table
      {"AR", ocs::tom::AR::descriptor.e, build_AR, true, nullptr, {
         {"qconf", {K::STATIC, AR_fields}, &qconf_sfi, F_QCONF | F_JSON, false, true, nullptr, true},
      }},
      {"PR", ocs::tom::PR::descriptor.e, build_PR, true, nullptr, {
         {"qconf", {K::PR, nullptr, false}, &qconf_sfi, F_QCONF | F_JSON, false, true, nullptr, true},
         {"spool", {K::PR, nullptr, true}, &qconf_sfi, F_SPOOL, false, true, nullptr, true},
      }},
      {"UU", ocs::tom::UU::descriptor.e, build_UU, true, nullptr, {
         {"qconf", {K::UU, nullptr, false}, &qconf_sfi, F_QCONF | F_JSON, false, true, nullptr, true},
         {"spool", {K::UU, nullptr, true}, &qconf_sfi, F_SPOOL, false, true, nullptr, true},
      }},
      {"STN", ocs::tom::STN::descriptor.e, build_STN, true, nullptr, {
         {"qconf", {K::STN, nullptr, false, true}, &qconf_name_value_list_sfi, F_QCONF, true, true, nullptr, true},
         {"qconf", {K::STN_JSON}, &qconf_name_value_list_sfi, F_JSON, true, true, "sharetree", true},
         {"spool", {K::STN, nullptr, true, true}, &qconf_name_value_list_sfi, F_SPOOL, false, true, nullptr, true},
      }},
      {"SC", ocs::tom::SC::descriptor.e, build_SC, true, nullptr, {
         {"qconf", {K::STATIC, SC_fields}, &qconf_comma_sfi, F_QCONF | F_JSON, false, true, "sched_conf", true},
         {"spool", {K::SC_SPOOL}, &qconf_sfi, F_SPOOL, false, true, nullptr, true},
      }},
      {"CONF", ocs::tom::CONF::descriptor.e, build_CONF, false, nullptr, {
         {"qconf", {K::CONF, nullptr, false}, &qconf_sfi, F_QCONF | F_JSON, false, false, nullptr, true},
         {"spool", {K::CONF, nullptr, true}, &qconf_sfi, F_SPOOL, false, false, nullptr, true},
      }},
      {"EH", ocs::tom::EH::descriptor.e, build_EH, true, fixup_EH, {
         // -Ae/-Me and the import: no runtime values
         {"qconf", {K::EH, nullptr, false, false, false, false}, &qconf_sfi, F_QCONF | F_JSON, false, true,
          "exechost", true},
         // -se shows the runtime values
         {"show", {K::EH, nullptr, false, true, false, false}, &qconf_sfi, F_QCONF | F_JSON, false, true,
          "exechost", true},
         // -se with MORE_INFO: auto_added_time is a formatted date, which the reader cannot take back
         {"show-more", {K::EH, nullptr, false, true, false, true}, &qconf_sfi, F_QCONF | F_JSON, false, true,
          "exechost", false},
         {"spool", {K::EH, nullptr, true, false, false, false}, &qconf_sfi, F_SPOOL, false, true, nullptr, true,
          prepare_EH_spool},
      }},
      {"QU", ocs::tom::QU::descriptor.e, build_QU, true, nullptr, {
         {"show", {K::QU, nullptr, true, false}, &qconf_sfi, F_QCONF | F_JSON, false, true, "qinstance", true},
         {"spool", {K::QU, nullptr, false, true}, &qconf_sfi, F_SPOOL, false, true, nullptr, true},
      }},
      {"CQ", ocs::tom::CQ::descriptor.e, build_CQ, true, nullptr, {
         {"qconf", {K::CQ, nullptr, true}, &qconf_sfi, F_QCONF | F_JSON, false, false, nullptr, true},
         {"spool", {K::CQ, nullptr, false}, &qconf_sfi, F_SPOOL, false, true, nullptr, true},
      }},
      {"RQS", ocs::tom::RQS::descriptor.e, build_RQS, false, nullptr, {
         {"spool", {K::STATIC, RQS_fields}, &qconf_rqs_sfi, F_SPOOL, false, true, nullptr, true},
      }},
   };
}

/**
 * @brief Every list the product writes as a whole
 * @return the cases
 */
std::vector<ListCase> list_cases() {
   using K = FieldKind;
   return {
      {"RQS-list", ocs::tom::RQS::descriptor.e, build_RQS_list, {K::STATIC, RQS_fields}, &qconf_rqs_sfi, false, true,
       ListJson::PLAIN, nullptr, nullptr, NoName, true},
      {"CE-list", ocs::tom::CE::descriptor.e, build_CE_list, {K::STATIC, CE_fields}, &qconf_ce_list_sfi, true, true,
       ListJson::PLAIN, nullptr, nullptr, NoName, true},
      {"CAT-list", ocs::tom::CT::descriptor.e, build_CAT_list, {K::STATIC, CAT_fields}, &qconf_cat_list_sfi,
       true, false, ListJson::TYPED_EX, "category-list", "category", NoName, false},
      {"QU-list", ocs::tom::QU::descriptor.e, build_QU_list, {K::QU, nullptr, true, false}, nullptr, false, true,
       ListJson::TYPED, "qinstance", nullptr, NoName, false},
      {"MGR-names", ocs::tom::UE::descriptor.e, build_manager_list, {K::STATIC, nullptr}, nullptr, false, true,
       ListJson::NAMES, "manager", nullptr, UE_name, false},
      {"AH-names", ocs::tom::HR::descriptor.e, build_adminhost_list, {K::STATIC, nullptr}, nullptr, false, true,
       ListJson::NAMES, "adminhost", nullptr, HR_name, false},
   };
}

// ---------------------------------------------------------------------------------------------------------------
// the runs

/**
 * @brief Write one object case in every variant, layout and form, and check the read-back
 * @param c the case
 */
void run_object_case(const ObjectCase &c) {
   const std::vector<Variant> variants = c.variants ? std::vector<Variant>{Variant::FULL, Variant::NONE,
                                                                           Variant::EMPTY}
                                                    : std::vector<Variant>{Variant::FULL};
   for (const Variant variant : variants) {
      tom::RawElementPtr ep(c.build(variant));
      for (const Layout &layout : c.layouts) {
         bool owned = false;
         spooling_field *fields = build_fields(layout.source, &owned);
         tom::RawElementPtr prepared(layout.prepare != nullptr ? layout.prepare(ep.get()) : nullptr);
         const lListElem *subject = prepared ? prepared.get() : ep.get();

         for (const Form form : forms_of(layout.forms)) {
            const std::string name = std::string(c.name) + "." + variant_name(variant) + "." + layout.label + "." +
                                     form_ext(form);
            const std::string text = render_object(subject, layout, fields, form, name);
            if (text.empty()) {
               fail(name, "nothing written");
               continue;
            }
            emit(name, text);

            if (g_roundtrip && layout.roundtrip) {
               auto back = parse_object(text, c.descr, layout, fields, form, name);
               if (!back) {
                  fail(name, "cannot be read back");
                  continue;
               }
               if (c.fixup != nullptr) {
                  c.fixup(back.get());
               }
               compare(name, text, render_object(back.get(), layout, fields, form, name));
               back.reset();
            }
         }
         prepared.reset();
         release_fields(fields, owned);
      }
      ep.reset();
   }
}

/**
 * @brief Write one list case in its forms, and check the read-back
 * @param c the case
 */
void run_list_case(const ListCase &c) {
   tom::RawListPtr lp(c.build());
   bool owned = false;
   spooling_field *fields = build_fields(c.source, &owned);

   std::vector<Form> forms;
   if (c.instr != nullptr) {
      forms = forms_of(F_QCONF);
   }
   if (c.json != ListJson::NONE) {
      forms.push_back(Form::JSON);
      forms.push_back(Form::JSON_NUM);
   }

   for (const Form form : forms) {
      const std::string name = std::string(c.name) + "." + form_ext(form);
      const std::string text = render_list(lp.get(), c, fields, form, name);
      if (text.empty()) {
         fail(name, "nothing written");
         continue;
      }
      emit(name, text);

      const bool readable = !select_format(form) || c.json == ListJson::PLAIN;
      if (g_roundtrip && c.roundtrip && readable) {
         auto back = parse_list(text, c, fields, form, name);
         if (!back) {
            fail(name, "cannot be read back");
            continue;
         }
         compare(name, text, render_list(back.get(), c, fields, form, name));
         back.reset();
      }
   }
   release_fields(fields, owned);
   lp.reset();
}

// ---------------------------------------------------------------------------------------------------------------
// classic job and advance reservation spooling

/**
 * @brief A tightly integrated parallel array job with two tasks, the first running a parallel task
 * @return the job
 */
tom::ElementPtr<tom::JB> build_job() {
   static const char script[] = "#!/bin/sh\n#$ -N golden\necho golden\n";

   auto job = tom::ElementPtr<tom::JB>::create();
   job->set<JB_job_number>(4711);
   job->set<JB_job_name>("golden");
   job->set<JB_version>(2);
   job->set<JB_project>("golden_prj");
   job->set<JB_department>("golden_acl");
   job->set<JB_directive_prefix>("#$");
   job->set<JB_exec_file>(EXEC_DIR "/4711");
   job->set<JB_script_file>("golden.sh");
   job->set<JB_script_size>(static_cast<uint32_t>(sizeof(script) - 1));
   job->set<JB_script_ptr>(script);
   job->set<JB_submission_time>(T0 + 6000000);
   job->set<JB_execution_time>(T0 + 7000000);
   job->set<JB_deadline>(T0 + 8000000);
   job->set<JB_owner>("alice");
   job->set<JB_uid>(1001);
   job->set<JB_group>("staff");
   job->set<JB_gid>(100);
   job->set<JB_account>("golden_account");
   job->set<JB_cwd>("/home/alice/work");
   job->set<JB_notify>(true);
   job->set<JB_priority>(1024 - 100);
   job->set<JB_jobshare>(5);
   job->set<JB_shell_list>(pairs<PN_path, PN_host, const char *>({{"/bin/bash", "gexec01"}}));
   job->set<JB_env_list>(pairs<VA_variable, VA_value, const char *>(
                                 {{"PATH", "/usr/bin:/bin"}, {"GOLDEN", "1"}}));
   job->set<JB_context>(pairs<VA_variable, VA_value, const char *>({{"ctx", "golden"}}));
   job->set<JB_job_args>(names<ST_name>({"-v", "input.dat"}));
   job->set<JB_stdout_path_list>(pairs<PN_path, PN_host, const char *>({{"/home/alice/out", "gexec01"}}));
   job->set<JB_stderr_path_list>(pairs<PN_path, PN_host, const char *>({{"/home/alice/err", HOST}}));
   job->set<JB_stdin_path_list>(pairs<PN_path, PN_host, const char *>({{"/dev/null", "gexec01"}}));
   job->set<JB_merge_stderr>(true);

   auto requests = tom::ListPtr<tom::JRS>::create("requests");
   auto request = tom::ElementPtr<tom::JRS>::create();
   request->set<JRS_hard_resource_list>(pairs<CE_name, CE_stringval, const char *>(
                                                {{"h_vmem", "1G"}, {"h_rt", "1:0:0"}}));
   request->set<JRS_soft_resource_list>(pairs<CE_name, CE_stringval, const char *>({{"arch", "lx-amd64"}}));
   request->set<JRS_hard_queue_list>(names<QR_name>({"golden.q"}));
   request->set<JRS_soft_queue_list>(names<QR_name>({"other.q"}));
   requests->push_back(std::move(request));
   job->set<JB_request_set_list>(std::move(requests));

   job->set<JB_mail_options>(MAIL_AT_BEGINNING | MAIL_AT_EXIT);
   auto mail = tom::ListPtr<tom::MR>::create("mail");
   auto mr = tom::ElementPtr<tom::MR>::create();
   mr->set<MR_user>("alice");
   mr->set<MR_host>("mail.example.com");
   mail->push_back(std::move(mr));
   job->set<JB_mail_list>(std::move(mail));
   job->set<JB_pe>("golden_pe");
   job->set<JB_pe_range>(range(2, 4, 1));
   job->set<JB_ja_structure>(range(1, 2, 1));

   auto tasks = tom::ListPtr<tom::JAT>::create("ja_tasks");
   for (uint32_t number = 1; number <= 2; number++) {
      auto task = tom::ElementPtr<tom::JAT>::create();
      task->set<JAT_task_number>(number);
      task->set<JAT_status>(0x80 * number);
      task->set<JAT_state>(0x40 + number);
      task->set<JAT_start_time>(T0 + 9000000 + number);
      task->set<JAT_granted_pe>("golden_pe");
      task->set<JAT_master_queue>("golden.q@gexec01");
      auto destinations = tom::ListPtr<tom::JG>::create("granted");
      destinations->push_back(granted("golden.q@gexec01", "gexec01", 2));
      destinations->push_back(granted("golden.q@gexec02", HOST, 2));
      task->set<JAT_granted_destin_identifier_list>(std::move(destinations));
      task->set<JAT_osjobid>(number == 1 ? "12345" : "12346");
      task->set<JAT_usage_list>(usage(number));
      if (number == 1) {
         auto pe_tasks = tom::ListPtr<tom::PET>::create("pe tasks");
         auto pet = tom::ElementPtr<tom::PET>::create();
         pet->set<PET_id>("1.gexec02");
         pet->set<PET_name>("golden_petask");
         pet->set<PET_status>(0x80);
         pet->set<PET_pid>(23456);
         pet->set<PET_osjobid>("23456");
         pet->set<PET_submission_time>(T0 + 10000000);
         pet->set<PET_start_time>(T0 + 11000000);
         pet->set<PET_cwd>("/home/alice/work");
         pet->set<PET_environment>(pairs<VA_variable, VA_value, const char *>({{"PE_TASK", "1"}}));
         pet->set<PET_usage>(usage(0.25));
         auto pet_destinations = tom::ListPtr<tom::JG>::create("granted");
         pet_destinations->push_back(granted("golden.q@gexec02", HOST, 1));
         pet->set<PET_granted_destin_identifier_list>(std::move(pet_destinations));
         pe_tasks->push_back(std::move(pet));
         task->set<JAT_task_list>(std::move(pe_tasks));
      }
      tasks->push_back(std::move(task));
   }
   job->set<JB_ja_tasks>(std::move(tasks));
   return job;
}

/**
 * @brief The regular files below a directory with their content
 * @param root the directory
 * @return relative path to content; the job scripts are left out, the spool reader does not read them
 */
std::map<std::string, std::string> snapshot(const std::string &root) {
   std::map<std::string, std::string> files;
   for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
      if (!entry.is_regular_file()) {
         continue;
      }
      const std::string rel = std::filesystem::relative(entry.path(), root).string();
      if (rel.compare(0, strlen(EXEC_DIR), EXEC_DIR) == 0) {
         continue;
      }
      std::ifstream in(entry.path(), std::ios::binary);
      std::ostringstream content;
      content << in.rdbuf();
      files[rel] = content.str();
   }
   return files;
}

/**
 * @brief Write the job, its script and the advance reservation as the classic backend does
 * @param job the job
 * @param ar the advance reservation
 * @return true when everything was written
 */
bool write_classic(const tom::Element<tom::JB> job, const tom::ConstElement<tom::AR> ar) {
   bool ok = job_write_spool_file(job, 0, nullptr, SPOOL_DEFAULT) == 0;
   std::filesystem::create_directories(AR_DIR);
   ok = ocs::spool::classic::ar_write_spool_file(ar) && ok;
   return ok;
}

/**
 * @brief Spool a job with array and parallel tasks, its script and an advance reservation, and read them back
 *
 * The files are packed CULL, not flatfile text; they are pinned here because the classic backend
 * spools these types this way. The job is spooled as a directory (one file per task) because
 * SGE_MAX_TASKS_PER_FILE is 1 and the job is tightly parallel, which the master PE list installed
 * here makes it.
 */
void run_classic() {
   const std::string dir = g_outdir + "/classic";
   std::filesystem::create_directories(dir);
   char previous[4096];
   if (getcwd(previous, sizeof(previous)) == nullptr || chdir(dir.c_str()) != 0) {
      fail("classic", "cannot change into the output directory");
      return;
   }

   // the PE of the job, with control_slaves, so that its parallel tasks are spooled as files of their own
   static_cast<void>(ocs::DataStore::replace_master_list<SGE_TYPE_PE>(names<PE_name>({"golden_pe"})));
   ocs::DataStore::master_list_rw<SGE_TYPE_PE>().front().set<PE_control_slaves>(true);

   auto job = build_job();
   auto ar = tom::ElementPtr<tom::AR>::from_typed(build_AR(Variant::FULL));
   if (!write_classic(*job, *ar)) {
      fail("classic", "the job or the advance reservation cannot be spooled");
   }
   std::filesystem::create_directories(EXEC_DIR);
   if (sge_string2file(job->get<JB_script_ptr>(), static_cast<int>(job->get<JB_script_size>()),
                       job->get<JB_exec_file>()) != 0) {
      fail("classic", "the job script cannot be written");
   }
   const auto before = snapshot(".");
   g_files += static_cast<int>(before.size()) + 1;

   if (g_roundtrip) {
      lList *jobs_raw = nullptr;
      job_list_read_from_disk(&jobs_raw, "jobs", 0, SPOOL_DEFAULT, nullptr);
      auto jobs = tom::ListPtr<tom::JB>::from_typed(jobs_raw);
      auto ar_back = ocs::spool::classic::ar_read_spool_file(815);
      tom::Element<tom::JB> job_back;
      if (jobs) {
         job_back = jobs->front();
      }

      if (!job_back || !ar_back) {
         fail("classic", "the job or the advance reservation cannot be read back");
      } else {
         // the directories stay: the job writer remembers the last task directory it created
         for (const auto &[rel, content] : before) {
            std::filesystem::remove(rel);
         }
         if (!write_classic(job_back, *ar_back)) {
            fail("classic", "the job or the advance reservation read back cannot be spooled again");
         }
         const auto after = snapshot(".");
         for (const auto &[rel, content] : before) {
            const auto it = after.find(rel);
            if (it == after.end() || it->second != content) {
               if (it != after.end()) {
                  std::ofstream(rel + ".roundtrip", std::ios::binary) << it->second;
               }
               fail("classic/" + rel, "read back and spooled again it differs or is missing");
            }
            std::ofstream(rel, std::ios::binary | std::ios::trunc) << content;
         }
         for (const auto &[rel, content] : after) {
            if (before.find(rel) == before.end()) {
               fail("classic/" + rel, "spooled again but not there before");
            }
         }
      }
      jobs.reset();
   }

   // the PE list of the data store is freed again
   static_cast<void>(ocs::DataStore::replace_master_list<SGE_TYPE_PE>(tom::ListPtr<tom::PE>()));
   if (chdir(previous) != 0) {
      fail("classic", "cannot change back");
   }
}

// ---------------------------------------------------------------------------------------------------------------
// environment

/**
 * @brief Install a throwaway bootstrap file and point SGE_ROOT/SGE_CELL at it
 *
 * Hashing a host field consults ocs::Bootstrap, which reads $SGE_ROOT/$SGE_CELL/common/bootstrap
 * and exits when it cannot; the values only have to parse (the same trick test_sgeobj_hgroup uses).
 *
 * @param root the directory to create it in
 * @return true when it was created
 */
bool setup_bootstrap(const std::string &root) {
   std::error_code error;
   std::filesystem::create_directories(root + "/default/common", error);
   std::ofstream out(root + "/default/common/bootstrap");
   out << "admin_user        none\n"
          "default_domain    none\n"
          "ignore_fqdn       true\n"
          "spooling_method   classic\n"
          "spooling_lib      none\n"
          "spooling_params   none\n"
          "binary_path       none\n"
          "qmaster_spool_dir none\n"
          "security_mode     none\n";
   if (!out.good()) {
      return false;
   }
   setenv("SGE_ROOT", root.c_str(), 1);
   setenv("SGE_CELL", "default", 1);
   return true;
}

} // namespace

/**
 * @brief Write the golden files and check the read-back
 * @param argc number of arguments
 * @param argv `[--no-roundtrip] [<outdir>]`
 * @return 0 when every check passed, 1 otherwise, 2 on a usage error
 */
int main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_spool_golden");

   std::string outdir;
   for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "--no-roundtrip") == 0) {
         g_roundtrip = false;
      } else if (strcmp(argv[i], "--roundtrip") == 0) {
         g_roundtrip = true;
      } else if (argv[i][0] != '-' && outdir.empty()) {
         outdir = argv[i];
      } else {
         fprintf(stderr, "usage: %s [--no-roundtrip] [<outdir>]\n", argv[0]);
         DRETURN(2);
      }
   }

   char scratch_template[] = "/tmp/test_spool_golden.XXXXXX";
   const char *scratch = mkdtemp(scratch_template);
   if (scratch == nullptr) {
      printf("FAIL - cannot create a temporary directory: %s\n", strerror(errno));
      DRETURN(1);
   }
   const bool temporary_outdir = outdir.empty();
   g_outdir = temporary_outdir ? std::string(scratch) + "/golden" : std::filesystem::absolute(outdir).string();
   std::filesystem::create_directories(g_outdir);

   // fixed time zone: two writers render a time stamp in local time
   setenv("TZ", "UTC", 1);
   tzset();
   // one task per file, so that the array tasks of the classic job get files of their own
   setenv("SGE_MAX_TASKS_PER_FILE", "1", 1);

   if (!setup_bootstrap(std::string(scratch) + "/sge_root")) {
      printf("FAIL - cannot create the throwaway bootstrap\n");
      DRETURN(1);
   }
   // read the bootstrap while errors still reach stderr
   ocs::Bootstrap::get_ignore_fqdn();
   component_set_daemonized(true);
   ocs::tom::install_model(ocs::tom::gcs_model());

   for (const ObjectCase &c : object_cases()) {
      run_object_case(c);
   }
   for (const ListCase &c : list_cases()) {
      run_list_case(c);
   }
   run_classic();
   ocs_json_value_format_opt = OCS_JSON_VALUES_COMPACT;

   std::error_code error;
   std::filesystem::remove_all(scratch, error);

   printf("\n%d file(s) written to %s%s\n", g_files, temporary_outdir ? "a temporary directory" : g_outdir.c_str(),
          g_roundtrip ? ", each read back and written again" : "");
   if (g_roundtrip) {
      for (const auto &known : g_known_deviations) {
         if (!known.seen) {
            // not through fail(), which would take it for the known deviation again
            printf("FAIL  %s: is a known deviation, but the read-back no longer fails; remove it from the list\n",
                   known.what);
            ++g_failures;
         }
      }
   }
   printf("%s - %d failure(s), %d known deviation(s)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures, g_known);
   DRETURN(g_failures == 0 ? 0 : 1);
}
