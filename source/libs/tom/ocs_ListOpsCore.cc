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


#include "tom/ocs_ListOpsCore.h"

#include <cstring>
#include <span>
#include <string>

#include "uti/sge_string.h"

#include "tom/cull_listP.h"
#include "tom/cull_multitypeP.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_FieldException.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_SelectCore.h"
#include "tom/ocs_SortCore.h"

namespace ocs::tom {

   namespace {

      /**
       * @brief Free the saved state of sge_strtok_r() when the parsing ends, also by an exception
       */
      struct TokenState {
         saved_vars_s *context = nullptr;   ///< the state, or nullptr

         /** @brief Free the state */
         ~TokenState() {
            if (context != nullptr) {
               sge_free_saved_vars(context);
            }
         }
      };

      /**
       * @brief The first element of a list holding a name in a string or host field
       * @param lp the list, or nullptr
       * @param nm the field
       * @param type lStringT or lHostT
       * @param name the name
       * @return the element, or nullptr
       */
      const lListElem *find_name(const lList *lp, const int nm, const int type, const char *name) {
         if (lp == nullptr) {
            return nullptr;
         }
         const int pos = present_pos(lp->descr, nm);
         if (pos < 0) {
            return nullptr;
         }
         const void *iterator = nullptr;
         return type == lStringT ? lookup_first_str(lp, pos, name, &iterator)
                                 : lookup_first_host(lp, pos, name, &iterator);
      }

   } // namespace

   void string_to_list(const char *s, lList *&lp, const lDescr *descr, const int nm, const char *delimiters) {
      if (s == nullptr) {
         return;
      }
      const int pos = present_pos(descr, nm);
      const int type = pos >= 0 ? mt_get_type(descr[pos].mt) : static_cast<int>(lEndT);
      if (type != lStringT && type != lHostT) {
         return;
      }
      TokenState state;
      try {
         for (const char *name = sge_strtok_r(s, delimiters, &state.context); name != nullptr;
              name = sge_strtok_r(nullptr, delimiters, &state.context)) {
            // a repeated name is skipped
            if (find_name(lp, nm, type, name) != nullptr) {
               continue;
            }
            if (type == lStringT) {
               add_elem_str(lp, nm, name, descr);
            } else {
               add_elem_host(lp, nm, name, descr);
            }
         }
      } catch (...) {
         free_list(lp);
         throw;
      }
   }

   void string_to_list_none(const char *s, lList *&lp, const lDescr *descr, const int nm, const char *delimiters) {
      string_to_list(s, lp, descr, nm, delimiters);
      if (lp == nullptr) {
         return;
      }
      const int pos = present_pos(descr, nm);
      const int type = pos >= 0 ? mt_get_type(descr[pos].mt) : static_cast<int>(lEndT);
      const lListElem *none = nullptr;
      if (type == lStringT) {
         none = lookup_first_str_nocase(lp, nm, "none");
      } else if (type == lHostT) {
         none = find_name(lp, nm, lHostT, "none");
      }
      if (none == nullptr) {
         return;
      }
      const bool alone = lp->nelem == 1;
      free_list(lp);
      if (!alone) {
         throw ListCoreError(0, "\"none\" stands beside other names");
      }
   }

   lList *split_off(lList *lp, const lCondition *cp, const char *name) {
      lList *rest = nullptr;
      for (lListElem *ep = lp->first; ep != nullptr;) {
         lListElem *next = ep->next;
         if (!compare(ep, cp)) {
            if (rest == nullptr) {
               rest = create_list(name, lp->descr, true);
            }
            dechain_elem(lp, ep);
            append_elem(rest, ep);
         }
         ep = next;
      }
      return rest;
   }

   void split_list(lList *&slp, lList **ulp, const char *ulp_name, const lCondition *cp) {
      if (slp == nullptr) {
         return;
      }
      lList *rest = split_off(slp, cp, ulp_name != nullptr ? ulp_name : "ulp");
      if (ulp == nullptr) {
         free_list(rest);
      } else if (*ulp == nullptr) {
         *ulp = rest;
      } else if (rest != nullptr) {
         append_list(*ulp, rest);
         free_list(rest);
      }
      if (slp->nelem == 0) {
         free_list(slp);
      }
   }

   void remove_common_str(lList *&lp1, lList *&lp2, const int nm, const bool free_empty) {
      if (lp1 == nullptr || lp2 == nullptr) {
         return;
      }
      for (lListElem *ep = lp1->first; ep != nullptr && lp1 != nullptr && lp2 != nullptr;) {
         const char *key = ocs_cull_value(ep, checked_pos(ep, nm, FieldType::String)).str;
         // step on first, the element may be removed
         ep = ep->next;
         if (key == nullptr || find_name(lp2, nm, lStringT, key) == nullptr) {
            continue;
         }
         // the key of the element removed from lp1 is freed with it
         const std::string held(key);
         remove_elem_str(lp2, nm, held.c_str());
         remove_elem_str(lp1, nm, held.c_str());
         if (free_empty) {
            if (lp2->nelem == 0) {
               free_list(lp2);
            }
            if (lp1->nelem == 0) {
               free_list(lp1);
            }
         }
      }
   }

   void unique_by_key(lList *lp, const int nm) {
      const int pos = present_pos(lp->descr, nm);
      if (pos < 0) {
         throw_field_exception(field_pos(lp->descr, nm) < 0 ? FieldException::Reason::ForeignField
                                                            : FieldException::Reason::AbsentField,
                               nm, lp->descr);
      }
      const int type = mt_get_type(lp->descr[pos].mt);
      if (type != lStringT && type != lHostT) {
         throw_field_exception(FieldException::Reason::WrongType, nm, lp->descr);
      }
      const SortKey key{nm, true};
      lSortOrder *so = make_sort_order(lp->descr, std::span<const SortKey>(&key, 1));
      try {
         sort_list(lp, so);
      } catch (...) {
         free_sort_order(so);
         throw;
      }
      free_sort_order(so);
      auto key_of = [pos, type](const lListElem *ep) {
         const lMultiType &value = ocs_cull_value(ep, pos);
         return type == lStringT ? value.str : value.host;
      };
      for (lListElem *ep = lp->first; ep != nullptr; ep = ep->next) {
         lListElem *rep = ep->next;
         while (rep != nullptr && sge_strnullcmp(key_of(rep), key_of(ep)) == 0) {
            remove_elem(lp, rep);
            rep = ep->next;
         }
      }
   }

   int count_nodes(const lListElem *ep, const lList *lp, const int nm) noexcept {
      int n = 0;
      if (ep != nullptr) {
         n = 1;
         const int pos = present_pos(ep->descr, nm);
         if (pos >= 0 && mt_get_type(ep->descr[pos].mt) == lListT) {
            const lList *children = ocs_cull_value(ep, pos).glp;
            if (children != nullptr) {
               n += count_nodes(nullptr, children, nm);
            }
         }
      } else if (lp != nullptr) {
         for (const lListElem *child = lp->first; child != nullptr; child = child->next) {
            n += count_nodes(child, nullptr, nm);
         }
      }
      return n;
   }

   int count_leaves(const lListElem *ep, const lList *lp, const int nm) noexcept {
      int n = 0;
      if (ep != nullptr) {
         const int pos = present_pos(ep->descr, nm);
         if (pos >= 0 && mt_get_type(ep->descr[pos].mt) == lListT) {
            const lList *children = ocs_cull_value(ep, pos).glp;
            n = children == nullptr ? 1 : count_leaves(nullptr, children, nm);
         }
      } else if (lp != nullptr) {
         for (const lListElem *child = lp->first; child != nullptr; child = child->next) {
            n += count_leaves(child, nullptr, nm);
         }
      }
      return n;
   }

} // namespace ocs::tom
