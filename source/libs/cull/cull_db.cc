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
 * @brief Bulk operations over cull lists: select, join, delete
 */

#include <cstring>

/* do not compile in monitoring code */
#ifndef NO_SGE_COMPILE_DEBUG
#define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_profiling.h"
#include "uti/sge_stdlib.h"

#include "cull/cull_db.h"
#include "cull/cull_where.h"
#include "cull/cull_listP.h"
#include "cull/cull_whatP.h"
#include "cull/cull_multitypeP.h"
#include "cull/cull_lerrnoP.h"
#include "cull/cull_hash.h"
#include "cull/cull_pack.h"
#include "cull/pack.h"


static lListElem *lJoinCopyElem(const lDescr *dp,
                                const lListElem *sep0,
                                const lEnumeration *ep0,
                                const lListElem *sep1,
                                const lEnumeration *ep1);

/**
 * @brief Combine two elements
 *
 * Returns a combined element with descriptor 'dp'. Uses 'src0'
 * with mask 'enp0' and 'src1' with mask 'enp1' as source.
 *
 * @param dp descriptor
 * @param src0 element1
 * @param enp0 mask1
 * @param src1 element2
 * @param enp1 mask2
 *
 * @return combined element
 */
static lListElem *lJoinCopyElem(const lDescr *dp,
                                const lListElem *src0,
                                const lEnumeration *enp0,
                                const lListElem *src1,
                                const lEnumeration *enp1) {
   DENTER(CULL_LAYER);

   lListElem *dst;
   int i;

   if (!src0 || !src1) {
      LERROR(LEELEMNULL);
      DRETURN(nullptr);
   }

   if (!(dst = lCreateElem(dp))) {
      LERROR(LECREATEELEM);
      DRETURN(nullptr);
   }

   i = 0;
   if (lCopyElemPartialPack(dst, &i, src0, enp0, true, nullptr) == -1) {
      sge_free(&dst);
      LERROR(LECOPYELEMPART);
      DRETURN(nullptr);
   }
   if (lCopyElemPartialPack(dst, &i, src1, enp1, true, nullptr) == -1) {
      sge_free(&dst);
      LERROR(LECOPYELEMPART);
      DRETURN(nullptr);
   }

   DRETURN(dst);
}

/**
 * @brief Joins two lists together
 *
 * Returns a new list joining together the lists 'lp0' and 'lp1'
 * For the join only these 'lines' described in condition 'cp0'
 * and 'cp1' are used.
 * The new list gets only these members described in 'enp0' and
 * 'enp1'. nullptr means every member of this list.
 * The list gets 'name' as listname.
 *
 * @param name name of new list
 * @param nm0
 * @param lp0 first list
 * @param cp0 selects rows of first list
 * @param enp0 selects column of first list
 * @param nm1
 * @param lp1 second list
 * @param cp1 selects rows of second list
 * @param enp1 selects column of seconf list
 *
 * @return Joined list
 */
lList *lJoin(const char *name, int nm0, const lList *lp0,
             const lCondition *cp0, const lEnumeration *enp0, int nm1,
             const lList *lp1, const lCondition *cp1, const lEnumeration *enp1) {
   DENTER(CULL_LAYER);

   lListElem *ep0, *ep1;
   lListElem *ep;
   lList *dlp = nullptr;
   lDescr *dp;
   int lp0_pos = 0, lp1_pos = 0;
   u_int i, j;
   int needed;

   if (!lp0 || !lp1 || !name || !enp0 || !enp1) {
      LERROR(LENULLARGS);
      DRETURN(nullptr);
   }

   if (nm1 != NoName) {
      if ((lp0_pos = lGetPosInDescr(lGetListDescr(lp0), nm0)) < 0) {
         LERROR(LENAMENOT);
         DRETURN(nullptr);
      }
      if ((lp1_pos = lGetPosInDescr(lGetListDescr(lp1), nm1)) < 0) {
         LERROR(LENAMENOT);
         DRETURN(nullptr);
      }

      if (mt_get_type(lp0->descr[lp0_pos].mt) != mt_get_type(lp1->descr[lp1_pos].mt) ||
          mt_get_type(lp0->descr[lp0_pos].mt) == lListT) {
         LERROR(LEDIFFDESCR);
         DRETURN(nullptr);
      }
   }

   /* the real join ?! */
   if (!(dp = lJoinDescr(lGetListDescr(lp0), lGetListDescr(lp1), enp0, enp1))) {
      LERROR(LEJOINDESCR);
      DRETURN(nullptr);
   }
   if (!(dlp = lCreateList(name, dp))) {
      LERROR(LECREATELIST);
      sge_free(&dp);
      DRETURN(nullptr);
   }
   /* free dp it has been copied by lCreateList */
   sge_free(&dp);

   for (i = 0, ep0 = lp0->first; i < lp0->nelem; i++, ep0 = ep0->next) {
      if (!lCompare(ep0, cp0))
         continue;
      for (j = 0, ep1 = lp1->first; j < lp1->nelem; j++, ep1 = ep1->next) {
         if (!lCompare(ep1, cp1))
            continue;
         if (nm1 != NoName) {   /* in this case take it always */
            /* This is a comparison of the join fields nm0 , nm1 */
            switch (mt_get_type(lp0->descr[lp0_pos].mt)) {
               case lIntT:
                  needed = (ep0->cont[lp0_pos].i == ep1->cont[lp1_pos].i);
                  break;
               case lUlongT:
                  needed = (ep0->cont[lp0_pos].ul == ep1->cont[lp1_pos].ul);
                  break;
               case lUlong64T:
                  needed = (ep0->cont[lp0_pos].ul64 == ep1->cont[lp1_pos].ul64);
                  break;
               case lStringT:
                  needed = !strcmp(ep0->cont[lp0_pos].str, ep1->cont[lp1_pos].str);
                  break;
               case lHostT:
                  needed = !strcmp(ep0->cont[lp0_pos].str, ep1->cont[lp1_pos].str);
                  break;
               case lLongT:
                  needed = (ep0->cont[lp0_pos].l == ep1->cont[lp1_pos].l);
                  break;
               case lDoubleT:
                  needed = (ep0->cont[lp0_pos].db == ep1->cont[lp1_pos].db);
                  break;
               case lBoolT:
                  needed = (ep0->cont[lp0_pos].b == ep1->cont[lp1_pos].b);
                  break;
               case lRefT:
                  needed = (ep0->cont[lp0_pos].ref == ep1->cont[lp1_pos].ref);
                  break;
               default:
                  unknownType("lJoin");
                  DRETURN(nullptr);
            }
            if (!needed)
               continue;
         }
         if (!(ep = lJoinCopyElem(dlp->descr, ep0, enp0, ep1, enp1))) {
            LERROR(LEJOINCOPYELEM);
            lFreeList(&dlp);
            DRETURN(nullptr);
         } else {
            if (lAppendElem(dlp, ep) == -1) {
               LERROR(LEAPPENDELEM);
               lFreeList(&dlp);
               DRETURN(nullptr);
            }
         }
      }
   }

   /* RETURN AN EMPTY LIST OR nullptr THAT'S THE QUESTION */

   if (lGetNumberOfElem(dlp) == 0) {
      lFreeList(&dlp);
   }

   DRETURN(dlp);
}

/**
 * @brief Splits a list into two list
 *
 * Unchains the list elements from the list 'slp' NOT fullfilling
 * the condition 'cp' and returns a list containing the
 * unchained elements in 'ulp'
 *
 * @param slp source list pointer
 * @param ulp unchained list pointer
 * @param ulp_name 'ulp' list name
 * @param cp selects rows within 'slp'
 *
 * @return error status 0 - OK -1 - Error
 */
int lSplit(lList **slp, lList **ulp, const char *ulp_name,
           const lCondition *cp) {
   DENTER(TOP_LAYER);

   lListElem *ep, *next;
   int has_been_allocated = 0;

   /*
      iterate through the source list call lCompare and chain all elems
      that don't fullfill the condition into a new list.
    */
   if (!slp) {
      DRETURN(-1);
   }

   for (ep = lFirstRW(*slp); ep; ep = next) {
      next = ep->next;          /* this is important, cause the elem is dechained */

      if (!lCompare(ep, cp)) {
         if (ulp && !*ulp) {
            *ulp = lCreateList(ulp_name ? ulp_name : "ulp", (*slp)->descr);
            if (!*ulp) {
               DRETURN(-1);
            }
            has_been_allocated = 1;
         }
         if (ulp) {
            ep = lDechainElem(*slp, ep);
            lAppendElem(*ulp, ep);
         } else {
            lRemoveElem(*slp, &ep);
         }
      }
   }

   /* if no elements remain, free the list and return nullptr */
   if (*slp && lGetNumberOfElem(*slp) == 0) {
      lFreeList(slp);
   }
   if (has_been_allocated && *ulp && lGetNumberOfElem(*ulp) == 0) {
      lFreeList(ulp);
   }

   DRETURN(0);
}

/**
 * @brief Removes the not needed list elements
 *
 * Removes the not needed list elements from the list 'slp' NOT
 * fulfilling the condition 'cp'
 *
 * @param slp source list pointer
 * @param cp selects rows
 *
 * @return List with the remaining elements
 */
lList *lSelectDestroy(lList *slp, const lCondition *cp) {
   DENTER(CULL_LAYER);

   if (lSplit(&slp, nullptr, nullptr, cp)) {
      lFreeList(&slp);
      DRETURN(nullptr);
   }
   DRETURN(slp);
}

/**
 * @brief Extracts some elements fulfilling a condition
 *
 * Creates a new list from the list 'slp' extracting the elements
 * fulfilling the condition 'cp' or extracts the elements and
 * stores the contend in 'pb'.
 *
 * @param slp source list pointer
 * @param cp selects rows
 * @param enp selects columns
 * @param isHash create hash or not
 * @param pb packbuffer
 * @param skip_no_transfer true when the result will be handed out, so
 *                         #CULL_NO_TRANSFER fields must not be taken over
 *
 * @return list containing the extracted elements
 */
lListElem *
lSelectElemPack(const lListElem *slp, const lCondition *cp,
                const lEnumeration *enp, bool isHash, sge_pack_buffer *pb,
                const bool skip_no_transfer) {
   DENTER(CULL_LAYER);

   lListElem *new_ep = nullptr;

   if (!slp) {
      DRETURN(nullptr);
   }
   if (enp != nullptr) {
      lDescr *dp;
      int n, index = 0;

      /* create new lList with partial descriptor */
      if ((n = lCountWhat(enp, slp->descr)) <= 0) {
         LERROR(LECOUNTWHAT);
         DRETURN(nullptr);
      }
      if (!(dp = (lDescr *) sge_malloc(sizeof(lDescr) * (n + 1)))) {
         LERROR(LEMALLOC);
         DRETURN(nullptr);
      }
      /* INITIALIZE THE INDEX IF YOU BUILD A NEW DESCRIPTOR */
      if (lPartialDescr(enp, slp->descr, dp, &index) < 0) {
         LERROR(LEPARTIALDESCR);
         sge_free(&dp);
         DRETURN(nullptr);
      }
      /* create reduced element */
      new_ep = lSelectElemDPack(slp, cp, dp, enp, isHash, pb, skip_no_transfer);
      /* free the descriptor, it has been copied by lCreateList */
      cull_hash_free_descr(dp);
      sge_free(&dp);
   } else {
      /* no enumeration => make a copy of element */
      new_ep = lCopyElemHash(slp, isHash, skip_no_transfer);
   }
   DRETURN(new_ep);
}

/**
 * @brief Extracts some elements fulfilling a condition
 *
 * Creates a new list from the list 'slp' extracting the elements
 * fulfilling the condition 'cp' or it packs those elemets into 'pb' if
 * it is not nullptr.
 *
 * @param slp source list pointer
 * @param cp selects rows
 * @param dp target descriptor for the element
 * @param enp which fields to copy, or nullptr for all of them
 * @param isHash creates hash or not
 * @param pb packbuffer
 * @param skip_no_transfer true when the result will be handed out, so
 *                         #CULL_NO_TRANSFER fields must not be taken over
 *
 * @return list containing the extracted elements
 */
lListElem *
lSelectElemDPack(const lListElem *slp, const lCondition *cp, const lDescr *dp,
                 const lEnumeration *enp, bool isHash, sge_pack_buffer *pb,
                 const bool skip_no_transfer) {
   DENTER(CULL_LAYER);

   lListElem *new_ep = nullptr;
   int index = 0;

   if (!slp || (!dp && !pb)) {
      DRETURN(nullptr);
   }
   /*
    * iterate through the source list call lCompare and add
    * depending on result of lCompare
    */
   if (lCompare(slp, cp)) {
      if (pb == nullptr) {
         if (!(new_ep = lCreateElem(dp))) {
            DRETURN(nullptr);
         }

         if (lCopyElemPartialPack(new_ep, &index, slp, enp, isHash, nullptr, skip_no_transfer)) {
            lFreeElem(&new_ep);
         }
      } else {
         // add a 1 to indicate that there is an element
         PROF_START_MEASUREMENT(SGE_PROF_PACKING);
         if (packint(pb, 1) != PACK_SUCCESS) {
            PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
            DRETURN(nullptr);
         }
         PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);

         lCopyElemPartialPack(nullptr, &index, slp, enp, isHash, pb);
         new_ep = nullptr;
      }
   }
   DRETURN(new_ep);
}

/**
 * @brief Extracts some elements fulfilling a condition
 *
 * Creates a new list from the list 'slp' extracting the elements
 * fulfilling the condition 'cp'.
 *
 * @param name name for the new list
 * @param slp source list pointer
 * @param cp selects rows
 * @param enp selects columns
 *
 * @return list containing the extracted elements
 */
lList *lSelect(const char *name, const lList *slp, const lCondition *cp,
               const lEnumeration *enp) {
   return lSelectHashPack(name, slp, cp, enp, true, nullptr);
}

/**
 * @brief Extracts some elements fulfilling a condition
 *
 * Creates a new list from the list 'slp' extracting the elements
 * fulfilling the condition 'cp' or fills the packbuffer if pb is
 * not nullptr.
 *
 * @param name name for the new list
 * @param slp source list pointer
 * @param cp selects rows
 * @param enp selects columns
 * @param isHash enables/disables the hash generation
 * @param pb packbuffer
 * @param skip_no_transfer true when the result will be handed out, so
 *                         #CULL_NO_TRANSFER fields must not be taken over
 *
 * @return list containing the extracted elements
 */
lList *lSelectHashPack(const char *name, const lList *slp,
                       const lCondition *cp, const lEnumeration *enp,
                       bool isHash, sge_pack_buffer *pb, const bool skip_no_transfer) {
   DENTER(CULL_LAYER);

   lList *ret = nullptr;

   if (slp == nullptr && pb == nullptr) {
      DRETURN(nullptr);
   }

   if (enp != nullptr) {
      if (pb == nullptr) {
         lDescr *dp;
         int n, index;

         /* create new lList with partial descriptor */
         n = lCountWhat(enp, slp->descr);
         if (n <= 0) {
            LERROR(LECOUNTWHAT);
            DRETURN(nullptr);
         }

         dp = (lDescr *) sge_malloc(sizeof(lDescr) * (n + 1));
         if (dp == nullptr) {
            LERROR(LEMALLOC);
            DRETURN(nullptr);
         }

         /* INITIALIZE THE INDEX IF YOU BUILD A NEW DESCRIPTOR */
         index = 0;
         if (lPartialDescr(enp, slp->descr, dp, &index) < 0) {
            LERROR(LEPARTIALDESCR);
            sge_free(&dp);
            DRETURN(nullptr);
         }
         ret = lSelectDPack(name, slp, cp, dp, enp, isHash, nullptr, skip_no_transfer);

         /* free the descriptor, it has been copied by lCreateList */
         cull_hash_free_descr(dp);
         sge_free(&dp);
      } else {
         const char *pack_name = "";
         int local_ret;

         if (name != nullptr) {
            pack_name = name;
         } else if (slp != nullptr) {
            pack_name = slp->listname;
         }

         PROF_START_MEASUREMENT(SGE_PROF_PACKING);
         if ((local_ret = packint(pb, slp != nullptr)) != PACK_SUCCESS) {
            PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
            DRETURN(ret);
         }

         if (slp != nullptr) {
            // add the list name to the packbuffer
            if ((local_ret = packstr(pb, pack_name)) != PACK_SUCCESS) {
               PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
               DRETURN(ret);
            }

            /* pack descriptor */
            if (enp == nullptr) {
               local_ret = cull_pack_descr(pb, slp->descr);
               if (local_ret != PACK_SUCCESS) {
                  PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
                  DRETURN(ret);
               }
            } else {
               local_ret = cull_pack_enum_as_descr(pb, enp, slp->descr);
               if (local_ret != PACK_SUCCESS) {
                  PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
                  DRETURN(ret);
               }
            }
         }
         PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);

         lSelectDPack(name, slp, cp, nullptr, enp, isHash, pb);
      }
   } else {
      if (pb == nullptr) {
         ret = lCopyListHash(slp->listname, slp, isHash, skip_no_transfer);
      } else {
         cull_pack_list(pb, slp);
      }
   }
   DRETURN(ret);
}

/**
 * @brief Extracts some elements fulfilling a condition
 *
 * Creates a new list from the list 'slp' extracting the elements
 * fulfilling the condition 'cp' or packs the elements into the
 * packbuffer 'pb' if it is not nullptr.
 *
 * @param name name for the new list
 * @param slp source list pointer
 * @param cp selects rows
 * @param dp descriptor for the new list
 * @param enp selects columns
 * @param isHash enables/disables the hash table creation
 * @param pb packbuffer
 * @param skip_no_transfer true when the result will be handed out, so
 *                         #CULL_NO_TRANSFER fields must not be taken over
 *
 * @return list containing the extracted elements
 */
lList *lSelectDPack(const char *name, const lList *slp, const lCondition *cp,
                    const lDescr *dp, const lEnumeration *enp, bool isHash, sge_pack_buffer *pb,
                    const bool skip_no_transfer) {
   DENTER(CULL_LAYER);

   lListElem *ep, *new_ep;
   lList *dlp = nullptr;
   const lDescr *descr = nullptr;

   if (!slp || (!dp && !pb)) {
      DRETURN(nullptr);
   }

   if (pb == nullptr) {
      if (!(dlp = lCreateListHash(name, dp, false))) {
         LERROR(LECREATELIST);
         DRETURN(nullptr);
      }
      descr = dlp->descr;
   }

   /*
      iterate through the source list call lCompare and add
      depending on result of lCompare
    */
   for (ep = slp->first; ep; ep = ep->next) {
      new_ep = lSelectElemDPack(ep, cp, descr, enp, isHash, pb, skip_no_transfer);
      if (new_ep != nullptr) {
         if (lAppendElem(dlp, new_ep) == -1) {
            LERROR(LEAPPENDELEM);
            lFreeElem(&new_ep);
            lFreeList(&dlp);
            DRETURN(nullptr);
         }
      }
   }

   // write 0 to the end of the list to indicate that there are no more elements
   if (pb != nullptr) {
      PROF_START_MEASUREMENT(SGE_PROF_PACKING);
      if (packint(pb, 0) != PACK_SUCCESS) {
         PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
         DRETURN(nullptr);
      }
      PROF_STOP_MEASUREMENT(SGE_PROF_PACKING);
   }

   if (pb == nullptr && isHash) {
      /* now create the hash tables */
      cull_hash_create_hashtables(dlp);

      /* 
       * This is a question of philosophy.
       * To return an empty list or not to return.
       */
      if (lGetNumberOfElem(dlp) == 0) {
         LERROR(LEGETNROFELEM);
         lFreeList(&dlp);
      }
   }

   DRETURN(dlp);
}

/**
 * @brief Extracts some fields of a descriptor
 *
 * Extracts some fields of the source descriptor 'sdp' masked
 * by an enumeration 'ep' of needed fields
 *
 * @param ep mask
 * @param sdp source
 * @param ddp destination
 * @param indexp
 *
 * @return error state 0 - OK -1 - Error
 */
int lPartialDescr(const lEnumeration *ep, const lDescr *sdp, lDescr *ddp,
                  int *indexp) {
   DENTER(CULL_LAYER);

   int i;
   bool reduced = false;

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }
   if (!sdp || !ddp) {
      LERROR(LEDESCRNULL);
      DRETURN(-1);
   }
   if (!indexp) {
      LERROR(LENULLARGS);
      DRETURN(-1);
   }

   switch (ep[0].pos) {
      case WHAT_NONE:
         DRETURN(0);
      case WHAT_ALL:
         for (i = 0; mt_get_type(sdp[i].mt) != lEndT; i++) {
            ddp[*indexp].mt = sdp[i].mt;
            ddp[*indexp].nm = sdp[i].nm;
            ddp[*indexp].ht = nullptr;

            (*indexp)++;
         }
         break;
      default: {
         int maxpos = 0;
         maxpos = lCountDescr(sdp);

         /* copy and check descr */
         for (i = 0; mt_get_type(ep[i].mt) != lEndT; i++) {
            if (mt_get_type(ep[i].mt) == mt_get_type(sdp[ep[i].pos].mt) &&
                ep[i].nm == sdp[ep[i].pos].nm) {

               if (ep[i].pos > maxpos || ep[i].pos < 0) {
                  LERROR(LEENUMDESCR);
                  DRETURN(-1);
               }
               ddp[*indexp].mt = sdp[ep[i].pos].mt;
               ddp[*indexp].nm = sdp[ep[i].pos].nm;
               ddp[*indexp].ht = nullptr;
               ddp[*indexp].mt |= CULL_IS_REDUCED;
               reduced = true;

               (*indexp)++;
            } else {
               LERROR(LEENUMDESCR);
               DRETURN(-1);
            }
         }
      }
   }
   /* copy end mark */
   ddp[*indexp].mt = lEndT;
   ddp[*indexp].nm = NoName;
   ddp[*indexp].ht = nullptr;
   if (reduced) {
      ddp[*indexp].mt |= CULL_IS_REDUCED;
   }

   /* 
      We don't do (*indexp)++ in order to end up correctly if
      nothing follows and to overwrite at the end position if
      we concatenate two descriptors
    */

   DRETURN(0);
}

/**
 * @brief Builds new descriptor using two others
 *
 * Bilds from two given descriptors 'sdp0' and 'sdp1' a new
 * descriptor masked by the enumerations 'ep0' and 'ep1'.
 *
 * @param sdp0 first descriptor
 * @param sdp1 second descriptor
 * @param ep0 first mask
 * @param ep1 second mask
 *
 * @return new descriptor
 */
lDescr *lJoinDescr(const lDescr *sdp0, const lDescr *sdp1,
                   const lEnumeration *ep0, const lEnumeration *ep1) {
   DENTER(CULL_LAYER);

   int n, m, index;
   lDescr *ddp;

   if (!sdp0 || !sdp1) {
      LERROR(LEDESCRNULL);
      DRETURN(nullptr);
   }

   if (!ep0 || !ep1) {
      LERROR(LEELEMNULL);
      DRETURN(nullptr);
   }

   /* compute size of new descr */
   n = lCountWhat(ep0, sdp0);
   m = lCountWhat(ep1, sdp1);

   if (n == -1 || m == -1) {
      LERROR(LECOUNTWHAT);
      DRETURN(nullptr);
   }

   /* There is WHAT_NONE specified in both lEnumeration ptr's */
   if (!n && !m) {
      LERROR(LEENUMBOTHNONE);
      DRETURN(nullptr);
   }

   if (!(ddp = (lDescr *) sge_malloc(sizeof(lDescr) * (n + m + 1)))) {
      LERROR(LEMALLOC);
      DRETURN(nullptr);
   }
   /* INITIALIZE THE INDEX IF YOU BUILD A NEW DESCRIPTOR */
   index = 0;
   if (lPartialDescr(ep0, sdp0, ddp, &index) < 0) {
      LERROR(LEPARTIALDESCR);
      sge_free(&ddp);
      DRETURN(nullptr);
   }
   /* This one is appended */
   if (lPartialDescr(ep1, sdp1, ddp, &index) < 0) {
      LERROR(LEPARTIALDESCR);
      sge_free(&ddp);
      DRETURN(nullptr);
   }

   DRETURN(ddp);
}

/**
 * @brief Build the reduced descriptor a field selection describes
 *
 * @param type the full object type
 * @param what the fields to keep
 * @return the reduced descriptor, owned by the caller, or nullptr on error
 */
lDescr *lGetReducedDescr(const lDescr *type, const lEnumeration *what) {
   DENTER(CULL_LAYER);

   lDescr *new_descr = nullptr;
   int index = 0;
   int n = 0;
   if ((n = lCountWhat(what, type)) <= 0) {
      DRETURN(nullptr);
   }

   if (!(new_descr = (lDescr *) sge_malloc(sizeof(lDescr) * (n + 1)))) {
      DRETURN(nullptr);
   }
   if (lPartialDescr(what, type, new_descr, &index) != 0) {
      sge_free(&new_descr);
      DRETURN(nullptr);
   }

   DRETURN(new_descr);
}

/**
 * @brief Convert char* string into CULL list
 *
 * Parses separated strings and adds them into the cull list *lpp
 * The string is a unique key for the list and resides at field 'nm'
 * If 'deleminator' is nullptr than isspace() is used.
 *
 * @param s String to parse
 * @param lpp reference to lList*
 * @param dp list Type
 * @param nm list field
 * @param dlmt the separator between entries; nullptr means any whitespace
 *
 * @return error state 1 - OK 0 - On error
 */
int lString2List(const char *s, lList **lpp, const lDescr *dp, int nm,
                 const char *dlmt) {
   DENTER(TOP_LAYER);

   int pos;
   int dataType;
   struct saved_vars_s *context = nullptr;

   if (!s) {
      DRETURN(1);
   }

   pos = lGetPosInDescr(dp, nm);
   dataType = lGetPosType(dp, pos);
   switch (dataType) {
      case lStringT:
         DPRINTF("lString2List: got lStringT data type\n");
         for (s = sge_strtok_r(s, dlmt, &context); s; s = sge_strtok_r(nullptr, dlmt, &context)) {
            if (lGetElemStr(*lpp, nm, s)) {
               /* silently ignore multiple occurencies */
               continue;
            }
            if (!lAddElemStr(lpp, nm, s, dp)) {
               sge_free_saved_vars(context);
               lFreeList(lpp);
               DRETURN(1);
            }
         }

         break;
      case lHostT:
         DPRINTF("lString2List: got lHostT data type\n");
         for (s = sge_strtok_r(s, dlmt, &context); s; s = sge_strtok_r(nullptr, dlmt, &context)) {
            if (lGetElemHost(*lpp, nm, s)) {
               /* silently ignore multiple occurencies */
               continue;
            }
            if (!lAddElemHost(lpp, nm, s, dp)) {
               sge_free_saved_vars(context);
               lFreeList(lpp);
               DRETURN(1);
            }
         }

         break;
      default:
         DPRINTF("lString2List: unexpected data type\n");
         break;
   }

   if (context)
      sge_free_saved_vars(context);

   DRETURN(0);
}

/**
 * @brief Like #lString2List, but treats `"none"` as an empty list
 *
 * @param s the string to parse
 * @param[out] lpp receives the list
 * @param dp the object type to create elements of
 * @param nm the field the parsed string is stored in
 * @param dlmt the separator between entries; nullptr means any whitespace
 * @return 1 on success, 0 on error
 */
int lString2ListNone(const char *s, lList **lpp, const lDescr *dp,
                     int nm, const char *dlmt) {
   int pos;
   int dataType;
   if (lString2List(s, lpp, dp, nm, dlmt))
      return 1;


   pos = lGetPosInDescr(dp, nm);
   dataType = lGetPosType(dp, pos);
   switch (dataType) {
      case lStringT:
         if (lGetNumberOfElem(*lpp) > 1 && lGetElemCaseStr(*lpp, nm, "none")) {
            lFreeList(lpp);
            return 1;
         }

         if (lGetNumberOfElem(*lpp) == 1 && lGetElemCaseStr(*lpp, nm, "none"))
            lFreeList(lpp);
         break;
      case lHostT:
         if (lGetNumberOfElem(*lpp) > 1 && lGetElemHost(*lpp, nm, "none")) {
            lFreeList(lpp);
            return 1;
         }

         if (lGetNumberOfElem(*lpp) == 1 && lGetElemHost(*lpp, nm, "none"))
            lFreeList(lpp);
         break;

      default:
         break;
   }

   return 0;
}

/**
 * @brief Remove elements with the same string
 *
 * Remove elements in both lists with the same string key in
 * field 'nm'.
 *
 * @param nm field name id
 * @param lpp1 first list
 * @param lpp2 second list
 *
 * @return error status 0 - OK -1 - Error
 */
int lDiffListStr(int nm, lList **lpp1, lList **lpp2) {
   DENTER(CULL_LAYER);

   const char *key;
   const lListElem *ep, *to_check;

   if (!lpp1 || !lpp2) {
      DRETURN(-1);
   }

   if (!*lpp1 || !*lpp2) {
      DRETURN(0);
   }

   ep = lFirst(*lpp1);
   while (ep) {
      to_check = ep;
      key = lGetString(to_check, nm);

      ep = lNext(ep);           /* point to next element before del */

      if (lGetElemStr(*lpp2, nm, key) != nullptr) {
         lDelElemStr(lpp2, nm, key);
         lDelElemStr(lpp1, nm, key);
      }
   }

   DRETURN(0);
}
