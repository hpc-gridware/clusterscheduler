/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2024,2026 HPC-Gridware GmbH
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
 * @brief Checks specific to RSMAP resources
 *
 * A resource map hands out named instances rather than a count, which
 * constrains how it may be configured.
 *
 * @see sge_centry_rsmap.h
 */

#include <cstring>

#include <unordered_set>
#include <vector>

#include <string>

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_eval_expression.h"

#include "uti/sge_hostname.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_resource_utilization.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "msg_common.h"

#include "sgeobj/sge_centry_rsmap.h"

/**
 * @brief Reject an RSMAP resource that is not consumable
 *
 * A resource map hands out named instances, so it only makes sense as a
 * consumable.
 *
 * @param[out] answer_list receives the reason it was rejected
 * @param consumable the resource's `CE_consumable` setting
 * @param attrname the resource's name, for the message
 * @return true when the combination is allowed
 */
bool centry_check_rsmap(lList **answer_list, uint32_t consumable, const char *attrname) {
   bool ret = true;

   if (consumable == CONSUMABLE_NO) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_INVALID_CENTRY_RSMAP_NOT_CONSUMABLE_S, attrname);
      ret = false;
   }

   return ret;
}

/**
 * Validate the per-instance characteristics on an RSMAP CE element. Each
 * RESL entry in CE_resource_map_list may carry a RESL_properties list of
 * ComplexEntry (CE_Type) elements — after the flatfile reader they hold only
 * CE_name and CE_stringval verbatim. This function resolves each name against
 * the master centry list, copies the referenced complex's valtype into the
 * property, type-checks its value via centry_fill_and_check, and rejects
 * duplicates within one id. Called from the host-side complex_values path
 * (centry_list_fill_request) after the top-level RSMAP entry has been typed.
 *
 * @param answer_list          answer list; validation errors are appended
 *                             with STATUS_EUNKNOWN/ANSWER_QUALITY_ERROR
 * @param centry               CE_Type element of a host's complex_values
 *                             entry; its CE_resource_map_list is walked and
 *                             each property's CE_name/CE_valtype/CE_doubleval
 *                             may be updated in place
 * @param master_centry_list   the master complex-list used to resolve
 *                             characteristic names and their valtypes
 * @return                     true if every property resolves and its value
 *                             parses; false on the first invalid property
 *                             (all detected problems are still reported to
 *                             the answer_list — the function does not stop
 *                             on the first error within a single centry)
 */
bool centry_check_rsmap_characteristics(lList **answer_list, lListElem *centry,
                                        const lList *master_centry_list) {
   bool ret = true;

   if (centry == nullptr) {
      return ret;
   }

   const char *rsmap_name = lGetString(centry, CE_name);
   lList *resource_map = lGetListRW(centry, CE_resource_map_list);
   if (resource_map == nullptr) {
      return ret;
   }

   lListElem *resl;
   for_each_rw (resl, resource_map) {
      lList *props = lGetListRW(resl, RESL_properties);
      if (props == nullptr || lGetNumberOfElem(props) == 0) {
         continue;
      }
      const char *id = lGetString(resl, RESL_value);

      std::unordered_set<std::string> seen;
      lListElem *prop;
      for_each_rw (prop, props) {
         const char *pname = lGetString(prop, CE_name);
         if (pname == nullptr) {
            continue;
         }

         // duplicate characteristic on the same id
         if (!seen.insert(pname).second) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_RSMAP_CHARACTERISTIC_DUPLICATE_SSS,
                                    rsmap_name, id != nullptr ? id : "", pname);
            ret = false;
            continue;
         }

         // resolve against master centry list
         const lListElem *master_cep = centry_list_locate(master_centry_list, pname);
         if (master_cep == nullptr) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_RSMAP_CHARACTERISTIC_UNKNOWN_SSS,
                                    rsmap_name, id != nullptr ? id : "", pname);
            ret = false;
            continue;
         }

         // copy the canonical name (in case the user wrote a shortcut) and the valtype
         lSetString(prop, CE_name, lGetString(master_cep, CE_name));
         lSetUlong(prop, CE_valtype, lGetUlong(master_cep, CE_valtype));

         // type-check the value; centry_fill_and_check populates CE_doubleval for
         // numeric types and returns -1 on parse failure (with its own answer_list
         // entry). We wrap that with our own message so the host/RSMAP/id context
         // is preserved in the error.
         lList *sub_answers = nullptr;
         if (centry_fill_and_check(prop, &sub_answers, false, false) != 0) {
            const char *sub_msg = "";
            const lListElem *first = lFirst(sub_answers);
            if (first != nullptr) {
               const char *txt = lGetString(first, AN_text);
               if (txt != nullptr) {
                  sub_msg = txt;
               }
            }
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_RSMAP_CHARACTERISTIC_PARSE_SSSS,
                                    rsmap_name, id != nullptr ? id : "", pname, sub_msg);
            lFreeList(&sub_answers);
            ret = false;
            continue;
         }
         lFreeList(&sub_answers);
      }
   }

   return ret;
}

const char *const RSMAP_REQUEST_PARAM_ID       = "id";
const char *const RSMAP_REQUEST_PARAM_SAME     = "same";
const char *const RSMAP_REQUEST_PARAM_SCOPE    = "scope";
const char *const RSMAP_REQUEST_PARAM_DISTINCT = "distinct";
const char *const RSMAP_REQUEST_PARAM_BIND     = "bind";

const char *const RSMAP_REQUEST_PARAM_SCOPE_HOST = "host";
const char *const RSMAP_REQUEST_PARAM_SCOPE_JOB  = "job";

/**
 * Is this the name of a parameter of a resource map request?
 *
 * The names are reserved across the whole complex namespace, not only inside a bracket: a
 * parameter which is not one of them names a characteristic to match, so a complex of one of
 * these names could never be matched. Reserving them is what keeps that unambiguous.
 *
 * @param name  a parameter or complex name
 * @return      true if the name is one of the reserved parameter names
 */
bool
centry_rsmap_is_reserved_param(const char *name) {
   if (name == nullptr) {
      return false;
   }
   return strcmp(name, RSMAP_REQUEST_PARAM_ID) == 0 ||
          strcmp(name, RSMAP_REQUEST_PARAM_SAME) == 0 ||
          strcmp(name, RSMAP_REQUEST_PARAM_SCOPE) == 0 ||
          strcmp(name, RSMAP_REQUEST_PARAM_DISTINCT) == 0 ||
          strcmp(name, RSMAP_REQUEST_PARAM_BIND) == 0;
}

/**
 * @brief find the bracketed parameter list of a request value
 *
 * "4[same=id,memory=40G]" holds its parameters between the brackets. Fills in the two positions
 * and returns true when there is a list; false when the value carries none, which is the
 * ordinary case of a request that is only an amount.
 *
 * Malformed lists - unclosed, or with text after the closing bracket - are reported by
 * centry_rsmap_check_request_params() at submission. Here they simply do not yield a list.
 */
static bool
centry_rsmap_find_params(const char *value, const char **open_out, const char **close_out) {
   if (value == nullptr) {
      return false;
   }

   const char *open = strchr(value, '[');
   if (open == nullptr) {
      return false;
   }

   // the closing bracket of the list is the last one at depth 0, so that a character class in
   // a value does not end the walk early
   const char *close = nullptr;
   int depth = 0;
   for (const char *p = open; *p != '\0'; ++p) {
      if (*p == '[') {
         depth++;
      } else if (*p == ']') {
         depth--;
         if (depth == 0) {
            close = p;
            break;
         }
      }
   }
   if (close == nullptr) {
      return false;
   }

   *open_out = open;
   *close_out = close;
   return true;
}

/**
 * @brief split a bracketed parameter list into its parameters
 *
 * Splits on the commas at bracket depth 0, so that a character class in a value - "[A,B]" as
 * the value of a string characteristic - stays with the parameter it belongs to. The parameters
 * are returned exactly as written, empty ones included, because whether an empty one is an
 * error depends on the caller: "[]" is a list of none and "[a=1,,b=2]" is a mistake.
 */
static void
centry_rsmap_split_params(const char *open, const char *close,
                          std::vector<std::string> &params) {
   const char *p = open + 1;

   while (p <= close) {
      const char *end = p;
      int d = 0;
      while (end < close && !(d == 0 && *end == ',')) {
         if (*end == '[') {
            d++;
         } else if (*end == ']') {
            d--;
         }
         ++end;
      }
      params.emplace_back(p, end - p);
      p = end + 1;
   }
}

/**
 * Validate the bracketed parameter list of a resource map request.
 *
 * A request may carry a list of named parameters after the amount:
 *
 *     -l 'gpu=4[id=gpu1*,same=id,memory=40G]'
 *
 * The whole string is in CE_stringval; CE_doubleval holds the amount in front of the bracket,
 * split off by centry_fill_and_check(). This function looks at what follows it.
 *
 * The list is order independent and its entries combine with AND, so a name given twice is a
 * conflict rather than a refinement and is refused. Each name is either one of the reserved
 * parameter names or the name of a complex, which is then matched against the characteristic of
 * that name on an instance. Nothing here evaluates a parameter - it establishes that the request
 * is well formed and that every name means something.
 *
 * A bracket in the value of a *configuration* is a different thing entirely, the per instance
 * characteristics of an id. Those never reach this function: the reader stores only the amount in
 * CE_stringval and puts the ids in CE_resource_map_list, so a bracket here is unambiguously a
 * request parameter list.
 *
 * @param answer_list        errors are appended here
 * @param centry             the request; CE_name and CE_stringval are read
 * @param master_centry_list complex list, used to resolve a parameter which is not reserved
 * @return                   true if the request carries no parameter list or a valid one
 */
bool
centry_rsmap_check_request_params(lList **answer_list, const lListElem *centry,
                                  const lList *master_centry_list) {
   const char *name = lGetString(centry, CE_name);
   const char *value = lGetString(centry, CE_stringval);

   if (value == nullptr) {
      return true;
   }

   // A parameter list belongs to a resource map request and to nothing else. Asked of any
   // other resource, a bracket is part of the value: a string valued resource is matched with
   // a pattern, so "-l h=[r]ocky-8-amd64-1" is a host name whose first character is written as
   // a character class, not a malformed parameter list. The type is known here - the caller
   // fills CE_valtype from the complex before asking - so the question is decided before a
   // bracket is looked for rather than after.
   if (static_cast<ocs::CEntry::Type>(lGetUlong(centry, CE_valtype)) != ocs::CEntry::Type::RSMAP) {
      return true;
   }

   const char *open = strchr(value, '[');
   if (open == nullptr) {
      return true;
   }

   // This is the single point where a parameter list enters the system from a request, the way
   // parse_id_characteristics() is for a configuration, so refusing it here keeps every consumer
   // downstream unreachable in an OCS build without a guard of its own. The list is parsed
   // everywhere - centry_list_parse_from_string() is shared with the open source clients and
   // cannot be made conditional - so what an OCS build rejects is a well formed request with a
   // clear message, not a parse error.
#if !defined(WITH_EXTENSIONS)
   answer_list_add_sprintf(answer_list, STATUS_ENOTAVAILABLE, ANSWER_QUALITY_ERROR,
                           MSG_RSMAP_PARAM_NOT_AVAILABLE_SS, name, value);
   return false;
#endif

   // the list has to be closed, and nothing may follow it. Bracket depth is tracked because a
   // value may itself contain a character class, "[id=gpu[01]*]"
   const char *close = nullptr;
   int depth = 0;
   for (const char *p = open; *p != '\0'; ++p) {
      if (*p == '[') {
         depth++;
      } else if (*p == ']') {
         depth--;
         if (depth == 0) {
            close = p;
            break;
         }
      }
   }
   if (close == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_RSMAP_PARAM_UNCLOSED_SS, name, value);
      return false;
   }
   if (*(close + 1) != '\0') {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_RSMAP_PARAM_TRAILING_SSS, name, close + 1, value);
      return false;
   }

   bool ret = true;
   std::unordered_set<std::string> seen;

   std::vector<std::string> params;
   centry_rsmap_split_params(open, close, params);

   for (const std::string &param : params) {
      if (param.empty()) {
         // "[]" is an empty list and means the same as no list at all; "[a=1,,b=2]" is a mistake
         if (open + 1 == close) {
            break;
         }
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_RSMAP_PARAM_EMPTY_SS, name, value);
         ret = false;
         continue;
      }

      const size_t eq = param.find('=');
      if (eq == std::string::npos) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_RSMAP_PARAM_NO_EQ_SS, name, param.c_str());
         ret = false;
         continue;
      }
      if (eq == 0) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_RSMAP_PARAM_NO_NAME_SS, name, value);
         ret = false;
         continue;
      }

      const std::string param_name = param.substr(0, eq);

      // order does not matter and the entries combine with AND, so a repeated name is a
      // conflict, not a refinement
      if (!seen.insert(param_name).second) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_RSMAP_PARAM_DUPLICATE_SS, name, param_name.c_str());
         ret = false;
         continue;
      }

      if (centry_rsmap_is_reserved_param(param_name.c_str())) {
         const std::string param_value = param.substr(eq + 1);

         // bind= is reserved and validated from the start, but nothing reads it until
         // affinity aware core binding exists (CS-2706). Only "no" is defined; rejecting any
         // other value now is what leaves room to add one later without breaking a script
         // which had been accepted and ignored.
         if (param_name == RSMAP_REQUEST_PARAM_BIND && param_value != "no") {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_RSMAP_PARAM_BAD_VALUE_SSS, name, param_name.c_str(),
                                    param_value.c_str());
            ret = false;
         }

         // same= takes "id" or the name of a characteristic, and the instances granted then
         // have to agree in whichever it names.
         //
         // The name has to resolve to a complex, which is what a characteristic is configured
         // as. Whether any instance of the map actually carries it is not a question for the
         // submission: which instances a job will be offered is not known until it is
         // scheduled, and the same request is valid on a host whose instances carry the
         // characteristic and unsatisfiable on one whose instances do not. A name which
         // resolves and is carried by nothing leaves the job pending, with the scheduling
         // message saying the map has no instances sharing one of them.
         if (param_name == RSMAP_REQUEST_PARAM_SAME &&
             param_value != RSMAP_REQUEST_PARAM_ID &&
             centry_list_locate(master_centry_list, param_value.c_str()) == nullptr) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_RSMAP_PARAM_BAD_VALUE_SSS, name, param_name.c_str(),
                                    param_value.c_str());
            ret = false;
         }

         // id= selects which instances are acceptable by their identifier. The value is an
         // expression over them - shell wildcards combined with !, & and |, and grouped with
         // brackets - the same one a string valued resource is matched with, see the
         // expression entry in sge_types(1).
         //
         // What can be checked here is that it parses. Whether it matches anything is not a
         // question for the submission: which instances a job will be offered is not known
         // until it is scheduled, and the same request is satisfiable on one host and not on
         // the next. An expression which matches nothing anywhere leaves the job pending with
         // a scheduling message saying so.
         //
         // The parse is checked by asking the expression about a probe value. A broken one
         // answers -1 whatever it is asked, and an empty one says nothing at all.
         if (param_name == RSMAP_REQUEST_PARAM_ID) {
            if (param_value.empty() ||
                sge_eval_expression(ocs::CEntry::Type::CSTR, param_value.c_str(), "0",
                                    nullptr) < 0) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                       MSG_RSMAP_PARAM_BAD_EXPRESSION_SSS, name,
                                       param_name.c_str(), param_value.c_str());
               ret = false;
            }
         }

         // scope= says how far the same= constraint reaches: over the instances granted on one
         // host, which is what it has always meant and remains the default, or over every
         // instance the job is granted anywhere. Only those two readings are defined, and a
         // value which is neither is refused rather than read as one of them.
         if (param_name == RSMAP_REQUEST_PARAM_SCOPE &&
             param_value != RSMAP_REQUEST_PARAM_SCOPE_HOST &&
             param_value != RSMAP_REQUEST_PARAM_SCOPE_JOB) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_RSMAP_PARAM_BAD_VALUE_SSS, name, param_name.c_str(),
                                    param_value.c_str());
            ret = false;
         }

         // distinct= is reserved so that the grammar has room for it, but nothing reads it yet.
         // Were it accepted, a job which asked for instances which differ would run as though
         // it had asked for nothing at all. Refusing it keeps that from happening quietly, and
         // a request the system refuses today can be accepted later without breaking anything,
         // which is not true the other way round.
         if (param_name == RSMAP_REQUEST_PARAM_DISTINCT) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_RSMAP_PARAM_NOT_YET_SS, name, param_name.c_str());
            ret = false;
         }

         continue;
      }

      // not a reserved name, so it has to name a complex - it will be matched against the
      // characteristic of that name on an instance. The name is resolved first so that one
      // nobody defined is still told it does not exist, which is the more useful of the two
      // messages.
      const lListElem *char_cep = centry_list_locate(master_centry_list, param_name.c_str());
      if (char_cep == nullptr) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_RSMAP_PARAM_UNKNOWN_SS, name, param_name.c_str());
         ret = false;
      } else if (static_cast<ocs::CEntry::Type>(lGetUlong(char_cep, CE_valtype)) ==
                 ocs::CEntry::Type::RSMAP) {
         // A resource map cannot be a characteristic of an instance of another one: it has
         // instances of its own and no single value to compare against.
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_RSMAP_PARAM_IS_RSMAP_SS, name, param_name.c_str());
         ret = false;
      } else if (lGetUlong(char_cep, CE_requestable) == REQU_NO) {
         // The complex says it may not be requested, and matching an instance against it is
         // requesting it. FORCED is a different matter and is not checked here: it is enforced
         // against the host's and queue's complex_values and the job's request sets, and a
         // bracket parameter is neither - so matching a characteristic does not satisfy a
         // FORCED complex.
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_RSMAP_PARAM_NOT_REQUESTABLE_SS, name, param_name.c_str());
         ret = false;
      } else if (lGetUlong(char_cep, CE_consumable) != CONSUMABLE_NO) {
         // A consumable characteristic would have to be booked as well as compared, and
         // nothing books one. This also disposes of an EXCL boolean, since EXCL implies
         // consumable. Refused rather than ignored, so that a request which would quietly
         // select nothing is not accepted.
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_RSMAP_PARAM_CONSUMABLE_SS, name, param_name.c_str());
         ret = false;
      }
   }

   // scope= widens a same= constraint and has nothing of its own to say. On a request which
   // carries no same= it would be accepted and read by nothing, which is the quiet failure the
   // parameter list is meant to avoid - so it is refused here rather than ignored.
   if (seen.count(RSMAP_REQUEST_PARAM_SCOPE) > 0 && seen.count(RSMAP_REQUEST_PARAM_SAME) == 0) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_RSMAP_PARAM_SCOPE_WITHOUT_SAME_SS, name,
                              RSMAP_REQUEST_PARAM_SCOPE);
      ret = false;
   }

   return ret;
}

/**
 * @brief resolve the characteristics a request asks for into something an instance can be
 *        matched against
 *
 * A bracket parameter which is not a reserved word names a complex, and the request is matched
 * against the characteristic of that name on an instance. Doing that needs more than the
 * request holds: the comparison uses the type and the relation operator of the characteristic's
 * own complex, and an instance stores only a name, a type and a value - never an operator. So
 * the complex is looked up here, once per request, rather than once per instance.
 *
 * Each requirement returned carries the canonical name, the type and the operator of that
 * complex, and the requested value parsed as that type. Nothing is returned for a request which
 * names no characteristic, which is the ordinary case.
 *
 * The parameters are assumed well formed: centry_rsmap_check_request_params() has refused a
 * request which is not, and refuses a name which resolves to nothing, to a resource map, to a
 * consumable, or to a complex which may not be requested. A name which somehow reaches here
 * without resolving is skipped rather than guessed at.
 *
 * @param centry              the request (CE_Type), its value holding the parameter list
 * @param master_centry_list  the complex list the names are resolved against
 * @param required            out: a new CE_Type list, untouched when the request names none
 * @return                    true when the request named at least one characteristic
 */
bool
centry_rsmap_resolve_request_properties(const lListElem *centry, const lList *master_centry_list,
                                        lList **required) {
   if (centry == nullptr || required == nullptr) {
      return false;
   }

   const char *open;
   const char *close;
   if (!centry_rsmap_find_params(lGetString(centry, CE_stringval), &open, &close)) {
      return false;
   }

   std::vector<std::string> params;
   centry_rsmap_split_params(open, close, params);

   lList *resolved = nullptr;
   for (const std::string &param : params) {
      const size_t eq = param.find('=');
      if (eq == std::string::npos || eq == 0) {
         continue;
      }
      const std::string param_name = param.substr(0, eq);
      if (centry_rsmap_is_reserved_param(param_name.c_str())) {
         continue;
      }

      const lListElem *char_cep = centry_list_locate(master_centry_list, param_name.c_str());
      if (char_cep == nullptr) {
         continue;
      }

      lListElem *req_ep = lAddElemStr(&resolved, CE_name, lGetString(char_cep, CE_name), CE_Type);
      if (req_ep == nullptr) {
         lFreeList(&resolved);
         return false;
      }
      lSetUlong(req_ep, CE_valtype, lGetUlong(char_cep, CE_valtype));
      lSetUlong(req_ep, CE_relop, lGetUlong(char_cep, CE_relop));
      lSetString(req_ep, CE_stringval, param.substr(eq + 1).c_str());

      // parse the value as the type of the complex, so that a numeric comparison has an amount
      // to work with. A value which does not parse was refused at submission; here it simply
      // compares as the zero centry_fill_and_check leaves behind.
      lList *sub_answers = nullptr;
      centry_fill_and_check(req_ep, &sub_answers, false, false);
      lFreeList(&sub_answers);
   }

   if (resolved == nullptr) {
      return false;
   }

   *required = resolved;
   return true;
}

/**
 * @brief does this request want its same= constraint to hold across the whole job?
 *
 * scope= takes "host" or "job". "host" is what same= has always meant - the instances granted
 * on one host have to agree, and a job spanning two hosts may well end up with a different
 * group on each - and is what a request which names no scope gets.
 *
 * Asked of the request in hand rather than looked up by name, for the reason
 * gru_list_add_request() gives: a constraint written in one request scope belongs to that
 * scope, and a lookup by name would have to pick one of them.
 *
 * @param centry  the request
 * @return        true when the request carries scope=job
 */
bool
centry_rsmap_request_is_job_scope(const lListElem *centry) {
   DSTRING_STATIC(scope, 32);

   if (!centry_rsmap_get_request_param(centry, RSMAP_REQUEST_PARAM_SCOPE, &scope)) {
      return false;
   }
   return strcmp(sge_dstring_get_string(&scope), RSMAP_REQUEST_PARAM_SCOPE_JOB) == 0;
}

/**
 * @brief read one parameter out of the bracketed list of a resource map request
 *
 * The parameters live in CE_stringval after the amount, "4[same=id,memory=40G]". They are read
 * back out on demand rather than stored anywhere, because a request is a spooled object and
 * caching the parsed form in a field of it would be a cull change - which the maintenance branch
 * cannot take.
 *
 * That makes the call site responsible for the cost. This is a scan of the value, so it is cheap
 * once and not cheap per queue instance: ri_time_by_slots() runs per request, per layer and per
 * queue instance, and in the parallel path again per slot count probe. Call it where the answer
 * is needed once - a per host constraint belongs at the host layer - rather than inside the
 * innermost loop.
 *
 * The list has already been validated by centry_rsmap_check_request_params() by the time anything
 * asks for a parameter, so this does not report errors - a malformed list simply has no
 * parameters to find.
 *
 * @param centry  the request
 * @param param   name of the parameter, e.g. RSMAP_REQUEST_PARAM_SAME
 * @param value   filled in with the value if the parameter is present; untouched otherwise
 * @return        true if the parameter is present
 */
bool
centry_rsmap_get_request_param(const lListElem *centry, const char *param, dstring *value) {
   const char *s = lGetString(centry, CE_stringval);

   if (s == nullptr || param == nullptr) {
      return false;
   }

   const char *open = strchr(s, '[');
   if (open == nullptr) {
      return false;
   }

   const size_t param_len = strlen(param);
   const char *p = open + 1;
   int depth = 0;

   while (*p != '\0') {
      // the start of an entry: does it begin with "<param>=" at this level?
      if (depth == 0 && strncmp(p, param, param_len) == 0 && *(p + param_len) == '=') {
         const char *v = p + param_len + 1;
         const char *end = v;
         int d = 0;
         while (*end != '\0' && !(d == 0 && (*end == ',' || *end == ']'))) {
            if (*end == '[') {
               d++;
            } else if (*end == ']') {
               d--;
            }
            ++end;
         }
         sge_dstring_clear(value);
         sge_dstring_sprintf(value, "%.*s", (int)(end - v), v);
         return true;
      }

      // skip to the start of the next entry at this level
      while (*p != '\0' && !(depth == 0 && *p == ',')) {
         if (*p == '[') {
            depth++;
         } else if (*p == ']') {
            depth--;
            if (depth < 0) {
               return false;
            }
         }
         ++p;
      }
      if (*p == ',') {
         ++p;
      }
   }

   return false;
}


/**
 * @brief the value an instance is grouped by for a same= constraint
 *
 * "Every instance I am granted must carry the same key" is one mechanism with the key read in
 * two ways. For same=id the key is the identifier itself, which is why that case groups nothing:
 * the reader has already folded repeated identifiers into one element, so every group has
 * exactly one member. For same=<characteristic> the key is that characteristic's value on the
 * instance, and several identifiers can share one.
 *
 * An instance which does not carry the characteristic has no key. It cannot satisfy the
 * constraint - there is nothing to agree with - so it is left out rather than being treated as
 * a group of its own, which would let a job be granted instances that agree about nothing.
 *
 * @param defined_ep  one instance of the map, a RESL_Type element of CE_resource_map_list
 * @param key_name    nullptr or "id" for the identifier, otherwise a characteristic name
 * @return            the key, or nullptr when the instance cannot take part
 */
static const char *
centry_rsmap_instance_key(const lListElem *defined_ep, const char *key_name) {
   if (key_name == nullptr || strcmp(key_name, RSMAP_REQUEST_PARAM_ID) == 0) {
      return lGetString(defined_ep, RESL_value);
   }

   const lListElem *property = lGetSubStr(defined_ep, CE_name, key_name, RESL_properties);
   if (property == nullptr) {
      return nullptr;
   }
   return lGetString(property, CE_stringval);
}

/**
 * @brief how many instances of one element of a resource map are free
 *
 * What is not free is passed in rather than read from a host, so that this stays arithmetic on
 * two lists. Who holds an instance, and over what period, is a scheduling question and is
 * answered where the utilization and the resource diagram are - see utilization_rsmap_max().
 *
 * @param defined_ep the instance, from CE_resource_map_list
 * @param taken      the identifiers which are spoken for and how many of each, may be nullptr
 * @return           the configured amount less what is taken of it
 */
static uint32_t
centry_rsmap_instance_free(const lListElem *defined_ep, const lList *taken) {
   uint32_t free = lGetUlong(defined_ep, RESL_amount);

   if (taken != nullptr) {
      const char *id = lGetString(defined_ep, RESL_value);
      const lListElem *used_ep = lGetElemStr(taken, RESL_value, id);
      if (used_ep != nullptr) {
         const uint32_t used = lGetUlong(used_ep, RESL_amount);
         // RESL_amount is unsigned: an id taken beyond its count must read as full rather
         // than wrap round to an enormous free amount
         free = (used >= free) ? 0 : free - used;
      }
   }

   return free;
}

/**
 * @brief the identifiers a resource map groups its instances by
 *
 * For same=id every identifier is its own group; for same=<characteristic> the instances
 * carrying the same value of that characteristic form one. An instance which does not carry the
 * characteristic belongs to no group and cannot satisfy the constraint, so it is left out.
 *
 * @param resource_definition the resource map on the host, from EH_consumable_config_list
 * @param key_name            "id" or the name of a characteristic
 * @return                    the distinct keys, an ST_Type list, to be freed by the caller
 */
lList *
centry_rsmap_group_keys(const lListElem *resource_definition, const char *key_name) {
   lList *keys = nullptr;

   const lListElem *defined_ep;
   for_each_ep (defined_ep, lGetList(resource_definition, CE_resource_map_list)) {
      const char *key = centry_rsmap_instance_key(defined_ep, key_name);
      if (key != nullptr && lGetElemStr(keys, ST_name, key) == nullptr) {
         lAddElemStr(&keys, ST_name, key, ST_Type);
      }
   }

   return keys;
}

/**
 * @brief compare a requested value against the one an instance carries
 *
 * The direction is the one an ordinary resource request uses - requested relop configured - and
 * the operator is the one the characteristic's own complex declares, so none appears in the
 * request and no table of them is written here. A complex defined <= makes "[memory=40G]" mean
 * at least 40G; one defined >= makes "[gpu_temp=40]" mean no hotter than 40.
 *
 * Whether a value may be a pattern follows from the type, which is what the string types are
 * there to distinguish: RESTRING is matched as an expression - wildcards combined with !, & and
 * | - CSTRING without regard to case, STRING exactly, and HOST by the host name rules.
 *
 * @note This dispatches on the type itself rather than handing every string type to
 *       sge_eval_expression(), which decides whether to match a pattern by looking at the
 *       value instead: is_expression() asks whether the requested string contains wildcards,
 *       and MatchPattern() then applies fnmatch() to a STRING as readily as to a RESTRING. A
 *       characteristic follows the type, so that STRING and RESTRING mean different things -
 *       otherwise the two types would be the same one.
 *
 * @note An ordering operator on a string type compares a match indicator rather than the
 *       strings, because that is what a string comparison yields here. A string characteristic
 *       is meant to be defined == or !=.
 *
 * @param type    the type of the characteristic's complex
 * @param relop   the operator that complex declares
 * @param request the requested value (CE_stringval, CE_doubleval)
 * @param offer   the value the instance carries
 * @return        true when the instance satisfies the request
 */
static bool
centry_rsmap_value_matches(ocs::CEntry::Type type, uint32_t relop, const lListElem *request,
                           const lListElem *offer) {
   int cmp;

   switch (type) {
      case ocs::CEntry::Type::STR:
      case ocs::CEntry::Type::CSTR:
      case ocs::CEntry::Type::HOST:
      case ocs::CEntry::Type::RESTR: {
         const char *req_str = lGetString(request, CE_stringval);
         const char *off_str = lGetString(offer, CE_stringval);
         if (req_str == nullptr || off_str == nullptr) {
            return false;
         }
         switch (type) {
            case ocs::CEntry::Type::RESTR:
               // the only string type which takes a pattern, and it takes a whole expression
               cmp = sge_eval_expression(ocs::CEntry::Type::RESTR, req_str, off_str, nullptr);
               break;
            case ocs::CEntry::Type::CSTR:
               cmp = (strcasecmp(req_str, off_str) == 0) ? 0 : 1;
               break;
            case ocs::CEntry::Type::HOST:
               cmp = (sge_hostcmp(req_str, off_str) == 0) ? 0 : 1;
               break;
            default:
               cmp = (strcmp(req_str, off_str) == 0) ? 0 : 1;
               break;
         }
         break;
      }
      default: {
         const double req = lGetDouble(request, CE_doubleval);
         const double off = lGetDouble(offer, CE_doubleval);
         cmp = (req < off) ? -1 : ((req > off) ? 1 : 0);
         break;
      }
   }

   switch (relop) {
      case CMPLXEQ_OP: return cmp == 0;
      case CMPLXNE_OP: return cmp != 0;
      case CMPLXLE_OP: return cmp <= 0;
      case CMPLXLT_OP: return cmp < 0;
      case CMPLXGE_OP: return cmp >= 0;
      case CMPLXGT_OP: return cmp > 0;
      default:         return false;
   }
}

/**
 * @brief whether an instance carries the characteristics a request asks for
 *
 * Every parameter which names a complex has to be satisfied, and an instance which does not
 * carry that characteristic at all never satisfies it - there is nothing to compare against,
 * and treating a missing value as acceptable would hand the job an instance the request was
 * written to avoid.
 *
 * The requirements arrive resolved: each carries the name, the type and the operator of the
 * characteristic's complex, looked up once per request rather than once per instance.
 *
 * @param defined_ep the instance, from CE_resource_map_list
 * @param required   the resolved requirements, or nullptr when the request names none
 * @return           true when the instance satisfies all of them
 */
static bool
centry_rsmap_instance_matches_properties(const lListElem *defined_ep, const lList *required) {
   if (required == nullptr) {
      return true;
   }

   const lListElem *req_ep;
   for_each_ep (req_ep, required) {
      const char *pname = lGetString(req_ep, CE_name);
      const lListElem *offer = lGetSubStr(defined_ep, CE_name, pname, RESL_properties);
      if (offer == nullptr) {
         return false;
      }
      const auto type = static_cast<ocs::CEntry::Type>(lGetUlong(req_ep, CE_valtype));
      if (!centry_rsmap_value_matches(type, lGetUlong(req_ep, CE_relop), req_ep, offer)) {
         return false;
      }
   }

   return true;
}

/**
 * @brief whether an instance's identifier is one an id= request accepts
 *
 * The expression is the one written in the request - shell wildcards combined with !, & and |,
 * and grouped with brackets - and it is asked of the identifier of the instance. Without an
 * expression every instance qualifies, which is what every request that does not name one wants.
 *
 * Asked at every point which looks at an instance, so that what matching counts and what the
 * selection takes are the same set. It is the request which is evaluated, never a host state,
 * so both sides reach the same answer from the same request.
 *
 * @param defined_ep the instance, from CE_resource_map_list
 * @param id_expr    the expression, or nullptr when the request names none
 * @return           true when the instance may be used
 */
static bool
centry_rsmap_instance_matches_id(const lListElem *defined_ep, const char *id_expr) {
   if (id_expr == nullptr) {
      return true;
   }

   const char *id = lGetString(defined_ep, RESL_value);
   if (id == nullptr) {
      return false;
   }

   return sge_eval_expression(ocs::CEntry::Type::CSTR, id_expr, id, nullptr) == 0;
}

/**
 * @brief how many instances of a resource map an id= request may use
 *
 * The whole map when the request names no expression, and what the instances it accepts have
 * free between them when it does. This is the unconstrained counterpart of
 * centry_rsmap_group_free(): the instances need not agree in anything, they only have to be
 * ones the request allows.
 *
 * @param resource_definition the resource map on the host, from EH_consumable_config_list
 * @param taken               the identifiers which are spoken for, may be nullptr
 * @param id_expr             the expression over the identifiers, or nullptr for all of them
 * @param required_props      nullptr when the request names no characteristic, otherwise the
 *                            resolved requirements an instance has to carry
 * @return                    the free instances the request may take
 */
uint32_t
centry_rsmap_free(const lListElem *resource_definition, const lList *taken, const char *id_expr,
                  const lList *required_props) {
   uint32_t free = 0;

   if (resource_definition == nullptr) {
      return 0;
   }

   const lListElem *defined_ep;
   for_each_ep (defined_ep, lGetList(resource_definition, CE_resource_map_list)) {
      if (!centry_rsmap_instance_matches_id(defined_ep, id_expr) ||
          !centry_rsmap_instance_matches_properties(defined_ep, required_props)) {
         continue;
      }
      free += centry_rsmap_instance_free(defined_ep, taken);
   }

   return free;
}

/**
 * @brief how many instances of one group of a resource map are free
 *
 * The group counterpart of centry_rsmap_instance_free(): what the instances sharing one key
 * have free between them. This is what a same= request asks for - the instances it is granted
 * have to come from one group, so what matters is what one group can serve, not the map.
 *
 * @param resource_definition the resource map on the host, from EH_consumable_config_list
 * @param taken               the identifiers which are spoken for, may be nullptr
 * @param key_name            "id" or the name of a characteristic
 * @param key                 which group
 * @param id_expr             nullptr when the request carries no id= parameter, otherwise the
 *                            expression an instance's identifier has to satisfy
 * @param required_props      nullptr when the request names no characteristic, otherwise the
 *                            resolved requirements an instance has to carry
 * @return                    the free instances of that group
 */
uint32_t
centry_rsmap_group_free(const lListElem *resource_definition, const lList *taken,
                        const char *key_name, const char *key, const char *id_expr,
                        const lList *required_props) {
   uint32_t free = 0;

   if (resource_definition == nullptr || key == nullptr) {
      return 0;
   }

   const lListElem *defined_ep;
   for_each_ep (defined_ep, lGetList(resource_definition, CE_resource_map_list)) {
      if (!centry_rsmap_instance_matches_id(defined_ep, id_expr) ||
          !centry_rsmap_instance_matches_properties(defined_ep, required_props)) {
         continue;
      }
      const char *instance_key = centry_rsmap_instance_key(defined_ep, key_name);
      if (instance_key != nullptr && strcmp(instance_key, key) == 0) {
         free += centry_rsmap_instance_free(defined_ep, taken);
      }
   }

   return free;
}

/**
 * @brief find the key of a resource map with the most free instances
 *
 * The general form of centry_rsmap_best_free_id(): same=id asks for the identifier with the
 * most free instances, same=<characteristic> for the characteristic value whose instances have
 * the most free between them.
 *
 * The group is named by its key and not handed back as a list of its members. Neither caller
 * needs the members: the matching side wants the count, and the booking side walks the map
 * again taking from whatever carries the key. Returning a list would mean an allocation on a
 * path which runs once per host per probe, and a question about how long the identifiers in it
 * stay valid, for nothing.
 *
 * The identifier case keeps its own loop rather than going through the accumulator. Every group
 * has one member there, so there is nothing to add up, and it is the case every existing request
 * takes - it should not start paying for a generalization it does not use.
 *
 * @param resource_definition  the resource map on the host, from EH_consumable_config_list
 * @param taken                the identifiers which are already spoken for, may be nullptr
 * @param key_name             nullptr or "id" for the identifier, otherwise a characteristic
 * @param free_amount          out: the free count of the group returned, 0 if there is none
 * @param id_expr              nullptr when the request carries no id= parameter, otherwise the
 *                             expression an instance's identifier has to satisfy
 * @param required_props       nullptr when the request names no characteristic, otherwise the
 *                             resolved requirements an instance has to carry
 * @return                     the key of the group with the most free instances, or nullptr
 */
const char *
centry_rsmap_best_free_group(const lListElem *resource_definition,
                             const lList *taken, const char *key_name,
                             uint32_t *free_amount, const char *id_expr,
                             const lList *required_props) {
   if (free_amount != nullptr) {
      *free_amount = 0;
   }
   if (resource_definition == nullptr) {
      return nullptr;
   }
   if (key_name == nullptr || strcmp(key_name, RSMAP_REQUEST_PARAM_ID) == 0) {
      return centry_rsmap_best_free_id(resource_definition, taken, free_amount, id_expr,
                                       required_props);
   }

   // sum the free instances per key. A resource map holds the devices of one host, so this is
   // a handful of entries and a linear scan beats hashing and copying the keys into strings
   std::vector<std::pair<const char *, uint32_t>> groups;

   const lListElem *defined_ep;
   for_each_ep (defined_ep, lGetList(resource_definition, CE_resource_map_list)) {
      if (!centry_rsmap_instance_matches_id(defined_ep, id_expr) ||
          !centry_rsmap_instance_matches_properties(defined_ep, required_props)) {
         continue;
      }
      const char *key = centry_rsmap_instance_key(defined_ep, key_name);
      if (key == nullptr) {
         continue;
      }
      const uint32_t free = centry_rsmap_instance_free(defined_ep, taken);

      bool found = false;
      for (auto &group : groups) {
         if (strcmp(group.first, key) == 0) {
            group.second += free;
            found = true;
            break;
         }
      }
      if (!found) {
         groups.emplace_back(key, free);
      }
   }

   const char *best_key = nullptr;
   uint32_t best_free = 0;
   for (const auto &group : groups) {
      if (best_key == nullptr || group.second > best_free) {
         best_key = group.first;
         best_free = group.second;
      }
   }

   if (free_amount != nullptr) {
      *free_amount = best_free;
   }
   return best_key;
}

/**
 * @brief take a number of instances, optionally restricted to one group
 *
 * The body shared by centry_rsmap_select_instances() and
 * centry_rsmap_select_group_instances(). With key_name nullptr every instance is a candidate;
 * otherwise only those whose key equals the one given.
 *
 * What an instance has free is what it is configured with, less what other jobs hold, less what
 * this job already holds - the last of those is not in the utilization yet, because nothing is
 * debited until the assignment is complete, and a map requested in more than one request scope
 * arrives here more than once.
 *
 * @param resource_definition  the resource map on the host, from EH_consumable_config_list
 * @param taken                the identifiers which are already spoken for, may be nullptr
 * @param already              what this job already holds of the map (RESL_Type), may be nullptr
 * @param key_name             nullptr, or "id" or the name of the characteristic to group by
 * @param key                  nullptr for every instance, otherwise the group to take from
 * @param amount               how many instances to take
 * @param selected             out: a new list of (RESL_value, RESL_amount), untouched on failure
 * @param id_expr              nullptr when the request carries no id= parameter, otherwise the
 *                             expression an instance's identifier has to satisfy
 * @param required_props       nullptr when the request names no characteristic, otherwise the
 *                             resolved requirements an instance has to carry
 * @return                     true when the amount was taken, false when it cannot be served
 */
static bool
centry_rsmap_take_instances(const lListElem *resource_definition,
                            const lList *taken, const lList *already,
                            const char *key_name, const char *key, uint32_t amount,
                            lList **selected, const char *id_expr,
                            const lList *required_props) {
   lList *chosen = nullptr;
   uint32_t remaining = amount;

   const lListElem *defined_ep;
   for_each_ep (defined_ep, lGetList(resource_definition, CE_resource_map_list)) {
      if (remaining == 0) {
         break;
      }
      if (key != nullptr) {
         const char *instance_key = centry_rsmap_instance_key(defined_ep, key_name);
         if (instance_key == nullptr || strcmp(instance_key, key) != 0) {
            continue;
         }
      }
      if (!centry_rsmap_instance_matches_id(defined_ep, id_expr) ||
          !centry_rsmap_instance_matches_properties(defined_ep, required_props)) {
         continue;
      }

      const char *id = lGetString(defined_ep, RESL_value);
      uint32_t free = centry_rsmap_instance_free(defined_ep, taken);

      if (already != nullptr) {
         const lListElem *mine = lGetElemStr(already, RESL_value, id);
         if (mine != nullptr) {
            const uint32_t held = lGetUlong(mine, RESL_amount);
            free = (held >= free) ? 0 : free - held;
         }
      }
      if (free == 0) {
         continue;
      }

      const uint32_t take = (free >= remaining) ? remaining : free;
      lListElem *ep = lAddElemStr(&chosen, RESL_value, id, RESL_Type);
      if (ep == nullptr) {
         lFreeList(&chosen);
         return false;
      }
      lSetUlong(ep, RESL_amount, take);
      remaining -= take;
   }

   if (remaining > 0) {
      lFreeList(&chosen);
      return false;
   }

   *selected = chosen;
   return true;
}

/**
 * @brief take a number of instances of a resource map, without any constraint between them
 *
 * The unconstrained counterpart of centry_rsmap_select_group_instances(): the instances are
 * taken in the order the map defines them, filling one identifier before touching the next, so
 * that whole identifiers stay free for the requests which need a whole one.
 *
 * @param resource_definition  the resource map on the host, from EH_consumable_config_list
 * @param taken                the identifiers which are already spoken for, may be nullptr
 * @param already              what this job already holds of the map (RESL_Type), may be nullptr
 * @param amount               how many instances to take
 * @param selected             out: a new list of (RESL_value, RESL_amount), untouched on failure
 * @param id_expr              nullptr when the request carries no id= parameter, otherwise the
 *                             expression an instance's identifier has to satisfy
 * @param required_props       nullptr when the request names no characteristic, otherwise the
 *                             resolved requirements an instance has to carry
 * @return                     true when the amount was taken, false when the map cannot serve it
 */
bool
centry_rsmap_select_instances(const lListElem *resource_definition,
                              const lList *taken, const lList *already,
                              uint32_t amount, lList **selected, const char *id_expr,
                              const lList *required_props) {
   if (resource_definition == nullptr || selected == nullptr || amount == 0) {
      return false;
   }
   return centry_rsmap_take_instances(resource_definition, taken, already,
                                      nullptr, nullptr, amount, selected, id_expr,
                                      required_props);
}

/**
 * @brief take a number of instances from one group of a resource map
 *
 * The selection half of the same= constraint. centry_rsmap_best_free_group() answers what a
 * group could serve; this takes the instances and says which they are.
 *
 * The answer is a list of (identifier, amount) pairs and not a list of identifiers, because the
 * reader folds repeated identifiers into one element with a count. Three instances from a node
 * made of two cards of two is two entries and not three.
 *
 * A request which names the map in more than one scope arrives here more than once, and the
 * later visits must not choose again: the group is read back from what has already been granted,
 * and what this job holds of each instance is subtracted along with what other jobs hold - the
 * former is not in the utilization yet, because nothing is debited until the assignment is
 * complete.
 *
 * Instances are taken in the order the map defines them, so one is filled before the next is
 * touched. That leaves whole identifiers free for the jobs which need a whole one, which is the
 * same thing the unconstrained path does.
 *
 * @param resource_definition  the resource map on the host, from EH_consumable_config_list
 * @param taken                the identifiers which are already spoken for, may be nullptr
 * @param already              what this job already holds of the map (RESL_Type), may be nullptr
 * @param key_name             nullptr or "id" for the identifier, otherwise a characteristic
 * @param amount               how many instances to take
 * @param selected             out: a new list of (RESL_value, RESL_amount), untouched on failure
 * @param id_expr              nullptr when the request carries no id= parameter, otherwise the
 *                             expression an instance's identifier has to satisfy
 * @param required_props       nullptr when the request names no characteristic, otherwise the
 *                             resolved requirements an instance has to carry
 * @param required_key         nullptr when this host may choose its own group, otherwise the
 *                             group the choice is confined to - what scope=job binds every host
 *                             of the job to, see rsmap_job_scope_key()
 * @return                     true when the amount was taken, false when no group can serve it
 */
bool
centry_rsmap_select_group_instances(const lListElem *resource_definition,
                                    const lList *taken, const lList *already,
                                    const char *key_name, uint32_t amount, lList **selected,
                                    const char *id_expr, const lList *required_props,
                                    const char *required_key) {
   if (resource_definition == nullptr || selected == nullptr || amount == 0) {
      return false;
   }

   // which group: the one an earlier request scope took from on this host, the one the job is
   // bound to everywhere, or the emptiest one. The first comes before the second because the
   // two can only disagree if the map changed underneath, and a request scope which has already
   // been granted instances cannot be given different ones now.
   const char *key;
   if (already != nullptr && lGetNumberOfElem(already) > 0) {
      const char *chosen_id = lGetString(lFirst(already), RESL_value);
      const lListElem *chosen_ep = lGetSubStr(resource_definition, RESL_value, chosen_id,
                                              CE_resource_map_list);
      if (chosen_ep == nullptr) {
         // the map changed under us between one scope and the next
         return false;
      }
      key = centry_rsmap_instance_key(chosen_ep, key_name);
   } else if (required_key != nullptr) {
      key = required_key;
   } else {
      uint32_t free_in_group = 0;
      key = centry_rsmap_best_free_group(resource_definition, taken, key_name,
                                         &free_in_group, id_expr, required_props);
   }
   if (key == nullptr) {
      return false;
   }

   return centry_rsmap_take_instances(resource_definition, taken, already,
                                      key_name, key, amount, selected, id_expr, required_props);
}

/**
 * @brief find the id of a resource map with the most free instances
 *
 * This is what "all the instances I am granted must carry the same id" reduces to. The reader
 * folds repeated identifiers into one element carrying a count, see store_resl() in the flatfile
 * reader, so "gpu=8(0 0 0 0 1 1 1 1)" is two elements of four rather than eight elements, and
 * the free count of an id is its configured amount less what is booked against it.
 *
 * The answer is a count rather than a yes or no because the scheduler wants both readings. A
 * request which takes its amount once - consumable JOB or HOST - only asks whether some id has
 * enough, while a per slot request asks how many slots one id can serve, which is the free count
 * divided by the amount. Returning the count lets the caller do either, and lets the constraint
 * take part in the ordinary "how many slots can this host offer" calculation instead of being a
 * separate filter beside it.
 *
 * Both the matching side and the side which chooses the instances call this, which is the point
 * of it living here. Matching asks how many slots a host can offer under the constraint and the
 * selection asks which instances serve it; one function answering both is what keeps a host from
 * being offered slots that the selection then cannot fill. The choice itself is made once and
 * recorded on the assignment for the booking to apply (CS-2805).
 *
 * @param resource_definition  the resource map on the host, from EH_consumable_config_list
 * @param taken                the identifiers which are already spoken for and how many of
 *                             each, may be nullptr when nothing is
 * @param free_amount          out: the free count of the id returned, 0 if there is none
 * @param id_expr              nullptr when the request carries no id= parameter, otherwise the
 *                             expression an instance's identifier has to satisfy
 * @param required_props       nullptr when the request names no characteristic, otherwise the
 *                             resolved requirements an instance has to carry
 * @return                     the id with the most free instances, or nullptr for an empty map
 */
const char *
centry_rsmap_best_free_id(const lListElem *resource_definition,
                          const lList *taken, uint32_t *free_amount, const char *id_expr,
                          const lList *required_props) {
   const char *best_id = nullptr;
   uint32_t best_free = 0;

   if (free_amount != nullptr) {
      *free_amount = 0;
   }
   if (resource_definition == nullptr) {
      return nullptr;
   }

   const lListElem *defined_ep;
   for_each_ep (defined_ep, lGetList(resource_definition, CE_resource_map_list)) {
      if (!centry_rsmap_instance_matches_id(defined_ep, id_expr) ||
          !centry_rsmap_instance_matches_properties(defined_ep, required_props)) {
         continue;
      }
      const char *id = lGetString(defined_ep, RESL_value);
      const uint32_t free = centry_rsmap_instance_free(defined_ep, taken);

      if (best_id == nullptr || free > best_free) {
         best_id = id;
         best_free = free;
      }
   }

   if (free_amount != nullptr) {
      *free_amount = best_free;
   }
   return best_id;
}

/**
 * Give an RSMAP that was configured with a bare amount the ids it is missing.
 *
 * An RSMAP value may be written as a plain number, e.g. "gpu=4". The amount is then
 * known but no instance is, and since the scheduler picks the granted instances from
 * CE_resource_map_list it cannot grant anything at all - it discards the whole granted
 * resource list of the task. Such a value is read as "4 instances named 0 to 3", which
 * is what an administrator writing it that way expects, and what the value would look
 * like when written back: "gpu=4(0-3)".
 *
 * A value that already carries ids is left untouched, and so is an amount of 0, which
 * legitimately means "no instances at all".
 *
 * The expansion is bounded. An RSMAP materialises one CULL element per id, all of which
 * are spooled and travel with the exec host object, so a mistyped or simply large amount
 * would otherwise cost the qmaster a lot of memory. Beyond max_ids the value is rejected
 * and the administrator has to list the ids or raise the limit. max_ids of 0 turns the
 * implicit ids off completely, so that a bare amount is rejected as an RSMAP that needs
 * to define its ids.
 *
 * @param answer_list  answer list; a rejection is appended with
 *                     STATUS_EUNKNOWN/ANSWER_QUALITY_ERROR
 * @param centry       CE_Type element of a host's complex_values entry. Requires
 *                     CE_valtype and CE_doubleval to be filled in, i.e. the element
 *                     must have passed centry_list_fill_request()
 * @param max_ids      upper bound for the number of ids to create, 0 disables the
 *                     implicit ids
 * @return             true if nothing had to be done or the ids were created,
 *                     false if the amount exceeds max_ids
 */
bool
centry_rsmap_expand_implicit_ids(lList **answer_list, lListElem *centry, uint32_t max_ids) {
   if (centry == nullptr || static_cast<ocs::CEntry::Type>(lGetUlong(centry, CE_valtype)) != ocs::CEntry::Type::RSMAP) {
      return true;
   }

   // ids have been configured - nothing to do
   if (lGetList(centry, CE_resource_map_list) != nullptr) {
      return true;
   }

   const double dval = lGetDouble(centry, CE_doubleval);
   if (dval <= 0) {
      // "gpu=0" - the host provides no instance, which is a valid configuration
      return true;
   }
   const auto amount = static_cast<uint32_t>(dval);

   if (max_ids == 0) {
      // the implicit ids are switched off - the instances have to be named explicitly
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_CONFIG_CONF_RSMAP_NEEDS_IDS_S, lGetString(centry, CE_name));
      return false;
   }
   if (amount > max_ids) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_RSMAP_TOO_MANY_IMPLICIT_IDS_SUU,
                              lGetString(centry, CE_name), amount, max_ids);
      return false;
   }

   for (uint32_t id = 0; id < amount; id++) {
      std::string id_str{std::to_string(id)};
      lListElem *resl = lAddSubStr(centry, RESL_value, id_str.c_str(),
                                   CE_resource_map_list, RESL_Type);
      if (resl != nullptr) {
         lSetUlong(resl, RESL_amount, 1);
      }
   }

   return true;
}

/**
 * Expand the implicit ids of every RSMAP in a host's complex_values list.
 *
 * Wrapper around centry_rsmap_expand_implicit_ids() which takes the limit from
 * MAX_RSMAP_IDS in qmaster_params. All entries are processed even if one of them is
 * rejected, so that an administrator sees every offending RSMAP at once.
 *
 * @param answer_list  answer list; rejections are appended
 * @param centry_list  CE_Type list of an exec host (EH_consumable_config_list)
 * @return             true if no entry was rejected
 */
bool
centry_list_rsmap_expand_implicit_ids(lList **answer_list, lList *centry_list) {
   bool ret = true;
   const auto max_ids = static_cast<uint32_t>(mconf_get_max_rsmap_ids());

   lListElem *centry;
   for_each_rw(centry, centry_list) {
      if (!centry_rsmap_expand_implicit_ids(answer_list, centry, max_ids)) {
         ret = false;
      }
   }

   return ret;
}
