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


#include "tom/ocs_PrintCore.h"

#include <cstring>
#include <string>

#include "uti/sge_dstring.h"
#include "uti/sge_log.h"

#include "tom/cull_lerrno.h"
#include "tom/cull_lerrnoP.h"
#include "tom/cull_listP.h"
#include "tom/cull_multitypeP.h"
#include "tom/cull_name.h"
#include "tom/cull_whatP.h"
#include "tom/cull_whereP.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_ListCore.h"

namespace ocs::tom {

   namespace {

      /**
       * @brief The indentation of a nesting level
       * @param level the level
       * @return three blanks per level
       */
      std::string indent(const int level) {
         return std::string(static_cast<std::size_t>(level < 0 ? 0 : level) * 3, ' ');
      }

      /**
       * @brief Append a list without the check for null
       * @param buffer the string to append to
       * @param lp the list, not nullptr
       * @param level the nesting level
       */
      void append_list(dstring *buffer, const lList *lp, int level);

      /**
       * @brief Append an element without the check for null
       * @param buffer the string to append to
       * @param ep the element, not nullptr
       * @param level the nesting level
       */
      void append_elem(dstring *buffer, const lListElem *ep, const int level) {
         const std::string space = indent(level);
         const char *sp = space.c_str();
         sge_dstring_sprintf_append(buffer, "%s-------------------------------\n", sp);
         for (const auto f : ocs_cull_present_fields(ep->descr)) {
            const char *nm_str = lNm2Str(ep->descr[f.pos].nm);
            const char *name = nm_str != nullptr ? nm_str : "(null)";
            const lMultiType &value = ocs_cull_value(ep, f.pos);
            switch (mt_get_type(ep->descr[f.pos].mt)) {
               case lIntT:
                  sge_dstring_sprintf_append(buffer, "%s%-20.20s (Integer) = %d\n", sp, name, value.i);
                  break;
               case lUlongT:
                  sge_dstring_sprintf_append(buffer, "%s%-20.20s (Ulong)   = " sge_u32 "\n", sp, name, value.ul);
                  break;
               case lUlong64T:
                  sge_dstring_sprintf_append(buffer, "%s%-20.20s (Ulong64) = " sge_u64 "\n", sp, name, value.ul64);
                  break;
               case lStringT:
                  sge_dstring_sprintf_append(buffer, "%s%-20.20s (String)  = %s\n", sp, name,
                                             value.str != nullptr ? value.str : "(null)");
                  break;
               case lHostT:
                  sge_dstring_sprintf_append(buffer, "%s%-20.20s (Host)    = %s\n", sp, name,
                                             value.host != nullptr ? value.host : "(null)");
                  break;
               case lListT:
                  sge_dstring_sprintf_append(buffer, "%s%-20.20s (List)    = %s\n", sp, name,
                                             value.glp != nullptr ? "full {" : "empty");
                  if (value.glp != nullptr) {
                     append_list(buffer, value.glp, level + 1);
                     sge_dstring_sprintf_append(buffer, "%s}\n", sp);
                  }
                  break;
               case lObjectT:
                  sge_dstring_sprintf_append(buffer, "%s%-20.20s (Object)  = %s\n", sp, name,
                                             value.obj != nullptr ? "object {" : "none");
                  if (value.obj != nullptr) {
                     append_elem(buffer, value.obj, level + 1);
                     sge_dstring_sprintf_append(buffer, "%s}\n", sp);
                  }
                  break;
               case lDoubleT:
                  sge_dstring_sprintf_append(buffer, "%s%-20.20s (Double)  = %f\n", sp, name, value.db);
                  break;
               case lLongT:
                  sge_dstring_sprintf_append(buffer, "%s%-20.20s (Long)    = %ld\n", sp, name, value.l);
                  break;
               case lBoolT:
                  sge_dstring_sprintf_append(buffer, "%s%-20.20s (Bool)    = %s\n", sp, name,
                                             value.b ? "true" : "false");
                  break;
               case lRefT:
                  sge_dstring_sprintf_append(buffer, "%s%-20.20s (Ref)     = %p\n", sp, name, value.ref);
                  break;
               default:
                  unknownType("lWriteElem");
            }
         }
      }

      void append_list(dstring *buffer, const lList *lp, const int level) {
         const char *name = list_name(lp);
         sge_dstring_sprintf_append(buffer, "\n%sList: <%s> #Elements: %d\n", indent(level).c_str(),
                                    name != nullptr ? name : "nullptr", static_cast<int>(lp->nelem));
         for (const lListElem *ep = lp->first; ep != nullptr; ep = ep->next) {
            append_elem(buffer, ep, level);
         }
      }

      /**
       * @brief Append a selection without the check for null
       * @param buffer the string to append to
       * @param what the selection, not nullptr
       * @param level the nesting level
       */
      void append_what(dstring *buffer, const lEnumeration *what, const int level) {
         for (int i = 0; mt_get_type(what[i].mt) != lEndT; i++) {
            sge_dstring_append(buffer, indent(level).c_str());
            const char *name;
            switch (what[i].pos) {
               case WHAT_NONE:
                  name = "NONE";
                  break;
               case WHAT_ALL:
                  name = "ALL";
                  break;
               default:
                  name = lNm2Str(what[i].nm);
                  break;
            }
            sge_dstring_sprintf_append(buffer, "nm: %6d %-20.20s mt: %7d pos: %3d\n", what[i].nm, name, what[i].mt,
                                       what[i].pos);
            if (what[i].ep != nullptr) {
               append_what(buffer, what[i].ep, level + 1);
            }
         }
      }

      /**
       * @brief The operator of a comparison as text
       * @param op the operator
       * @return the text, or nullptr for an operator the core does not know
       */
      const char *compare_op_text(const int op) {
         switch (op) {
            case EQUAL:
               return "==";
            case NOT_EQUAL:
               return "!=";
            case LOWER_EQUAL:
               return "<=";
            case LOWER:
               return "<";
            case GREATER_EQUAL:
               return ">=";
            case GREATER:
               return ">";
            case BITMASK:
               return "m=";
            case STRCASECMP:
               return "c=";
            case PATTERNCMP:
               return "p=";
            case HOSTNAMECMP:
               return "h=";
            default:
               return nullptr;
         }
      }

      /**
       * @brief Append a condition; a null one sets LECONDNULL and appends nothing
       * @param buffer the string to append to
       * @param cp the condition, or nullptr
       * @param level the nesting level
       */
      void append_where(dstring *buffer, const lCondition *cp, const int level) {
         if (cp == nullptr) {
            set_lerrno(LECONDNULL);
            return;
         }
         const std::string space = indent(level);
         const char *sp = space.c_str();
         switch (cp->op) {
            case EQUAL:
            case NOT_EQUAL:
            case LOWER_EQUAL:
            case LOWER:
            case GREATER_EQUAL:
            case GREATER:
            case BITMASK:
            case STRCASECMP:
            case PATTERNCMP:
            case HOSTNAMECMP:
            case SUBSCOPE: {
               // the line is cut at 255 bytes, as before
               char out[256];
               snprintf(out, sizeof(out), "%s %s(%d) ", sp, lNm2Str(cp->operand.cmp.nm), cp->operand.cmp.nm);
               if (cp->op == SUBSCOPE) {
                  sge_dstring_sprintf_append(buffer, "%s ->\n", out);
                  append_where(buffer, cp->operand.cmp.val.cp, level + 1);
               } else {
                  const std::size_t used = strlen(out);
                  snprintf(&out[used], sizeof(out) - used, "%s", compare_op_text(cp->op));
               }
               const lMultiType &val = cp->operand.cmp.val;
               switch (mt_get_type(cp->operand.cmp.mt)) {
                  case lIntT:
                     sge_dstring_sprintf_append(buffer, "%s %d\n", out, val.i);
                     break;
                  case lUlongT:
                     sge_dstring_sprintf_append(buffer, "%s " sge_u32 "\n", out, val.ul);
                     break;
                  case lUlong64T:
                     sge_dstring_sprintf_append(buffer, "%s " sge_u64 "\n", out, val.ul64);
                     break;
                  case lStringT:
                     sge_dstring_sprintf_append(buffer, "%s \"%s\"\n", out, val.str);
                     break;
                  case lHostT:
                     sge_dstring_sprintf_append(buffer, "%s \"%s\"\n", out, val.host);
                     break;
                  case lListT:
                     break;
                  case lDoubleT:
                     sge_dstring_sprintf_append(buffer, "%s %f\n", out, val.db);
                     break;
                  case lLongT:
                     sge_dstring_sprintf_append(buffer, "%s %ld\n", out, val.l);
                     break;
                  case lBoolT:
                     sge_dstring_sprintf_append(buffer, "%s %s\n", out, val.b ? "true" : "false");
                     break;
                  case lRefT:
                     sge_dstring_sprintf_append(buffer, "%s %p\n", out, val.ref);
                     break;
                  default:
                     unknownType("lWriteWhere");
                     return;
               }
               break;
            }
            case AND:
            case OR:
               sge_dstring_sprintf_append(buffer, "%s(\n", sp);
               append_where(buffer, cp->operand.log.first, level + 1);
               sge_dstring_sprintf_append(buffer, "%s%s\n", sp, cp->op == AND ? "&&" : "||");
               append_where(buffer, cp->operand.log.second, level + 1);
               sge_dstring_sprintf_append(buffer, "%s)\n", sp);
               break;
            case NEG:
               sge_dstring_sprintf_append(buffer, "%s!(\n", sp);
               append_where(buffer, cp->operand.log.first, level + 1);
               sge_dstring_sprintf_append(buffer, "%s)\n", sp);
               break;
            default:
               set_lerrno(LEOPUNKNOWN);
               return;
         }
      }

   } // namespace

   void print_elem(dstring *buffer, const lListElem *ep, const int level) {
      if (ep == nullptr) {
         set_lerrno(LEELEMNULL);
         throw ListCoreError(LEELEMNULL, "printing a null element");
      }
      append_elem(buffer, ep, level);
   }

   void print_list(dstring *buffer, const lList *lp, const int level) {
      if (lp == nullptr) {
         set_lerrno(LELISTNULL);
         throw ListCoreError(LELISTNULL, "printing a null list");
      }
      append_list(buffer, lp, level);
   }

   void print_what(dstring *buffer, const lEnumeration *what, const int level) {
      if (what == nullptr) {
         set_lerrno(LEENUMNULL);
         throw ListCoreError(LEENUMNULL, "printing a null selection");
      }
      append_what(buffer, what, level);
   }

   void print_where(dstring *buffer, const lCondition *cp, const int level) {
      if (cp == nullptr) {
         set_lerrno(LECONDNULL);
         throw ListCoreError(LECONDNULL, "printing a null condition");
      }
      append_where(buffer, cp, level);
   }

} // namespace ocs::tom
