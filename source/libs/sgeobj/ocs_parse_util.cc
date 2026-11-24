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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Parsing textual lists into lists, and printing them back
 *
 * The two forms users write are a plain list (`a,b,c`) and a definition list
 * (`name=value,name=value`). Both are parsed through an *interpretation rule*:
 * an array of attribute numbers saying which attribute each parsed word goes
 * into, which is what lets one parser fill any element type.
 *
 * @see ocs_parse_util.h
 */

#include <cstdio>
#include <cstring>
#include <sstream>
#include <ostream>
#include <format>
#include <optional>
#include <string>

#include "tom/ocs_Types.h"

#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_string.h"
#include "uti/sge_component.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/ocs_parse_util.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/tom/ocs_tom_RUE.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JRS.h"
#include "sgeobj/tom/ocs_tom_PN.h"
#include "sgeobj/tom/ocs_tom_SPA.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Raw.h"

/**
 * @brief The type of a field of an interpretation rule, as the descriptor holds it
 *
 * The converters of this file take their fields as an `int` table, so the type
 * of each entry is looked up at run time.
 *
 * @param descr the descriptor of the list's elements, or nullptr
 * @param nm the field number from the rule
 * @return the field type, or std::nullopt if @p descr is nullptr or does not hold @p nm (another type's field,
 *         or one a reduced descriptor leaves out)
 */
static std::optional<ocs::tom::FieldType> ocs_parse_field_type(const lDescr *descr, const int nm) {
   const int pos = ocs::tom::present_pos(descr, nm);
   if (pos < 0) {
      return std::nullopt;
   }
   return ocs::tom::field_type_of(descr[pos].mt);
}

/**
 * @brief Whether a field type is printed as a number
 * @param type the field type, or std::nullopt for a field the descriptor does not hold
 * @return true for a double and the 32 bit unsigned, 64 bit signed and 32 bit signed numbers, as
 *         L_IS_NUM_TYPE() decides for the field types
 */
static bool ocs_parse_is_num_type(const std::optional<ocs::tom::FieldType> type) {
   return type == ocs::tom::FieldType::Double || type == ocs::tom::FieldType::Uint32 ||
          type == ocs::tom::FieldType::Int64 || type == ocs::tom::FieldType::Int32;
}

/**
 * @brief Parse an argv style array of strings into a list
 *
 * Each word goes into the field the interpretation rule names at its place;
 * when the rule is used up, a new element is started and the rule starts
 * anew. A string or host word `NONE` (any case) leaves the field unset; a list
 * field of the rule is skipped. The list this array may come from is the one
 * string_list() makes. A nullptr ends the array, so a nullptr cannot stand for
 * a field that stays unset.
 *
 * @param pstrlist the nullptr terminated strings to parse
 * @param listname the name the resulting list is created under
 * @param descr the descriptor of the elements to create
 * @param interpretation_rule the fields the words are parsed into, terminated by 0
 * @param[out] pplist receives the list on success, is left as it is otherwise
 * @return 0 on success; -1 for a nullptr argument, -2 for an empty rule, -3 if the list cannot be created,
 *         -4 or -5 if an element cannot be created, -7, -8, -9 or -11 for a word that is no double, unsigned,
 *         long or int, -12 for a field of another type than these, a string, a host or a list, or one
 *         @p descr does not hold, -13 if the words do not fill the last element
 *
 * @note MT-NOTE: ocs_parse_string_list() is MT safe
 */
int ocs_parse_string_list(char **pstrlist, const char *listname, const lDescr *descr, int *interpretation_rule,
                           lList **pplist) {
   DENTER(BASIS_LAYER);

   if (!pstrlist || !descr || !interpretation_rule || !pplist) {
      DPRINTF("ocs_parse_string_list: nullptr pointer received\n");
      DRETURN(-1);
   }
   if (*interpretation_rule == 0) {
      DPRINTF("ocs_parse_string_list: zero interpretation rule\n");
      DRETURN(-2);
   }
   // the creations report a failure by an exception, the codes of the former nullptr results are kept
   ocs::tom::RawListPtr list;
   try {
      list = ocs::tom::create_raw_list(descr, ocs::tom::Hashing::On, listname);
   } catch (const ocs::tom::FieldException &) {
      // list stays empty
   } catch (const ocs::tom::ListCoreError &) {
      // list stays empty
   }
   if (!list) {
      DPRINTF("ocs_parse_string_list: cannot create list\n");
      DRETURN(-3);
   }
   ocs::tom::RawElement ep;
   try {
      ep = list->push_back(ocs::tom::create_raw_elem(descr));
   } catch (const ocs::tom::FieldException &) {
      ep = nullptr;
   } catch (const ocs::tom::ListCoreError &) {
      ep = nullptr;
   }
   if (!ep) {
      DPRINTF("ocs_parse_string_list: cannot create element\n");
      DRETURN(-4);
   }
   /*
   ** process the string list till it ends
   */
   int *rule = interpretation_rule;
   for (; *pstrlist; pstrlist++) {
      if (*rule == 0) {
         rule = interpretation_rule;
         try {
            ep = list->push_back(ocs::tom::create_raw_elem(descr));
         } catch (const ocs::tom::FieldException &) {
            ep = nullptr;
         } catch (const ocs::tom::ListCoreError &) {
            ep = nullptr;
         }
         if (!ep) {
            DPRINTF("ocs_parse_string_list: cannot create another element\n");
            DRETURN(-5);
         }
      }
      // the element holds every field of descr, and the case is the field's type: no access below throws a
      // FieldException; setting a string or a host copies it and throws OutOfMemory if that fails (not caught)
      const std::optional<ocs::tom::FieldType> type = ocs_parse_field_type(descr, *rule);
      switch (type.value_or(ocs::tom::FieldType::Ref)) {
         /*
         ** the format specifiers were copied from the old list library
         ** where similar things happen
         */
         case ocs::tom::FieldType::Double: {
            double dd;

            if (sscanf(*pstrlist, "%99lg", &dd) != 1) {
               DPRINTF("ocs_parse_string_list: error interpreting double: %s\n", *pstrlist);
               DRETURN(-7);
            }
            ep.set(*rule, dd);
         } break;

         case ocs::tom::FieldType::Uint32: {
            std::uint32_t ul;

            if (sscanf(*pstrlist, sge_u32, &ul) != 1) {
               DPRINTF("ocs_parse_string_list: error interpreting ulong: %s\n", *pstrlist);
               DRETURN(-8);
            }
            ep.set(*rule, ul);
         } break;

         case ocs::tom::FieldType::Int64: {
            long l;

            if (sscanf(*pstrlist, "%ld", &l) != 1) {
               DPRINTF("ocs_parse_string_list: error interpreting long: %s\n", *pstrlist);
               DRETURN(-9);
            }
            ep.set(*rule, static_cast<std::int64_t>(l));
         } break;

         case ocs::tom::FieldType::Int32: {
            std::int32_t i;

            if (sscanf(*pstrlist, "%d", &i) != 1) {
               DPRINTF("ocs_parse_string_list: error interpreting int: %s\n", *pstrlist);
               DRETURN(-11);
            }
            ep.set(*rule, i);
         } break;

         case ocs::tom::FieldType::String:
            if (strcasecmp("NONE", *pstrlist) != 0) {
               ep.set<const char *>(*rule, *pstrlist);
            }
            break;

         case ocs::tom::FieldType::Host:
            if (strcasecmp("NONE", *pstrlist) != 0) {
               ep.set(*rule, ocs::tom::Host(*pstrlist));
            }
            break;

         case ocs::tom::FieldType::List:
            /*
            ** list types are skipped at the moment
            */
            DPRINTF("skipped list type");
            break;

         default:
            // a 64 bit unsigned, bool, object or reference field, or one descr does not hold (type MT_END)
            DPRINTF("encountered unknown list field type %d\n", ocs::tom::field_type(descr, *rule));
            DRETURN(-12);

      } /* end switch */
      rule++;
   } /* end for */

   if (*rule != 0) {
      DPRINTF("invalid number of entries specified\n");
      DRETURN(-13);
   }
   *pplist = list.release();
   DPRINTF("list parsed: \n");

   DRETURN(0);
}


/*
** NAME
**   ocs_parse_definition_list
** PARAMETER
**   str                 - string to be parsed
**   lpp                 - list pointer-pointer to be set the list,
**                         or nullptr if first token is NONE
**   name                - name for the list
**   descr               - type of the list
**   interpretation_rule - fields to be filled, normally 2 entries
** RETURN
**
** EXTERNAL
**
** DESCRIPTION
**   parses list of form var=value,var=value,...
*/
/**
 * @brief Parse a `name=value,name=value` string into a list
 *
 * @param str the text to parse; it is modified in place by the tokenizer
 * @param[out] lpp receives the list
 * @param name the name the resulting list is created under
 * @param descr the descriptor of the elements to create
 * @param interpretation_rule the attributes the name and the value are stored in
 * @return 0 on success
 */
int ocs_parse_definition_list(
char *str,
lList **lpp,
const char *name,
const lDescr *descr,
int *interpretation_rule 
) {
   DENTER(BASIS_LAYER);

   char **pstr;
   int ret;

   if (!str || !lpp) {
      DRETURN(-1);
   }
   pstr = string_list(str, " ,=\t\n\f\r\v", nullptr);
   if (!pstr) {
      DRETURN(-2);
   }
   if (!strcasecmp("NONE", pstr[0]) || !strcasecmp("UNDEFINED", pstr[0])) {
      *lpp = nullptr;
      sge_free(&pstr);
      DRETURN(0);
   }
   ret = ocs_parse_string_list(pstr, name, descr, interpretation_rule, lpp);
   sge_free(&pstr);
   if (ret) {
      DRETURN(-3);
   }
   DRETURN(0);
}


/**
 * @brief Collapse repeated entries of a definition list
 *
 * Removes duplicate entries within a definition list, e.g. `a=1,a=2` becomes
 * `a=2`: the last entry for a key wins, which is what a user writing the same
 * option twice expects. An element is compared with every element before it,
 * and an earlier one with the same key is removed.
 *
 * @param[in,out] lp the list to compress, or an empty handle
 * @param nm_var the field holding an entry's name, of any type but a list
 * @param nm_value the field holding its value, compared only with @p double_keys for a string or host name
 * @param double_keys non-zero when the key is the name and the value together
 * @return 0 on success, -4 for a list field @p nm_var, -5 for a 64 bit unsigned, bool, object or reference field
 *         or one the list does not hold; the errors are found only for a list of two elements or more
 */
int ocs_compress_definition_list(const ocs::tom::RawList lp, const int nm_var, const int nm_value,
                                  const int double_keys) {
   DENTER(BASIS_LAYER);

   // the elements of lp share its descriptor, which holds nm_var with this type: the reads of nm_var do not throw
   // (they only read, nothing is copied)
   const std::optional<ocs::tom::FieldType> type = ocs_parse_field_type(lp.descr(), nm_var);

   for (const auto ep_one : lp) {
      // the iterator steps on before ep_other may be erased, erase() of the current element would invalidate it
      for (auto it = lp.begin(); it != lp.end();) {
         const ocs::tom::RawElement ep_other = *it;
         if (ep_one.raw() == ep_other.raw()) {
            break;
         }
         ++it;
         bool is_there = false;
         switch (type.value_or(ocs::tom::FieldType::Ref)) {
            case ocs::tom::FieldType::Double:
               is_there = ep_one.get<double>(nm_var) == ep_other.get<double>(nm_var);
               break;
            case ocs::tom::FieldType::Uint32:
               is_there = ep_one.get<std::uint32_t>(nm_var) == ep_other.get<std::uint32_t>(nm_var);
               break;
            case ocs::tom::FieldType::Int64:
               is_there = ep_one.get<std::int64_t>(nm_var) == ep_other.get<std::int64_t>(nm_var);
               break;
            case ocs::tom::FieldType::Int32:
               is_there = ep_one.get<std::int32_t>(nm_var) == ep_other.get<std::int32_t>(nm_var);
               break;
            case ocs::tom::FieldType::String:
               if (double_keys) {
                  // nm_value is read as a string as well; another type throws WrongType, where lGetString() aborted
                  is_there = !sge_strnullcmp(ep_one.get<const char *>(nm_var), ep_other.get<const char *>(nm_var)) &&
                             !sge_strnullcmp(ep_one.get<const char *>(nm_value),
                                             ep_other.get<const char *>(nm_value));
               } else {
                  is_there = ep_one.get<const char *>(nm_var) != nullptr &&
                             !sge_strnullcmp(ep_one.get<const char *>(nm_var), ep_other.get<const char *>(nm_var));
               }
               break;
            case ocs::tom::FieldType::Host:
               if (double_keys) {
                  is_there = !sge_strnullcmp(ep_one.get<ocs::tom::Host>(nm_var).c_str(),
                                             ep_other.get<ocs::tom::Host>(nm_var).c_str()) &&
                             !sge_strnullcmp(ep_one.get<ocs::tom::Host>(nm_value).c_str(),
                                             ep_other.get<ocs::tom::Host>(nm_value).c_str());
               } else {
                  is_there = ep_one.get<ocs::tom::Host>(nm_var).c_str() != nullptr &&
                             !sge_strnullcmp(ep_one.get<ocs::tom::Host>(nm_var).c_str(),
                                             ep_other.get<ocs::tom::Host>(nm_var).c_str());
               }
               break;

            case ocs::tom::FieldType::List:
               DPRINTF(("ocs_compress_definition_list: " \
                  "list type not implemented with this function\n"));
               DRETURN(-4);
            default:
               DPRINTF("ocs_compress_definition_list: invalid type\n");
               DRETURN(-5);
         } /* end switch */

         // the element that comes later in the list is kept, because ep_other is always before ep_one
         if (is_there) {
            lp.erase(ep_other);
         }
      }
   }
   DRETURN(0);
}



/**
 * @brief Print selected fields of a list into a text stream.
 *
 * This function serializes the elements of an @c lList into a textual
 * representation and writes the result to the provided @c std::ostream.
 * The fields to be printed and their order are defined by
 * @p which_elements_rule.
 *
 * For each element in the list, the specified fields are converted to
 * their string representation and concatenated using delimiters from
 * @p pdelis.
 *
 * @param os Output stream where the formatted list is written.
 * @param lp The list to be printed, of any type. If an empty handle, the string "NONE" is written.
 * @param which_elements_rule Array of field identifiers (terminated by 0)
 *           that defines which elements of each list entry are printed
 *           and in which order.
 * @param pdelis Array of delimiter strings:
 *        - pdelis[0]: delimiter between fields within one list element
 *        - pdelis[1]: delimiter between list elements
 *        - pdelis[2]: delimiter appended after the entire list
 * @param flags Formatting flags controlling delimiter suppression and
 *        handling of empty values:
 *        - FLG_NO_VALUE_AS_EMPTY: print empty string instead of "NONE"
 *        - FLG_NO_DELIS_STRINGS: suppress delimiters for empty string fields
 *        - FLG_NO_DELIS_NUMBERS: suppress delimiters for numeric zero values
 *
 * @return 0 on success,
 *         negative value on error:
 *         - -1: invalid input parameters
 *         - -2: empty or invalid rule set
 *         - -3: list descriptor missing
 *         - -4/-5/-6: stream write error
 *         - -19: unknown field type encountered
 *
 * @note List-type fields (@c MT_LIST) are currently skipped.
 *
 * @note Output is written directly to the stream without size limitations.
 *       Stream state should be checked by the caller if needed.
 */
int uni_print_list(std::ostream &os, const ocs::tom::ConstRawList lp, const int *which_elements_rule,
                   const char *pdelis[], const unsigned long flags) {
   DENTER(BASIS_LAYER);

   const int* rule;
   const lDescr* descr;
   bool begin = true;
   std::string field;
   const char* cp = nullptr;
   std::size_t prev_len = 0;

   if (!which_elements_rule || !pdelis) {
      DPRINTF("uni_print_list: nullptr pointer received\n");
      DRETURN(-1);
   }

   if (!lp) {
      os << "NONE";
      if (!os) {
         DPRINTF("uni_print_list: error writing NONE\n");
         DRETURN(-6);
      }

      if (pdelis[2] && *pdelis[2]) {
         os << pdelis[2];
         if (!os) {
            DPRINTF("uni_print_list: error writing delimiter 2\n");
            DRETURN(-4);
         }
      }
      DRETURN(0);
   }

   if (*which_elements_rule == 0) {
      DPRINTF("uni_print_list: zero interpretation rule\n");
      DRETURN(-2);
   }

   descr = lp.descr();
   if (!descr) {
      DPRINTF("uni_print_list: list has no descriptor\n");
      DRETURN(-3);
   }

   // the elements of lp share its descriptor, and the case is the field's type there: no read below throws
   for (const auto ep : lp) {
      if (!begin && pdelis[1] && *pdelis[1]) {
         os << pdelis[1];
         if (!os) {
            DPRINTF("uni_print_list: error writing delimiter 1\n");
            DRETURN(-4);
         }
      }

      prev_len = 0;

      for (rule = which_elements_rule; *rule; ++rule) {
         field.clear();
         cp = nullptr;
         const std::optional<ocs::tom::FieldType> type = ocs_parse_field_type(descr, *rule);

         switch (type.value_or(ocs::tom::FieldType::Ref)) {
            case ocs::tom::FieldType::Double:
               field = std::format("{:.10g}", ep.get<double>(*rule));
               cp = field.c_str();
               break;

            case ocs::tom::FieldType::Uint32:
               field = std::format("{}", ep.get<std::uint32_t>(*rule));
               cp = field.c_str();
               break;

            case ocs::tom::FieldType::Int64:
               field = std::format("{}", ep.get<std::int64_t>(*rule));
               cp = field.c_str();
               break;

            case ocs::tom::FieldType::Int32:
               field = std::format("{}", ep.get<std::int32_t>(*rule));
               cp = field.c_str();
               break;

            case ocs::tom::FieldType::String:
               cp = ep.get<const char *>(*rule);
               if (!cp) {
                  cp = (flags & FLG_NO_VALUE_AS_EMPTY) ? "" : "NONE";
               }
               break;

            case ocs::tom::FieldType::Host:
               cp = ep.get<ocs::tom::Host>(*rule).c_str();
               if (!cp) {
                  cp = (flags & FLG_NO_VALUE_AS_EMPTY) ? "" : "NONE";
               }
               break;

            case ocs::tom::FieldType::List:
               DPRINTF("skipped list type");
               cp = "";
               break;

            default:
               // a 64 bit unsigned, bool, object or reference field, or one descr does not hold (type MT_END)
               DPRINTF("encountered unknown list field type %d\n", ocs::tom::field_type(descr, *rule));
               DRETURN(-19);
         }

         if (pdelis[0] && *pdelis[0] &&
             (rule != which_elements_rule) &&
             (!(flags & FLG_NO_DELIS_STRINGS) || ((*cp && prev_len > 0) || (flags & FLG_NO_VALUE_AS_EMPTY))) &&
             (!(flags & FLG_NO_DELIS_NUMBERS) || !ocs_parse_is_num_type(type) || std::strcmp(cp, "0"))) {
            os << pdelis[0];
            if (!os) {
               DPRINTF("uni_print_list: error writing delimiter\n");
               DRETURN(-5);
            }
         }

         if (*cp) {
            os << cp;
            if (!os) {
               DPRINTF("uni_print_list: error writing to stream\n");
               DRETURN(-6);
            }
         }

         prev_len = std::strlen(cp);
         begin = false;
      }
   }

   if (!begin && pdelis[2] && *pdelis[2]) {
      os << pdelis[2];
      if (!os) {
         DPRINTF("uni_print_list: error writing delimiter 2\n");
         DRETURN(-4);
      }
   }

   DRETURN(0);
}

/**
 * @brief Legacy wrapper for uni_print_list(std::ostream&, ...).
 *
 * This function provides backward-compatible output to either a C-style
 * @c FILE pointer or a character buffer. Internally, it delegates formatting
 * to the @c std::ostream-based implementation and copies the result to the
 * requested output target.
 *
 * @param fp Output file pointer. If non-null, the result is written via @c fprintf.
 * @param buff Output buffer. Used if @p fp is null.
 * @param buff_size Size of @p buff in bytes.
 * @param lp The list to be printed, of any type.
 * @param which_elements_rule Field selection rule (see ostream overload).
 * @param pdelis Delimiters (see ostream overload).
 * @param flags Formatting flags (see ostream overload).
 * @return Same as uni_print_list(std::ostream&, ...).
 *
 * @see uni_print_list(std::ostream&, ocs::tom::ConstRawList, const int*, const char*[], unsigned long)
 *
 * @note If @p buff is used, the output must fit into @p buff_size, otherwise an error is returned.
 */
int uni_print_list(FILE *fp, char *buff, const uint32_t buff_size, const ocs::tom::ConstRawList lp,
                   const int *which_elements_rule, const char *pdelis[], const unsigned long flags) {
   DENTER(BASIS_LAYER);

   std::ostringstream ss;
   const int ret = uni_print_list(ss, lp, which_elements_rule, pdelis, flags);
   if (ret != 0) {
      DRETURN(ret);
   }

   const std::string out = ss.str();

   if (fp) {
      if (fprintf(fp, "%s", out.c_str()) < 0) {
         DRETURN(-7);
      }
   } else {
      if (!buff || buff_size == 0) {
         DRETURN(-1);
      }
      if (out.size() + 1 > buff_size) {
         DPRINTF("max_len too small\n");
         DRETURN(-1);
      }
      std::memcpy(buff, out.c_str(), out.size() + 1);
   }

   DRETURN(0);
}


/**
 * @brief Parse a switch that appears in both a hard and a soft form
 *
 * `-l` and `-q` may be written before or after `-soft`, which decides whether
 * the request has to be satisfied or only should be. This sorts the parsed
 * entries into the two attributes accordingly. An entry given in several
 * switches is merged by the rule of its type (#ocs_object_list_add_unique): a
 * queue reference is kept once, a resource request keeps its last value.
 *
 * @tparam Hard the request set attribute the hard requests go into
 * @tparam Soft the request set attribute the soft requests go into, NoName if the switch has no soft form
 * @param cmdline the parsed option list to consume from
 * @param option the switch to look for
 * @param[in,out] job the job the requests are stored in
 * @param scope which request set the entries belong to
 */
template <auto Hard, auto Soft>
   requires ocs::tom::FieldTraits<Hard>::has_sub
void parse_list_hardsoft(const ocs::tom::List<ocs::tom::SPA> cmdline, const char *option,
                         const ocs::tom::Element<ocs::tom::JB> job, const uint32_t scope) {
   DENTER(TOP_LAYER);
   constexpr bool has_soft = ocs::tom::TypedField<decltype(Soft)>;
   using T = typename ocs::tom::FieldTraits<Hard>::Sub;
   bool handled_option = false;
   // the merge below works on raw lists, ocs_object_list_add_unique() has no typed form: the copies are handed
   // over raw and taken back typed when they are stored
   lList *hard_list = nullptr;
   lList *soft_list = nullptr;

   auto jrs = job_get_request_setRW(job, scope);
   if (jrs != nullptr) {
      hard_list = jrs.get<Hard>().clone().release();
      if constexpr (has_soft) {
         soft_list = jrs.get<Soft>().clone().release();
      }
   }

   const auto options = cmdline.find_all<SPA_switch_val>(option);
   for (auto next_ep = options.begin(); next_ep != options.end();) {
      const auto ep = *next_ep;
      // the next match is looked up before the current one is erased
      ++next_ep;

      if (int(ep.get<SPA_argval_lLongT>()) == int(scope)) {
         // the argument list is of the type of the request field, so it stays untyped
         lList *lp = ep.exchange<SPA_argval_lListT>(ocs::tom::RawListPtr()).release();
         if (lp != nullptr) {
            // merged without ever holding a key twice (N-H-18)
            if (!has_soft || ep.get<SPA_argval_lIntT>() < 2) {
               ocs_object_list_add_unique(&hard_list, &lp);
            } else {
               ocs_object_list_add_unique(&soft_list, &lp);
            }
         }
         cmdline.erase(ep);
         handled_option = true;
      }
   }

   // now store back the request lists to the job
   if (hard_list != nullptr || soft_list != nullptr) {
      // create the job_request_set if necessary
      if (jrs == nullptr) {
         jrs = job_get_or_create_request_setRW(job, scope);
      }
      jrs.set<Hard>(ocs::tom::ListPtr<T>::from_typed(hard_list));
      if constexpr (has_soft) {
         jrs.set<Soft>(ocs::tom::ListPtr<T>::from_typed(soft_list));
      }
   }

   // in case of masterq: the global -q requests become the slave -q requests
   if (handled_option && sge_strnullcmp(option, "-masterq") == 0) {
      const auto global = job.get<JB_request_set_list>().find_first<JRS_scope>(JRS_SCOPE_GLOBAL);
      if (global != nullptr) {
         job_set_hard_queue_list(job, global.exchange<JRS_hard_queue_list>(nullptr), JRS_SCOPE_SLAVE);
         // also swap the soft requests, should there be any
         // this will then trigger an error as we do not support soft slave queue requests
         job_set_soft_queue_list(job, global.exchange<JRS_soft_queue_list>(nullptr), JRS_SCOPE_SLAVE);
      }
   }

   DRETURN_VOID;
}

/// @cond explicit instantiations for the switches in use
template void parse_list_hardsoft<JRS_hard_resource_list, JRS_soft_resource_list>(
   ocs::tom::List<ocs::tom::SPA> cmdline, const char *option, ocs::tom::Element<ocs::tom::JB> job, uint32_t scope);
template void parse_list_hardsoft<JRS_hard_queue_list, JRS_soft_queue_list>(
   ocs::tom::List<ocs::tom::SPA> cmdline, const char *option, ocs::tom::Element<ocs::tom::JB> job, uint32_t scope);
template void parse_list_hardsoft<JRS_hard_queue_list, NoName>(
   ocs::tom::List<ocs::tom::SPA> cmdline, const char *option, ocs::tom::Element<ocs::tom::JB> job, uint32_t scope);
/// @endcond


/**
 * @brief Parse a path list
 *
 * Parse a path list of the format: [[host]:]path[,[[host]:]path...]
 *
 * @param path_str input string
 *
 * @return error code 0 = okay 1 = error, and the parsed list (`PN`), an empty handle on an error
 *
 * @note MT-NOTE: ocs_parse_path_list() is MT safe
 */
std::pair<int, ocs::tom::ListPtr<ocs::tom::PN>> ocs_parse_path_list(const char *path_str) {
   DENTER(TOP_LAYER);

   char **str_str = nullptr;
   ocs::tom::ListPtr<ocs::tom::PN> list;

   char *path_string = sge_strdup(nullptr, path_str);
   bool ret_error = path_string == nullptr;
   if (!ret_error) {
      str_str = string_list(path_string, ",", nullptr);
      ret_error = str_str == nullptr || *str_str == nullptr;
   }

   if (!ret_error) {
      list = ocs::tom::ListPtr<ocs::tom::PN>::create("path_list");
      for (char **pstr = str_str; *pstr; pstr++) {
         char *path;
         char *cell;

         /* cell given ? */
         if (*pstr[0] == ':') {  /* :path */
            cell = nullptr;
            path = *pstr + 1;
         } else if ((path = strstr(*pstr, ":"))) { /* host:path */
            path[0] = '\0';
            cell = strdup(*pstr);
            path[0] = ':';
            path += 1;
         } else { /* path */
            cell = nullptr;
            path = *pstr;
         }

         const auto ep = list->push_back(ocs::tom::ElementPtr<ocs::tom::PN>::create());
         ep.set<PN_path>(path);
         if (cell) {
            ep.set<PN_host>(ocs::tom::Host(cell));
            sge_free(&cell);
         }
      }
   }
   if (path_string) {
      sge_free(&path_string);
   }
   if (str_str) {
      sge_free(&str_str);
   }
   DRETURN(std::make_pair(ret_error ? 1 : 0, std::move(list)));
}

/**
 * @brief Parse a jid list
 *
 * parse a jid list of the format jid[,jid,...]
 *
 * @param str input string to be parsed
 *
 * @return 0 on success, 2 when the string holds no job id, 3 when it could not be parsed; and the
 *         `ST` result list, an empty handle on an error
 *
 * @note MT-NOTE: ocs_parse_jid_hold_list() is MT safe
 */
std::pair<int, ocs::tom::ListPtr<ocs::tom::ST>> ocs_parse_jid_hold_list(const char *str) {
   DENTER(TOP_LAYER);

   int rule[] = {ST_name, 0};

   char *s = sge_strdup(nullptr, str);
   if (!s) {
      DRETURN(std::make_pair(3, ocs::tom::ListPtr<ocs::tom::ST>()));
   }
   char **str_str = string_list(s, ",", nullptr);
   if (!str_str || !*str_str) {
      sge_free(&s);
      DRETURN(std::make_pair(2, ocs::tom::ListPtr<ocs::tom::ST>()));
   }
   // ocs_parse_string_list() is generic over the descriptor; it fills the list only on success
   lList *parsed = nullptr;
   const int i_ret = ocs_parse_string_list(str_str, "jid_hold list", ocs::tom::ST::descriptor.e, rule, &parsed);
   // the list was created with ST above, so it is taken over unchecked
   auto list = ocs::tom::ListPtr<ocs::tom::ST>::from_typed(parsed);

   sge_free(&s);
   sge_free(&str_str);
   DRETURN(std::make_pair(i_ret != 0 ? 3 : 0, std::move(list)));
}

/**
 * @brief Parse -h switch of qsub and qalter
 *
 * Parse the hold flags of -h switches which can be used with
 * qaub and qalter
 *
 * @param hold_str string tobe parsed
 * @param prog_number program number
 *
 * @return hold state -1 in case of error
 *
 * @note MT-NOTE: sge_parse_hold_list() is MT safe
 */
int 
sge_parse_hold_list(const char *hold_str, uint32_t prog_number) {
   DENTER(TOP_LAYER);

   int i, j;
   int target = 0;
   int op_code = 0;

   i = strlen(hold_str);

   for (j = 0; j < i; j++) {
      switch (hold_str[j]) {
      case 'n':
         if ((prog_number == QHOLD)  || 
             (prog_number == QRLS) || 
             (op_code && op_code != MINUS_H_CMD_SUB)) {
            target = -1;
            break;
         }
         op_code = MINUS_H_CMD_SUB;
         target = MINUS_H_TGT_USER|MINUS_H_TGT_OPERATOR|MINUS_H_TGT_SYSTEM;
         break;
      case 's':
         if (prog_number == QRLS) {
            if (op_code && op_code != MINUS_H_CMD_SUB) {
               target = -1;
               break;
            }
            op_code = MINUS_H_CMD_SUB;
            target = target|MINUS_H_TGT_SYSTEM;         
         }
         else {
            if (op_code && op_code != MINUS_H_CMD_ADD) {
               target = -1;
               break;
            }
            op_code = MINUS_H_CMD_ADD;
            target = target|MINUS_H_TGT_SYSTEM;
         }   
         break;
      case 'o':
         if (prog_number == QRLS) {
            if (op_code && op_code != MINUS_H_CMD_SUB) {
               target = -1;
               break;
            }
            op_code = MINUS_H_CMD_SUB;
            target = target|MINUS_H_TGT_OPERATOR;         
         }
         else {
            if (op_code && op_code != MINUS_H_CMD_ADD) {
               target = -1;
               break;
            }
            op_code = MINUS_H_CMD_ADD;
            target = target|MINUS_H_TGT_OPERATOR;
         }
         break;
         
      case 'u':
         if (prog_number == QRLS) {
            if (op_code && op_code != MINUS_H_CMD_SUB) {
               target = -1;
               break;
            }
            op_code = MINUS_H_CMD_SUB;
            target = target|MINUS_H_TGT_USER;
         }
         else {
            if (op_code && op_code != MINUS_H_CMD_ADD) {
               target = -1;
               break;
            }
            op_code = MINUS_H_CMD_ADD;
            target = target|MINUS_H_TGT_USER;
         }
         break;
      case 'S':
         if ((prog_number == QHOLD)  || 
             (prog_number == QRLS) || 
             (op_code && op_code != MINUS_H_CMD_SUB)) {
            target = -1;
            break;
         }
         op_code = MINUS_H_CMD_SUB;
         target = target|MINUS_H_TGT_SYSTEM;
         break;
      case 'U':
         if ((prog_number == QHOLD)  || 
             (prog_number == QRLS) || 
             (op_code && op_code != MINUS_H_CMD_SUB)) {
            target = -1;
            break;
         }
         op_code = MINUS_H_CMD_SUB;
         target = target|MINUS_H_TGT_USER;
         break;
      case 'O':
         if ((prog_number == QHOLD)  || 
             (prog_number == QRLS) || 
             (op_code && op_code != MINUS_H_CMD_SUB)) {
            target = -1;
            break;
         }
         op_code = MINUS_H_CMD_SUB;
         target = target|MINUS_H_TGT_OPERATOR;
         break;
      default:
         target = -1;
      }

      if (target == -1)
         break;
   }

   if (target != -1)
      target |= op_code;

   DRETURN(target);
}

