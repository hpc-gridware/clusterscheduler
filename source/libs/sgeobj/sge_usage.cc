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

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <string>

#include "cull/cull.h"

#include "sgeobj/sge_usage.h"
#include "sgeobj/sge_host.h"

/****** sgeobj/usage/usage_list_get_ulong_usage() *****************************
*  NAME
*     usage_list_get_ulong_usage() -- return ulong usage value
*
*  SYNOPSIS
*     uint32_t
*     usage_list_get_ulong_usage(const lList *usage_list, const char *name,
*                                uint32_t def)
*
*  FUNCTION
*     Searches a usage object with the given name in the given usage
*     list. If such an element is found, returns the value of the
*     usage object as uint32_t value.
*     If no such element is found, return the given default value.
*
*  INPUTS
*     const lList *usage_list - the usage list
*     const char *name        - name of the element to search
*     uint32_t def            - default value
*
*  RESULT
*     uint32_t - value of found object or default
*
*  SEE ALSO
*     gdi/usage/usage_list_get_double_usage()
*******************************************************************************/
int
usage_list_get_int_usage(const lList *usage_list, const char *name, int def) {
   const lListElem *ep = lGetElemStr(usage_list, UA_name, name);
   if (ep != nullptr) {
      return static_cast<int>(lGetDouble(ep, UA_value));
   } else {
      return def;
   }
}

uint32_t
usage_list_get_ulong_usage(const lList *usage_list, const char *name, uint32_t def) {
   const lListElem *ep = lGetElemStr(usage_list, UA_name, name);
   if (ep != nullptr) {
      return static_cast<uint32_t>(lGetDouble(ep, UA_value));
   } else {
      return def;
   }
}

uint64_t
usage_list_get_ulong64_usage(const lList *usage_list, const char *name, uint64_t def) {
   const lListElem *ep = lGetElemStr(usage_list, UA_name, name);
   if (ep != nullptr) {
      return static_cast<uint64_t>(lGetDouble(ep, UA_value));
   } else {
      return def;
   }
}

/****** sgeobj/usage/usage_list_get_double_usage() ****************************
*  NAME
*     usage_list_get_double_usage() -- return double usage value
*
*  SYNOPSIS
*     double
*     usage_list_get_double_usage(const lList *usage_list, const char *name,
*                                 double def)
*
*  FUNCTION
*     Searches a usage object with the given name in the given usage
*     list. If such an element is found, returns the value of the
*     usage object as double value.
*     If no such element is found, return the given default value.
*
*  INPUTS
*     const lList *usage_list - the usage list
*     const char *name        - name of the element to search
*     double def              - default value
*
*  RESULT
*     double - value of found object or default
*
*  SEE ALSO
*     gdi/usage/usage_list_get_ulong_usage()
*******************************************************************************/
double
usage_list_get_double_usage(const lList *usage_list, const char *name,
                            double def)
{
   const lListElem *ep = lGetElemStr(usage_list, UA_name, name);
   if(ep != nullptr) {
      return lGetDouble(ep, UA_value);
   } else {
      return def;
   }
}

/****** sgeobj/usage/usage_list_set_ulong_usage() ******************************
*  NAME
*     usage_list_set_ulong_usage() -- create/update a usage record
*
*  SYNOPSIS
*     void
*     usage_list_set_ulong_usage(lList *usage_list, const char *name,
*                                uint32_t value)
*
*  FUNCTION
*     Updates the value of a usage record. If no usage record exists with the
*     given name in usage_list, a new record is created.
*
*  INPUTS
*     lList *usage_list - list containing the usage record to update
*     const char *name  - name of the usage record to update
*     uint32_t value    - the new value
*
*  NOTES
*     MT-NOTE: usage_list_set_ulong_usage() is MT safe
*
*  SEE ALSO
*     sgeobj/usage/usage_list_set_double_usage()
*     sgeobj/usage/usage_list_get_ulong_usage()
*     sgeobj/usage/usage_list_get_double_usage()
*******************************************************************************/
void
usage_list_set_ulong_usage(lList *usage_list, const char *name, uint32_t value)
{
   usage_list_set_double_usage(usage_list, name, value);
}

void
usage_list_set_ulong64_usage(lList *usage_list, const char *name, uint64_t value)
{
   usage_list_set_double_usage(usage_list, name, value);
}

/****** sgeobj/usage/usage_list_set_double_usage() ******************************
*  NAME
*     usage_list_set_double_usage() -- create/update a usage record
*
*  SYNOPSIS
*     void
*     usage_list_set_double_usage(lList *usage_list, const char *name,
*                                 double value)
*
*  FUNCTION
*     Updates the value of a usage record. If no usage record exists with the
*     given name in usage_list, a new record is created.
*
*  INPUTS
*     lList *usage_list - list containing the usage record to update
*     const char *name  - name of the usage record to update
*     double value      - the new value
*     bool create_usage - create the usage element if it does not exist? Default: yes.
*
*  NOTES
*     MT-NOTE: usage_list_set_double_usage() is MT safe
*
*  SEE ALSO
*     sgeobj/usage/usage_list_set_ulong_usage()
*     sgeobj/usage/usage_list_get_ulong_usage()
*     sgeobj/usage/usage_list_get_double_usage()
*******************************************************************************/
void
usage_list_set_double_usage(lList *usage_list, const char *name, double value, bool create_usage)
{
   lListElem *ep = lGetElemStrRW(usage_list, UA_name, name);
   if (ep == nullptr && create_usage) {
      ep = lAddElemStr(&usage_list, UA_name, name, UA_Type);
   }

   if (ep != nullptr) {
      lSetDouble(ep, UA_value, value);
   }
}


/**
 * @brief Set the maximum value for a usage record.A
 *
 * For a usage record with the given name, this function sets the value to the maximum
 * of the current value and the provided value. If no usage record exists with the
 * given name, a new record is created with the provided value.
 *
 * @param usage_list    - the usage list to update
 * @param name          - name of the usage record to update
 * @param value         - the value to compare and set
 * @param create_usage  - if true, create the usage element if it does not exist; default is true.
 */
void
usage_list_max_double_usage(lList *usage_list, const char *name, double value, bool create_usage)
{
   lListElem *ep = lGetElemStrRW(usage_list, UA_name, name);
   if (ep == nullptr && create_usage) {
      ep = lAddElemStr(&usage_list, UA_name, name, UA_Type);
   }

   if (ep != nullptr) {
      lSetDouble(ep, UA_value, std::max(value, lGetDouble(ep, UA_value)));
   }
}

/****** sge_usage/usage_list_sum() *********************************************
*  NAME
*     usage_list_sum() -- sum up usage of two lists
*
*  SYNOPSIS
*     void
*     usage_list_sum(lList *usage_list, const lList *add_usage_list)
*
*  FUNCTION
*     Add the usage reported in add_usage_list to usage_list.
*     Summing up of usage will only be done for certain attributes:
*        - cpu
*        - io
*        - iow
*        - mem
*        - vmem
*        - maxvmem
*        - all ru_* attributes (see man getrusage.2)
*
*  INPUTS
*     lList *usage_list           - the usage list to contain all usage
*     const lList *add_usage_list - usage to add to usage_list
*
*  NOTES
*     MT-NOTE: usage_list_sum() is MT safe
*******************************************************************************/
void
usage_list_sum(lList *usage_list, const lList *add_usage_list)
{
   for_each_ep_lv(usage, add_usage_list) {
      const char *name = lGetString(usage, UA_name);
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
         lListElem *sum = lGetElemStrRW(usage_list, UA_name, name);
         if (sum == nullptr) {
            lAppendElem(usage_list, lCopyElem(usage));
         } else {
            lAddDouble(sum, UA_value, lGetDouble(usage, UA_value));
         }
      }
   }
}

/* if the scaled usage list does not yet exist, it is created and returned */
lList *scale_usage(
const lList *scaling,     /* HS_Type */
const lList *prev_usage,  /* HS_Type */
lList *scaled_usage /* UA_Type */
) {
   const lListElem *sep;
   lListElem *ep;
   const lListElem *prev;

   if (!scaling) {
      return nullptr;
   }

   if (scaled_usage == nullptr) {
      scaled_usage = lCreateList("usage", UA_Type);
   }

   for_each_rw (ep, scaled_usage) {
      if ((sep=lGetElemStr(scaling, HS_name, lGetString(ep, UA_name)))) {
         lSetDouble(ep, UA_value, lGetDouble(ep, UA_value) * lGetDouble(sep, HS_value));
      }
   }

   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_CPU)) != nullptr) {
      if ((ep=lGetElemStrRW(scaled_usage, UA_name, USAGE_ATTR_CPU))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }
   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_IO)) != nullptr) {
      if ((ep=lGetElemStrRW(scaled_usage, UA_name, USAGE_ATTR_IO))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }
   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_IOW)) != nullptr) {
      if ((ep=lGetElemStrRW(scaled_usage, UA_name, USAGE_ATTR_IOW))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }
   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_VMEM)) != nullptr) {
      if ((ep=lGetElemStrRW(scaled_usage, UA_name, USAGE_ATTR_VMEM))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }
   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_MAXVMEM)) != nullptr) {
      if ((ep=lGetElemStrRW(scaled_usage, UA_name, USAGE_ATTR_MAXVMEM))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }
   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_RSS)) != nullptr) {
      if ((ep=lGetElemStrRW(scaled_usage, UA_name, USAGE_ATTR_RSS))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }
   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_MAXRSS)) != nullptr) {
      if ((ep=lGetElemStrRW(scaled_usage, UA_name, USAGE_ATTR_MAXRSS))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }
   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_MEM)) != nullptr) {
      if ((ep=lGetElemStrRW(scaled_usage, UA_name, USAGE_ATTR_MEM))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }

   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_PSS)) != nullptr) {
      if ((ep=lGetElemStrRW(scaled_usage, UA_name, USAGE_ATTR_PSS))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }
   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_MAXPSS)) != nullptr) {
      if ((ep=lGetElemStrRW(scaled_usage, UA_name, USAGE_ATTR_MAXPSS))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }
   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_PMEM)) != nullptr) {
      if ((ep=lGetElemStrRW(scaled_usage, UA_name, USAGE_ATTR_PMEM))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }
   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_SMEM)) != nullptr) {
      if ((ep=lGetElemStrRW(scaled_usage, UA_name, USAGE_ATTR_SMEM))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }

   return scaled_usage;
}

/**
 * Case-insensitive equality for a null-terminated string against a fixed
 * literal. Local helper for the bool-coercion branch in usage_parse_value.
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
 * Try to parse @p value as a full double. Returns true and sets @p out when
 * the entire string was consumed as a valid finite double; false otherwise
 * (including leading/trailing whitespace, partial parse, NaN/Infinity).
 */
static bool
parse_full_double(const char *value, double &out) {
   if (value[0] == '\0') {
      return false;
   }
   char *end = nullptr;
   errno = 0;
   double d = std::strtod(value, &end);
   if (errno != 0 || end == value || *end != '\0') {
      return false;
   }
   out = d;
   return true;
}

/**
 * Parse a raw shepherd-usage-file value into a fresh UA_Type element.
 * Discrimination rule (CS-849, origin AE3):
 *   1. length ≥ 2 && value starts and ends with the same quote character
 *      ('"' or '\''): strip both quotes, UA_svalue = interior, no escape processing.
 *      Empty pair "" yields an empty-string UA_svalue.
 *   2. Else parses as a full double: UA_value = the parsed number.
 *   3. Else case-insensitively equals "true" (UA_value = 1) or "false" (UA_value = 0).
 *   4. Else raw string: UA_svalue = the value as-is, including any stray
 *      leading quote (unmatched, mismatched, or single-character).
 * Standard USAGE_ATTR_* code paths never invoke this helper; UA_svalue is
 * reserved for custom usage values that arrive through the shepherd path.
 */
lListElem *
usage_parse_value(const char *name, const char *value) {
   if (name == nullptr) {
      return nullptr;
   }
   if (value == nullptr) {
      value = "";
   }

   lListElem *ep = lCreateElem(UA_Type);
   lSetString(ep, UA_name, name);

   // (1) quote-prefix rule — matched pair strips.
   // Empty quoted pair ("" or '') is a special case: CULL's wire format
   // (packstr / unpackstr) cannot distinguish an empty string from nullptr,
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
         lFreeElem(&ep);
         return nullptr;
      }
      std::string interior(value + 1, len - 2);
      lSetString(ep, UA_svalue, interior.c_str());
      return ep;
   }

   // (2) full-double parse.
   double d = 0.0;
   if (parse_full_double(value, d)) {
      lSetDouble(ep, UA_value, d);
      return ep;
   }

   // (3) case-insensitive bool coercion.
   if (str_iequal(value, "true")) {
      lSetDouble(ep, UA_value, 1.0);
      return ep;
   }
   if (str_iequal(value, "false")) {
      lSetDouble(ep, UA_value, 0.0);
      return ep;
   }

   // (4) raw string.
   lSetString(ep, UA_svalue, value);
   return ep;
}
