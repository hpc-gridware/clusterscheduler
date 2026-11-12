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
 * @brief Bulk operations over cull lists: select, delete
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
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_FieldException.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_ListOpsCore.h"
#include "tom/ocs_SelectCore.h"
#include "tom/ocs_SoftPath.h"
#include "cull/cull_lerrnoP.h"
#include "cull/ocs_cull_descr.h"
#include "cull/cull_hash.h"
#include "cull/cull_pack.h"
#include "cull/pack.h"

namespace {

   /**
    * @brief Parse a string into a list through the core, as lString2List() and lString2ListNone() do
    * @param function the old function's name, for the abort
    * @param lpp the list reference
    * @param dp the descriptor of the elements
    * @param parse calls the core
    * @return 0 on success, 1 on error, the list freed and nullptr then
    */
   template <typename Parse>
   int ocs_parse_to_list(const char *function, lList **lpp, const lDescr *dp, Parse parse) {
      if (lpp == nullptr) {
         ocs::tom::report_soft_path(function, NoName, ocs::tom::SoftPathReason::NullArgument);
         return 1;
      }
      if (dp == nullptr) {
         LERROR(LEDESCRNULL);
         return 0;
      }
      try {
         parse(*lpp);
      } catch (const ocs::tom::FieldException &e) {
         if (e.reason() == ocs::tom::FieldException::Reason::DuplicateKey) {
            ocs::tom::legacy_abort(function, e);
         }
         return 1;
      } catch (const ocs::tom::ListCoreError &e) {
         if (e.lerrno() != 0) {
            LERROR(e.lerrno());
         }
         return 1;
      }
      return 0;
   }

} // namespace

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
 *
 * @note A thin wrapper over ocs::tom::split_list() of the core (CS-3077).
 *
 * @see Typed form: ocs::tom::List<T>::split_off().
 */
int lSplit(lList **slp, lList **ulp, const char *ulp_name, const lCondition *cp) {
   if (slp == nullptr) {
      return -1;
   }
   try {
      ocs::tom::split_list(*slp, ulp, ulp_name, cp);
   } catch (const ocs::tom::ListCoreError &e) {
      LERROR(e.lerrno());
      return -1;
   }
   return 0;
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
 *
 * @note Evaluates the condition through lCompare() and so through ocs::tom::compare().
 * @see Typed form: ocs::tom::List<T>::remove_if(), with the condition negated
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
 *
 * @note Without a pack buffer and with a selection a thin wrapper over ocs::tom::select_elem().
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
   if (enp != nullptr && pb == nullptr) {
      if (!lCompare(slp, cp)) {
         DRETURN(nullptr);
      }
      try {
         new_ep = ocs::tom::select_elem(slp, enp, isHash, skip_no_transfer);
      } catch (const ocs::tom::FieldException &) {
         LERROR(LEELEMNULL);
      } catch (const ocs::tom::ListCoreError &e) {
         LERROR(e.lerrno());
      }
   } else if (enp != nullptr) {
      /* the shared partial descriptor, cached in the selection */
      bool owned;
      const lDescr *dp = ocs_cull_what_descr(enp, slp->descr, &owned);
      if (dp == nullptr) {
         DRETURN(nullptr);
      }
      /* pack the reduced element */
      new_ep = lSelectElemDPack(slp, cp, dp, enp, isHash, pb, skip_no_transfer);
      if (owned) {
         ocs_cull_descr_release(&dp);
      }
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

         if (lCopyElemPartialPack(new_ep, slp, enp, isHash, nullptr, skip_no_transfer)) {
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

         lCopyElemPartialPack(nullptr, slp, enp, isHash, pb);
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
 *
 * @note Built on lSelectHashPack() and so on ocs::tom::select_list().
 * @see Typed form: ocs::tom::ConstList<T>::select()
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
 *
 * @note Without a pack buffer a thin wrapper over ocs::tom::select_list().
 * @see Typed form: ocs::tom::ConstList<T>::select()
 */
lList *lSelectHashPack(const char *name, const lList *slp,
                       const lCondition *cp, const lEnumeration *enp,
                       bool isHash, sge_pack_buffer *pb, const bool skip_no_transfer) {
   DENTER(CULL_LAYER);

   lList *ret = nullptr;

   if (slp == nullptr && pb == nullptr) {
      ocs::tom::report_soft_path("lSelect", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullList);
      DRETURN(nullptr);
   }

   if (enp != nullptr) {
      if (pb == nullptr) {
         try {
            ret = ocs::tom::select_list(name, slp, cp, enp, isHash, skip_no_transfer);
         } catch (const ocs::tom::FieldException &) {
            // a null list: nothing to select from
            ocs::tom::report_soft_path("lSelect", ocs::tom::SoftPathNoName, ocs::tom::SoftPathReason::NullList);
            DRETURN(nullptr);
         } catch (const ocs::tom::ListCoreError &e) {
            LERROR(e.lerrno());
            DRETURN(nullptr);
         }
         /* an empty result of a list that may be hashed is no list, as it always was */
         if (isHash && lGetNumberOfElem(ret) == 0) {
            LERROR(LEGETNROFELEM);
            lFreeList(&ret);
         }
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
   // a selection from a list that was checked for duplicate keys when it was filled cannot create one
   const ocs::tom::UniqueCheckSuspension suspension;
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
      /* the list may be hashed from now on; its tables are built by the first lookups */
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
 * @brief Lay out the descriptor a field selection reduces another one to (CS-439)
 *
 * The result has the full length of the type, with the selected fields in
 * their slots and the others absent (see ocs_cull_descr_layout()). The source
 * may itself be a reduction; every selected field must be present in it.
 *
 * @param ep the selection
 * @param sdp the source descriptor
 * @param ddp receives the entries; room for lCountDescr(sdp) + 1 of them
 * @param indexp set to the number of entries written, without the terminating one
 *
 * @return error state 0 - OK -1 - Error
 *
 * @note A thin wrapper over ocs::tom::partial_descr().
 */
int lPartialDescr(const lEnumeration *ep, const lDescr *sdp, lDescr *ddp, int *indexp) {
   return ocs::tom::partial_descr(ep, sdp, ddp, indexp);
}

/**
 * @brief Build the shared reduced descriptor a field selection describes
 *
 * @param type the full object type
 * @param what the fields to keep
 * @return the shared descriptor holding a reference for the caller, who gives it
 *         up with ocs_cull_descr_release(), or nullptr on error
 *
 * @note A thin wrapper over ocs::tom::reduced_descr() of the core (CS-3077).
 */
const lDescr *lGetReducedDescr(const lDescr *type, const lEnumeration *what) {
   return ocs::tom::reduced_descr(type, what);
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
 * @return 0 on success, 1 on error or for a null string; on an error the list is freed and nullptr
 *
 * @note A thin wrapper over ocs::tom::string_to_list() of the core (CS-3077).
 *
 * @see Typed form: ocs::tom::ListPtr<T>::from_string().
 */
int lString2List(const char *s, lList **lpp, const lDescr *dp, int nm, const char *dlmt) {
   if (s == nullptr) {
      return 1;
   }
   return ocs_parse_to_list("lString2List", lpp, dp,
                            [&](lList *&lp) { ocs::tom::string_to_list(s, lp, dp, nm, dlmt); });
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
 *
 * @note A thin wrapper over ocs::tom::string_to_list_none() of the core (CS-3077).
 *
 * @see Typed form: ocs::tom::ListPtr<T>::from_string() with ocs::tom::NoneName::Empty.
 */
int lString2ListNone(const char *s, lList **lpp, const lDescr *dp, int nm, const char *dlmt) {
   if (s == nullptr) {
      return 1;
   }
   return ocs_parse_to_list("lString2ListNone", lpp, dp,
                            [&](lList *&lp) { ocs::tom::string_to_list_none(s, lp, dp, nm, dlmt); });
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
 *
 * @note A thin wrapper over ocs::tom::remove_common_str() of the core (CS-3077).
 *
 * @see Typed form: ocs::tom::List<T>::remove_common().
 */
int lDiffListStr(int nm, lList **lpp1, lList **lpp2) {
   if (lpp1 == nullptr || lpp2 == nullptr) {
      return -1;
   }
   try {
      ocs::tom::remove_common_str(*lpp1, *lpp2, nm, true);
   } catch (const ocs::tom::FieldException &e) {
      ocs::tom::legacy_abort("lDiffListStr", e);
   }
   return 0;
}
