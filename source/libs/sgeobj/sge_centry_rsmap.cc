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

#include <cstring>

#include <unordered_set>

#include <string>

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_resource_utilization.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "msg_common.h"

#include "sgeobj/sge_centry_rsmap.h"

bool centry_check_rsmap(lList **answer_list, u_long32 consumable, const char *attrname) {
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

   const char *open = strchr(value, '[');
   if (open == nullptr) {
      return true;
   }

   // a parameter list is meaningful only for a resource map
   if (lGetUlong(centry, CE_valtype) != TYPE_RSMAP) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_RSMAP_PARAM_NOT_RSMAP_SS, name, value);
      return false;
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
   const char *p = open + 1;

   while (p <= close) {
      // find the end of this parameter: the next comma at depth 0, or the closing bracket
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

      const std::string param(p, end - p);
      p = end + 1;

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

         // id=, scope= and distinct= are reserved so that the grammar has room for them, but
         // nothing reads any of them yet - same= is the only one the scheduler honours. Were
         // one of them accepted, a job which asked for a named instance, or for a constraint
         // across the whole job rather than per host, would run as though it had asked for
         // nothing at all. Refusing them keeps that from happening quietly, and a request the
         // system refuses today can be accepted later without breaking anything, which is not
         // true the other way round.
         if (param_name == RSMAP_REQUEST_PARAM_ID ||
             param_name == RSMAP_REQUEST_PARAM_SCOPE ||
             param_name == RSMAP_REQUEST_PARAM_DISTINCT) {
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
      if (centry_list_locate(master_centry_list, param_name.c_str()) == nullptr) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_RSMAP_PARAM_UNKNOWN_SS, name, param_name.c_str());
         ret = false;
      } else {
         // Nothing matches a request against the characteristics of an instance yet, so a
         // request naming one would select nothing and the job would run on whichever
         // instances happened to be free - the same quiet failure id= and scope= are refused
         // for above. CS-2733 implements the matching and lifts this.
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_RSMAP_PARAM_NOT_YET_SS, name, param_name.c_str());
         ret = false;
      }
   }

   return ret;
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
 * @brief does the job require any of its resource maps to be granted from one id?
 *
 * Answered from the request itself rather than from a flag on the job, so that it cannot fall
 * out of step with what the request says. It walks every request scope, because the constraint
 * is a statement about the resource map and holds over all of them.
 *
 * @param job  the job
 * @return     true if any hard request carries a same= parameter
 */
bool
centry_rsmap_job_has_same_constraint(const lListElem *job) {
   DSTRING_STATIC(param, 64);

   const lListElem *jrs;
   for_each_ep (jrs, lGetList(job, JB_request_set_list)) {
      const lListElem *req;
      for_each_ep (req, lGetList(jrs, JRS_hard_resource_list)) {
         if (lGetUlong(req, CE_valtype) == TYPE_RSMAP &&
             centry_rsmap_get_request_param(req, RSMAP_REQUEST_PARAM_SAME, &param)) {
            return true;
         }
      }
   }

   return false;
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
 * Both the matching and the booking side call this, which is the point of it living here. They
 * run against the same host configuration and the same utilization - add_granted_resource_list()
 * is called as soon as the assignment is fixed, before anything is debited - so they reach the
 * same answer without the choice having to be carried from one to the other. If they ever
 * disagreed a job would be granted a mixed set while matching believed otherwise, which is why
 * there is one function and not two.
 *
 * @param resource_definition  the resource map on the host, from EH_consumable_config_list
 * @param resource_utilization the matching element of EH_resource_utilization, may be nullptr
 *                             when nothing is booked yet
 * @param free_amount          out: the free count of the id returned, 0 if there is none
 * @return                     the id with the most free instances, or nullptr for an empty map
 */
const char *
centry_rsmap_best_free_id(const lListElem *resource_definition,
                          const lListElem *resource_utilization, u_long32 *free_amount) {
   const char *best_id = nullptr;
   u_long32 best_free = 0;

   if (free_amount != nullptr) {
      *free_amount = 0;
   }
   if (resource_definition == nullptr) {
      return nullptr;
   }

   const lListElem *defined_ep;
   for_each_ep (defined_ep, lGetList(resource_definition, CE_resource_map_list)) {
      const char *id = lGetString(defined_ep, RESL_value);
      u_long32 free = lGetUlong(defined_ep, RESL_amount);

      if (resource_utilization != nullptr) {
         const lListElem *used_ep = lGetSubStr(resource_utilization, RESL_value, id,
                                               RUE_utilized_now_resource_map_list);
         if (used_ep != nullptr) {
            const u_long32 used = lGetUlong(used_ep, RESL_amount);
            // RESL_amount is unsigned: an id booked beyond its count must read as full rather
            // than wrap round to an enormous free amount
            free = (used >= free) ? 0 : free - used;
         }
      }

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
 * @brief find an id which has at least the requested amount free
 *
 * The threshold reading of centry_rsmap_best_free_id(), for a request which takes its amount
 * once rather than per slot.
 *
 * @param resource_definition  the resource map on the host
 * @param resource_utilization what is booked against it, may be nullptr
 * @param amount               how many instances of one id are needed
 * @return                     the id, or nullptr if no single id has that many free
 */
const char *
centry_rsmap_find_id_with_free(const lListElem *resource_definition,
                               const lListElem *resource_utilization, u_long32 amount) {
   u_long32 best_free = 0;

   if (amount == 0) {
      return nullptr;
   }

   const char *best_id = centry_rsmap_best_free_id(resource_definition, resource_utilization,
                                                   &best_free);
   return (best_id != nullptr && best_free >= amount) ? best_id : nullptr;
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
centry_rsmap_expand_implicit_ids(lList **answer_list, lListElem *centry, u_long32 max_ids) {
   if (centry == nullptr || lGetUlong(centry, CE_valtype) != TYPE_RSMAP) {
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
   const auto amount = static_cast<u_long32>(dval);

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

   for (u_long32 id = 0; id < amount; id++) {
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
   const auto max_ids = static_cast<u_long32>(mconf_get_max_rsmap_ids());

   lListElem *centry;
   for_each_rw(centry, centry_list) {
      if (!centry_rsmap_expand_implicit_ids(answer_list, centry, max_ids)) {
         ret = false;
      }
   }

   return ret;
}
