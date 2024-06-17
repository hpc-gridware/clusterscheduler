/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2024 HPC-Gridware GmbH
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

#include <string>

#include "cull/cull_multitype.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_range.h"

#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "msg_spoollib_flatfile.h"

#include "sge_flatfile_obj_rsmap.h"


static void
store_resl(lListElem *centry, const char *id) {
   lListElem *resl = lGetSubStrRW(centry, RESL_value, id, CE_resource_map_list);
   if (resl == nullptr) {
      resl = lAddSubStr(centry, RESL_value, id, CE_resource_map_list, RESL_Type);
   }
   if (resl != nullptr) {
      lAddUlong(resl, RESL_amount, 1);
   }
}

/**
 * @brief parse an entry of complex_values
 *
 * @param ep        CE_Type element which will store the complex_value
 * @param nm        attribute to fill in (CE_stringval)
 * @param buf       string buffer with the complex value
 * @param alpp      for returning errors
 * @return          1 in case of success, else 0
 */
int
read_CE_stringval_host(lListElem *ep, int nm, const char *buf, lList **alpp) {
   int ret = 1;

   DENTER(TOP_LAYER);

   /*
    * The following code reads both ordinary complex_values (name=value)
    * and RSMAPs (name=amount(ids) like
    *    gpu=2
    *    gpu=2(A B)
    *    gpu=2(1-2)
    *    gpu=4(A B 1-2)
    */
   struct saved_vars_s *context = nullptr;
   char *token;
   if ((token = sge_strtok_r(buf, " (", &context))) {
      // first token is the amount
      lSetString(ep, nm, token);
      u_long32 amount = SGE_STRTOU_LONG32(token);

      // the following tokens are individual ids or ranges
      if ((token = sge_strtok_r(nullptr, " )", &context)) != nullptr) {
         u_long32 num_ids = 0;
         do {
            u_long32 range_start = 0;
            u_long32 range_end = 0;
            u_long32 range_step = 0;

            // handle range
            if (range_parse_get_ids(token, 0, range_start, range_end, range_step)) {
               for (; range_start <= range_end; range_start += range_step) {
                  std::string id_str{std::to_string(range_start)};
                  store_resl(ep, id_str.c_str());
                  num_ids++;
               }
            } else {
               // handle individual id
               store_resl(ep, token);
               num_ids++;
            }
         } while ((token = sge_strtok_r(nullptr, " )", &context)));

         // check if data is consistent
         if (amount != num_ids) {
            answer_list_add_sprintf(alpp, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                    MSG_RSMAP_INCONSISTENTAMOUNT_SSUU, lGetString(ep, CE_name),
                                    buf, amount, num_ids);
            lSetList(ep, CE_resource_map_list, nullptr);
            ret = 0;
         }
      }
   }

   sge_free_saved_vars(context);
   DRETURN(ret);
}

static void
store_item(std::string &str_out, const std::string_view &str_new, const u_long32 amount) {
   for (u_long32 i = 0; i < amount; i++) {
      if (!str_out.empty()) {
         str_out += ' ';
      }
      str_out += str_new;
   }
}

static void
store_range(std::string &str_out, long &range_start, long &range_last, u_long32 amount, const long range_new) {
   // range ended - if it has at least 3 elements
   // then write it as range
   if (range_last - range_start > 1) {
      std::string str_range{};
      str_range += std::to_string(range_start);
      str_range += '-';
      str_range += std::to_string(range_last);
      store_item(str_out, str_range, amount);
   } else {
      // write it as individual entries
      for (long i = range_start; i <= range_last; ++i) {
         store_item(str_out, std::to_string(i), amount);
      }
   }
   range_start = range_last = range_new;
}

/**
 * @brief write an entry of complex_values
 *
 * @param ep        CE_Type element representing a complex_value
 * @param nm        attribute to read from (CE_stringval)
 * @param buf       string buffer we fill in with the data
 * @param alpp      for returning errors
 * @return          1 in case of success, else 0
 */
int
write_CE_stringval_host(const lListElem *ep, int nm, dstring *buffer, lList **alp) {
   DENTER(TOP_LAYER);

   std::string str_out{};

   // add the value of the complex_value to the output buffer
   const char *str_value = lGetString(ep, CE_stringval);
   if (str_value != nullptr) {
      str_out += str_value;
   } else {
      // should never happen - but you never know...
      double dbl_value = lGetDouble(ep, CE_doubleval);
      str_out += std::to_string(dbl_value);
   }
   sge_dstring_append(buffer, str_out.c_str());
   str_out.clear();

   if (lGetUlong(ep, CE_valtype) == TYPE_RSMAP) {
      /*
       * fill in the IDs from the CE_resource_map_list
       * we can have individual ids or a range or a combination of both
       *    gpu=2
       *    gpu=2(A B)
       *    gpu=2(1-2)
       *    gpu=4(A B 1-2)
       * we cannot use range functions from sgeobj lib as:
       *       - there is only the special case of ranges with step size 1
       *       - the colon is required for topology masks (GPU1:SCCSCC)
       *       - multiple occurrences of one entry are possible (0 0 0 1 1 1)
       */
      const lList *resource_map = lGetList(ep, CE_resource_map_list);
      if (resource_map != nullptr && lGetNumberOfElem(resource_map) > 0) {
         long range_start = -1, range_last = -1, range_current;
         u_long32 amount = 0;
         const lListElem *resource;

         for_each_ep (resource, resource_map) {
            str_value = lGetString(resource, RESL_value);
            amount = lGetUlong(resource, RESL_amount);

            if (sge_str_is_number(str_value)) {
               // can belong to a range
               range_current = strtol(str_value, nullptr, 10);
               if (range_start == -1) {
                  // a new potential range started
                  range_start = range_last = range_current;
               } else {
                  // additional number in range
                  if (range_current == range_last + 1) {
                     // we only support step = 1
                     // continue the range
                     range_last = range_current;
                  } else {
                     // store and start a new range
                     store_range(str_out, range_start, range_last, amount, range_current);
                  }
               }
            } else {
               // it is a name
               // if we have stored range values, then add the range to our output list
               if (range_start != -1) {
                  store_range(str_out, range_start, range_last, amount, -1);
               }

               // add the non number value to our output list
               store_item(str_out, str_value, amount);
            }
         }
         // we might end up having stored range values, add the range to our output list
         if (range_start != -1) {
            store_range(str_out, range_start, range_last, amount, -1);
         }

         // add the RSMAP entries to the output buffer
         sge_dstring_append_char(buffer, '(');
         sge_dstring_append(buffer, str_out.c_str());
         sge_dstring_append_char(buffer, ')');
      }
   }

   DRETURN(1);
}

