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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>

/* do not compile in monitoring code */
#ifndef NO_SGE_COMPILE_DEBUG
#define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

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

/****** cull/db/lJoinCopyElem() ***********************************************
*  NAME
*     lJoinCopyElem() -- Combine two elements 
*
*  SYNOPSIS
*     static lListElem* lJoinCopyElem(const lDescr *dp, 
*                                     const lListElem *src0, 
*                                     const lEnumeration *enp0, 
*                                     const lListElem *src1, 
*                                     const lEnumeration *enp1) 
*
*  FUNCTION
*     Returns a combined element with descriptor 'dp'. Uses 'src0'
*     with mask 'enp0' and 'src1' with mask 'enp1' as source. 
*
*  INPUTS
*     const lDescr *dp         - descriptor 
*     const lListElem *src0    - element1 
*     const lEnumeration *enp0 - mask1 
*     const lListElem *src1    - element2 
*     const lEnumeration *enp1 - mask2 
*
*  RESULT
*     static lListElem* - combined element 
******************************************************************************/
static lListElem *lJoinCopyElem(const lDescr *dp,
                                const lListElem *src0,
                                const lEnumeration *enp0,
                                const lListElem *src1,
                                const lEnumeration *enp1) {
   lListElem *dst;
   int i;

   DENTER(CULL_LAYER);

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

/****** cull/db/lJoinSublist() ************************************************
*  NAME
*     lJoinSublist() -- Join a list with one of its sublists 
*
*  SYNOPSIS
*     lList* lJoinSublist(const char *name, 
*                         int nm0, 
*                         const lList *lp, 
*                         const lCondition *cp0, 
*                         const lEnumeration *enp0, 
*                         const lDescr *sldp, 
*                         const lCondition *cp1, 
*                         const lEnumeration *enp1) 
*
*  FUNCTION
*     Joins a list and one of its sublists together. The other 
*     parameters are equal to them from lJoin(). In the enumeration
*     'enp0' the sublist field neither may be selected nor 'enp0'
*     may be nullptr.
*
*  INPUTS
*     const char *name         - new list name 
*     int nm0                  - 
*     const lList *lp          - list 
*     const lCondition *cp0    - selects rows within 'lp' 
*     const lEnumeration *enp0 - selects columns within 'lp' 
*     const lDescr *sldp       - sublist descriptor pointer 
*     const lCondition *cp1    - selects rows within 'sldp' 
*     const lEnumeration *enp1 - selects columns within 'enp1' 
*
*  RESULT
*     lList* - Joined list 
******************************************************************************/
lList *lJoinSublist(const char *name, int nm0, const lList *lp,
                    const lCondition *cp0, const lEnumeration *enp0,
                    const lDescr *sldp, const lCondition *cp1,
                    const lEnumeration *enp1) {
   lList *dlp, *tlp, *joinedlist, *sublist;
   const lListElem *ep;
   lDescr *dp;
   const lDescr *tdp;
   int i, pos;

   DENTER(CULL_LAYER);

   /* check different pointers */
   if (!name || !lp || !enp0 || !sldp || !enp1) {
      LERROR(LENULLARGS);
      DRETURN(nullptr);
   }

   /* make sure that nm0 is a sublist field of lp */
   if (!(tdp = lGetListDescr(lp))) {
      LERROR(LEDESCRNULL);
      DRETURN(nullptr);
   }
   if ((pos = lGetPosInDescr(tdp, nm0)) < 0) {
      LERROR(LENAMENOT);
      DRETURN(nullptr);
   }

   if (mt_get_type(tdp[pos].mt) != lListT) {
      LERROR(LEINCTYPE);
      DRETURN(nullptr);
   }

   /* is nm0 enumerated in enp0 ? */
   if (enp0[0].pos == WHAT_ALL) {
      LERROR(LEFALSEFIELD);
      DRETURN(nullptr);
   }
   for (i = 0; enp0[i].nm != NoName; i++)
      if (enp0[i].nm == nm0) {
         LERROR(LEFALSEFIELD);
         DRETURN(nullptr);
      }

   /* create destination list */
   if (!(dp = lJoinDescr(lGetListDescr(lp), sldp, enp0, enp1))) {
      LERROR(LEJOINDESCR);
      DRETURN(nullptr);
   }
   if (!(dlp = lCreateList(name, dp))) {
      sge_free(&dp);
      LERROR(LECREATELIST);
      DRETURN(nullptr);
   }
   /* free dp it has been copied in lCreateList */
   sge_free(&dp);

   /* create a temporary list to be used by lJoin */
   if (!(tlp = lCreateList("lJoinSublist: tlp", lGetListDescr(lp)))) {
      lFreeList(&dlp);
      LERROR(LECREATELIST);
      DRETURN(nullptr);
   }

   for_each_where(ep, lp, cp0) {
      /* is there a sublist for the join */
      if ((sublist = lGetListRW(ep, nm0)) != nullptr) {

         /* put each element in the tlp to be used by lJoin */
         if (lAppendElem(tlp, lCopyElem(ep)) == -1) {
            lFreeList(&tlp);
            lFreeList(&dlp);
            LERROR(LEAPPENDELEM);
            DRETURN(nullptr);
         }

         /* join the tlp with one element together with its sublist */
         joinedlist = lJoin("lJoinSublist: joinedlist", nm0, tlp, nullptr, enp0,
                            NoName, sublist, cp1, enp1);

         if (!joinedlist) {
            lFreeList(&tlp);
            lFreeList(&dlp);
            LERROR(LEJOIN);
            DRETURN(nullptr);
         }

         /* joinedlist is freed in lAddList */
         if (joinedlist && lAddList(dlp, &joinedlist) == -1) {
            LERROR(LEADDLIST);
            lFreeList(&tlp);
            lFreeList(&dlp);
            DRETURN(nullptr);
         }

         /* dechain the only element from tlp and free it (copy) */
         lRemoveElem(tlp, &(tlp->first));
      }
   }
   /* temporary list has to be freed */
   lFreeList(&tlp);

   /* RETURN AN EMPTY LIST OR nullptr THAT'S THE QUESTION */

   if (lGetNumberOfElem(dlp) == 0) {
      lFreeList(&dlp);
   }

   DRETURN(dlp);
}

/****** cull/db/lJoin() *******************************************************
*  NAME
*     lJoin() -- Joins two lists together
*
*  SYNOPSIS
*     lList* lJoin(const char *name, int nm0, const lList *lp0, 
*                  const lCondition *cp0, const lEnumeration *enp0, 
*                  int nm1, const lList *lp1, const lCondition *cp1, 
*                  const lEnumeration *enp1) 
*
*  FUNCTION
*     Returns a new list joining together the lists 'lp0' and 'lp1'
*     For the join only these 'lines' described in condition 'cp0'
*     and 'cp1' are used.
*     The new list gets only these members described in 'enp0' and
*     'enp1'. nullptr means every member of this list.
*     The list gets 'name' as listname.
*
*  INPUTS
*     const char *name         - name of new list 
*     int nm0                  - 
*     const lList *lp0         - first list 
*     const lCondition *cp0    - selects rows of first list 
*     const lEnumeration *enp0 - selects column of first list 
*     int nm1                  - 
*     const lList *lp1         - second list 
*     const lCondition *cp1    - selects rows of second list 
*     const lEnumeration *enp1 - selects column of seconf list 
*
*  RESULT
*     lList* - Joined list 
******************************************************************************/
lList *lJoin(const char *name, int nm0, const lList *lp0,
             const lCondition *cp0, const lEnumeration *enp0, int nm1,
             const lList *lp1, const lCondition *cp1, const lEnumeration *enp1) {
   lListElem *ep0, *ep1;
   lListElem *ep;
   lList *dlp = nullptr;
   lDescr *dp;
   int lp0_pos = 0, lp1_pos = 0;
   u_int i, j;
   int needed;

   DENTER(CULL_LAYER);

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
               case lFloatT:
                  needed = (ep0->cont[lp0_pos].fl == ep1->cont[lp1_pos].fl);
                  break;
               case lDoubleT:
                  needed = (ep0->cont[lp0_pos].db == ep1->cont[lp1_pos].db);
                  break;
               case lCharT:
                  needed = (ep0->cont[lp0_pos].c == ep1->cont[lp1_pos].c);
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

/****** cull/db/lSplit() ******************************************************
*  NAME
*     lSplit() -- Splits a list into two list 
*
*  SYNOPSIS
*     int lSplit(lList **slp, lList **ulp, const char *ulp_name, 
*                const lCondition *cp) 
*
*  FUNCTION
*     Unchains the list elements from the list 'slp' NOT fullfilling
*     the condition 'cp' and returns a list containing the 
*     unchained elements in 'ulp' 
*
*  INPUTS
*     lList **slp          - source list pointer 
*     lList **ulp          - unchained list pointer 
*     const char *ulp_name - 'ulp' list name 
*     const lCondition *cp - selects rows within 'slp' 
*
*  RESULT
*     int - error status
*         0 - OK
*        -1 - Error 
******************************************************************************/
int lSplit(lList **slp, lList **ulp, const char *ulp_name,
           const lCondition *cp) {

   lListElem *ep, *next;
   int has_been_allocated = 0;

   DENTER(TOP_LAYER);

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

/****** cull/db/lSelectDestroy() **********************************************
*  NAME
*     lSelectDestroy() -- Removes the not needed list elements 
*
*  SYNOPSIS
*     lList* lSelectDestroy(lList *slp, const lCondition *cp) 
*
*  FUNCTION
*     Removes the not needed list elements from the list 'slp' NOT
*     fulfilling the condition 'cp' 
*
*  INPUTS
*     lList *slp           - source list pointer 
*     const lCondition *cp - selects rows 
*
*  RESULT
*     lList* - List with the remaining elements 
******************************************************************************/
lList *lSelectDestroy(lList *slp, const lCondition *cp) {

   DENTER(CULL_LAYER);

   if (lSplit(&slp, nullptr, nullptr, cp)) {
      lFreeList(&slp);
      DRETURN(nullptr);
   }
   DRETURN(slp);
}

/****** cull/db/lSelectElemPack() *********************************************
*  NAME
*     lSelectElemPack() -- Extracts some elements fulfilling a condition 
*
*  SYNOPSIS
*     lListElem* 
*     lSelectElemPack(const lListElem *slp, const lCondition *cp, 
*                     const lEnumeration *enp, bool isHash, 
*                     sge_pack_buffer *pb) 
*
*  FUNCTION
*     Creates a new list from the list 'slp' extracting the elements
*     fulfilling the condition 'cp' or extracts the elements and
*     stores the contend in 'pb'. 
*
*  INPUTS
*     const lListElem *slp    - source list pointer 
*     const lCondition *cp    - selects rows 
*     const lEnumeration *enp - selects columns 
*     bool isHash             - create hash or not
*     sge_pack_buffer *pb     - packbuffer
*
*  RESULT
*     lListElem* - list containing the extracted elements
******************************************************************************/
lListElem *
lSelectElemPack(const lListElem *slp, const lCondition *cp,
                const lEnumeration *enp, bool isHash, sge_pack_buffer *pb) {
   lListElem *new_ep = nullptr;

   DENTER(CULL_LAYER);

   if (!slp) {
      DRETURN(nullptr);
   }
   if (enp != nullptr) {
      lDescr *dp;
      int n, index = 0;
      u_long32 elements = 0;

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
      new_ep = lSelectElemDPack(slp, cp, dp, enp, isHash, pb, &elements);
      /* free the descriptor, it has been copied by lCreateList */
      cull_hash_free_descr(dp);
      sge_free(&dp);
   } else {
      /* no enumeration => make a copy of element */
      new_ep = lCopyElemHash(slp, isHash);
   }
   DRETURN(new_ep);
}

/****** cull/db/lSelectElemDPack() ********************************************
*  NAME
*     lSelectElemDPack() -- Extracts some elements fulfilling a condition 
*
*  SYNOPSIS
*     lListElem* 
*     lSelectElemDPack(const lListelem *slp, const lCondition *cp, 
*                      const lEnumeration *enp, bool isHash, 
*                      sge_pack_buffer *pb) 
*
*  FUNCTION
*     Creates a new list from the list 'slp' extracting the elements
*     fulfilling the condition 'cp' or it packs those elemets into 'pb' if 
*     it is not nullptr.
*
*  INPUTS
*     const lListElem *slp     - source list pointer 
*     const lCondition *cp     - selects rows 
*     const lDescr *dp         - target descriptor for the element
*     bool  isHash             - creates hash or not
*     sge_pack_buffer *pb      - packbuffer
*     u_long32 *elements       - increases the number of elems, if one is
*                                added to the pb. Only, when elements is 
*                                not nullptr (only used if pb != nullptr)
*
*  RESULT
*     lListElem* - list containing the extracted elements
******************************************************************************/
lListElem *
lSelectElemDPack(const lListElem *slp, const lCondition *cp, const lDescr *dp,
                 const lEnumeration *enp, bool isHash, sge_pack_buffer *pb,
                 u_long32 *elements) {
   lListElem *new_ep = nullptr;
   int index = 0;

   DENTER(CULL_LAYER);
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

         if (lCopyElemPartialPack(new_ep, &index, slp, enp, isHash, nullptr)) {
            lFreeElem(&new_ep);
         }
      } else {
         if (elements != nullptr) {
            (*elements)++;
         }

         lCopyElemPartialPack(nullptr, &index, slp, enp, isHash, pb);
         new_ep = nullptr;
      }
   }
   DRETURN(new_ep);
}

/****** cull/db/lSelect() *****************************************************
*  NAME
*     lSelect() -- Extracts some elements fulfilling a condition 
*
*  SYNOPSIS
*     lList* lSelect(const char *name, const lList *slp, 
*                    const lCondition *cp, const lEnumeration *enp) 
*
*  FUNCTION
*     Creates a new list from the list 'slp' extracting the elements
*     fulfilling the condition 'cp'. 
*
*  INPUTS
*     const char *name        - name for the new list 
*     const lList *slp        - source list pointer 
*     const lCondition *cp    - selects rows 
*     const lEnumeration *enp - selects columns 
*
*  RESULT
*     lList* - list containing the extracted elements
******************************************************************************/
lList *lSelect(const char *name, const lList *slp, const lCondition *cp,
               const lEnumeration *enp) {
   return lSelectHashPack(name, slp, cp, enp, true, nullptr);
}

/****** cull/db/lSelectHashPack() *********************************************
*  NAME
*     lSelectHashPack() -- Extracts some elements fulfilling a condition 
*
*  SYNOPSIS
*     lList* 
*     lSelectHashPack(const char *name, const lList *slp, 
*                     const lCondition *cp, const lEnumeration *enp, 
*                     bool isHash, sge_pack_buffer *pb) 
*
*  FUNCTION
*     Creates a new list from the list 'slp' extracting the elements
*     fulfilling the condition 'cp' or fills the packbuffer if pb is 
*     not nullptr.
*
*  INPUTS
*     const char *name        - name for the new list 
*     const lList *slp        - source list pointer 
*     const lCondition *cp    - selects rows 
*     const lEnumeration *enp - selects columns 
*     bool  isHash            - enables/disables the hash generation
*     sge_pack_buffer *pb     - packbuffer
*
*  RESULT
*     lList* - list containing the extracted elements
******************************************************************************/
lList *lSelectHashPack(const char *name, const lList *slp,
                       const lCondition *cp, const lEnumeration *enp,
                       bool isHash, sge_pack_buffer *pb) {
   lList *ret = nullptr;

   DENTER(CULL_LAYER);
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
         ret = lSelectDPack(name, slp, cp, dp, enp, isHash, nullptr, nullptr);

         /* free the descriptor, it has been copied by lCreateList */
         cull_hash_free_descr(dp);
         sge_free(&dp);
      } else {
         u_long32 number_of_packed_elements = 0;
         size_t offset = 0;
         size_t used = 0;
         const char *pack_name = "";
         int local_ret;

         if (name != nullptr) {
            pack_name = name;
         } else if (slp != nullptr) {
            pack_name = slp->listname;
         }

         local_ret = cull_pack_list_summary(pb, slp, enp, pack_name, &offset, &used);
         if (local_ret != PACK_SUCCESS) {
            LERROR(LEMALLOC);
            DRETURN(nullptr);
         }

         lSelectDPack(name, slp, cp, nullptr, enp, isHash, pb,
                      &number_of_packed_elements);

         /*
          * change number of elements contained in the packbuffer 
          */
         if (slp != nullptr && pb != nullptr) {
            char *old_cur_ptr = nullptr;
            size_t old_used = 0;

            old_cur_ptr = pb->cur_ptr;
            old_used = pb->bytes_used;
            pb->cur_ptr = pb->head_ptr + offset;
            pb->bytes_used = used;

            local_ret = repackint(pb, number_of_packed_elements);
            if (local_ret != PACK_SUCCESS) {
               LERROR(LEMALLOC);
               DRETURN(nullptr);
            }
            pb->cur_ptr = old_cur_ptr;
            pb->bytes_used = old_used;
         }
      }
   } else {
      if (pb == nullptr) {
         ret = lCopyListHash(slp->listname, slp, isHash);
      } else {
         cull_pack_list(pb, slp);
      }
   }
   DRETURN(ret);
}

/****** cull_db/lSelectDPack() ************************************************
*  NAME
*     lSelectDPack() --  Extracts some elements fulfilling a condition 
*
*  SYNOPSIS
*     lList* lSelectDPack(const char *name, const lList *slp, 
*                         const lCondition *cp, const lDescr *dp, 
*                         bool isHash, sge_pack_buffer *pb) 
*
*
*  FUNCTION
*     Creates a new list from the list 'slp' extracting the elements
*     fulfilling the condition 'cp' or packs the elements into the
*     packbuffer 'pb' if it is not nullptr.
*
*  INPUTS
*     const char *name        - name for the new list 
*     const lList *slp        - source list pointer 
*     const lCondition *cp    - selects rows 
*     const lDescr *dp        - descriptor for the new list
*     const lEnumeration *enp - selects columns
*     bool  isHash            - enables/disables the hash table creation 
*     sge_pack_buffer *pb     - packbuffer
*     u_long32 *elements      - number of packed elements 
*                               (only used if pb != nullptr)
*
*  RESULT
*     lList* - list containing the extracted elements
*******************************************************************************/
lList *lSelectDPack(const char *name, const lList *slp, const lCondition *cp,
                    const lDescr *dp, const lEnumeration *enp, bool isHash,
                    sge_pack_buffer *pb, u_long32 *elements) {

   lListElem *ep, *new_ep;
   lList *dlp = (lList *) nullptr;
   const lDescr *descr = nullptr;

   DENTER(CULL_LAYER);

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
      new_ep = lSelectElemDPack(ep, cp, descr, enp, isHash, pb, elements);
      if (new_ep != nullptr) {
         if (lAppendElem(dlp, new_ep) == -1) {
            LERROR(LEAPPENDELEM);
            lFreeElem(&new_ep);
            lFreeList(&dlp);
            DRETURN(nullptr);
         }
      }
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

/****** cull/db/lPartialDescr() ***********************************************
*  NAME
*     lPartialDescr() -- Extracts some fields of a descriptor 
*
*  SYNOPSIS
*     int lPartialDescr(const lEnumeration *ep, const lDescr *sdp, 
*                       lDescr *ddp, int *indexp) 
*
*  FUNCTION
*     Extracts some fields of the source descriptor 'sdp' masked
*     by an enumeration 'ep' of needed fields 
*
*  INPUTS
*     const lEnumeration *ep - mask 
*     const lDescr *sdp      - source 
*     lDescr *ddp            - destination 
*     int *indexp            - 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error 
*******************************************************************************/
int lPartialDescr(const lEnumeration *ep, const lDescr *sdp, lDescr *ddp,
                  int *indexp) {
   int i;
   bool reduced = false;

   DENTER(CULL_LAYER);

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

/****** cull/db/lJoinDescr() **************************************************
*  NAME
*     lJoinDescr() -- Builds new descriptor using two others 
*
*  SYNOPSIS
*     lDescr* lJoinDescr(const lDescr *sdp0, 
*                        const lDescr *sdp1, 
*                        const lEnumeration *ep0, 
*                        const lEnumeration *ep1) 
*
*  FUNCTION
*     Bilds from two given descriptors 'sdp0' and 'sdp1' a new
*     descriptor masked by the enumerations 'ep0' and 'ep1'. 
*
*  INPUTS
*     const lDescr *sdp0      - first descriptor 
*     const lDescr *sdp1      - second descriptor 
*     const lEnumeration *ep0 - first mask 
*     const lEnumeration *ep1 - second mask 
*
*  RESULT
*     lDescr* - new descriptor
******************************************************************************/
lDescr *lJoinDescr(const lDescr *sdp0, const lDescr *sdp1,
                   const lEnumeration *ep0, const lEnumeration *ep1) {
   int n, m, index;
   lDescr *ddp;

   DENTER(CULL_LAYER);

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

lDescr *lGetReducedDescr(const lDescr *type, const lEnumeration *what) {

   lDescr *new_descr = nullptr;
   int index = 0;
   int n = 0;
   DENTER(CULL_LAYER);

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

/****** cull/db/lString2List() ************************************************
*  NAME
*     lString2List() -- Convert char* string into CULL list 
*
*  SYNOPSIS
*     int lString2List(const char *s, lList **lpp, const lDescr *dp, 
*                      int nm, const char *delimitor); 
*
*  FUNCTION
*     Parses separated strings and adds them into the cull list *lpp
*     The string is a unique key for the list and resides at field 'nm'
*     If 'deleminator' is nullptr than isspace() is used.
*
*  INPUTS
*     const char *s         - String to parse   
*     lList **lpp           - reference to lList*      
*     const lDescr *dp      - list Type     
*     int nm                - list field       
*     const char *delimitor - string delimitor        
*
*  RESULT
*     int - error state
*         1 - OK
*         0 - On error
******************************************************************************/
int lString2List(const char *s, lList **lpp, const lDescr *dp, int nm,
                 const char *dlmt) {
   int pos;
   int dataType;
   struct saved_vars_s *context = nullptr;

   DENTER(TOP_LAYER);

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

/****** cull/db/lString2ListNone() ********************************************
*  NAME
*     lString2ListNone() -- 
*
*  SYNOPSIS
*     int lString2ListNone(const char *s, lList **lpp, const lDescr *dp, 
*                          int nm, const char *dlmt) 
*
*  FUNCTION
*
*  INPUTS
*     const char *s    - ??? 
*     lList **lpp      - ??? 
*     const lDescr *dp - ??? 
*     int nm           - ??? 
*     const char *dlmt - ??? 
*
*  RESULT
*     int - error state 
*         0 - OK
*  EXAMPLE
*
*  NOTES
*
*  BUGS
*
*  SEE ALSO
******************************************************************************/
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
         DPRINTF("lString2ListNone: got lStringT data type\n");
         if (lGetNumberOfElem(*lpp) > 1 && lGetElemCaseStr(*lpp, nm, "none")) {
            lFreeList(lpp);
            return 1;
         }

         if (lGetNumberOfElem(*lpp) == 1 && lGetElemCaseStr(*lpp, nm, "none"))
            lFreeList(lpp);
         break;
      case lHostT:
         DPRINTF("lString2ListNone: got lHostT data type\n");
         if (lGetNumberOfElem(*lpp) > 1 && lGetElemHost(*lpp, nm, "none")) {
            lFreeList(lpp);
            return 1;
         }

         if (lGetNumberOfElem(*lpp) == 1 && lGetElemHost(*lpp, nm, "none"))
            lFreeList(lpp);
         break;

      default:
         DPRINTF("lString2ListNone: unexpected data type\n");
         break;
   }

   return 0;
}

/****** cull/db/lDiffListStr() ************************************************
*  NAME
*     lDiffListStr() -- Remove elements with the same string
*
*  SYNOPSIS
*     int lDiffListStr(int nm, lList **lpp1, lList **lpp2) 
*
*  FUNCTION
*     Remove elements in both lists with the same string key in 
*     field 'nm'.
*
*  INPUTS
*     int nm       - field name id 
*     lList **lpp1 - first list 
*     lList **lpp2 - second list 
*
*  RESULT
*     int - error status
*         0 - OK
*        -1 - Error
******************************************************************************/
int lDiffListStr(int nm, lList **lpp1, lList **lpp2) {
   const char *key;
   const lListElem *ep, *to_check;

   DENTER(CULL_LAYER);

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

int lDiffListUlong(int nm, lList **lpp1, lList **lpp2) {
   u_long32 key;
   const lListElem *ep, *to_check;

   DENTER(CULL_LAYER);

   if (!lpp1 || !lpp2) {
      DRETURN(-1);
   }

   if (!*lpp1 || !*lpp2) {
      DRETURN(0);
   }

   ep = lFirst(*lpp1);
   while (ep) {
      to_check = ep;
      key = lGetUlong(to_check, nm);

      ep = lNext(ep);

      if (lGetElemUlong(*lpp2, nm, key) != nullptr) {
         lDelElemUlong(lpp2, nm, key);
         lDelElemUlong(lpp1, nm, key);
      }
   }
   DRETURN(0);
}
