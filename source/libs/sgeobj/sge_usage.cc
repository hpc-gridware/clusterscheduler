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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Resource usage: reading, writing and scaling usage attributes
 *
 * Usage is stored as a list of name/value pairs, with the value always a
 * double; the typed accessors convert on read and write. A custom attribute
 * arriving through the shepherd may instead carry a string - see
 * #usage_parse_value.
 *
 * @see sge_usage.h
 */

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

#include "tom/ocs_Types.h"

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_usage.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/tom/ocs_tom_HS.h"
#include "sgeobj/tom/ocs_tom_UA.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

/**
 * @brief Return a usage value as int
 *
 * Searches a usage object with the given name in the given usage
 * list. If such an element is found, returns the value of the
 * usage object as int value.
 * If no such element is found, return the given default value.
 *
 * @param usage_list the usage list (UA), may be an empty handle
 * @param name name of the element to search
 * @param def default value
 *
 * @return value of found object or default
 *
 * @see #usage_list_get_double_usage
 */
int
usage_list_get_int_usage(const ocs::tom::ConstList<ocs::tom::UA> usage_list, const char *name, const int def) {
   DENTER(BASIS_LAYER);
   int ret = def;
   const auto ep = ocs::tom::find_first<UA_name>(usage_list, name);
   if (ep) {
      ret = static_cast<int>(ocs::tom::get<UA_value>(ep));
   }
   DRETURN(ret);
}


/**
 * @brief Read a usage value as a 32 bit unsigned integer
 *
 * Usage is stored as a double throughout; this converts on read.
 *
 * @param usage_list list to search (UA), may be an empty handle
 * @param name name of the element to search
 * @param def default value returned when the list has no such element
 * @return value of found object or default
 *
 * @note MT-NOTE: usage_list_get_ulong_usage() is MT safe
 */
uint32_t
usage_list_get_ulong_usage(const ocs::tom::ConstList<ocs::tom::UA> usage_list, const char *name,
                           const uint32_t def) {
   DENTER(BASIS_LAYER);
   uint32_t ret = def;
   const auto ep = ocs::tom::find_first<UA_name>(usage_list, name);
   if (ep) {
      ret = static_cast<uint32_t>(ocs::tom::get<UA_value>(ep));
   }
   DRETURN(ret);
}


/**
 * @brief Read a usage value as a 64 bit unsigned integer
 *
 * @param usage_list list to search (UA), may be an empty handle
 * @param name name of the element to search
 * @param def default value returned when the list has no such element
 * @return value of found object or default
 *
 * @note MT-NOTE: usage_list_get_ulong64_usage() is MT safe
 */
uint64_t
usage_list_get_ulong64_usage(const ocs::tom::ConstList<ocs::tom::UA> usage_list, const char *name,
                             const uint64_t def) {
   DENTER(BASIS_LAYER);
   uint64_t ret = def;
   const auto ep = ocs::tom::find_first<UA_name>(usage_list, name);
   if (ep) {
      ret = static_cast<uint64_t>(ocs::tom::get<UA_value>(ep));
   }
   DRETURN(ret);
}


/**
 * @brief Return double usage value
 *
 * Searches a usage object with the given name in the given usage
 * list. If such an element is found, returns the value of the
 * usage object as double value.
 * If no such element is found, return the given default value.
 *
 * @param usage_list the usage list (UA), may be an empty handle
 * @param name name of the element to search
 * @param def default value
 *
 * @return value of found object or default
 *
 * @see #usage_list_get_ulong_usage
 */
double
usage_list_get_double_usage(const ocs::tom::ConstList<ocs::tom::UA> usage_list, const char *name,
                            const double def) {
   DENTER(BASIS_LAYER);
   double ret = def;
   const auto ep = ocs::tom::find_first<UA_name>(usage_list, name);
   if (ep) {
      ret = ocs::tom::get<UA_value>(ep);
   }
   DRETURN(ret);
}


/**
 * @brief Create/update a usage record
 *
 * Updates the value of a usage record. If no usage record exists with the
 * given name in usage_list, a new record is created.
 *
 * @param usage_list list containing the usage record to update (UA), may be an empty handle
 * @param name name of the usage record to update
 * @param value the new value
 *
 * @note MT-NOTE: usage_list_set_ulong_usage() is MT safe
 *
 * @see #usage_list_set_double_usage, #usage_list_get_ulong_usage, #usage_list_get_double_usage
 */
void
usage_list_set_ulong_usage(const ocs::tom::List<ocs::tom::UA> usage_list, const char *name, const uint32_t value) {
   DENTER(BASIS_LAYER);
   usage_list_set_double_usage(usage_list, name, value);
   DRETURN_VOID;
}


/**
 * @brief Create or update a usage record from a 64 bit unsigned integer
 *
 * @param usage_list list containing the usage record to update (UA), may be an empty handle
 * @param name name of the usage record to update
 * @param value the new value
 *
 * @note MT-NOTE: usage_list_set_ulong64_usage() is MT safe
 */
void
usage_list_set_ulong64_usage(const ocs::tom::List<ocs::tom::UA> usage_list, const char *name,
                             const uint64_t value) {
   DENTER(BASIS_LAYER);
   usage_list_set_double_usage(usage_list, name, value);
   DRETURN_VOID;
}


/**
 * @brief Find a usage record, creating it on request
 *
 * An empty list handle or a null name gives no record: the list belongs to
 * the caller and cannot be created here.
 *
 * @param usage_list the usage list (UA), may be an empty handle
 * @param name name of the usage record
 * @param create_usage create the usage record if it does not exist
 * @return the record, or an empty handle
 */
static ocs::tom::Element<ocs::tom::UA>
ocs_usage_list_find_or_add(const ocs::tom::List<ocs::tom::UA> usage_list, const char *name,
                           const bool create_usage) {
   DENTER(BASIS_LAYER);
   auto ep = ocs::tom::find_first_rw<UA_name>(usage_list, name);
   if (!ep && create_usage && usage_list && name != nullptr) {
      ep = usage_list.emplace_back<UA_name>(name);
   }
   DRETURN(ep);
}

/**
 * @brief Create/update a usage record
 *
 * Updates the value of a usage record. If no usage record exists with the
 * given name in usage_list, a new record is created.
 *
 * @param usage_list list containing the usage record to update (UA), may be an empty handle
 * @param name name of the usage record to update
 * @param value the new value
 * @param create_usage create the usage element if it does not exist? Default: yes.
 *
 * @note MT-NOTE: usage_list_set_double_usage() is MT safe
 *
 * @see #usage_list_set_ulong_usage, #usage_list_get_ulong_usage, #usage_list_get_double_usage
 */
void
usage_list_set_double_usage(const ocs::tom::List<ocs::tom::UA> usage_list, const char *name, const double value,
                            const bool create_usage) {
   DENTER(BASIS_LAYER);
   const auto ep = ocs_usage_list_find_or_add(usage_list, name, create_usage);
   if (ep) {
      ocs::tom::set<UA_value>(ep, value);
   }
   DRETURN_VOID;
}


/**
 * @brief Set the maximum value for a usage record
 *
 * For a usage record with the given name, this function sets the value to the maximum
 * of the current value and the provided value. If no usage record exists with the
 * given name, a new record is created with the provided value.
 *
 * @param usage_list the usage list to update (UA), may be an empty handle
 * @param name name of the usage record to update
 * @param value the value to compare and set
 * @param create_usage if true, create the usage element if it does not exist; default is true.
 */
void
usage_list_max_double_usage(const ocs::tom::List<ocs::tom::UA> usage_list, const char *name, const double value,
                            const bool create_usage) {
   DENTER(BASIS_LAYER);
   const auto ep = ocs_usage_list_find_or_add(usage_list, name, create_usage);
   if (ep) {
      ocs::tom::set<UA_value>(ep, std::max(value, ocs::tom::get<UA_value>(ep)));
   }
   DRETURN_VOID;
}

/**
 * @brief Sum up usage of two lists
 *
 * Add the usage reported in add_usage_list to usage_list.
 * Summing up of usage will only be done for certain attributes:
 *    - cpu
 *    - io
 *    - iow
 *    - mem
 *    - vmem
 *    - maxvmem
 *    - all ru_* attributes (see man getrusage.2)
 *
 * @param usage_list the usage list to contain all usage (UA), not empty if @p add_usage_list holds a
 *                   summed attribute
 * @param add_usage_list usage to add to usage_list (UA), may be an empty handle
 *
 * @note MT-NOTE: usage_list_sum() is MT safe
 */
void
usage_list_sum(const ocs::tom::List<ocs::tom::UA> usage_list, const ocs::tom::ConstList<ocs::tom::UA> add_usage_list) {
   DENTER(BASIS_LAYER);
   for (const auto usage : add_usage_list) {
      const char *name = ocs::tom::get<UA_name>(usage);
      /* Sum up all usage attributes. */
      if (strcmp(name, USAGE_ATTR_CPU) == 0 ||
          strcmp(name, USAGE_ATTR_IO) == 0 ||
          strcmp(name, USAGE_ATTR_IOW) == 0 ||
          strcmp(name, USAGE_ATTR_VMEM) == 0 ||
          strcmp(name, USAGE_ATTR_RSS) == 0 ||
          strcmp(name, USAGE_ATTR_MEM) == 0 ||
          strcmp(name, USAGE_ATTR_PSS) == 0 ||
          strcmp(name, USAGE_ATTR_PMEM) == 0 ||
          strcmp(name, USAGE_ATTR_SMEM) == 0 ||
          strncmp(name, "acct_", 5) == 0 ||
          strncmp(name, "ru_", 3) == 0) {
         const auto sum = ocs::tom::find_first_rw<UA_name>(usage_list, name);
         if (!sum) {
            usage_list.push_back(usage.clone());
         } else {
            ocs::tom::add<UA_value>(sum, ocs::tom::get<UA_value>(usage));
         }
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Apply an execution host's usage scaling factors
 *
 * A slow host can be configured to report its usage scaled, so that jobs are
 * charged comparably across a heterogeneous cluster. The scaling applies to
 * the usage accumulated *since* `prev_usage`, not to the total.
 *
 * Without scaling factors nothing is done. An empty @p scaled_usage is left
 * empty: the list belongs to the caller.
 *
 * @param scaling the host's scaling factors, one per usage attribute (HS), may be an empty handle
 * @param prev_usage the usage already accounted for (UA), may be an empty handle
 * @param scaled_usage the usage to scale in place (UA), may be an empty handle
 */
void
scale_usage(const ocs::tom::ConstList<ocs::tom::HS> scaling, const ocs::tom::ConstList<ocs::tom::UA> prev_usage,
            const ocs::tom::List<ocs::tom::UA> scaled_usage) {
   DENTER(BASIS_LAYER);

   if (!scaling || !scaled_usage) {
      DRETURN_VOID;
   }

   for (const auto ep : scaled_usage) {
      const auto sep = ocs::tom::find_first<HS_name>(scaling, ocs::tom::get<UA_name>(ep));
      if (sep) {
         ocs::tom::set<UA_value>(ep, ocs::tom::get<UA_value>(ep) * ocs::tom::get<HS_value>(sep));
      }
   }

   // the attributes whose previous usage is added, in this order
   static constexpr const char *summed[] = {
      USAGE_ATTR_CPU, USAGE_ATTR_IO, USAGE_ATTR_IOW, USAGE_ATTR_VMEM, USAGE_ATTR_MAXVMEM, USAGE_ATTR_RSS,
      USAGE_ATTR_MAXRSS, USAGE_ATTR_MEM, USAGE_ATTR_PSS, USAGE_ATTR_MAXPSS, USAGE_ATTR_PMEM, USAGE_ATTR_SMEM
   };
   for (const char *attr : summed) {
      const auto prev = ocs::tom::find_first<UA_name>(prev_usage, attr);
      if (prev) {
         const auto ep = ocs::tom::find_first_rw<UA_name>(scaled_usage, attr);
         if (ep) {
            ocs::tom::add<UA_value>(ep, ocs::tom::get<UA_value>(prev));
         } else {
            scaled_usage.push_back(prev.clone());
         }
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Case-insensitive equality for a null-terminated string against a fixed literal
 *
 * Local helper for the bool-coercion branch in usage_parse_value.
 *
 * @param value the text, not nullptr
 * @param literal the literal to compare with, not nullptr
 * @return true if both are equal ignoring case
 */
static bool
str_iequal(const char *value, const char *literal) {
   while (*literal != '\0') {
      if (std::tolower(static_cast<unsigned char>(*value)) !=
          std::tolower(static_cast<unsigned char>(*literal))) {
         return false;
      }
      ++value;
      ++literal;
   }
   return *value == '\0';
}

/**
 * @brief Try to parse a string as a full double
 *
 * @param value the text to parse
 * @param[out] out receives the number when the whole string was consumed
 * @return true only when the entire string was a valid finite double; false
 *         for leading or trailing whitespace, a partial parse, NaN and Infinity
 */
static bool
parse_full_double(const char *value, double &out) {
   if (value[0] == '\0') {
      return false;
   }
   char *end = nullptr;
   errno = 0;
   double d = std::strtod(value, &end);
   if (errno != 0 || end == value || *end != '\0' || !std::isfinite(d)) {
      return false;
   }
   out = d;
   return true;
}

/**
 * @brief Parse a raw `variable=value` pair from the shepherd usage file into a fresh `UA` element
 *
 * Also used for equivalent sources. See `test/libs/sgeobj/test_sgeobj_usage_parse.cc`
 * for the cases it covers.
 *
 * Discrimination rule (CS-849, origin AE3):
 *   1. length >= 2 && value starts and ends with the same quote character
 *      ('"' or '\''): strip both quotes, UA_svalue = interior, no escape processing.
 *      An empty pair yields no element, see below.
 *   2. Else parses as a full double: UA_value = the parsed number.
 *   3. Else case-insensitively equals "true" (UA_value = 1) or "false" (UA_value = 0).
 *   4. Else raw string: UA_svalue = the value as-is, including any stray
 *      leading quote (unmatched, mismatched, or single-character).
 * Standard `USAGE_ATTR_*` code paths never invoke this helper; `UA_svalue` is
 * reserved for custom usage values that arrive through the shepherd path.
 * An empty value and an empty variable name are both accepted.
 *
 * @param name the usage attribute name; nullptr yields no element
 * @param value the raw text; nullptr is treated as the empty string
 * @return the new element, owned by the caller; empty when `name` was nullptr or the value an empty quoted pair
 */
ocs::tom::ElementPtr<ocs::tom::UA>
usage_parse_value(const char *name, const char *value) {
   DENTER(BASIS_LAYER);

   if (name == nullptr) {
      DRETURN(nullptr);
   }
   if (value == nullptr) {
      value = "";
   }

   auto ep = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep, name);

   // (1) quote-prefix rule — matched pair strips.
   // Empty quoted pair ("" or '') is a special case: CULL's wire format
   // (ocs::tom::put_str() / ocs::tom::get_str()) cannot distinguish an empty string from nullptr,
   // so an empty svalue would silently become nullptr after execd → qmaster
   // GDI transport and the downstream writer would fall through to the
   // numeric branch. To keep semantics consistent end-to-end we treat an
   // empty quoted pair as "no value" and return nullptr, which the caller
   // (reaper_execd) already handles by skipping the entry.
   const std::size_t len = std::strlen(value);
   if (len >= 2 &&
       (value[0] == '"' || value[0] == '\'') &&
       value[len - 1] == value[0]) {
      if (len == 2) {
         DRETURN(nullptr);
      }
      std::string interior(value + 1, len - 2);
      ocs::tom::set<UA_svalue>(ep, interior.c_str());
      DRETURN(ep);
   }

   // (2) full-double parse.
   double d = 0.0;
   if (parse_full_double(value, d)) {
      ocs::tom::set<UA_value>(ep, d);
      DRETURN(ep);
   }

   // (3) case-insensitive bool coercion.
   if (str_iequal(value, "true")) {
      ocs::tom::set<UA_value>(ep, 1.0);
      DRETURN(ep);
   }
   if (str_iequal(value, "false")) {
      ocs::tom::set<UA_value>(ep, 0.0);
      DRETURN(ep);
   }

   // (4) raw string.
   ocs::tom::set<UA_svalue>(ep, value);
   DRETURN(ep);
}
