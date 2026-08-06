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

#include "cull/msg_cull.h"
#include "cull/cull_db.h"
#include "cull/cull_sortP.h"
#include "cull/cull_where.h"
#include "cull/cull_listP.h"
#include "cull/cull_multitypeP.h"
#include "cull/cull_whatP.h"
#include "cull/cull_lerrnoP.h"
#include "cull/cull_hash.h"
#include "cull/cull_state.h"
#include "cull/pack.h"
#include "cull/cull_pack.h"
#include "cull/cull_observe.h"

#ifdef OBSERVE
#  include "cull/cull_observe.h"
#endif

#define CULL_BASIS_LAYER CULL_LAYER ///< rmon layer this file logs under


static void lWriteList_(const lList *lp, dstring *buffer, int nesting_level);

static void lWriteElem_(const lListElem *lp, dstring *buffer, int nesting_level);

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
 *
 * @return copy of 'ep'
 */
lListElem *lCopyElemHash(const lListElem *ep, bool isHash) {
   lListElem *new_ep;
   int index;
   int max;

   DENTER(CULL_LAYER);

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(nullptr);
   }

   max = lCountDescr(ep->descr);

   if (!(new_ep = lCreateElem(ep->descr))) {
      LERROR(LECREATEELEM);
      DRETURN(nullptr);
   }

   for (index = 0; index < max; index++) {
      if (lCopySwitchPack(ep, new_ep, index, index, isHash, nullptr, nullptr) != 0) {
         lFreeElem(&new_ep);

         LERROR(LECOPYSWITCH);
         DRETURN(nullptr);
      }
   }

   new_ep->status = FREE_ELEM;

   DRETURN(new_ep);
}

/**
 * @brief Copy parts of an element
 *
 * Copies elements from 'src' to 'dst' using the enumeration 'enp'
 * as a mask or copies all elements if 'enp' is nullptr
 *
 * @param dst destination element
 * @param src source element
 * @param enp mask
 *
 * @return error state 0 - OK -1 - Error
 */
int lModifyWhat(lListElem *dst, const lListElem *src, const lEnumeration *enp) {
   int ret, i = 0;

   DENTER(CULL_LAYER);

   ret = lCopyElemPartialPack(dst, &i, src, enp, true, nullptr);

   DRETURN(ret);
}

/**
 * @brief Copies parts of an element
 *
 * Copies elements from list element 'src' to 'dst' using the
 * enumeration 'enp' as a mask or copies all elements if
 * 'enp' is nullptr. Copying starts at index *jp. If pb is not nullptr
 * then the elements will be stored in the packbuffer 'pb' instead of
 * being copied.
 *
 * @param dst destination element
 * @param jp Where should the copy operation start
 * @param src src element
 * @param enp enumeration
 * @param isHash true to build the hash tables of the copy as well
 * @param pb packbuffer
 *
 * @return error state 0 - OK -1 - Error
 */
int
lCopyElemPartialPack(lListElem *dst, int *jp, const lListElem *src,
                     const lEnumeration *enp, bool isHash,
                     sge_pack_buffer *pb) {
   int i;

   DENTER(CULL_LAYER);

   if (!enp || (!dst && !pb) || !jp) {
      LERROR(LEENUMNULL);
      DRETURN(-1);
   }

   switch (enp[0].pos) {
      case WHAT_ALL:               /* all fields of element src is copied */
         if (pb == nullptr) {
            for (i = 0; src->descr[i].nm != NoName; i++, (*jp)++) {
               if (lCopySwitchPack(src, dst, i, *jp, isHash, enp[0].ep, nullptr) != 0) {
                  LERROR(LECOPYSWITCH);
                  DRETURN(-1);
               }
            }
         } else {
            cull_pack_elem(pb, src);
         }
         break;

      case WHAT_NONE:              /* no field of element src is copied */
         break;

      default:                     /* copy only the in enp enumerated elems */
         if (pb == nullptr) {
            int maxpos = 0;
            maxpos = lCountDescr(src->descr);

            for (i = 0; enp[i].nm != NoName; i++, (*jp)++) {
               if (enp[i].pos > maxpos || enp[i].pos < 0) {
                  LERROR(LEENUMDESCR);
                  DRETURN(-1);
               }

               if (lCopySwitchPack(src, dst, enp[i].pos, *jp, isHash, enp[i].ep, nullptr) != 0) {
                  LERROR(LECOPYSWITCH);
                  DRETURN(-1);
               }
            }
         } else {
            cull_pack_elem_partial(pb, src, enp, 0);
         }
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
 * @param pb pack buffer
 *
 * @return error state 0 - OK -1 - Error
 */
int
lCopySwitchPack(const lListElem *sep, lListElem *dep, int src_idx, int dst_idx,
                bool isHash, lEnumeration *ep, sge_pack_buffer *pb) {
   lList *tlp;
   lListElem *tep;

   DENTER(CULL_LAYER);

   if ((!dep && !pb) || !sep) {
      DRETURN(-1);
   }

   switch (mt_get_type(dep->descr[dst_idx].mt)) {
      case lUlongT:
         dep->cont[dst_idx].ul = sep->cont[src_idx].ul;
         break;
      case lUlong64T:
         dep->cont[dst_idx].ul64 = sep->cont[src_idx].ul64;
         break;
      case lStringT:
         if (!sep->cont[src_idx].str)
            dep->cont[dst_idx].str = nullptr;
         else
            dep->cont[dst_idx].str = strdup(sep->cont[src_idx].str);
         break;
      case lHostT:
         if (!sep->cont[src_idx].host)
            dep->cont[dst_idx].host = nullptr;
         else
            dep->cont[dst_idx].host = strdup(sep->cont[src_idx].host);
         break;

      case lDoubleT:
         dep->cont[dst_idx].db = sep->cont[src_idx].db;
         break;
      case lListT:
         if ((tlp = sep->cont[src_idx].glp) == nullptr)
            dep->cont[dst_idx].glp = nullptr;
         else {
            dep->cont[dst_idx].glp = lSelectHashPack(tlp->listname, tlp, nullptr,
                                                     ep, isHash, pb);
         }
         break;
      case lObjectT:
         if ((tep = sep->cont[src_idx].obj) == nullptr) {
            dep->cont[dst_idx].obj = nullptr;
         } else {
            lListElem *new_ep = lSelectElemPack(tep, nullptr, ep, isHash, pb);
            new_ep->status = OBJECT_ELEM;
            dep->cont[dst_idx].obj = new_ep;
         }
         break;
      case lIntT:
         dep->cont[dst_idx].i = sep->cont[src_idx].i;
         break;
      case lLongT:
         dep->cont[dst_idx].l = sep->cont[src_idx].l;
         break;
      case lBoolT:
         dep->cont[dst_idx].b = sep->cont[src_idx].b;
         break;
      case lRefT:
         dep->cont[dst_idx].ref = sep->cont[src_idx].ref;
         break;
      default:
         DRETURN(-1);
   }

   DRETURN(0);
}

/**
 * @brief Returns the user defined name of a list
 *
 * Returns the user defined name of a list.
 *
 * @param lp list pointer
 *
 * @return list name
 */
const char *lGetListName(const lList *lp) {
   DENTER(CULL_LAYER);

   if (!lp) {
      LERROR(LELISTNULL);
      DRETURN("No List specified");
   }

   if (!lp->listname) {
      LERROR(LENULLSTRING);
      DRETURN("No list name specified");
   }

   DRETURN(lp->listname);
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

   if (!ep || ep->status != BOUND_ELEM) {
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
 */
uint32_t lGetNumberOfRemainingElem(const lListElem *ep) {
   DENTER(CULL_LAYER);

   uint32_t i = 0;
   if (ep != nullptr) {
      while ((ep = lNext(ep))) {
         i++;
      }
   }

   DRETURN(i);
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
 * @brief Write a element to monitoring level CULL_LAYER
 *
 * Write a element to monitoring level CULL_LAYER a info message
 *
 * @param ep element
 */
void lWriteElem(const lListElem *ep) {
   dstring buffer = DSTRING_INIT;
   const char *str;

   DENTER(CULL_LAYER);
   lWriteElem_(ep, &buffer, 0);
   str = sge_dstring_get_string(&buffer);
   if (str != nullptr) {
      fprintf(stderr, "%s", str);
   }
   sge_dstring_free(&buffer);
   DRETURN_VOID;
}

/**
 * @brief Write a element to file stream
 *
 * Write a element to file stream
 *
 * @param ep element
 * @param fp file stream ???/???
 */
void lWriteElemTo(const lListElem *ep, FILE *fp) {
   dstring buffer = DSTRING_INIT;
   const char *str;

   DENTER(CULL_LAYER);
   lWriteElem_(ep, &buffer, 0);
   str = sge_dstring_get_string(&buffer);
   if (str != nullptr) {
      fprintf(fp, "%s", str);
   }
   sge_dstring_free(&buffer);
   DRETURN_VOID;
}

/**
 * @brief Render an element into a string
 *
 * @param ep the element to render
 * @param[out] buffer receives the rendered element
 */
void lWriteElemToStr(const lListElem *ep, dstring *buffer) {
   DENTER(CULL_LAYER);
   lWriteElem_(ep, buffer, 0);
   DRETURN_VOID;
}

static void lWriteElem_(const lListElem *ep, dstring *buffer, int nesting_level) {
   int i;
   char space[128];
   lList *tlp;
   lListElem *tep;
   const char *str;

   DENTER(TOP_LAYER);

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN_VOID;
   }

   for (i = 0; i < nesting_level * 3; i++) {
      space[i] = ' ';
   }
   space[i] = '\0';

   sge_dstring_sprintf_append(buffer, "%s-------------------------------\n", space);

   for (i = 0; mt_get_type(ep->descr[i].mt) != lEndT; i++) {
      const char *name = ((lNm2Str(ep->descr[i].nm) != nullptr) ? lNm2Str(ep->descr[i].nm) : "(null)");

      switch (mt_get_type(ep->descr[i].mt)) {
         case lIntT:
            sge_dstring_sprintf_append(buffer, "%s%-20.20s (Integer) = %d\n", space, name, lGetPosInt(ep, i));
            break;
         case lUlongT:
            sge_dstring_sprintf_append(buffer, "%s%-20.20s (Ulong)   = " sge_u32 "\n", space, name, lGetPosUlong(ep, i));
            break;
         case lUlong64T:
            sge_dstring_sprintf_append(buffer, "%s%-20.20s (Ulong64) = " sge_u64"\n", space, name, lGetPosUlong64(ep, i));
            break;
         case lStringT:
            str = lGetPosString(ep, i);
            sge_dstring_sprintf_append(buffer, "%s%-20.20s (String)  = %s\n", space, name, str ? str : "(null)");
            break;
         case lHostT:
            str = lGetPosHost(ep, i);
            sge_dstring_sprintf_append(buffer, "%s%-20.20s (Host)    = %s\n", space, name, str ? str : "(null)");
            break;
         case lListT:
            tlp = lGetPosList(ep, i);
            sge_dstring_sprintf_append(buffer, "%s%-20.20s (List)    = %s\n", space, name, tlp ? "full {" : "empty");
            if (tlp) {
               lWriteList_(tlp, buffer, nesting_level + 1);
               sge_dstring_sprintf_append(buffer, "%s}\n", space);
            }
            break;
         case lObjectT:
            tep = lGetPosObject(ep, i);
            sge_dstring_sprintf_append(buffer, "%s%-20.20s (Object)  = %s\n", space, name, tep ? "object {" : "none");
            if (tep) {
               lWriteElem_(tep, buffer, nesting_level + 1);
               sge_dstring_sprintf_append(buffer, "%s}\n", space);
            }
            break;
         case lDoubleT:
            sge_dstring_sprintf_append(buffer, "%s%-20.20s (Double)  = %f\n", space, name, lGetPosDouble(ep, i));
            break;
         case lLongT:
            DTRACE;
            sge_dstring_sprintf_append(buffer, "%s%-20.20s (Long)    = %ld\n", space, name, lGetPosLong(ep, i));
            break;
         case lBoolT:
            DTRACE;
            sge_dstring_sprintf_append(buffer, "%s%-20.20s (Bool)    = %s\n", space, name, lGetPosBool(ep, i) ? "true" : "false");
            break;
         case lRefT:
            DTRACE;
            sge_dstring_sprintf_append(buffer, "%s%-20.20s (Ref)     = %p\n", space, name, lGetPosRef(ep, i));
            break;
         default:
            DTRACE;
            unknownType("lWriteElem");
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Write a list to monitoring level CULL_LAYER
 *
 * Write a list to monitoring level CULL_LAYER as info message.
 *
 * @param lp list
 */
void lWriteList(const lList *lp) {
   dstring buffer = DSTRING_INIT;
   const char *str;

   DENTER(CULL_LAYER);
   if (!lp) {
      DRETURN_VOID;
   }
   lWriteList_(lp, &buffer, 0);
   str = sge_dstring_get_string(&buffer);
   if (str != nullptr) {
      fprintf(stderr, "%s", str);
   }
   sge_dstring_free(&buffer);
   DRETURN_VOID;
}

/**
 * @brief Write a list to a file stream
 *
 * Write a list to a file stream
 *
 * @param lp list
 * @param fp file stream
 */
void lWriteListTo(const lList *lp, FILE *fp) {
   dstring buffer = DSTRING_INIT;
   const char *str;

   DENTER(CULL_LAYER);
   lWriteList_(lp, &buffer, 0);
   str = sge_dstring_get_string(&buffer);
   if (str != nullptr) {
      fprintf(fp, "%s", str);
   }
   sge_dstring_free(&buffer);
   DRETURN_VOID;
}

/**
 * @brief Render a list and all its elements into a string
 *
 * @param lp the list to render
 * @param[out] buffer receives the rendered list
 */
void lWriteListToStr(const lList *lp, dstring *buffer) {
   DENTER(CULL_LAYER);
   lWriteList_(lp, buffer, 0);
   DRETURN_VOID;
}

static void lWriteList_(const lList *lp, dstring *buffer, int nesting_level) {
   char indent[128];
   int i;

   DENTER(CULL_LAYER);
   if (!lp) {
      LERROR(LELISTNULL);
      DRETURN_VOID;
   }
   for (i = 0; i < nesting_level * 3; i++) {
      indent[i] = ' ';
   }
   indent[i] = '\0';

   sge_dstring_sprintf_append(buffer, "\n%sList: <%s> #Elements: %d\n",
                              indent, (lGetListName(lp) != nullptr) ? lGetListName(lp) : "nullptr",
                              lGetNumberOfElem(lp));
   for_each_ep_lv(ep, lp) {
      lWriteElem_(ep, buffer, nesting_level);
   }
   DRETURN_VOID;
}

/**
 * @brief Create an element for a specific list
 *
 * Create an element for a specific list
 *
 * @param dp descriptor
 *
 * @return element pointer or nullptr
 */
lListElem *lCreateElem(const lDescr *dp) {
   int n, i;
   lListElem *ep;

   DENTER(CULL_LAYER);

   if ((n = lCountDescr(dp)) <= 0) {
      LERROR(LECOUNTDESCR);
      DRETURN(nullptr);
   }

   ep = (lListElem *) sge_malloc(sizeof(lListElem));

   if (ep == nullptr) {
      LERROR(LEMALLOC);
      DRETURN(nullptr);
   }

   ep->next = nullptr;
   ep->prev = nullptr;

   ep->descr = (lDescr *) sge_malloc(sizeof(lDescr) * (n + 1));
   if (!ep->descr) {
      LERROR(LEMALLOC);
      sge_free(&ep);
      DRETURN(nullptr);
   }
   memcpy(ep->descr, dp, sizeof(lDescr) * (n + 1));

   /* new descr has no htables yet */
   for (i = 0; i <= n; i++) {
      ep->descr[i].ht = nullptr;
      ep->descr[i].mt |= (dp->mt & CULL_IS_REDUCED);
   }

   ep->status = FREE_ELEM;
   if (!(ep->cont = (lMultiType *) calloc(1, sizeof(lMultiType) * n))) {
      LERROR(LEMALLOC);
      sge_free(&(ep->descr));
      sge_free(&ep);
      DRETURN(nullptr);
   }

#ifdef OBSERVE
      lObserveAdd(ep, nullptr, false);
#endif

   DRETURN(ep);
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
 */
lList *lCreateList(const char *listname, const lDescr *descr) {
   return lCreateListHash(listname, descr, true);
}

/**
 * @brief Create an empty list
 *
 * Create an empty list with a given descriptor and a user defined
 * listname.
 * The caller can choose whether hashtables shall be created or not.
 *
 * @param listname list name
 * @param descr descriptor
 * @param hash shall hashtables be created?
 *
 * @return list pointer or nullptr
 */
lList *lCreateListHash(const char *listname, const lDescr *descr, bool hash) {
   lList *lp;
   int i, n;

   DENTER(CULL_LAYER);

   if (listname == nullptr) {
      listname = "No list name specified";
   }

   if (!descr || mt_get_type(descr[0].mt) == lEndT) {
      LERROR(LEDESCRNULL);
      DRETURN(nullptr);
   }

   if (!(lp = (lList *) sge_malloc(sizeof(lList)))) {
      LERROR(LEMALLOC);
      DRETURN(nullptr);
   }
   if (!(lp->listname = strdup(listname))) {
      sge_free(&lp);
      LERROR(LESTRDUP);
      DRETURN(nullptr);
   }

   lp->nelem = 0;
   if ((n = lCountDescr(descr)) <= 0) {
      sge_free(&(lp->listname));
      sge_free(&lp);
      LERROR(LECOUNTDESCR);
      DRETURN(nullptr);
   }

   lp->first = nullptr;
   lp->last = nullptr;
   if (!(lp->descr = (lDescr *) sge_malloc(sizeof(lDescr) * (n + 1)))) {
      sge_free(&(lp->listname));
      sge_free(&lp);
      LERROR(LEMALLOC);
      DRETURN(nullptr);
   }
   /* copy descriptor array */
   for (i = 0; i <= n; i++) {
      lp->descr[i].mt = descr[i].mt;
      lp->descr[i].nm = descr[i].nm;

      /* create hashtable if necessary */
      if (hash && mt_do_hashing(lp->descr[i].mt)) {
         lp->descr[i].ht = cull_hash_create(&descr[i], 0);
      } else {
         lp->descr[i].ht = nullptr;
      }
      lp->descr[i].mt |= (descr[i].mt & CULL_IS_REDUCED);
   }

#ifdef OBSERVE
   lObserveAdd(lp, nullptr, true);
#endif

   DRETURN(lp);
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
   lList *lp = nullptr;
   lListElem *ep = nullptr;
   int i;

   DENTER(CULL_LAYER);

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
 */
void lFreeElem(lListElem **ep1) {
   int i = 0;
   lListElem *ep = nullptr;

   DENTER(CULL_LAYER);

   if (ep1 == nullptr || *ep1 == nullptr) {
      DRETURN_VOID;
   }

   ep = *ep1;

   if (ep->descr == nullptr) {
      LERROR(LEDESCRNULL);
      DPRINTF("nullptr descriptor not allowed !!!\n");
      ocs::TerminationManager::trigger_abort();
   }

   for (i = 0; mt_get_type(ep->descr[i].mt) != lEndT; i++) {
      /* remove element from hash tables */
      if (ep->descr[i].ht != nullptr) {
         cull_hash_remove(ep, i);
      }

      switch (mt_get_type(ep->descr[i].mt)) {

         case lIntT:
         case lUlongT:
         case lUlong64T:
         case lDoubleT:
         case lLongT:
         case lBoolT:
         case lRefT:
            break;

         case lStringT:
            if (ep->cont[i].str != nullptr) {
               sge_free(&(ep->cont[i].str));
            }
            break;

         case lHostT:
            if (ep->cont[i].host != nullptr) {
               sge_free(&(ep->cont[i].host));
            }
            break;

         case lListT:
            if (ep->cont[i].glp != nullptr) {
               lFreeList(&(ep->cont[i].glp));
            }
            break;

         case lObjectT:
            if (ep->cont[i].obj != nullptr) {
               lFreeElem(&(ep->cont[i].obj));
            }
            break;

         default:
            unknownType("lFreeElem");
            break;
      }
   }

   /* lFreeElem is not responsible for list descriptor array */
   if (ep->status == FREE_ELEM || ep->status == OBJECT_ELEM) {
      cull_hash_free_descr(ep->descr);
      sge_free(&(ep->descr));
   }

   if (ep->cont != nullptr) {
      sge_free(&(ep->cont));
   }

#ifdef OBSERVE
   lObserveRemove(*ep1);
#endif

   sge_free(ep1);
   DRETURN_VOID;
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
 */
void lFreeList(lList **lp) {
   DENTER(CULL_LAYER);

   if (lp == nullptr || *lp == nullptr) {
      DRETURN_VOID;
   }

   /*
    * remove all hash tables,
    * it is more efficient than removing it at the end
    */
   if ((*lp)->descr != nullptr) {
      cull_hash_free_descr((*lp)->descr);
   }

   while ((*lp)->first) {
      lListElem *elem = (*lp)->first;
      lRemoveElem(*lp, &elem);
   }

   if ((*lp)->descr) {
      sge_free(&((*lp)->descr));
   }

   if ((*lp)->listname) {
      sge_free(&((*lp)->listname));
   }

#ifdef OBSERVE
   lObserveRemove(*lp);
#endif

   sge_free(lp);
   DRETURN_VOID;
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
 */
int lAddList(lList *lp0, lList **lp1) {
   /* No need to do any safety checks.  lAppendList will do them for us. */
   int res = 0;

   DENTER(CULL_LAYER);
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
 */
int lAppendList(lList *lp0, lList *lp1) {
   lListElem *ep;
   const lDescr *dp0, *dp1;

   DENTER(CULL_LAYER);

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
      if (lAppendElem(lp0, ep) == -1) {
         LERROR(LEAPPENDELEM);
         DRETURN(-1);
      }
   }

   DRETURN(0);
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
   lListElem *ep;
   const lDescr *dp0, *dp1;
   bool overridden = false;

   DENTER(CULL_LAYER);

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
 * Compare two descriptors
 *
 * @param dp0 descriptor one
 * @param dp1 descriptor two
 *
 * @return Result of compare operation 0 - equivalent -1 - not equivalent
 *
 * @note MT-NOTE: lCompListDescr() is MT safe
 */
int lCompListDescr(const lDescr *dp0, const lDescr *dp1) {
   int i, n, m;

   DENTER(CULL_LAYER);

   if (!dp0 || !dp1) {
      LERROR(LELISTNULL);
      DRETURN(-1);
   }

   /* Check if the two lists are equal */
   if ((n = lCountDescr(dp0)) <= 0) {
      LERROR(LECOUNTDESCR);
      DRETURN(-1);
   }
   if ((m = lCountDescr(dp1)) <= 0) {
      LERROR(LECOUNTDESCR);
      DRETURN(-1);
   }
   if (n == m) {
      for (i = 0; i < n; i++) {
#if 1
         int type0 = mt_get_type(dp0[i].mt);
         int type1 = mt_get_type(dp1[i].mt);
         if (dp0[i].nm != dp1[i].nm || type0 != type1) {
#else
            /*
             * comparing the mt field might be too restrictive
             * former implementation mt only contained the type information
             * now also the various flags
             */
            if (dp0[i].nm != dp1[i].nm || dp0[i].mt != dp1[i].mt) {
#endif
            LERROR(LEDIFFDESCR);
            DRETURN(-1);
         }
      }
   } else {
      LERROR(LEDIFFDESCR);
      DRETURN(-1);
   }

   DRETURN(0);
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
 *
 * @return Copy of 'src' or nullptr
 */
lList *lCopyListHash(const char *name, const lList *src, bool hash) {
   lList *dst = nullptr;
   lListElem *sep;

   DENTER(CULL_LAYER);

   if (!src) {
      LERROR(LELISTNULL);
      DRETURN(nullptr);
   }

   if (!name)
      name = src->listname;

   if (!name)
      name = "No list name specified";


   /* create new list without hashes - we'll hash it once it is filled */
   if (!(dst = lCreateListHash(name, src->descr, false))) {
      LERROR(LECREATELIST);
      DRETURN(nullptr);
   }

   for (sep = src->first; sep; sep = sep->next) {
      if (lAppendElem(dst, lCopyElem(sep)) == -1) {
         lFreeList(&dst);
         LERROR(LEAPPENDELEM);
         DRETURN(nullptr);
      }
   }
   if (hash) {
      /* now create the hash tables */
      cull_hash_create_hashtables(dst);
   }

   DRETURN(dst);
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
 * bound element aborts the process.
 *
 * @param lp      The list to insert into. Must not be nullptr.
 * @param ep      The element after which @p new_ep is inserted, or nullptr
 *                to insert at the front of the list.
 * @param new_ep  The element to insert. Must be unbound (not in any list).
 * @return        0 on success, -1 if @p lp or @p new_ep is nullptr.
 */
int lInsertElem(lList *lp, lListElem *ep, lListElem *new_ep) {
   DENTER(CULL_LAYER);

   if (!lp) {
      LERROR(LELISTNULL);
      DRETURN(-1);
   }

   if (!new_ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   /* is the element new_ep still chained in an other list, this is not allowed ? */
   if (new_ep->status == BOUND_ELEM || new_ep->status == OBJECT_ELEM) {
      DPRINTF("WARNING: tried to insert chained element\n");
      lWriteElem(new_ep);
      ocs::TerminationManager::trigger_abort();
   }

   if (ep) {
      new_ep->prev = ep;
      new_ep->next = ep->next;
      ep->next = new_ep;
      if (new_ep->next)            /* the new element has successors */
         new_ep->next->prev = new_ep;
      else                      /* the new element is the last element */
         lp->last = new_ep;
   } else {                       /* insert as first element */
      new_ep->prev = nullptr;
      new_ep->next = lp->first;
      if (!lp->first)           /* empty list ? */
         lp->last = new_ep;
      else
         lp->first->prev = new_ep;
      lp->first = new_ep;
   }

   if (new_ep->status == FREE_ELEM) {
      cull_hash_free_descr(new_ep->descr);
      sge_free(&(new_ep->descr));
   }
   new_ep->status = BOUND_ELEM;
   new_ep->descr = lp->descr;

   cull_hash_elem(new_ep);

   lp->nelem++;

#ifdef OBSERVE
   lObserveChangeOwner(ep, lp, nullptr, NoName);
#endif

   DRETURN(0);
}

/**
 * @brief Append element at the end of a list
 *
 * Append element 'ep' at the end of list 'lp'
 *
 * @param lp list
 * @param ep element
 *
 * @return error state 0 - OK -1 - Error
 *
 * @note MT-NOTE: lAppendElem() is MT safe
 */
int lAppendElem(lList *lp, lListElem *ep) {
   DENTER(CULL_LAYER);

   if (!lp) {
      LERROR(LELISTNULL);
      DRETURN(-1);
   }
   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   /* is the element ep still chained in an other list, this is not allowed ? */
   if (ep->status == BOUND_ELEM || ep->status == OBJECT_ELEM) {
      DPRINTF("WARNING: tried to append chained element\n");
      ocs::TerminationManager::trigger_abort();
   }

   if (lp->last) {
      lp->last->next = ep;
      ep->prev = lp->last;
      lp->last = ep;
      ep->next = nullptr;
   } else {
      lp->last = lp->first = ep;
      ep->prev = ep->next = nullptr;
   }

   if (ep->status == FREE_ELEM) {
      cull_hash_free_descr(ep->descr);
      sge_free(&(ep->descr));
   }
   ep->status = BOUND_ELEM;
   ep->descr = lp->descr;

   cull_hash_elem(ep);
   lp->nelem++;

#ifdef OBSERVE
   lObserveChangeOwner(ep, lp, nullptr, NoName);
#endif

   DRETURN(0);
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
 */
int lRemoveElem(lList *lp, lListElem **ep1) {
   lListElem *ep = nullptr;

   DENTER(CULL_LAYER);

   if (lp == nullptr || ep1 == nullptr || *ep1 == nullptr) {
      DRETURN(-1);
   }

   ep = *ep1;

   if (lp->descr != ep->descr) {
      CRITICAL("Removing element from other list !!!\n");
      ocs::TerminationManager::trigger_abort();
   }

   if (ep->prev) {
      ep->prev->next = ep->next;
   } else {
      lp->first = ep->next;
   }

   if (ep->next) {
      ep->next->prev = ep->prev;
   } else {
      lp->last = ep->prev;
   }

   /* nullptr the ep next and previous pointers */
   ep->prev = ep->next = nullptr;

   lp->nelem--;

#ifdef OBSERVE
   lObserveChangeOwner(ep, nullptr, lp, NoName);
#endif

   lFreeElem(ep1);
   DRETURN(0);
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
   lListElem *target_last;

   DENTER(CULL_LAYER);

   if (source == nullptr || target == nullptr) {
      LERROR(LELISTNULL);
      DRETURN_VOID;
   }
   if (ep == nullptr) {
      LERROR(LEELEMNULL);
      DRETURN_VOID;
   }

   if (source->descr != ep->descr) {
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

   cull_hash_free_descr(source->descr);
   cull_hash_free_descr((*target)->descr);

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
      (*target)->nelem++;
      source->nelem--;
   }

   cull_hash_create_hashtables(source);
   cull_hash_create_hashtables(*target);

#ifdef OBSERVE
   for_each_ep_lv(elem, *target) {
      lObserveChangeOwner(elem, *target, source, NoName);
   }
#endif

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
 */
lListElem *lDechainElem(lList *lp, lListElem *ep) {
   int i;

   DENTER(CULL_LAYER);

   if (!lp) {
      LERROR(LELISTNULL);
      DRETURN(nullptr);
   }
   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(nullptr);
   }
   if (lp->descr != ep->descr) {
      CRITICAL("Dechaining element from other list !!!");
      ocs::TerminationManager::trigger_abort();
   }

   if (ep->prev) {
      ep->prev->next = ep->next;
   } else {
      lp->first = ep->next;
   }

   if (ep->next) {
      ep->next->prev = ep->prev;
   } else {
      lp->last = ep->prev;
   }

   /* remove hash entries */
   for (i = 0; mt_get_type(ep->descr[i].mt) != lEndT; i++) {
      if (ep->descr[i].ht != nullptr) {
         cull_hash_remove(ep, i);
      }
   }

   /* nullptr the ep next and previous pointers */
   ep->prev = ep->next = (lListElem *) nullptr;
   ep->descr = lCopyDescr(ep->descr);
   ep->status = FREE_ELEM;
   lp->nelem--;

#ifdef OBSERVE
   lObserveChangeOwner(ep, nullptr, lp, NoName);
#endif

   DRETURN(ep);
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
   int pos;
   lListElem *dep;

   DENTER(CULL_LAYER);

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

   dep = (lListElem *) parent->cont[pos].obj;

   if (dep != nullptr) {
      dep->status = FREE_ELEM;
      parent->cont[pos].obj = nullptr;
   }

#ifdef OBSERVE
      lObserveChangeOwner(dep, nullptr, parent, NoName);
#endif

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
 */
lListElem *lFindFirstRW(const lList *lp, const lCondition *cp) {
   lListElem *ep;

   DENTER(CULL_LAYER);

   if (!lp) {
      LERROR(LELISTNULL);
      DRETURN(nullptr);
   }

   /* ep->next=nullptr for the last element */
   for (ep = lp->first; ep && !lCompare(ep, cp); ep = ep->next);

   DRETURN(ep);
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
 */
lListElem *lFindLastRW(const lList *lp, const lCondition *cp) {
   lListElem *ep;

   DENTER(CULL_LAYER);

   if (!lp) {
      LERROR(LELISTNULL);
      DRETURN(nullptr);
   }

   /* ep->prev=nullptr for the first element */
   for (ep = lp->last; ep && !lCompare(ep, cp); ep = ep->prev);

   DRETURN(ep);
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
 */
lListElem *lFindNextRW(const lListElem *ep, const lCondition *cp) {
   DENTER(CULL_LAYER);

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(nullptr);
   }

   do {
      ep = ep->next;
   } while (ep && (lCompare(ep, cp) == 0));

   DRETURN((lListElem *) ep);
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
 */
lListElem *lFindPrevRW(const lListElem *ep, const lCondition *cp) {
   DENTER(CULL_LAYER);

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(nullptr);
   }

   do {
      ep = ep->prev;
   } while (ep && (lCompare(ep, cp) == 0));

   DRETURN((lListElem *) ep);
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
   va_list ap;

   lSortOrder *sp;

   DENTER(CULL_LAYER);

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
 */
int lSortList(lList *lp, const lSortOrder *sp) {
   lListElem *ep;
   lListElem **pointer;
   int i, n;

   DENTER(CULL_LAYER);

   if (!lp) {
      DRETURN(0);                 /* ok list is sorted */
   }

   /*
    * step 1: build up a pointer array for use of qsort
    */

   n = lGetNumberOfElem(lp);
   if (n < 2) {
      DRETURN(0);                 /* ok list is sorted */
   }

   if (!(pointer = (lListElem **) sge_malloc(sizeof(lListElem *) * n))) {
      DRETURN(-1);                /* low memory */
   }

#ifdef RANDOMIZE_QSORT_ELEMENTS

   for (i = 0, ep = lFirstRW(lp); ep; i++, ep = lNextRW(ep)) {
      int j = (int)((double)i*rand()/(RAND_MAX+1.0));
      pointer[i] = pointer[j];
      pointer[j] = ep;
   }

#else

   for (i = 0, ep = lFirstRW(lp); ep; i++, ep = lNextRW(ep))
      pointer[i] = ep;

#endif

   /*
    * step 2: sort the pointer array using parsed sort order
    */
   cull_state_set_global_sort_order(sp);
   /* this is done to pass the sort order */
   /* to the lSortCompare function called */
   /* by lSortCompareUsingGlobal          */
   qsort((void *) pointer, n, sizeof(lListElem *), lSortCompareUsingGlobal);

   /*
    * step 3: relink elements in list according pointer array
    */
   lp->first = pointer[0];
   lp->last = pointer[n - 1];

   /* handle first element separatly */
   pointer[0]->prev = nullptr;
   pointer[n - 1]->next = nullptr;

   if (n > 1) {
      pointer[0]->next = pointer[1];
      pointer[n - 1]->prev = pointer[n - 2];
   }

   for (i = 1; i < n - 1; i++) {
      pointer[i]->prev = pointer[i - 1];
      pointer[i]->next = pointer[i + 1];
   }

   sge_free(&pointer);

   cull_hash_recreate_after_sort(lp);

   DRETURN(0);
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
 */
int lUniqStr(lList *lp, int keyfield) {
   lListElem *ep;
   lListElem *rep;

   DENTER(CULL_LAYER);

   /*
    * sort the list first to make our algorithm work
    */
   if (lPSortList(lp, "%I+", keyfield)) {
      DRETURN(-1);
   }

   /*
    * go over all elements and remove following elements
    */
   ep = lFirstRW(lp);
   while (ep) {
      rep = lNextRW(ep);
      while (rep && !strcmp(lGetString(rep, keyfield), lGetString(ep, keyfield))) {
         lRemoveElem(lp, &rep);
         rep = lNextRW(ep);
      }
      ep = lNextRW(ep);
   }

   DRETURN(0);
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
 */
int lUniqHost(lList *lp, int keyfield) {
   lListElem *ep;
   lListElem *rep;

   DENTER(CULL_LAYER);

   /*
    * sort the list first to make our algorithm work
    */
   if (lPSortList(lp, "%I+", keyfield)) {
      DRETURN(-1);
   }

   /*
    * go over all elements and remove following elements
    */
   ep = lFirstRW(lp);
   while (ep) {
      rep = lNextRW(ep);
      while (rep && !strcmp(lGetHost(rep, keyfield), lGetHost(ep, keyfield))) {
         lRemoveElem(lp, &rep);
         rep = lNextRW(ep);
      }
      ep = lNextRW(ep);
   }

   DRETURN(0);
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
