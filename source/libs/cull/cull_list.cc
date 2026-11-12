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
 *  Portions of this code are Copyright 2011 Univa Inc.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Creating, copying and walking cull lists and elements
 */

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>

/* do not compile in monitoring code */
#ifndef NO_SGE_COMPILE_DEBUG
/// Suppresses the monitoring code in the rmon macros for this file
#define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/ocs_TerminationManager.h"
#include "uti/sge_stdlib.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_ListOpsCore.h"
#include "tom/ocs_PrintCore.h"
#include "tom/ocs_SortCore.h"
#include "tom/ocs_SelectCore.h"
#include "tom/ocs_SoftPath.h"

#include "cull/msg_cull.h"
#include "cull/cull_db.h"
#include "cull/cull_sortP.h"
#include "cull/cull_where.h"
#include "cull/cull_listP.h"
#include "cull/cull_multitypeP.h"
#include "cull/cull_stats.h"
#include "cull/cull_whatP.h"
#include "cull/cull_lerrnoP.h"
#include "cull/cull_hash.h"
#include "cull/cull_state.h"
#include "cull/pack.h"
#include "cull/cull_pack.h"
#include "cull/ocs_cull_descr.h"

#define CULL_BASIS_LAYER CULL_LAYER ///< rmon layer this file logs under




/**
 * @defgroup cull_field_attributes Field attributes
 * @brief Attributes a cull object field can be given
 *
 * When a field of a cull object type is defined, any number of attributes can
 * be combined into its flags. The syntax of a field definition is
 *
 * @code
 * <typedef>(<field_name>, <attrib1> [| <attrib2> ...])
 * e.g.
 * SGE_STRING(QU_qname, CULL_HASH | CULL_UNIQUE)
 * @endcode
 *
 * See #CULL_DEFAULT and its neighbours for the individual attributes.
 *
 * @note Further attributes can be introduced as necessary, e.g. `CULL_ARRAY`
 *       for a field holding an array of the given data type.
 * @{
 * @}
 */


/**
 * @brief Copies a whole list element
 *
 * Copies a whole list element
 *
 * @param ep element
 *
 * @return copy of 'ep'
 *
 * @see Typed form: ocs::tom::ConstElement<T>::clone().
 */
lListElem *lCopyElem(const lListElem *ep) {
   return lCopyElemHash(ep, true);
}

/**
 * @brief Copies a whole list element
 *
 * Copies a whole list element
 *
 * @param ep element
 * @param isHash true to build the hash tables of the copy as well
 * @param skip_no_transfer true when the copy is one that will be handed out, so
 *                         #CULL_NO_TRANSFER fields must not be taken over
 *
 * @return copy of 'ep'
 *
 * @note A thin wrapper over ocs::tom::copy_elem() of the core, which maps its exceptions to the
 *       result, error number and abort of before (CS-3077).
 *
 * @see Typed form: ocs::tom::ConstElement<T>::clone().
 */
lListElem *lCopyElemHash(const lListElem *ep, bool isHash, const bool skip_no_transfer) {
   try {
      return ocs::tom::copy_elem(ep, isHash, skip_no_transfer);
   } catch (const ocs::tom::FieldException &) {
      ocs::tom::report_soft_path("lCopyElem", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullElement);
      LERROR(LEELEMNULL);
   } catch (const ocs::tom::ListCoreError &e) {
      LERROR(e.lerrno());
   }
   return nullptr;
}

/**
 * @brief Copies parts of an element
 *
 * Copies the fields of 'src' that the enumeration 'enp' selects to 'dst',
 * or all fields that have a value for #WHAT_ALL. Every descriptor has the
 * length of its type, so a field keeps its position (N-B-3); it must be present
 * in both elements. If 'pb' is not nullptr the fields are packed into it
 * instead of being copied.
 *
 * @param dst destination element
 * @param src src element
 * @param enp enumeration
 * @param isHash true to build the hash tables of the copy as well
 * @param pb packbuffer
 * @param skip_no_transfer true when the copy is one that will be handed out, so
 *                         #CULL_NO_TRANSFER fields must not be taken over
 *
 * @return error state 0 - OK -1 - Error
 *
 * @note Without a pack buffer a thin wrapper over ocs::tom::copy_elem_partial().
 * @see Typed form: ocs::tom::ConstList<T>::select() reduces through it
 */
int
lCopyElemPartialPack(lListElem *dst, const lListElem *src,
                     const lEnumeration *enp, bool isHash,
                     sge_pack_buffer *pb, const bool skip_no_transfer) {
   DENTER(CULL_LAYER);

   if (!enp || (!dst && !pb)) {
      LERROR(LEENUMNULL);
      DRETURN(-1);
   }

   if (pb == nullptr) {
      try {
         ocs::tom::copy_elem_partial(dst, src, enp, isHash, skip_no_transfer);
      } catch (const ocs::tom::FieldException &) {
         LERROR(LEELEMNULL);
         DRETURN(-1);
      } catch (const ocs::tom::ListCoreError &e) {
         LERROR(e.lerrno());
         DRETURN(-1);
      }
      DRETURN(0);
   }

   // with a pack buffer the selected fields are packed instead of copied
   switch (enp[0].pos) {
      case WHAT_ALL:               /* all fields of element src are packed */
         cull_pack_elem(pb, src);
         break;

      case WHAT_NONE:              /* no field of element src is packed */
         break;

      default:                     /* only the enumerated fields are packed */
         cull_pack_elem_partial(pb, src, enp, 0);
   }
   DRETURN(0);
}

/**
 * @brief Copy parts of elements indedendent from type
 *
 * Copies from the element 'sep' (using index 'src_idx') to
 * the element 'dep' (using index 'dst_idx') in dependence
 * of the type or it copies the it directly into pb.
 *
 * @param sep source element
 * @param dep destination element
 * @param src_idx source index
 * @param dst_idx destination index
 * @param isHash create Hash or not
 * @param ep enumeration oiter to be used for sublists
 * @param skip_no_transfer true when the copy is one that will be handed out, so
 *                         #CULL_NO_TRANSFER fields must not be taken over
 *
 * @return error state 0 - OK -1 - Error
 *
 * @note A thin wrapper over ocs::tom::copy_field(); the pack buffer it took is gone, no caller passed one.
 */
int
lCopySwitchPack(const lListElem *sep, lListElem *dep, int src_idx, int dst_idx,
                bool isHash, lEnumeration *ep, const bool skip_no_transfer) {
   try {
      ocs::tom::copy_field(sep, dep, src_idx, dst_idx, isHash, ep, skip_no_transfer);
   } catch (const ocs::tom::FieldException &) {
      return -1;
   } catch (const ocs::tom::ListCoreError &e) {
      LERROR(e.lerrno());
      return -1;
   }
   return 0;
}

/**
 * @brief Returns the user defined name of a list
 *
 * Returns the user defined name of a list.
 *
 * @param lp list pointer
 *
 * @return list name
 *
 * @see Typed form: ocs::tom::ConstList<T>::name().
 *
 * @note A thin wrapper over ocs::tom::list_name().
 */
const char *lGetListName(const lList *lp) {
   if (lp == nullptr) {
      LERROR(LELISTNULL);
      return "No List specified";
   }
   if (lp->listname == nullptr) {
      LERROR(LENULLSTRING);
   }
   return ocs::tom::list_name(lp);
}

/**
 * @brief Returns the descriptor of a list
 *
 * Returns the descriptor of a list
 *
 * @param lp list pointer
 *
 * @return destriptor
 *
 * @note MT-NOTE: lGetListDescr() is MT safe
 */
const lDescr *lGetListDescr(const lList *lp) {
   DENTER(CULL_LAYER);

   if (!lp) {
      LERROR(LELISTNULL);
      DRETURN(nullptr);
   }
   DRETURN(lp->descr);
}

/**
 * @brief Returns the number of elements in a list
 *
 * Returns the number of elements in a list
 *
 * @param lp list pointer
 *
 * @return number of elements
 *
 * @note MT-NOTE: lGetNumberOfElem() is MT safe
 *
 * @see Typed form: ocs::tom::ConstList<T>::size().
 */
uint32_t lGetNumberOfElem(const lList *lp) {
   DENTER(CULL_LAYER);

   if (lp == nullptr) {
      LERROR(LELISTNULL);
      DRETURN(0);
   }

   DRETURN(lp->nelem);
}

/**
 * @brief Returns the index of element in list lp
 *
 * returns the index of element in list lp
 *
 * @param ep element
 * @param lp list
 *
 * @return index number
 */
int lGetElemIndex(const lListElem *ep, const lList *lp) {
   DENTER(CULL_LAYER);

   if (!ep || elem_kind(ep) != BOUND_ELEM) {
      DRETURN(-1);
   }

   int i = -1;
   for_each_ep_lv(ep2, lp) {
      i++;
      if (ep2 == ep)
         break;
   }

   DRETURN(i);
}

/**
 * @brief Count the elements that follow @p ep in its list (exclusive).
 *
 * Walks the chain of successors starting at `lNext(ep)` and counts them.
 * The element @p ep itself is **not** included in the count. Callers that
 * need the inclusive count (ep + its successors) must add 1 themselves.
 *
 * @param ep  The reference element. May be nullptr.
 * @return    Number of elements that come after @p ep.
 *            0 if @p ep is the last element in its list or if @p ep is nullptr.
 *
 * @note Walks the chain of the storage layer, without the old interface (CS-3077).
 */
uint32_t lGetNumberOfRemainingElem(const lListElem *ep) {
   uint32_t i = 0;
   if (ep != nullptr) {
      for (const lListElem *next = ep->next; next != nullptr; next = next->next) {
         i++;
      }
   }
   return i;
}

/**
 * @brief Returns the descriptor of a list element
 *
 * returns the descriptor of a list element
 *
 * @param ep CULL element
 *
 * @return Pointer to descriptor
 */
const lDescr *lGetElemDescr(const lListElem *ep) {
   DENTER(CULL_LAYER);

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(nullptr);
   }
   DRETURN(ep->descr);
}

/**
 * @brief Tells whether an element is free standing
 *
 * A free standing element belongs to no list and is no sub-object of another
 * element; whoever holds it frees it. Code outside CULL asks this function
 * instead of reading the element's status, which is private to CULL.
 *
 * @param ep CULL element
 *
 * @return true if the element is free standing, false if it is bound to a
 *         list, a sub-object, or ep is nullptr
 *
 * @note MT-NOTE: lIsFreeElem() is MT safe
 */
bool lIsFreeElem(const lListElem *ep) {
   DENTER(CULL_LAYER);

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(false);
   }
   DRETURN(elem_kind(ep) == FREE_ELEM);
}

/**
 * @brief Write a element to monitoring level CULL_LAYER
 *
 * Write a element to monitoring level CULL_LAYER a info message
 *
 * @param ep element
 */
namespace {

   /**
    * @brief Write the text the print core appends to a stream, as the old print functions did
    * @param fp the stream, or nullptr to write nothing
    * @param print appends the text to a string; a ListCoreError it throws leaves the text empty
    */
   template <typename Print>
   void ocs_print_to(FILE *fp, Print print) {
      dstring buffer = DSTRING_INIT;
      try {
         print(&buffer);
      } catch (const ocs::tom::ListCoreError &) {
         // the core has set the error number
      }
      const char *str = sge_dstring_get_string(&buffer);
      if (fp != nullptr && str != nullptr) {
         fprintf(fp, "%s", str);
      }
      sge_dstring_free(&buffer);
   }

} // namespace

/**
 * @brief Write an element to stderr
 *
 * @param ep element
 *
 * @note A thin wrapper over ocs::tom::print_elem() of the core (CS-3077).
 *
 * @see Typed form: ocs::tom::ConstElement<T>::print().
 */
void lWriteElem(const lListElem *ep) {
   ocs_print_to(stderr, [ep](dstring *buffer) { ocs::tom::print_elem(buffer, ep, 0); });
}

/**
 * @brief Write a element to file stream
 *
 * Write a element to file stream
 *
 * @param ep element
 * @param fp file stream
 *
 * @note A thin wrapper over ocs::tom::print_elem() of the core (CS-3077).
 *
 * @see Typed form: ocs::tom::ConstElement<T>::print().
 */
void lWriteElemTo(const lListElem *ep, FILE *fp) {
   ocs_print_to(fp, [ep](dstring *buffer) { ocs::tom::print_elem(buffer, ep, 0); });
}

/**
 * @brief Render an element into a string
 *
 * @param ep the element to render
 * @param[out] buffer receives the rendered element
 *
 * @note A thin wrapper over ocs::tom::print_elem() of the core (CS-3077).
 */
void lWriteElemToStr(const lListElem *ep, dstring *buffer) {
   try {
      ocs::tom::print_elem(buffer, ep, 0);
   } catch (const ocs::tom::ListCoreError &) {
      // the core has set the error number
   }
}

/**
 * @brief Write a list to a file stream
 *
 * Write a list to a file stream
 *
 * @param lp list
 * @param fp file stream
 *
 * @note A thin wrapper over ocs::tom::print_list() of the core (CS-3077).
 *
 * @see Typed form: ocs::tom::ConstList<T>::print().
 */
void lWriteListTo(const lList *lp, FILE *fp) {
   ocs_print_to(fp, [lp](dstring *buffer) { ocs::tom::print_list(buffer, lp, 0); });
}

/**
 * @brief Create an element for a specific list
 *
 * Create an element for a specific list
 *
 * @param dp descriptor
 *
 * @return element pointer or nullptr
 *
 * @note A thin wrapper over ocs::tom::create_elem() of the core, which maps its exceptions to the
 *       result, error number and abort of before (CS-3077).
 *
 * @see Typed form: ocs::tom::ElementPtr<T>::create().
 */
lListElem *lCreateElem(const lDescr *dp) {
   try {
      return ocs::tom::create_elem(dp);
   } catch (const ocs::tom::FieldException &) {
      LERROR(LECOUNTDESCR);
   } catch (const ocs::tom::ListCoreError &e) {
      LERROR(e.lerrno());
   }
   return nullptr;
}

/**
 * @brief Create an empty list
 *
 * Create an empty list with a given descriptor and a user defined
 * listname.
 *
 * @param listname list name
 * @param descr descriptor
 *
 * @return list pointer or nullptr
 *
 * @see Typed form: ocs::tom::ListPtr<T>::create().
 */
lList *lCreateList(const char *listname, const lDescr *descr) {
   return lCreateListHash(listname, descr, true);
}

/**
 * @brief Create an empty list
 *
 * Create an empty list with a given descriptor and a user defined
 * listname.
 * The caller can choose whether the list may be hashed. The hash tables of a
 * list that may are not built here but by the first lookup on their field,
 * once the list is long enough.
 *
 * @param listname list name
 * @param descr descriptor
 * @param hash may the list be hashed? If false, it is always searched linearly
 *
 * @return list pointer or nullptr
 *
 * @note A thin wrapper over ocs::tom::create_list() of the core, which maps its exceptions to the
 *       result, error number and abort of before (CS-3077).
 *
 * @see Typed form: ocs::tom::ListPtr<T>::create() with ocs::tom::Hashing.
 */
lList *lCreateListHash(const char *listname, const lDescr *descr, bool hash) {
   try {
      return ocs::tom::create_list(listname, descr, hash);
   } catch (const ocs::tom::FieldException &) {
      LERROR(LEDESCRNULL);
   } catch (const ocs::tom::ListCoreError &e) {
      LERROR(e.lerrno());
   }
   return nullptr;
}

/**
 * @brief Create a list with n elements
 *
 * Create a list with a given descriptor and insert 'nr_elem'
 * only initialized elements
 *
 * @param listname list name
 * @param descr descriptor
 * @param nr_elem number of elements
 *
 * @return list or nullptr
 */
lList *lCreateElemList(const char *listname, const lDescr *descr, int nr_elem) {
   DENTER(CULL_LAYER);

   lList *lp = nullptr;
   lListElem *ep = nullptr;
   int i;

   if (!(lp = lCreateList(listname, descr))) {
      LERROR(LECREATELIST);
      DRETURN(nullptr);
   }

   for (i = 0; i < nr_elem; i++) {
      if (!(ep = lCreateElem(descr))) {
         LERROR(LECREATEELEM);
         lFreeList(&lp);
         DRETURN(nullptr);
      }
      lAppendElem(lp, ep);
   }

   DRETURN(lp);
}

/**
 * @brief Free a element including strings and sublists
 *
 * Free a element including strings and sublists
 *
 * @param[in,out] ep1 element to remove; set to nullptr on return
 *
 * @note MT-NOTE: lRemoveElem() is MT safe
 *
 * @note A thin wrapper over ocs::tom::free_elem() of the core, which maps its exceptions to the
 *       result, error number and abort of before (CS-3077).
 *
 * @see Typed form: ocs::tom::ElementPtr<T>::reset() and its destructor.
 */
void lFreeElem(lListElem **ep1) {
   if (ep1 == nullptr) {
      return;
   }
   try {
      ocs::tom::free_elem(*ep1);
   } catch (const ocs::tom::FieldException &) {
      LERROR(LEDESCRNULL);
      DPRINTF("nullptr descriptor not allowed !!!\n");
      ocs::TerminationManager::trigger_abort();
   }
}

/**
 * @brief Frees a list including all elements
 *
 * Frees a list including all elements
 *
 * @param lp list
 *
 *
 * @note MT-NOTE: lFreeList() is MT safe
 *
 * @note A thin wrapper over ocs::tom::free_list() of the core, which maps its exceptions to the
 *       result, error number and abort of before (CS-3077).
 *
 * @see Typed form: ocs::tom::ListPtr<T>::reset() and its destructor.
 */
void lFreeList(lList **lp) {
   if (lp == nullptr) {
      return;
   }
   try {
      ocs::tom::free_list(*lp);
   } catch (const ocs::tom::FieldException &) {
      LERROR(LEDESCRNULL);
      DPRINTF("nullptr descriptor not allowed !!!\n");
      ocs::TerminationManager::trigger_abort();
   }
}


/**
 * @brief Append a list to the sublist of an element
 *
 * Appends the list 'to_add' to the sublist 'nm' of the element
 * 'ep'. The list pointer becomes invalid and the returned pointer
 * should be used instead to access the complete sublist.
 *
 * @param ep The CULL list element
 * @param nm The CULL field name of a sublist
 * @param to_add The list to be added
 *
 * @return Returns
 *
 * @note MT-NOTE: lAddSubList() is MT safe
 */
lList *lAddSubList(lListElem *ep, int nm, lList *to_add) {
   lList *tmp;
   if (lGetNumberOfElem(to_add)) {
      if ((tmp = lGetListRW(ep, nm)))
         lAddList(tmp, &to_add);
      else
         lSetList(ep, nm, to_add);
   }
   return lGetListRW(ep, nm);
}

/**
 * @brief Concatenate two lists
 *
 * Concatenate two lists of equal type throwing away the second list
 *
 * @param lp0 first list
 * @param lp1 second list
 *
 * @return error state 0 - OK -1 - Error
 *
 * @note MT-NOTE: lAddList() is MT safe
 *
 * @see Typed form: ocs::tom::List<T>::splice() with an owning handle.
 */
int lAddList(lList *lp0, lList **lp1) {
   DENTER(CULL_LAYER);

   /* No need to do any safety checks.  lAppendList will do them for us. */
   int res = 0;

   res = lAppendList(lp0, *lp1);
   lFreeList(lp1);
   DRETURN(res);
}

/**
 * @brief Concatenate two lists
 *
 * Concatenate two lists of equal type without throwing away the second list
 *
 * @param lp0 first list
 * @param lp1 second list
 *
 * @return error state 0 - OK -1 - Error
 *
 * @note MT-NOTE: lAppendList() is MT safe
 *
 * @note A thin wrapper over ocs::tom::append_list() of the core, which maps its exceptions to the
 *       result, error number and abort of before (CS-3077).
 *
 * @see Typed form: ocs::tom::List<T>::splice() with a non-owning handle.
 */
int lAppendList(lList *lp0, lList *lp1) {
   try {
      ocs::tom::append_list(lp0, lp1);
      return 0;
   } catch (const ocs::tom::FieldException &e) {
      switch (e.reason()) {
         case ocs::tom::FieldException::Reason::NullList:
            ocs::tom::report_soft_path("lAppendList", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullList);
            LERROR(LELISTNULL);
            break;
         case ocs::tom::FieldException::Reason::WrongType:
            LERROR(LEDIFFDESCR);
            break;
         default:
            // a duplicate key aborted in the lAppendElem() the old loop called
            ocs::tom::legacy_abort("lAppendElem", e);
      }
   }
   return -1;
}

/**
 * @brief Merge two lists
 *
 * Merge two lists of equal type, and replace values in the first list
 * with values from the second list.
 *
 * This only applies to values equal str.
 *
 * @param lp0 first list
 * @param lp1 second list
 * @param nm field name used for merging
 * @param str override criteria, e.g. "-q" for "override all -q switches, all others are simply merged"
 *
 * @return error state 0 - OK -1 - Error
 *
 * @note MT-NOTE: lOverrideStrList() is MT safe
 */
int lOverrideStrList(lList *lp0, lList *lp1, int nm, const char *str) {
   DENTER(CULL_LAYER);

   lListElem *ep;
   const lDescr *dp0, *dp1;
   bool overridden = false;

   if (!lp1 || !lp0) {
      LERROR(LELISTNULL);
      DRETURN(-1);
   }

   /* Check if the two lists are equal */
   dp0 = lGetListDescr(lp0);
   dp1 = lGetListDescr(lp1);
   if (lCompListDescr(dp0, dp1)) {
      LERROR(LEDIFFDESCR);
      DRETURN(-1);
   }

   while (lp1->first) {
      if (!(ep = lDechainElem(lp1, lp1->first))) {
         LERROR(LEDECHAINELEM);
         DRETURN(-1);
      }

      /*
       * if we find elements to override, override them
       * override means:
       * remove all occurencies of the str in lp0
       *
       * Iterate via lGetElemStrFirstRW / lGetElemStrNextRW so we walk the
       * hash chain (when the field carries a non-unique hash) instead of
       * doing a fresh first-lookup for every removal. lRemoveElem() frees
       * the current element, so the next one has to be fetched BEFORE the
       * remove — otherwise the iterator state points into freed memory.
       */
      if (sge_strnullcmp(lGetString(ep, nm), str) == 0 && !overridden) {
         const void *it = nullptr;
         lListElem *tmp = lGetElemStrFirstRW(lp0, nm, str, &it);
         while (tmp != nullptr) {
            lListElem *next = lGetElemStrNextRW(lp0, nm, str, &it);
            lRemoveElem(lp0, &tmp);
            tmp = next;
         }
         overridden = true;
      }

      /* now copy the elem */
      lAppendElem(lp0, ep);
   }

   lFreeList(&lp1);
   DRETURN(0);
}

/**
 * @brief Compare two descriptors
 *
 * Two descriptors are equivalent if they list the same fields with the same
 * types and hold the same of them; attributes other than the type are not
 * compared.
 *
 * @param dp0 descriptor one
 * @param dp1 descriptor two
 *
 * @return Result of compare operation 0 - equivalent -1 - not equivalent
 *
 * @note MT-NOTE: lCompListDescr() is MT safe
 *
 * @note A thin wrapper over ocs::tom::layout_difference().
 */
int lCompListDescr(const lDescr *dp0, const lDescr *dp1) {
   const int difference = ocs::tom::layout_difference(dp0, dp1);
   if (difference != 0) {
      LERROR(difference);
      return -1;
   }
   return 0;
}

/**
 * @brief Copy a list including strings and sublists
 *
 * Copy a list including strings and sublists. The new list will
 * get 'name' as user defined name
 *
 * @param name list name
 * @param src source list
 *
 * @return Copy of 'src' or nullptr
 *
 * @see Typed form: ocs::tom::ConstList<T>::clone().
 */
lList *lCopyList(const char *name, const lList *src) {
   return lCopyListHash(name, src, true);
}

/**
 * @brief Copy a list including strings and sublists
 *
 * Copy a list including strings and sublists. The new list will
 * get 'name' as user defined name
 *
 * @param name list name
 * @param src source list
 * @param hash if set to true, a hash table is generated
 * @param skip_no_transfer true when the copy is one that will be handed out, so
 *                         #CULL_NO_TRANSFER fields must not be taken over
 *
 * @return Copy of 'src' or nullptr
 *
 * @note A thin wrapper over ocs::tom::copy_list() of the core, which maps its exceptions to the
 *       result, error number and abort of before (CS-3077).
 *
 * @see Typed form: ocs::tom::ConstList<T>::clone().
 */
lList *lCopyListHash(const char *name, const lList *src, bool hash, const bool skip_no_transfer) {
   try {
      return ocs::tom::copy_list(name, src, hash, skip_no_transfer);
   } catch (const ocs::tom::FieldException &e) {
      if (e.reason() != ocs::tom::FieldException::Reason::NullList) {
         ocs::tom::legacy_abort("lCopyList", e);
      }
      ocs::tom::report_soft_path("lCopyList", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullList);
      LERROR(LELISTNULL);
   } catch (const ocs::tom::ListCoreError &e) {
      LERROR(e.lerrno());
   }
   return nullptr;
}

/**
 * @brief Insert @p new_ep immediately **after** @p ep in list @p lp.
 *
 * The insertion point is determined by @p ep:
 * - If @p ep is non-null, @p new_ep is placed directly after it. Any
 *   elements that previously followed @p ep now follow @p new_ep.
 * - If @p ep is **nullptr**, @p new_ep is prepended as the new first element
 *   of @p lp (equivalent to inserting before `lFirst(lp)`).
 *
 * @p new_ep must not already belong to any list. Attempting to insert a
 * bound element aborts the process. In a debug build, so does inserting an
 * element whose key in a unique field the list already holds (N-H-18).
 *
 * @param lp      The list to insert into. Must not be nullptr.
 * @param ep      The element after which @p new_ep is inserted, or nullptr
 *                to insert at the front of the list.
 * @param new_ep  The element to insert. Must be unbound (not in any list).
 * @return        0 on success, -1 if @p lp or @p new_ep is nullptr.
 *
 * @note A thin wrapper over ocs::tom::insert_elem() of the core, which maps its exceptions to the
 *       result, error number and abort of before (CS-3077).
 *
 * @see Typed form: ocs::tom::List<T>::push_front() and insert_after().
 */
int lInsertElem(lList *lp, lListElem *ep, lListElem *new_ep) {
   try {
      ocs::tom::insert_elem(lp, ep, new_ep);
      return 0;
   } catch (const ocs::tom::FieldException &e) {
      switch (e.reason()) {
         case ocs::tom::FieldException::Reason::NullList:
            ocs::tom::report_soft_path("lInsertElem", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullList);
            LERROR(LELISTNULL);
            break;
         case ocs::tom::FieldException::Reason::NullElement:
            ocs::tom::report_soft_path("lInsertElem", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullElement);
            LERROR(LEELEMNULL);
            break;
         case ocs::tom::FieldException::Reason::BoundElement:
            DPRINTF("WARNING: tried to insert chained element\n");
         lWriteElem(new_ep);
            ocs::TerminationManager::trigger_abort();
            break;
         default:
            ocs::tom::legacy_abort("lInsertElem", e);
      }
   }
   return -1;
}

/**
 * @brief Append element at the end of a list
 *
 * Append element 'ep' at the end of list 'lp'. In a debug build, appending
 * an element whose key in a unique field the list already holds aborts the
 * process (N-H-18).
 *
 * @param lp list
 * @param ep element
 *
 * @return error state 0 - OK -1 - Error
 *
 * @note MT-NOTE: lAppendElem() is MT safe
 *
 * @note A thin wrapper over ocs::tom::append_elem() of the core, which maps its exceptions to the
 *       result, error number and abort of before (CS-3077).
 *
 * @see Typed form: ocs::tom::List<T>::push_back().
 */
int lAppendElem(lList *lp, lListElem *ep) {
   try {
      ocs::tom::append_elem(lp, ep);
      return 0;
   } catch (const ocs::tom::FieldException &e) {
      switch (e.reason()) {
         case ocs::tom::FieldException::Reason::NullList:
            ocs::tom::report_soft_path("lAppendElem", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullList);
            LERROR(LELISTNULL);
            break;
         case ocs::tom::FieldException::Reason::NullElement:
            ocs::tom::report_soft_path("lAppendElem", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullElement);
            LERROR(LEELEMNULL);
            break;
         case ocs::tom::FieldException::Reason::BoundElement:
            DPRINTF("WARNING: tried to append chained element\n");
            ocs::TerminationManager::trigger_abort();
            break;
         default:
            ocs::tom::legacy_abort("lAppendElem", e);
      }
   }
   return -1;
}

/**
 * @brief Delete a element from a list
 *
 * Remove element 'ep' from list 'lp'. 'ep' gets deleted.
 *
 * @param lp list
 * @param[in,out] ep1 element to remove; set to nullptr on return
 *
 * @return error state 0 - OK -1 - Error
 *
 * @note MT-NOTE: lRemoveElem() is MT safe
 *
 * @note A thin wrapper over ocs::tom::remove_elem() of the core, which maps its exceptions to the
 *       result, error number and abort of before (CS-3077).
 *
 * @see Typed form: ocs::tom::List<T>::erase().
 */
int lRemoveElem(lList *lp, lListElem **ep1) {
   if (ep1 == nullptr) {
      ocs::tom::report_soft_path("lRemoveElem", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullArgument);
      return -1;
   }
   try {
      ocs::tom::remove_elem(lp, *ep1);
      return 0;
   } catch (const ocs::tom::FieldException &e) {
      if (e.reason() == ocs::tom::FieldException::Reason::ForeignList) {
         CRITICAL("Removing element from other list !!!\n");
         ocs::TerminationManager::trigger_abort();
      }
      // a null list or element returns -1 without an error number, as before
      ocs::tom::report_soft_path("lRemoveElem", ocs::tom::SoftPathNoName,
                                 e.reason() == ocs::tom::FieldException::Reason::NullList
                                    ? ocs::tom::SoftPathReason::NullList
                                    : ocs::tom::SoftPathReason::NullElement);
   }
   return -1;
}

/**
 * @brief Splits a list into two at the given elem
 *
 * splits a list into two at the given elem.
 * If no target list is given, new one is created, otherwise the splited
 * list is appended to the second one.
 *
 * @param source list
 * @param target list
 * @param ep element
 *
 * @note MT-NOTE: lDechainList() is MT safe
 */
void
lDechainList(lList *source, lList **target, lListElem *ep) {
   DENTER(CULL_LAYER);

   lListElem *target_last;

   if (source == nullptr || target == nullptr) {
      LERROR(LELISTNULL);
      DRETURN_VOID;
   }
   if (ep == nullptr) {
      LERROR(LEELEMNULL);
      DRETURN_VOID;
   }

   /* descriptors are shared between lists, so only the owner tells the list of an element */
   if (elem_list(ep) != source) {
      CRITICAL("Dechaining element from other list !!!\n");
      ocs::TerminationManager::trigger_abort();
   }

   if (*target == nullptr) {
      *target = lCreateList(lGetListName(source), source->descr);
   } else {
      if (lCompListDescr(source->descr, (*target)->descr) != 0) {
         CRITICAL("Dechaining element into a different list !!!\n");
         ocs::TerminationManager::trigger_abort();
      }
   }

   cull_hash_free_tables(source);
   cull_hash_free_tables(*target);

   target_last = source->last;

   if (ep->prev != nullptr) {
      ep->prev->next = nullptr;
      source->last = ep->prev;
   } else {
      source->first = nullptr;
      source->last = nullptr;
   }

   if ((*target)->first != nullptr) {
      (*target)->last->next = ep;
      ep->prev = (*target)->last;
   } else {
      ep->prev = nullptr;
      (*target)->first = ep;
   }
   (*target)->last = target_last;

   for (; ep != nullptr; ep = ep->next) {
      ep->descr = (*target)->descr;
      elem_set_bound(ep, *target);
      (*target)->nelem++;
      source->nelem--;
   }

   /* the tables of both lists were dropped above; the next lookup on a field builds its table again */

   DRETURN_VOID;
}

/**
 * @brief Remove a element from a list
 *
 * Remove element 'ep' from list 'lp'. 'ep' gets not deleted.
 *
 * @param lp list
 * @param ep element
 *
 * @return dechained element or nullptr
 *
 * @note MT-NOTE: lDechainElem() is MT safe
 *
 * @note A thin wrapper over ocs::tom::dechain_elem() of the core, which maps its exceptions to the
 *       result, error number and abort of before (CS-3077).
 *
 * @see Typed form: ocs::tom::List<T>::extract().
 */
lListElem *lDechainElem(lList *lp, lListElem *ep) {
   try {
      ocs::tom::dechain_elem(lp, ep);
      return ep;
   } catch (const ocs::tom::FieldException &e) {
      switch (e.reason()) {
         case ocs::tom::FieldException::Reason::NullList:
            ocs::tom::report_soft_path("lDechainElem", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullList);
            LERROR(LELISTNULL);
            break;
         case ocs::tom::FieldException::Reason::NullElement:
            ocs::tom::report_soft_path("lDechainElem", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullElement);
            LERROR(LEELEMNULL);
            break;
         default:
            CRITICAL("Dechaining element from other list !!!");
            ocs::TerminationManager::trigger_abort();
      }
   }
   return nullptr;
}

/**
 * @brief Remove a element from a list
 *
 * Removes the sub-object held in a field and returns it. The object is not
 * deleted, and the field is left holding nullptr.
 *
 * @param parent element holding the sub-object
 * @param name the field the sub-object lives in
 *
 * @return the dechained object, now owned by the caller, or nullptr
 */
lListElem *lDechainObject(lListElem *parent, int name) {
   DENTER(CULL_LAYER);

   int pos;
   lListElem *dep;

   if (parent == nullptr) {
      LERROR(LEELEMNULL);
      DRETURN(nullptr);
   }

   pos = lGetPosViaElem(parent, name, SGE_DO_ABORT);

   if (mt_get_type(parent->descr[pos].mt) != lObjectT) {
      incompatibleType2(MSG_CULL_DECHAINOBJECT_WRONGTYPEFORFIELDXY_S,
                        lNm2Str(name));
      DRETURN(nullptr);
   }

   dep = (lListElem *) ocs_cull_value(parent, pos).obj;

   if (dep != nullptr) {
      elem_set_free(dep);
      ocs_cull_value(parent, pos).obj = nullptr;
   }

   DRETURN(dep);
}

/**
 * @brief Return the first element of a list
 *
 * Return the first element of a list.
 *
 * @param slp list
 *
 * @return first element or nullptr
 *
 * @see Typed form: ocs::tom::List<T>::front() and begin().
 */
lListElem *lFirstRW(const lList *slp) {
   DENTER(CULL_LAYER);
   DRETURN(slp ? slp->first : nullptr);
}

/**
 * @brief Return the first element of a list, read only
 *
 * @param slp list
 * @return first element or nullptr
 *
 * @see Typed form: ocs::tom::ConstList<T>::front() and begin().
 */
const lListElem *lFirst(const lList *slp) {
   return lFirstRW(slp);
}

/**
 * @brief Returns the last element of a list
 *
 * Returns the last element of a list.
 *
 * @param slp list
 *
 * @return last element or nullptr
 *
 * @see Typed form: ocs::tom::List<T>::back().
 */
lListElem *lLastRW(const lList *slp) {
   DENTER(CULL_LAYER);
   DRETURN(slp ? slp->last : nullptr);
}

/**
 * @brief Return the last element of a list, read only
 *
 * @param slp list
 * @return last element or nullptr
 *
 * @see Typed form: ocs::tom::ConstList<T>::back().
 */
const lListElem *lLast(const lList *slp) {
   return lLastRW(slp);
}

/**
 * @brief Returns the next element or nullptr
 *
 * Returns the next element of 'sep' or nullptr
 *
 * @param sep element
 *
 * @return next element or nullptr
 *
 * @see Typed form: ocs::tom::Element<T>::next() and the list iterator.
 */
lListElem *lNextRW(const lListElem *sep) {
   DENTER(CULL_LAYER);
   DRETURN(sep ? sep->next : nullptr);
}

/**
 * @brief Return the following element, read only
 *
 * @param sep the current element
 * @return the next element, or nullptr at the end of the list
 *
 * @see Typed form: ocs::tom::ConstElement<T>::next() and the list iterator.
 */
const lListElem *lNext(const lListElem *sep) {
   return lNextRW(sep);
}

/**
 * @brief Returns the previous element or nullptr
 *
 * Returns the previous element or nullptr.
 *
 * @param sep element
 *
 * @return previous element
 *
 * @see Typed form: ocs::tom::Element<T>::prev().
 */
lListElem *lPrevRW(const lListElem *sep) {
   DENTER(CULL_LAYER);
   DRETURN(sep ? sep->prev : nullptr);
}

/**
 * @brief Return the preceding element, read only
 *
 * @param sep the current element
 * @return the previous element, or nullptr at the head of the list
 *
 * @see Typed form: ocs::tom::ConstElement<T>::prev().
 */
const lListElem *lPrev(const lListElem *sep) {
   return lPrevRW(sep);
}

/**
 * @brief Returns first element fulfilling condition
 *
 * Returns the first element fulfilling the condition 'cp' or
 * nullptr if nothing is found. If the condition is nullptr the first
 * element is delivered.
 *
 * @param lp list
 * @param cp condition
 *
 * @return element or nullptr
 *
 * @note A thin wrapper over ocs::tom::find_first().
 * @see Typed form: ocs::tom::List<T>::find_if()
 */
lListElem *lFindFirstRW(const lList *lp, const lCondition *cp) {
   try {
      return ocs::tom::find_first(lp, cp);
   } catch (const ocs::tom::FieldException &) {
      ocs::tom::report_soft_path("lFindFirstRW", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullList);
      LERROR(LELISTNULL);
   }
   return nullptr;
}

/**
 * @brief Returns last element fulfilling condition
 *
 * Retruns the last element fulfilling the condition 'cp' or nullptr
 * if nothing is found. If the condition is nullptr then the last
 * element is delivered.
 *
 * @param lp list
 * @param cp condition
 *
 * @return element or nullptr
 *
 * @note A thin wrapper over ocs::tom::find_last().
 * @see Typed form: ocs::tom::List<T>::find_last_if()
 */
lListElem *lFindLastRW(const lList *lp, const lCondition *cp) {
   try {
      return ocs::tom::find_last(lp, cp);
   } catch (const ocs::tom::FieldException &) {
      ocs::tom::report_soft_path("lFindLastRW", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullList);
      LERROR(LELISTNULL);
   }
   return nullptr;
}

/**
 * @brief Returns the next element fulfilling condition
 *
 * Returns the next element fulfilling the condition 'cp' or nullptr
 * if nothing is found. If condition is nullptr than the following
 * element is delivered.
 *
 * @param ep element
 * @param cp condition
 *
 * @return element or nullptr
 *
 * @note A thin wrapper over ocs::tom::find_next().
 * @see Typed form: ocs::tom::Element<T>::next_if()
 */
lListElem *lFindNextRW(const lListElem *ep, const lCondition *cp) {
   try {
      return ocs::tom::find_next(ep, cp);
   } catch (const ocs::tom::FieldException &) {
      ocs::tom::report_soft_path("lFindNextRW", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullElement);
      LERROR(LEELEMNULL);
   }
   return nullptr;
}

/**
 * @brief Returns previous element fulfilling condition
 *
 * Returns the previous element fulfilling the condition 'cp' or
 * nullptr if nothing is found. If condition is nullptr than the following
 * element is delivered.
 *
 * @param ep element
 * @param cp condition
 *
 * @return element or nullptr
 *
 * @note A thin wrapper over ocs::tom::find_prev().
 * @see Typed form: ocs::tom::Element<T>::prev_if()
 */
lListElem *lFindPrevRW(const lListElem *ep, const lCondition *cp) {
   try {
      return ocs::tom::find_prev(ep, cp);
   } catch (const ocs::tom::FieldException &) {
      ocs::tom::report_soft_path("lFindPrevRW", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullElement);
      LERROR(LEELEMNULL);
   }
   return nullptr;
}

/**
 * @brief Sort a given list
 *
 * Sort a given list. The sorting order is given by the format
 * string and additional arguments.
 *
 * @param lp list
 * @param fmt format string (see lParseSortOrder()) ...             - additional arguments (see lParseSortOrder())
 *
 * @return error state 0 - OK -1 - Error
 *
 * @see `lParseSortOrder()`
 */
int lPSortList(lList *lp, const char *fmt, ...) {
   DENTER(CULL_LAYER);

   va_list ap;

   lSortOrder *sp;

   va_start(ap, fmt);
   if (!lp || !fmt) {
      LERROR(LELISTNULL);
      va_end(ap);
      DRETURN(-1);
   }
   if (!(sp = lParseSortOrder(lp->descr, fmt, ap))) {
      LERROR(LEPARSESORTORD);
      va_end(ap);
      DRETURN(-1);
   }

   lSortList(lp, sp);

   va_end(ap);
   lFreeSortOrder(&sp);

   DRETURN(0);
}

/**
 * @brief Sort list according to sort order object
 *
 * Sort list according to sort order object.
 *
 * @param lp list
 * @param sp sort order object
 *
 * @return error state 0 - OK -1 - Error
 *
 * @note A thin wrapper over ocs::tom::sort_list(), which sorts stably since CS-3077: elements with equal keys
 *       keep their order, which qsort() did not promise.
 * @see Typed form: ocs::tom::List::sort()
 */
int lSortList(lList *lp, const lSortOrder *sp) {
   try {
      ocs::tom::sort_list(lp, sp);
   } catch (const ocs::tom::ListCoreError &) {
      return -1;                  /* low memory */
   } catch (const ocs::tom::FieldException &e) {
      ocs::tom::legacy_abort("lSortList", e);
   }
   return 0;
}

/**
 * @brief Uniq a string key list
 *
 * Uniq a string key list
 *
 * @param lp list
 * @param keyfield string field name id
 *
 * @return error state 0 - OK -1 - Error
 *
 * @note A thin wrapper over ocs::tom::unique_by_key() of the core (CS-3077). Two null keys are equal; before,
 *       they ended the process.
 *
 * @see Typed form: ocs::tom::List<T>::unique().
 */
int lUniqStr(lList *lp, int keyfield) {
   if (lp == nullptr) {
      LERROR(LELISTNULL);
      return -1;
   }
   try {
      ocs::tom::unique_by_key(lp, keyfield);
   } catch (const ocs::tom::FieldException &) {
      LERROR(LEPARSESORTORD);
      return -1;
   } catch (const ocs::tom::ListCoreError &e) {
      LERROR(e.lerrno());
      return -1;
   }
   return 0;
}

/**
 * @brief Uniq a host key list
 *
 * Uniq a hostname key list.
 *
 * @param lp list
 * @param keyfield host field
 *
 * @return error state 0 - OK -1 - Error
 *
 * @note A thin wrapper over ocs::tom::unique_by_key() of the core (CS-3077). Two null keys are equal; before,
 *       they ended the process.
 *
 * @see Typed form: ocs::tom::List<T>::unique().
 */
int lUniqHost(lList *lp, int keyfield) {
   if (lp == nullptr) {
      LERROR(LELISTNULL);
      return -1;
   }
   try {
      ocs::tom::unique_by_key(lp, keyfield);
   } catch (const ocs::tom::FieldException &) {
      LERROR(LEPARSESORTORD);
      return -1;
   } catch (const ocs::tom::ListCoreError &e) {
      LERROR(e.lerrno());
      return -1;
   }
   return 0;
}

/**
 * @brief Get data type for cull object field
 *
 * Returns the data type of a cull object field given the multitype
 * attribute of a cull descriptor.
 *
 * @code
 * switch(mt_get_type(descr[i].mt)) {
 *    case lDoubleT:
 *       ...
 * }
 * @endcode
 *
 * @param mt mt (multitype) struct element of a field descriptor
 *
 * @return cull data type enum value (from _enum_lMultiType)
 *
 * @note MT-NOTE: mt_get_type() is MT safe
 */
/**
 * @brief Is there hash access for a field
 *
 * Returns the information if hashing is active for a cull object field
 * given the multitype attribute of a cull descriptor.
 *
 * @param mt mt (multitype) struct element of a field descriptor
 *
 * @return 1, if hashing is requested, else 0
 */
/**
 * @brief Is the cull object field unique
 *
 * Returns the information if a certain cull object field is unique within
 * a cull list given the multitype attribute of a cull descriptor.
 *
 * @code
 * if(mt_is_unique(descr[i].mt)) {
 *    // check for uniqueness before inserting new elemente into a list
 *    if(lGetElemUlong(....) != nullptr) {
 *       WARNING(MSG_DUPLICATERECORD....);
 *       DRETURN(nullptr);
 *    }
 * }
 * @endcode
 *
 * @param mt mt (multitype) struct element of a field descriptor
 *
 * @return 1 = unique, 0 = not unique
 */

/**
 * @brief The descriptor registry the code of this object uses (N-A-22)
 *
 * Lives in another translation unit than the registry, so that in a loaded
 * object holding its own copy of CULL the call goes through the dynamic linker
 * and returns the registry the code of the object is bound to. It is exported
 * with C linkage for dlsym().
 *
 * @return the identity of that registry, see ocs_cull_descr_registry_id()
 */
extern "C" const void *ocs_cull_descr_registry_seen() {
   return ocs_cull_descr_registry_id();
}
