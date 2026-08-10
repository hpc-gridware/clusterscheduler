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
 * @brief Reading and writing the fields of a cull element
 */

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

/* do not compile in monitoring code */
#ifndef NO_SGE_COMPILE_DEBUG
/// Suppresses the monitoring code in the rmon macros for this file
#define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/ocs_TerminationManager.h"
#include "uti/sge_stdlib.h"

#include "cull/msg_cull.h"
#include "cull/cull_multitypeP.h"
#include "cull/cull_listP.h"
#include "cull/cull_whatP.h"
#include "cull/cull_lerrnoP.h"
#include "cull/cull_hash.h"

#ifdef OBSERVE
#  include "cull/cull_observe.h"
#endif

#define CULL_BASIS_LAYER CULL_LAYER ///< rmon layer this file logs under

/* ---------- global variable --------------------------------- */


/// Name of each @ref _enum_lMultiType, indexed by the enumerator; used in error messages and dumps
const char *multitypes[] =
        {
                "lEndT",
                "lDoubleT",
                "lUlongT",
                "lLongT",
                "lBoolT",
                "lIntT",
                "lStringT",
                "lListT",
                "lObjectT",
                "lRefT",
                "lHostT",
                "lUlong64T"
        };

/**
 * @brief Report a field accessed as the wrong type, and abort
 *
 * @param str name of the function that detected the mismatch
 * @return never returns; the process is aborted
 */
int incompatibleType(const char *str) {
   DENTER(TOP_LAYER);

   int i;

   for (i = 0; i < 5; i++)
           DPRINTF("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
   DPRINTF("incompatible type in function %s()\n", str);
   for (i = 0; i < 5; i++)
           DPRINTF("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

   ocs::TerminationManager::trigger_abort();
   DRETURN(-1);
}

/**
 * @brief Report a field accessed as the wrong type, with a formatted message, and abort
 *
 * @param fmt `printf` style format naming the field and the types involved
 * @param ... the format's arguments
 * @return never returns; the process is aborted
 */
int incompatibleType2(const char *fmt, ...) {
   DENTER(TOP_LAYER);

   va_list ap;
   char buf[MAX_STRING_SIZE];

   va_start(ap, fmt);
   vsnprintf(buf, sizeof(buf), fmt, ap);
   va_end(ap);

   CRITICAL(SFNMAX, buf);
   fprintf(stderr, SFNMAX, buf);

   ocs::TerminationManager::trigger_abort();
   DRETURN(-1);
}

/**
 * @brief Report an unknown field type, and abort
 *
 * @param str name of the function that detected it
 * @return never returns; the process is aborted
 */
int unknownType(const char *str) {
   DENTER(CULL_LAYER);

   /* abort is used, so we don't free any memory; if you change this
      function please free your memory                      */
   DPRINTF("Unknown Type in %s.\n", str);
   LERROR(LEUNKTYPE);
   DRETURN(-1);
}

/**
 * @brief Get Position of name within element
 *
 * Get Position of field 'name' within 'element'
 *
 * @param element element
 * @param name field name id
 * @param do_abort call do_abort if do_abort=1
 *
 * @return position or -1 in case of an error
 */
int lGetPosViaElem(const lListElem *element, int name, int do_abort) {
   DENTER(CULL_BASIS_LAYER);

   int pos = -1;

   if (!element) {
      if (do_abort) {
         CRITICAL(MSG_CULL_POINTER_NULLELEMENTFORX_S, lNm2Str(name));
         ocs::TerminationManager::trigger_abort();
      }
      DRETURN(-1);
   }
   pos = lGetPosInDescr(element->descr, name);

   if (do_abort && (pos < 0)) {
      /* someone has called lGetPosViaElem() with invalid name */
      CRITICAL(MSG_CULL_XNOTFOUNDINELEMENT_S, lNm2Str(name));
      ocs::TerminationManager::trigger_abort();
   }

   DRETURN(pos);
}

/**
 * @brief Returns the string representation of a type id
 *
 * @param mt see the read/write variant
 * @return the string, or nullptr when the field is unset
 */
const char *lMt2Str(int mt) {
   if (mt >= 0 && mt < (int) (sizeof(multitypes) / sizeof(char *))) {
      return multitypes[mt];
   } else {
      return "unknown multitype";
   }
}

/**
 * @brief Returns the size of a descriptor
 *
 * Returns the size of a descriptor excluding lEndT Descr.
 *
 * @param dp pointer to descriptor
 *
 * @return size or -1 on error
 *
 * @note MT-NOTE: lCountDescr() is MT safe
 */
int lCountDescr(const lDescr *dp) {
   DENTER(CULL_BASIS_LAYER);

   const lDescr *p;

   if (!dp) {
      LERROR(LEDESCRNULL);
      DRETURN(-1);
   }

   p = &dp[0];
   while (mt_get_type(p->mt) != lEndT)
      p++;

   DRETURN((p - &dp[0]));
}

/**
 * @brief Copys a descriptor
 *
 * Returns a pointer to a copied descriptor, has to be freed by
 * the user.
 *
 * @param dp descriptor
 *
 * @return descriptor pointer or nullptr in case of error
 */
lDescr *lCopyDescr(const lDescr *dp) {
   DENTER(CULL_BASIS_LAYER);

   int i;
   lDescr *new_descr = nullptr;

   if (!dp) {
      LERROR(LEDESCRNULL);
      goto error;
   }

   if ((i = lCountDescr(dp)) == -1) {
      LERROR(LEDESCRNULL);
      goto error;
   }

   if (!(new_descr = (lDescr *) sge_malloc(sizeof(lDescr) * (i + 1)))) {
      LERROR(LEMALLOC);
      goto error;
   }
   memcpy(new_descr, dp, sizeof(lDescr) * (i + 1));

   /* copy hashing information */
   for (i = 0; mt_get_type(dp[i].mt) != lEndT; i++) {
      new_descr[i].ht = nullptr;
   }

   DRETURN(new_descr);

   error:
   DPRINTF("lCopyDescr failed\n");
   DRETURN(nullptr);
}

/**
 * @brief Writes a descriptor (for debugging purpose)
 *
 * Writes a descriptor (for debugging purpose)
 *
 * @param dp descriptor
 * @param fp output stream
 */
void lWriteDescrTo(const lDescr *dp, FILE *fp) {
   DENTER(CULL_LAYER);

   int i;

   if (!dp) {
      LERROR(LEDESCRNULL);
      DRETURN_VOID;
   }

   for (i = 0; mt_get_type(dp[i].mt) != lEndT; i++) {
      const char *format = "nm: %d(%-20.20s) mt: %d %c%c\n";
      int do_hash = ' ';
      int is_hash = ' ';
      if (dp[i].mt & CULL_HASH) {
         if (dp[i].mt & CULL_UNIQUE) {
            do_hash = 'u';
         } else {
            do_hash = 'h';
         }
      }
      if (dp[i].ht != nullptr) {
         is_hash = '+';
      }

      if (!fp) {
         DPRINTF(format, dp[i].nm, lNm2Str(dp[i].nm), dp[i].mt, do_hash, is_hash);
      } else {
         fprintf(fp, format, dp[i].nm, lNm2Str(dp[i].nm), dp[i].mt, do_hash, is_hash);
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Returns position of a name in a descriptor
 *
 * Returns position of a name in a descriptor array. Does a full search
 * in the descriptor even if the element is not a reduced element.
 *
 * @param dp descriptor
 * @param name namse
 *
 * @return position or -1 if not found
 */
int _lGetPosInDescr(const lDescr *dp, int name) {
   const lDescr *ldp;

   if (!dp) {
      LERROR(LEDESCRNULL);
      return -1;
   }

   for (ldp = dp; ldp->nm != name && ldp->nm != NoName; ldp++) { ;
   }

   if (ldp->nm == NoName) {
      LERROR(LENAMENOT);
      return -1;
   }

   return ldp - dp;
}

/**
 * @brief Returns position of a name in a descriptor
 *
 * Returns position of a name in a descriptor array
 *
 * @param dp descriptor
 * @param name namse
 *
 * @return position or -1 if not found
 */
int lGetPosInDescr(const lDescr *dp, int name) {
   const lDescr *ldp;

   if (!dp) {
      LERROR(LEDESCRNULL);
      return -1;
   }

   if ((dp->mt & CULL_IS_REDUCED) == 0) {
      long pos = name - dp->nm;

      if (pos < 0 || pos > MAX_DESCR_SIZE) {
         pos = -1;
      }
      return pos;
   }

   for (ldp = dp; ldp->nm != name && ldp->nm != NoName; ldp++) { ;
   }

   if (ldp->nm == NoName) {
      LERROR(LENAMENOT);
      return -1;
   }

   return ldp - dp;
}

/**
 * @brief Returns type at position
 *
 * Returns the type at specified position in a descriptor array. The
 * Position must be inside the valid range of the descriptor. Returns
 * NoName if descriptor is nullptr or pos < 0.
 *
 * @param dp Descriptor
 * @param pos Position
 *
 * @return Type
 */
int lGetPosType(const lDescr *dp, int pos) {
   if (!dp) {
      LERROR(LEDESCRNULL);
      return (int) NoName;
   }
   if (pos < 0) {
      return (int) NoName;
   }
   return mt_get_type(dp[pos].mt);
}

/**
 * @brief Returns the sub-list held in a field, without copying, read only
 *
 * @param ep the element
 * @param name the field
 * @return the sub-list, or nullptr when the field is unset
 */
lList **lGetListRef(const lListElem *ep, int name) {
   DENTER(CULL_BASIS_LAYER);

   int pos;

   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lListT)
      incompatibleType("lGetPosListRef");

   DRETURN(&(ep->cont[pos].glp));
}

/**
 * @brief Returns the string held at a field position, without copying, read only
 *
 * @param ep the element
 * @param pos the field position
 * @return the string, or nullptr when the field is unset
 */
char **lGetPosStringRef(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (mt_get_type(ep->descr[pos].mt) != lStringT)
      incompatibleType("lGetPosStringRef");

   DRETURN(&(ep->cont[pos].str));
}

/**
 * @brief Returns the host name held at a field position, without copying, read only
 *
 * @param ep the element
 * @param pos the field position
 * @return the string, or nullptr when the field is unset
 */
char **lGetPosHostRef(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (mt_get_type(ep->descr[pos].mt) != lHostT)
      incompatibleType("lGetPosHostRef");

   DRETURN(&(ep->cont[pos].host));
}


/* 
   FOR THE lGet{Type} FUNCTIONS THERE IS NO REAL ERRORHANDLING
   IF EP IS nullptr THERE WILL BE A COREDUMP
   THIS IS NECESSARY, OTHERWISE IT WOULD BE DIFFICULT TO CASCADE
   THE GET FUNCTIONS
   SO HIGHER LEVEL FUNCTIONS OR THE USER SHOULD CHECK IF THE
   ARGUMENTS ARE ALLRIGHT.
 */

/**
 * @brief Returns the int value at position
 *
 * Returns the int value at position 'pos'
 *
 * @param ep element pointer
 * @param pos position id
 *
 * @return int
 */
lInt lGetPosInt(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (mt_get_type(ep->descr[pos].mt) != lIntT)
      incompatibleType("lGetPosInt");

   DRETURN((lInt) ep->cont[pos].i);
}

/**
 * @brief Returns the int value for field name
 *
 * Returns the int value for field name
 *
 * @param ep element
 * @param name field name id
 *
 * @return int
 */
lInt lGetInt(const lListElem *ep, int name) {
   DENTER(CULL_BASIS_LAYER);

   int pos;
   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lIntT)
      incompatibleType2(MSG_CULL_GETINT_WRONGTYPEFORFIELDXY_SS,
                        lNm2Str(name), multitypes[mt_get_type(ep->descr[pos].mt)]);

   DRETURN((lInt) ep->cont[pos].i);
}

/**
 * @brief Returns the ulong value at position pos
 *
 * Returns the ulong value at position pos
 *
 * @param ep element
 * @param pos pos value
 *
 * @return ulong
 */
lUlong lGetPosUlong(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (pos < 0) {
      /* someone has called lGetPosUlong() */
      /* makro with an invalid nm        */
      CRITICAL(SFNMAX, MSG_CULL_GETPOSULONG_GOTINVALIDPOSITION);
      ocs::TerminationManager::trigger_abort();
   }

   if (mt_get_type(ep->descr[pos].mt) != lUlongT)
      incompatibleType("lGetPosUlong");
   DRETURN((lUlong) ep->cont[pos].ul);
}

/**
 * @brief Return 'uint32_t' value for specified fieldname
 *
 * Return the content of the field specified by fieldname 'name' of
 * list element 'ep'. The type of the field 'name' has to be of
 * type 'uint32_t'.
 *
 * @param ep Pointer to list element
 * @param name field name
 *
 * @return uint32_t value
 */
lUlong lGetUlong(const lListElem *ep, int name) {
   DENTER(CULL_BASIS_LAYER);

   int pos;
   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lUlongT)
      incompatibleType2(MSG_CULL_GETULONG_WRONGTYPEFORFIELDXY_SS,
                        lNm2Str(name), multitypes[mt_get_type(ep->descr[pos].mt)]);

   DRETURN((lUlong) ep->cont[pos].ul);
}

/**
 * @brief Returns the ulong64 value at position pos
 *
 * Returns the ulong64 value at position pos
 *
 * @param ep element
 * @param pos pos value
 *
 * @return ulong64
 */
lUlong64 lGetPosUlong64(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (pos < 0) {
      /* someone has called lGetPosUlong64() */
      /* makro with an invalid nm        */
      CRITICAL(SFNMAX, MSG_CULL_GETPOSULONG64_GOTINVALIDPOSITION);
      ocs::TerminationManager::trigger_abort();
   }

   if (mt_get_type(ep->descr[pos].mt) != lUlong64T)
      incompatibleType("lGetPosUlong64");
   DRETURN((lUlong64) ep->cont[pos].ul64);
}

/**
 * @brief Return 'uint64_t' value for specified fieldname
 *
 * Return the content of the field specified by fieldname 'name' of
 * list element 'ep'. The type of the field 'name' has to be of
 * type 'uint64_t'.
 *
 * @param ep Pointer to list element
 * @param name field name
 *
 * @return uint64_t value
 */
lUlong64 lGetUlong64(const lListElem *ep, int name) {
   DENTER(CULL_BASIS_LAYER);

   int pos;
   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lUlong64T)
      incompatibleType2(MSG_CULL_GETULONG64_WRONGTYPEFORFIELDXY_SS,
                        lNm2Str(name), multitypes[mt_get_type(ep->descr[pos].mt)]);

   DRETURN((lUlong64) ep->cont[pos].ul64);
}

/**
 * @brief Returns the string ptr value at position pos
 *
 * @param ep the element
 * @param pos the field position
 * @return the string, or nullptr when the field is unset
 */
const char *lGetPosString(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (pos < 0) {
      /* someone has called lGetString() */
      /* makro with an invalid nm        */
      DPRINTF("!!!!!!!!!!!! lGetPosString() got an invalid pos !!!!!!!!!\n");
      DRETURN(nullptr);
   }

   if (mt_get_type(ep->descr[pos].mt) != lStringT)
      incompatibleType("lGetPosString");

   DRETURN((lString) ep->cont[pos].str);
}

/**
 * @brief Returns the hostname value at position pos
 *
 * @param ep the element
 * @param pos the field position
 * @return the string, or nullptr when the field is unset
 */
const char *lGetPosHost(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (pos < 0) {
      /* someone has called lGetString() */
      /* makro with an invalid nm        */
      DPRINTF("!!!!!!!!!!!!!! lGetPosHost() got an invalid pos !!!!!!!!!\n");
      DRETURN(nullptr);
   }

   if (mt_get_type(ep->descr[pos].mt) != lHostT)
      incompatibleType("lGetPosHost");
   DRETURN((lHost) ep->cont[pos].host);
}

/**
 * @brief Return type of field within descriptor
 *
 * Return type of field within descriptor.
 *
 * @param dp descriptor
 * @param nm field name id
 *
 * @return Type id or lEndT
 */
int lGetType(const lDescr *dp, int nm) {
   DENTER(CULL_BASIS_LAYER);

   int pos;

   pos = lGetPosInDescr(dp, nm);
   if (pos < 0) {
      DRETURN(lEndT);
   }

   DRETURN(mt_get_type(dp[pos].mt));
}

/**
 * @brief Return string for specified fieldname
 *
 * Return the content of the field specified by fieldname 'name' of
 * list element 'ep'. The type of the field 'name' has to be of
 * type string.
 *
 * @param ep Pointer to list element
 * @param name field name
 *
 * @return string pointer (no copy)
 */
const char *lGetString(const lListElem *ep, int name) {
   DENTER(CULL_BASIS_LAYER);

   int pos;
   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lStringT) {
      incompatibleType2(MSG_CULL_GETSTRING_WRONGTYPEFORFILEDXY_SS,
                        lNm2Str(name), multitypes[mt_get_type(ep->descr[pos].mt)]);
   }

   DRETURN((lString) ep->cont[pos].str);
}

/*
* NOTES
*     MT-NOTE: MT-safety depends on lGetString (which has no MT-NOTE)
*              lGetStringNotNull() itself is MT-safe
*/
/**
 * @brief The string held in a field, never nullptr
 *
 * @param ep the element
 * @param name the field
 * @return the string, or the empty string when the field is unset
 */
const char *lGetStringNotNull(const lListElem *ep, int name) {
   const char *ret = lGetString(ep, name);
   if (ret == nullptr) {
      ret = "";
   }
   return ret;
}

/**
 * @brief Return hostname string for specified field
 *
 * This procedure returns the hostname string for the field name,
 * but doesn't copy the string (runtime type checking)
 *
 * @param ep list element pointer
 * @param name name of list element
 *
 * @return value of list entry
 */
const char *lGetHost(const lListElem *ep, int name) {
   DENTER(CULL_BASIS_LAYER);

   int pos;
   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lHostT)
      incompatibleType2(MSG_CULL_GETHOST_WRONGTYPEFORFILEDXY_SS,
                        lNm2Str(name), multitypes[mt_get_type(ep->descr[pos].mt)]);

   DRETURN((lHost) ep->cont[pos].host);
}

/**
 * @brief Returns the CULL object at position pos (no copy)
 *
 * @param ep the element
 * @param pos the field position
 * @return the matching element, or nullptr when there is none
 */
lListElem *lGetPosObject(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (pos < 0) {
      /* someone has called lGetPosUlong() */
      /* makro with an invalid nm        */
      CRITICAL(SFNMAX, MSG_CULL_GETPOSOBJECT_GOTANINVALIDPOS);
      ocs::TerminationManager::trigger_abort();
   }

   if (mt_get_type(ep->descr[pos].mt) != lObjectT)
      incompatibleType("lGetPosObject");

   DRETURN((lListElem *) ep->cont[pos].obj);
}

/**
 * @brief Returns the CULL list at position pos (no copy)
 *
 * @param ep the element
 * @param pos the field position
 * @return the sub-list, or nullptr when the field is unset
 */
lList *lGetPosList(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (pos < 0) {
      /* someone has called lGetPosUlong() */
      /* makro with an invalid nm        */
      CRITICAL(SFNMAX, MSG_CULL_GETPOSLIST_GOTANINVALIDPOS);
      ocs::TerminationManager::trigger_abort();
   }

   if (mt_get_type(ep->descr[pos].mt) != lListT)
      incompatibleType("lGetPosList");

   DRETURN((lList *) ep->cont[pos].glp);
}

/**
 * @brief Returns the CULL object for a field name
 *
 * @param ep the element
 * @param name the field
 * @return the matching element, or nullptr when there is none
 */
lListElem *lGetObject(const lListElem *ep, int name) {
   DENTER(CULL_BASIS_LAYER);

   int pos;
   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lObjectT)
      incompatibleType2(MSG_CULL_GETOBJECT_WRONGTYPEFORFIELDXY_SS,
                        lNm2Str(name), multitypes[mt_get_type(ep->descr[pos].mt)]);
   DRETURN((lListElem *) ep->cont[pos].obj);
}

/**
 * @brief Returns the CULL list for a field name
 *
 * @param ep the element
 * @param name the field
 * @return the sub-list, or nullptr when the field is unset
 */
lList *lGetListRW(const lListElem *ep, int name) {
   DENTER(CULL_BASIS_LAYER);

   int pos;
   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lListT) {
      incompatibleType2(MSG_CULL_GETLIST_WRONGTYPEFORFIELDXY_SS,
                        lNm2Str(name), multitypes[mt_get_type(ep->descr[pos].mt)]);
   }

   DRETURN((lList *) ep->cont[pos].glp);
}

/**
 * @brief Returns the sub-list held in a field, read only
 *
 * @param ep the element
 * @param nm the field
 * @return the sub-list, or nullptr when the field is unset
 */
const lList *lGetList(const lListElem *ep, int nm) {
   return lGetListRW(ep, nm);
}

/**
 * @brief Returns the CULL list for a field name
 *
 * @param ep the element
 * @param name the field
 * @param list_name see the read/write variant
 * @param descr see the read/write variant
 * @return the sub-list, or nullptr when the field is unset
 */
lList *lGetOrCreateList(lListElem *ep, int name,
                        const char *list_name, const lDescr *descr) {
   lList *list = nullptr;

   if (ep != nullptr) {
      list = lGetListRW(ep, name);
      if (list == nullptr) {
         list = lCreateList(list_name, descr);
         lSetList(ep, name, list);
      }
   }

   return list;
}

/**
 * @brief Returns a double value at pos
 *
 * Returns a double value at pos
 *
 * @param ep element
 * @param pos pos
 *
 * @return double value
 */
lDouble lGetPosDouble(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);
   if (mt_get_type(ep->descr[pos].mt) != lDoubleT)
      incompatibleType("lGetPosDouble");
   DRETURN(ep->cont[pos].db);
}

/**
 * @brief Returns the double value for field name
 *
 * Returns the double value for field name
 *
 * @param ep element
 * @param name field name value
 *
 * @return double value
 */
lDouble lGetDouble(const lListElem *ep, int name) {
   DENTER(CULL_BASIS_LAYER);

   int pos;
   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lDoubleT)
      incompatibleType2(MSG_CULL_GETDOUBLE_WRONGTYPEFORFIELDXY_SS, lNm2Str(name),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
   DRETURN(ep->cont[pos].db);
}

/**
 * @brief Returns the long value at position pos
 *
 * Returns the long value at position pos
 *
 * @param ep element
 * @param pos position
 *
 * @return long
 */
lLong lGetPosLong(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);
   if (mt_get_type(ep->descr[pos].mt) != lLongT)
      incompatibleType("lGetPosLong");
   DRETURN(ep->cont[pos].l);
}

/**
 * @brief Returns the long value for a field name
 *
 * Returns the long value for a field name
 *
 * @param ep element
 * @param name name
 *
 * @return long
 */
lLong lGetLong(const lListElem *ep, int name) {
   DENTER(CULL_BASIS_LAYER);

   int pos;
   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lLongT)
      incompatibleType2(MSG_CULL_GETLONG_WRONGTYPEFORFIELDXY_SS, lNm2Str(name),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
   DRETURN(ep->cont[pos].l);
}

/**
 * @brief Returns the boolean value at position pos
 *
 * Returns the boolean value at position pos
 *
 * @param ep element
 * @param pos position
 *
 * @return boolean
 */
lBool lGetPosBool(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (mt_get_type(ep->descr[pos].mt) != lBoolT)
      incompatibleType("lGetPosBool");
   DRETURN(ep->cont[pos].b);
}

/**
 * @brief Returns the boolean value for a field name
 *
 * Returns the boolean value for a field name
 *
 * @param ep element
 * @param name field name
 *
 * @return boolean
 */
lBool lGetBool(const lListElem *ep, int name) {
   DENTER(CULL_BASIS_LAYER);

   int pos;
   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lBoolT)
      incompatibleType2(MSG_CULL_GETBOOL_WRONGTYPEFORFIELDXY_SS, lNm2Str(name),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
   DRETURN(ep->cont[pos].b);
}

/**
 * @brief Returns the reference at position pos
 *
 * Returns the reference at position pos
 *
 * @param ep element
 * @param pos position
 *
 * @return reference (pointer)
 */
lRef lGetPosRef(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);
   if (mt_get_type(ep->descr[pos].mt) != lRefT) {
      incompatibleType("lGetPosRef");
   }
   DRETURN(ep->cont[pos].ref);
}

/**
 * @brief Returns the reference for a field name
 *
 * Returns the reference for a field name
 *
 * @param ep element
 * @param name field name value
 *
 * @return reference
 */
lRef lGetRef(const lListElem *ep, int name) {
   DENTER(CULL_BASIS_LAYER);

   int pos;
   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lRefT)
      incompatibleType2(MSG_CULL_GETREF_WRONGTYPEFORFIELDXY_SS, lNm2Str(name),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
   DRETURN(ep->cont[pos].ref);
}

/**
 * @brief Sets the int value
 *
 * Sets in the element 'ep' at position 'pos' the int 'value'
 *
 * @param ep element
 * @param pos position
 * @param value value
 *
 * @return error state 0 - OK -1 - Error
 */
int lSetPosInt(lListElem *ep, int pos, int value) {
   DENTER(CULL_BASIS_LAYER);

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   if (pos < 0) {
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lIntT) {
      incompatibleType("lSetPosInt");
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, false, lGetPosName(ep->descr, pos));
#endif

   if (ep->cont[pos].i != value) {
      ep->cont[pos].i = value;
   }

   DRETURN(0);
}

/**
 * @brief Sets an int within an element
 *
 * Sets an int within an element
 *
 * @param ep element
 * @param name field name id
 * @param value new value
 *
 * @return error state 0 - OK -1 - Error
 */
int lSetInt(lListElem *ep, int name, int value) {
   DENTER(CULL_BASIS_LAYER);

   int pos;
   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   pos = lGetPosViaElem(ep, name, SGE_NO_ABORT);
   if (pos < 0) {
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lIntT) {
      incompatibleType2(MSG_CULL_SETINT_WRONGTYPEFORFIELDXY_SS, lNm2Str(name),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, false, name);
#endif

   if (value != ep->cont[pos].i) {
      ep->cont[pos].i = value;
   }

   DRETURN(0);
}

/**
 * @brief Get ulong at a certain position
 *
 * Get ulong at a certain position
 *
 * @param ep element
 * @param pos position
 * @param value new value
 *
 * @return error state 0 - OK -1 - Error
 */
int lSetPosUlong(lListElem *ep, int pos, lUlong value) {
   DENTER(CULL_BASIS_LAYER);
   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   if (pos < 0) {
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lUlongT) {
      incompatibleType("lSetPosUlong");
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, ep->descr[pos].ht != nullptr, lGetPosName(ep->descr, pos));
#endif

   if (value != ep->cont[pos].ul) {
      /* remove old hash entry */
      if (ep->descr[pos].ht != nullptr) {
         cull_hash_remove(ep, pos);
      }

      ep->cont[pos].ul = value;

      /* create entry in hash table */
      if (ep->descr[pos].ht != nullptr) {
         cull_hash_insert(ep, (void *) &(ep->cont[pos].ul), ep->descr[pos].ht,
                          mt_is_unique(ep->descr[pos].mt));
      }
   }

   DRETURN(0);
}

/**
 * @brief Set ulong value at the given field name id
 *
 * Set ulong value at the given field name id
 *
 * @param ep element
 * @param name field name id
 * @param value new value
 *
 * @return error state 0 - OK -1 - Error
 */
int lSetUlong(lListElem *ep, int name, lUlong value) {
   DENTER(CULL_BASIS_LAYER);

   int pos;

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   pos = lGetPosViaElem(ep, name, SGE_NO_ABORT);
   if (pos < 0) {
      DPRINTF(("!!!!!!!!!! lSetUlong(): %s not found in element !!!!!!!!!!\n",
              lNm2Str(name)));
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lUlongT) {
      incompatibleType2(MSG_CULL_SETULONG_WRONGTYPEFORFIELDXY_SS, lNm2Str(name),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, ep->descr[pos].ht != nullptr, name);
#endif

   if (value != ep->cont[pos].ul) {
      /* remove old hash entry */
      if (ep->descr[pos].ht != nullptr) {
         cull_hash_remove(ep, pos);
      }

      ep->cont[pos].ul = value;

      /* create entry in hash table */
      if (ep->descr[pos].ht != nullptr) {
         cull_hash_insert(ep, (void *) &(ep->cont[pos].ul), ep->descr[pos].ht,
                          mt_is_unique(ep->descr[pos].mt));
      }
   }

   DRETURN(0);
}

/**
 * @brief Adds a lUlong offset to the lUlong field
 *
 * The 'offset' is added to the lUlong field 'name' of
 * the CULL element 'ep'.
 *
 * @code
 * int - error state
 *     0 - OK
 *    -1 - Error
 * @endcode
 *
 * @param ep element
 * @param name field name id
 * @param offset the offset
 *
 * @return int -
 */
int lAddUlong(lListElem *ep, int name, lUlong offset) {
   DENTER(CULL_BASIS_LAYER);

   int pos;

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   pos = lGetPosViaElem(ep, name, SGE_NO_ABORT);
   if (pos < 0) {
      DPRINTF(("!!!!!!!!!! lSetUlong(): %s not found in element !!!!!!!!!!\n",
              lNm2Str(name)));
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lUlongT) {
      incompatibleType2(MSG_CULL_SETULONG_WRONGTYPEFORFIELDXY_SS, lNm2Str(name),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, ep->descr[pos].ht != nullptr, name);
#endif

   if (offset != 0) {
      /* remove old hash entry */
      if (ep->descr[pos].ht != nullptr) {
         cull_hash_remove(ep, pos);
      }

      ep->cont[pos].ul += offset;

      /* create entry in hash table */
      if (ep->descr[pos].ht != nullptr) {
         cull_hash_insert(ep, (void *) &(ep->cont[pos].ul), ep->descr[pos].ht,
                          mt_is_unique(ep->descr[pos].mt));
      }
   }

   DRETURN(0);
}

/**
 * @brief Set ulong value at the given field name id
 *
 * Set ulong64 value at the given field name id
 *
 * @param ep element
 * @param name field name id
 * @param value new value
 *
 * @return error state 0 - OK -1 - Error
 */
int lSetUlong64(lListElem *ep, int name, lUlong64 value) {
   DENTER(CULL_BASIS_LAYER);

   int pos;

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   pos = lGetPosViaElem(ep, name, SGE_NO_ABORT);
   if (pos < 0) {
      DPRINTF(("!!!!!!!!!! lSetUlong64(): %s not found in element !!!!!!!!!!\n",
              lNm2Str(name)));
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lUlong64T) {
      incompatibleType2(MSG_CULL_SETULONG64_WRONGTYPEFORFIELDXY_SS, lNm2Str(name),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, ep->descr[pos].ht != nullptr, name);
#endif

   if (value != ep->cont[pos].ul64) {
      /* remove old hash entry */
      if (ep->descr[pos].ht != nullptr) {
         cull_hash_remove(ep, pos);
      }

      ep->cont[pos].ul64 = value;

      /* create entry in hash table */
      if (ep->descr[pos].ht != nullptr) {
         cull_hash_insert(ep, (void *) &(ep->cont[pos].ul64), ep->descr[pos].ht,
                          mt_is_unique(ep->descr[pos].mt));
      }
   }

   DRETURN(0);
}

/**
 * @brief Adds a lUlong64 offset to the lUlong64 field
 *
 * The 'offset' is added to the lUlong64 field 'name' of
 * the CULL element 'ep'.
 *
 * @code
 * int - error state
 *     0 - OK
 *    -1 - Error
 * @endcode
 *
 * @param ep element
 * @param name field name id
 * @param offset the offset
 *
 * @return int -
 */
int lAddUlong64(lListElem *ep, int name, lUlong64 offset) {
   DENTER(CULL_BASIS_LAYER);

   int pos;

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   pos = lGetPosViaElem(ep, name, SGE_NO_ABORT);
   if (pos < 0) {
      DPRINTF(("!!!!!!!!!! lSetUlong64(): %s not found in element !!!!!!!!!!\n",
              lNm2Str(name)));
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lUlong64T) {
      incompatibleType2(MSG_CULL_SETULONG64_WRONGTYPEFORFIELDXY_SS, lNm2Str(name),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, ep->descr[pos].ht != nullptr, name);
#endif

   if (offset != 0) {
      /* remove old hash entry */
      if (ep->descr[pos].ht != nullptr) {
         cull_hash_remove(ep, pos);
      }

      ep->cont[pos].ul64 += offset;

      /* create entry in hash table */
      if (ep->descr[pos].ht != nullptr) {
         cull_hash_insert(ep, (void *) &(ep->cont[pos].ul64), ep->descr[pos].ht,
                          mt_is_unique(ep->descr[pos].mt));
      }
   }

   DRETURN(0);
}

/**
 * @brief Get ulong64 at a certain position
 *
 * Get ulong64 at a certain position
 *
 * @param ep element
 * @param pos position
 * @param value new value
 *
 * @return error state 0 - OK -1 - Error
 */
int lSetPosUlong64(lListElem *ep, int pos, lUlong64 value) {
   DENTER(CULL_BASIS_LAYER);
   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   if (pos < 0) {
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lUlong64T) {
      incompatibleType("lSetPosUlong64");
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, ep->descr[pos].ht != nullptr, lGetPosName(ep->descr, pos));
#endif

   if (value != ep->cont[pos].ul64) {
      /* remove old hash entry */
      if (ep->descr[pos].ht != nullptr) {
         cull_hash_remove(ep, pos);
      }

      ep->cont[pos].ul64 = value;

      /* create entry in hash table */
      if (ep->descr[pos].ht != nullptr) {
         cull_hash_insert(ep, (void *) &(ep->cont[pos].ul64), ep->descr[pos].ht,
                          mt_is_unique(ep->descr[pos].mt));
      }
   }

   DRETURN(0);
}

/**
 * @brief Sets the string at a certain position
 *
 * Sets the string at a certain position.
 *
 * @param ep element
 * @param pos position
 * @param value string value
 *
 * @return error state 0 - OK -1 - Error
 */
int lSetPosString(lListElem *ep, int pos, const char *value) {
   DENTER(CULL_BASIS_LAYER);

   char *str = nullptr;
   int changed;

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   if (pos < 0) {
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lStringT) {
      incompatibleType("lSetPosString");
      DRETURN(-1);
   }

   /* has the string value changed?
   ** if both new and old are nullptr, nothing changed,
   ** if one of them is nullptr, it changed,
   ** else do a string compare
   */
   str = ep->cont[pos].str;
   if (value == nullptr && str == nullptr) {
      changed = 0;
   } else {
      if (value == nullptr || str == nullptr) {
         changed = 1;
      } else {
         changed = strcmp(value, str);
      }
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, ep->descr[pos].ht != nullptr, lGetPosName(ep->descr, pos));
#endif

   if (changed) {
      /* remove old hash entry */
      if (ep->descr[pos].ht != nullptr) {
         cull_hash_remove(ep, pos);
      }

      /* strdup new string value */
      if (value) {
         if (!(str = strdup(value))) {
            LERROR(LESTRDUP);
            DRETURN(-1);
         }
      }                            /* these brackets are required */
      else
         str = nullptr;               /* value is nullptr */

      /* free old string value */
      sge_free(&(ep->cont[pos].str));
      ep->cont[pos].str = str;

      /* create entry in hash table */
      if (ep->descr[pos].ht != nullptr) {
         cull_hash_insert(ep, ep->cont[pos].str, ep->descr[pos].ht,
                          mt_is_unique(ep->descr[pos].mt));
      }
   }

   DRETURN(0);
}

/**
 * @brief Sets the hostname at a certain position
 *
 * Sets the hostname at a certain position
 *
 * @param ep element
 * @param pos position
 * @param value new hostname
 *
 * @return error state 0 - OK -1 - Error
 */
int lSetPosHost(lListElem *ep, int pos, const char *value) {
   DENTER(CULL_BASIS_LAYER);

   char *str = nullptr;
   int changed;

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   if (pos < 0) {
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lHostT) {
      incompatibleType("lSetPosHost");
      DRETURN(-1);
   }

   /* has the host value changed?
   ** if both new and old are nullptr, nothing changed,
   ** if one of them is nullptr, it changed,
   ** else do a string compare (a hostcmp would be more accurate,
   ** but most probably not neccessary and too expensive
   */
   str = ep->cont[pos].host;
   if (value == nullptr && str == nullptr) {
      changed = 0;
   } else {
      if (value == nullptr || str == nullptr) {
         changed = 1;
      } else {
         changed = strcmp(value, str);
      }
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, ep->descr[pos].ht != nullptr, lGetPosName(ep->descr, pos));
#endif

   if (changed) {
      /* remove old hash entry */
      if (ep->descr[pos].ht != nullptr) {
         cull_hash_remove(ep, pos);
      }

      /* strdup new string value */
      if (value) {
         if (!(str = strdup(value))) {
            LERROR(LESTRDUP);
            DRETURN(-1);
         }
      }                            /* these brackets are required */
      else
         str = nullptr;               /* value is nullptr */

      /* free old string value */
      sge_free(&(ep->cont[pos].host));
      ep->cont[pos].host = str;

      /* create entry in hash table */
      if (ep->descr[pos].ht != nullptr) {
         char host_key[CL_MAXHOSTNAMELEN + 1];
         cull_hash_insert(ep, cull_hash_key(ep, pos, host_key),
                          ep->descr[pos].ht, mt_is_unique(ep->descr[pos].mt));
      }
   }

   DRETURN(0);
}

/**
 * @brief Sets the string at the given field name id
 *
 * Sets the string at the given field name id
 *
 * @param ep element
 * @param name field name id
 * @param value new string
 *
 * @return error state 0 - OK -1 - Error
 */
int lSetString(lListElem *ep, int name, const char *value) {
   DENTER(CULL_BASIS_LAYER);

   char *str;
   int pos;
   int changed;

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   pos = lGetPosViaElem(ep, name, SGE_NO_ABORT);
   if (pos < 0) {
      incompatibleType2(MSG_CULL_SETSTRING_NOSUCHNAMEXYINDESCRIPTOR_IS,
                        name, lNm2Str(name));
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lStringT) {
      incompatibleType2(MSG_CULL_SETSTRING_WRONGTYPEFORFIELDXY_SS, lNm2Str(name),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
      DRETURN(-1);
   }

   /* has the string value changed?
   ** if both new and old are nullptr, nothing changed,
   ** if one of them is nullptr, it changed,
   ** else do a string compare
   */
   str = ep->cont[pos].str;
   if (value == nullptr && str == nullptr) {
      changed = 0;
   } else {
      if (value == nullptr || str == nullptr) {
         changed = 1;
      } else {
         changed = strcmp(value, str);
      }
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, ep->descr[pos].ht != nullptr, name);
#endif

   if (changed) {
      /* remove old hash entry */
      if (ep->descr[pos].ht != nullptr) {
         cull_hash_remove(ep, pos);
      }

      /* strdup new string value */
      /* do so before freeing the old one - they could point to the same object! */
      if (value) {
         if (!(str = strdup(value))) {
            LERROR(LESTRDUP);
            DRETURN(-1);
         }
      } else {
         str = nullptr;               /* value is nullptr */
      }

      /* free old string value */
      sge_free(&(ep->cont[pos].str));
      ep->cont[pos].str = str;

      /* create entry in hash table */
      if (ep->descr[pos].ht != nullptr) {
         cull_hash_insert(ep, ep->cont[pos].str, ep->descr[pos].ht,
                          mt_is_unique(ep->descr[pos].mt));
      }
   }

   DRETURN(0);
}

/**
 * @brief Set hostname for field name in element
 *
 * Sets in the element ep for field name the char * value.
 * Also duplicates the pointed to char array
 * (runtime type checking)
 *
 * @param ep list element pointer
 * @param name name of list element (e.g. EH_name)
 * @param value new value for list element
 *
 * @return error state -1 - Error 0 - OK
 */
int lSetHost(lListElem *ep, int name, const char *value) {
   DENTER(CULL_BASIS_LAYER);

   char *str;
   int pos;
   int changed;

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   pos = lGetPosViaElem(ep, name, SGE_NO_ABORT);
   if (pos < 0) {
      incompatibleType2(MSG_CULL_SETHOST_NOSUCHNAMEXYINDESCRIPTOR_IS,
                        name, lNm2Str(name));
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lHostT) {
      incompatibleType2(MSG_CULL_SETHOST_WRONGTYPEFORFIELDXY_SS,
                        lNm2Str(name), multitypes[mt_get_type(ep->descr[pos].mt)]);
      DRETURN(-1);
   }

   /* has the host value changed?
   ** if both new and old are nullptr, nothing changed,
   ** if one of them is nullptr, it changed,
   ** else do a string compare (a hostcmp would be more accurate,
   ** but most probably not neccessary and too expensive
   */
   str = ep->cont[pos].host;
   if (value == nullptr && str == nullptr) {
      changed = 0;
   } else {
      if (value == nullptr || str == nullptr) {
         changed = 1;
      } else {
         changed = strcmp(value, str);
      }
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, ep->descr[pos].ht != nullptr, name);
#endif

   if (changed) {
      /* remove old hash entry */
      if (ep->descr[pos].ht != nullptr) {
         cull_hash_remove(ep, pos);
      }
      /* strdup new string value */
      /* do so before freeing the old one - they could point to the same object! */
      if (value) {
         if (!(str = strdup(value))) {
            LERROR(LESTRDUP);
            DRETURN(-1);
         }
      } else {
         str = nullptr;               /* value is nullptr */
      }
      sge_free(&(ep->cont[pos].host));
      ep->cont[pos].host = str;

      /* create entry in hash table */
      if (ep->descr[pos].ht != nullptr) {
         char host_key[CL_MAXHOSTNAMELEN + 1];
         cull_hash_insert(ep, cull_hash_key(ep, pos, host_key),
                          ep->descr[pos].ht, mt_is_unique(ep->descr[pos].mt));
      }
   }
   DRETURN(0);
}

/**
 * @brief Set list element at position pos
 *
 * Sets in the element 'ep' at position 'pos' the list element 'value'.
 * Doesn't copy the object. Does runtime type checking.
 *
 * @param ep element
 * @param pos position
 * @param value value
 *
 * @return error state 0 - OK -1 - Error
 */
int lSetPosObject(lListElem *ep, int pos, lListElem *value) {
   DENTER(CULL_BASIS_LAYER);

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   if (pos < 0) {
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lObjectT) {
      incompatibleType("lSetPosObject");
      DRETURN(-1);
   }

   if (value != nullptr && value->status != FREE_ELEM && value->status != TRANS_BOUND_ELEM) {
      LERROR(LEBOUNDELEM);
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeOwner(ep->cont[pos].obj, nullptr, ep, lGetPosName(ep->descr, pos));
   lObserveChangeOwner(value, ep, nullptr, lGetPosName(ep->descr, pos));
#endif

   if (value != ep->cont[pos].obj) {
      /* free old element */
      if (ep->cont[pos].obj != nullptr) {
         lFreeElem(&(ep->cont[pos].obj));
      }

      /* set new list */
      ep->cont[pos].obj = value;

      /* mark lListElem as bound */
      value->status = OBJECT_ELEM;
   }

   DRETURN(0);
}

/**
 * @brief Set list at position pos
 *
 * Sets in the element 'ep' at position 'pos' the lists 'value'.
 * Doesn't copy the list. Does runtime type checking.
 *
 * @param ep element
 * @param pos position
 * @param value value
 *
 * @return error state 0 - OK -1 - Error
 */
int lSetPosList(lListElem *ep, int pos, lList *value) {
   DENTER(CULL_BASIS_LAYER);

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }
   if (pos < 0) {
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lListT) {
      incompatibleType("lSetPosList");
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeOwner(ep->cont[pos].glp, nullptr, ep, lGetPosName(ep->descr, pos));
   lObserveChangeOwner(value, ep, nullptr, lGetPosName(ep->descr, pos));
#endif

   if (value != ep->cont[pos].glp) {
      /* free old list */
      if (ep->cont[pos].glp) {
         lFreeList(&(ep->cont[pos].glp));
      }

      /* set new list */
      ep->cont[pos].glp = value;
   }

   DRETURN(0);
}

/**
 * @brief Exchange field name value string pointer
 *
 * Exchange the string pointer, which has the given field name value.
 *
 * @param ep element
 * @param name field name value
 * @param str pointer to a string
 *
 * @return error state 0 - OK -1 - Error
 */
int lXchgString(lListElem *ep, int name, char **str) {
   DENTER(CULL_BASIS_LAYER);

   int pos;
   char *tmp;

   if (ep == nullptr || str == nullptr) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }
   pos = lGetPosViaElem(ep, name, SGE_NO_ABORT);
   if (pos < 0) {
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lStringT) {
      incompatibleType2(MSG_CULL_XCHGLIST_WRONGTYPEFORFIELDXY_SS,
                        lNm2Str(name), multitypes[mt_get_type(ep->descr[pos].mt)]);
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, false, name);
#endif

   if (*str != ep->cont[pos].str) {
      tmp = ep->cont[pos].str;
      ep->cont[pos].str = *str;
      *str = tmp;
   }

   DRETURN(0);

}

/**
 * @brief Exchange field name value list pointer
 *
 * Exchange the list pointer which has the given field name value.
 *
 * @param ep element
 * @param name field name value
 * @param lpp pointer to CULL list
 *
 * @return error state 0 - OK -1 - Error
 */
int lXchgList(lListElem *ep, int name, lList **lpp) {
   DENTER(CULL_BASIS_LAYER);

   int pos;
   lList *tmp;

   if (ep == nullptr || lpp == nullptr) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }
   pos = lGetPosViaElem(ep, name, SGE_NO_ABORT);
   if (pos < 0) {
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lListT) {
      incompatibleType2(MSG_CULL_XCHGLIST_WRONGTYPEFORFIELDXY_SS,
                        lNm2Str(name), multitypes[mt_get_type(ep->descr[pos].mt)]);
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveSwitchOwner(ep->cont[pos].glp, *lpp, ep, nullptr, name);
#endif

   if (*lpp != ep->cont[pos].glp) {
      tmp = ep->cont[pos].glp;
      ep->cont[pos].glp = *lpp;
      *lpp = tmp;
   }

   DRETURN(0);

}

/**
 * @brief Exchange two lists within two elements
 *
 * Exchange two lists within two elements.
 *
 * @param to element one
 * @param nm_to field name id of a list attribute of 'to'
 * @param from element two
 * @param nm_from field name id of a list attribute of 'from'
 *
 * @return error state 0 - OK -1 - Error
 */
int lSwapList(lListElem *to, int nm_to, lListElem *from, int nm_from) {
   DENTER(CULL_BASIS_LAYER);

   lList *tmp = nullptr;

   if (lXchgList(from, nm_from, &tmp) == -1) {
      DRETURN(-1);
   }
   if (lXchgList(to, nm_to, &tmp) == -1) {
      DRETURN(-1);
   }
   if (lXchgList(from, nm_from, &tmp) == -1) {
      DRETURN(-1);
   }


   DRETURN(0);
}

/**
 * @brief Sets a list at the given field name id
 *
 * Sets a list at the given field name id. List will not be copyed.
 *
 * @param ep element
 * @param name field name id
 * @param value new list pointer
 *
 * @return error state 0 - OK -1 - Error
 */
int lSetObject(lListElem *ep, int name, lListElem *value) {
   DENTER(CULL_BASIS_LAYER);

   int pos;

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   pos = lGetPosViaElem(ep, name, SGE_NO_ABORT);
   if (pos < 0) {
      DPRINTF(("!!!!!!!!!! lSetObject(): %s not found in element !!!!!!!!!!\n",
              lNm2Str(name)));
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lObjectT) {
      incompatibleType2(MSG_CULL_SETLIST_WRONGTYPEFORFIELDXY_SS, lNm2Str(name),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
      DRETURN(-1);
   }

   if (value != nullptr && value->status != FREE_ELEM && value->status != TRANS_BOUND_ELEM) {
      LERROR(LEBOUNDELEM);
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeOwner(ep->cont[pos].obj, nullptr, ep, name);
   lObserveChangeOwner(value, ep, nullptr, name);
#endif

   if (value != ep->cont[pos].obj) {
      /* free old element */
      if (ep->cont[pos].obj) {
         lFreeElem(&(ep->cont[pos].obj));
      }

      /* set new list */
      ep->cont[pos].obj = value;

      /* mark lListElem as bound */
      if (value != nullptr) {
         value->status = OBJECT_ELEM;
      }
   }

   DRETURN(0);
}

/**
 * @brief Sets a list at the given field name id
 *
 * Sets a list at the given field name id. List will not be copied.
 *
 * @param ep element
 * @param name field name id
 * @param value new list pointer
 *
 * @return error state 0 - OK -1 - Error
 *
 * @note MT-NOTE: lAddSubList() is MT safe
 */
int lSetList(lListElem *ep, int name, lList *value) {
   DENTER(CULL_BASIS_LAYER);

   int pos;

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }
   pos = lGetPosViaElem(ep, name, SGE_NO_ABORT);
   if (pos < 0) {
      DPRINTF(("!!!!!!!!!! lSetList(): %s not found in element !!!!!!!!!!\n",
              lNm2Str(name)));
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lListT) {
      incompatibleType2(MSG_CULL_SETLIST_WRONGTYPEFORFIELDXY_SS, lNm2Str(name),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeOwner(ep->cont[pos].glp, ep, nullptr, name);
   lObserveChangeOwner(value, ep, nullptr, name);
#endif

   if (value != ep->cont[pos].glp) {
      /* free old list */
      lFreeList(&(ep->cont[pos].glp));

      /* set new list */
      ep->cont[pos].glp = value;
   }

   DRETURN(0);
}

/**
 * @brief Set double value at given position
 *
 * Set double value at given position.
 *
 * @param ep element
 * @param pos position
 * @param value new double value
 *
 * @return error state 0 - OK -1 - Error
 */
int lSetPosDouble(lListElem *ep, int pos, lDouble value) {
   DENTER(CULL_BASIS_LAYER);
   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   if (pos < 0) {
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lDoubleT) {
      incompatibleType("lSetPosDouble");
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, false, lGetPosName(ep->descr, pos));
#endif

   if (value != ep->cont[pos].db) {
      ep->cont[pos].db = value;
   }

   DRETURN(0);
}


/**
 * @brief Set double value with given field name id
 *
 * Set double value with given field name id
 *
 * @param ep element
 * @param name field name id
 * @param value new double value
 *
 * @return error state 0 - OK -1 - Error
 */
int lSetDouble(lListElem *ep, int name, lDouble value) {
   DENTER(CULL_BASIS_LAYER);

   int pos;

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   pos = lGetPosViaElem(ep, name, SGE_NO_ABORT);
   if (pos < 0) {
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lDoubleT) {
      incompatibleType2(MSG_CULL_SETDOUBLE_WRONGTYPEFORFIELDXY_SS, lNm2Str(name),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, false, name);
#endif

   if (value != ep->cont[pos].db) {
      ep->cont[pos].db = value;
   }

   DRETURN(0);
}

/**
 * @brief Adds an offset to a double field
 *
 * @p value is added to the double field @p name of the element @p ep.
 *
 * @param ep element
 * @param name field name id
 * @param value the amount to add
 *
 * @return 0 on success, -1 on error
 */
int lAddDouble(lListElem *ep, int name, lDouble value) {
   DENTER(CULL_BASIS_LAYER);

   int pos;

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   pos = lGetPosViaElem(ep, name, SGE_NO_ABORT);
   if (pos < 0) {
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lDoubleT) {
      incompatibleType2(MSG_CULL_SETDOUBLE_WRONGTYPEFORFIELDXY_SS, lNm2Str(name),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, false, name);
#endif

   if (value != 0.0) {
      ep->cont[pos].db += value;
   }

   DRETURN(0);
}


/**
 * @brief Set long value at given position
 *
 * Set long value at given position.
 *
 * @param ep element
 * @param pos position
 * @param value new long value
 *
 * @return error state 0 - OK -1 - Error
 */
int lSetPosLong(lListElem *ep, int pos, lLong value) {
   DENTER(CULL_BASIS_LAYER);
   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   if (pos < 0) {
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lLongT) {
      incompatibleType("lSetPosLong");
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, false, lGetPosName(ep->descr, pos));
#endif

   if (value != ep->cont[pos].l) {
      ep->cont[pos].l = value;
   }

   DRETURN(0);
}

/**
 * @brief Set long value with given field name id
 *
 * Set long value with given field name id.
 *
 * @param ep element
 * @param name field name id
 * @param value value
 *
 * @return error state 0 - OK -1 - Error
 */
int lSetLong(lListElem *ep, int name, lLong value) {
   DENTER(CULL_BASIS_LAYER);

   int pos;

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   pos = lGetPosViaElem(ep, name, SGE_NO_ABORT);
   if (pos < 0) {
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lLongT) {
      incompatibleType2(MSG_CULL_SETLONG_WRONGTYPEFORFIELDXY_SS, lNm2Str(name),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, false, name);
#endif

   if (value != ep->cont[pos].l) {
      ep->cont[pos].l = value;
   }

   DRETURN(0);
}

/**
 * @brief Sets the character a the given position
 *
 * Sets the character a the given position.
 *
 * @param ep element
 * @param pos position
 * @param value value
 *
 * @return error state 0 - OK -1 - Error
 */
int lSetPosBool(lListElem *ep, int pos, lBool value) {
   DENTER(CULL_BASIS_LAYER);
   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   if (pos < 0) {
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lBoolT) {
      incompatibleType("lSetPosBool");
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, false, lGetPosName(ep->descr, pos));
#endif

   if (value != ep->cont[pos].b) {
      ep->cont[pos].b = value;
   }

   DRETURN(0);
}

/**
 * @brief Sets character with the given field name id
 *
 * Sets character with the given field name id
 *
 * @param ep element
 * @param name field name id
 * @param value new character
 *
 * @return error state 0 - OK -1 - Error
 */
int lSetBool(lListElem *ep, int name, lBool value) {
   DENTER(CULL_BASIS_LAYER);

   int pos;

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   pos = lGetPosViaElem(ep, name, SGE_NO_ABORT);
   if (pos < 0) {
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lBoolT) {
      incompatibleType2(MSG_CULL_SETBOOL_WRONGTYPEFORFIELDXY_SS, lNm2Str(name),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, false, name);
#endif

   if (value != ep->cont[pos].b) {
      ep->cont[pos].b = value;
   }

   DRETURN(0);
}

/**
 * @brief Set pointer at given position
 *
 * Set pointer at given position
 *
 * @param ep element
 * @param pos position
 * @param value pointer
 *
 * @return error state 0 - OK -1 - Error
 */
int lSetPosRef(lListElem *ep, int pos, lRef value) {
   DENTER(CULL_BASIS_LAYER);
   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   if (pos < 0) {
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lRefT) {
      incompatibleType("lSetPosRef");
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, false, lGetPosName(ep->descr, pos));
#endif

   if (value != ep->cont[pos].ref) {
      ep->cont[pos].ref = value;
   }

   DRETURN(0);
}

/**
 * @brief Set pointer with the given field name id
 *
 * Set pointer with the given field name id
 *
 * @param ep element
 * @param name field name id
 * @param value new pointer
 *
 * @return error state 0 - OK -1 - Error
 */
int lSetRef(lListElem *ep, int name, lRef value) {
   DENTER(CULL_BASIS_LAYER);

   int pos;

   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   pos = lGetPosViaElem(ep, name, SGE_NO_ABORT);
   if (pos < 0) {
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lRefT) {
      incompatibleType2(MSG_CULL_SETREF_WRONGTYPEFORFIELDXY_SS, lNm2Str(name),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, false, name);
#endif

   if (value != ep->cont[pos].ref) {
      ep->cont[pos].ref = value;
   }

   DRETURN(0);
}

/**
 * @brief Compare two ints
 *
 * @param i0 first value
 * @param i1 second value
 * @return 0 when equal, -1 when @p i0 is the smaller, 1 when it is the larger
 */
int intcmp(int i0, int i1) {
   return i0 == i1 ? 0 : (i0 < i1 ? -1 : 1);
}

/**
 * @brief Compare two 32 bit unsigned values
 *
 * @param u0 first value
 * @param u1 second value
 * @return 0 when equal, -1 when @p u0 is the smaller, 1 when it is the larger
 */
int ulongcmp(lUlong u0, lUlong u1) {
   return u0 == u1 ? 0 : (u0 < u1 ? -1 : 1);
}

/**
 * @brief Does a bit mask contain every bit of another?
 *
 * @param bm0 the mask to test
 * @param bm1 the bits required to be set in @p bm0
 * @return 1 when every bit of @p bm1 is set in @p bm0, 0 otherwise
 *
 * @note Despite its name and its neighbours, this is a containment test, not
 *       an ordering: it never returns -1 and its result is not strcmp-like.
 */
int bitmaskcmp(lUlong bm0, lUlong bm1) {
   return ((bm0 & bm1) == bm1) ? 1 : 0;
}

/**
 * @brief Compare two 64 bit unsigned values
 *
 * @param u0 first value
 * @param u1 second value
 * @return 0 when equal, -1 when @p u0 is the smaller, 1 when it is the larger
 */
int ulong64cmp(lUlong64 u0, lUlong64 u1) {
   return u0 == u1 ? 0 : (u0 < u1 ? -1 : 1);
}

/**
 * @brief Compare two floats
 *
 * @param f0 first value
 * @param f1 second value
 * @return 0 when equal, -1 when @p f0 is the smaller, 1 when it is the larger
 */
int floatcmp(lFloat f0, lFloat f1) {
   return f0 == f1 ? 0 : (f0 < f1 ? -1 : 1);
}

/**
 * @brief Compare two doubles
 *
 * @param d0 first value
 * @param d1 second value
 * @return 0 when equal, -1 when @p d0 is the smaller, 1 when it is the larger
 */
int doublecmp(lDouble d0, lDouble d1) {
   return d0 == d1 ? 0 : (d0 < d1 ? -1 : 1);
}

/**
 * @brief Compare two longs
 *
 * @param l0 first value
 * @param l1 second value
 * @return 0 when equal, -1 when @p l0 is the smaller, 1 when it is the larger
 */
int longcmp(lLong l0, lLong l1) {
   return l0 == l1 ? 0 : (l0 < l1 ? -1 : 1);
}

/**
 * @brief Compare two booleans
 *
 * @param b0 first value
 * @param b1 second value
 * @return 0 when equal, -1 when @p b0 is the smaller, 1 when it is the larger
 */
int boolcmp(lBool b0, lBool b1) {
   return b0 == b1 ? 0 : (b0 < b1 ? -1 : 1);
}

/**
 * @brief Compare two characters
 *
 * @param c0 first value
 * @param c1 second value
 * @return 0 when equal, -1 when @p c0 is the smaller, 1 when it is the larger
 */
int charcmp(lChar c0, lChar c1) {
   return c0 == c1 ? 0 : (c0 < c1 ? -1 : 1);
}

/**
 * @brief Compare two opaque references, by address
 *
 * @param c0 first value
 * @param c1 second value
 * @return 0 when equal, -1 when @p c0 is the smaller, 1 when it is the larger
 */
int refcmp(lRef c0, lRef c1) {
   return c0 == c1 ? 0 : (c0 < c1 ? -1 : 1);
}

/**
 * @brief Adds a string to the string sublist
 *
 * This function add a new element into a sublist snm of an
 * element ep. The field nm of this added element will get the
 * initial value specified with str.
 *
 * @param ep list element
 * @param nm field id contained in the element which will be created
 * @param str initial value if nm
 * @param snm field id of the sublist within ep
 * @param dp Type of the new element
 *
 * @return nullptr in case of error otherwise pointer to the added element
 */
lListElem *lAddSubStr(lListElem *ep, int nm, const char *str, int snm,
                      const lDescr *dp) {
   DENTER(CULL_LAYER);

   lListElem *ret;
   int sublist_pos;

   if (!ep) {
      DPRINTF("error: nullptr ptr passed to lAddSubStr\n");
      DRETURN(nullptr);
   }

   if (!(ep->descr)) {
      DPRINTF("nullptr descriptor in element not allowed !!!");
      ocs::TerminationManager::trigger_abort();
   }

   /* run time type checking */
   if ((sublist_pos = lGetPosViaElem(ep, snm, SGE_NO_ABORT)) < 0) {
      CRITICAL(MSG_CULL_ADDSUBSTRERRORXRUNTIMETYPE_S, lNm2Str(snm));
      DRETURN(nullptr);
   }

   ret = lAddElemStr(&(ep->cont[sublist_pos].glp), nm, str, dp);

   DRETURN(ret);
}

/**
 * @brief Adds a string to the string sublist
 *
 * This function add a new element into a sublist snm of an
 * element ep. The field nm of this added element will get the
 * initial value specified with str.
 *
 * @param ep list element
 * @param nm field id contained in the element which will be created
 * @param str initial value if nm
 * @param snm field id of the sublist within ep
 * @param dp Type of the new element
 *
 * @return nullptr in case of error otherwise pointer to the added element
 */
lListElem *lAddSubHost(lListElem *ep, int nm, const char *str, int snm,
                       const lDescr *dp) {
   DENTER(CULL_LAYER);

   lListElem *ret;
   int sublist_pos;

   if (!ep) {
      DPRINTF("error: nullptr ptr passed to lAddSubHost\n");
      DRETURN(nullptr);
   }

   if (!(ep->descr)) {
      DPRINTF("nullptr descriptor in element not allowed !!!");
      ocs::TerminationManager::trigger_abort();
   }

   /* run time type checking */
   if ((sublist_pos = lGetPosViaElem(ep, snm, SGE_NO_ABORT)) < 0) {
      CRITICAL(MSG_CULL_ADDSUBHOSTERRORXRUNTIMETYPE_S, lNm2Str(snm));
      DRETURN(nullptr);
   }

   ret = lAddElemHost(&(ep->cont[sublist_pos].glp), nm, str, dp);

   DRETURN(ret);
}


/**
 * @brief Adds a string to the string list
 *
 * This function adds a new element of type dp to the list referenced
 * by lpp. The field nm will get the initial value str.
 *
 * @param lpp list reference
 * @param nm field id
 * @param str initial value
 * @param dp Type of the object which will be added
 *
 * @return lListElem* -
 */
lListElem *lAddElemStr(lList **lpp, int nm, const char *str, const lDescr *dp) {
   DENTER(CULL_LAYER);

   lListElem *sep;
   int pos;
   int data_type;

   if (!lpp || !str || !dp) {
      DPRINTF("error: nullptr ptr passed to lAddElemStr\n");
      DRETURN(nullptr);
   }

   /* get position of nm in sdp */
   pos = lGetPosInDescr(dp, nm);

   /* run time type checking */
   if (pos < 0) {
      CRITICAL(MSG_CULL_ADDELEMSTRERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }
   data_type = lGetPosType(dp, pos);
   if (data_type != lStringT) {
      DPRINTF("error: lAddElemStr called to field which is no lStringT type\n");
      CRITICAL(MSG_CULL_ADDELEMSTRERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   if (!*lpp) {
      /* ensure existence of a str list in ep */
      *lpp = lCreateList("", dp);
   }

   /* add new host str element to sublist */
   sep = lCreateElem(dp);
   lSetPosString(sep, pos, (lString) str);
   lAppendElem(*lpp, sep);

   DRETURN(sep);
}

/**
 * @brief Adds a hostname to a hostname list
 *
 * Adds a hostname to a hostname list
 *
 * @param lpp list reference
 * @param nm hostname field id
 * @param str new hostname
 * @param dp descriptor of new element
 *
 * @return new element or nullptr
 */
lListElem *lAddElemHost(lList **lpp, int nm, const char *str, const lDescr *dp) {
   DENTER(CULL_LAYER);

   lListElem *sep;
   int pos;
   int data_type;

   if (!lpp || !str || !dp) {
      DPRINTF("error: nullptr ptr passed to lAddElemHost\n");
      DRETURN(nullptr);
   }

   /* get position of nm in sdp */
   pos = lGetPosInDescr(dp, nm);

   /* run time type checking */
   if (pos < 0) {
      CRITICAL(MSG_CULL_ADDELEMHOSTERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }
   data_type = lGetPosType(dp, pos);
   if (data_type != lHostT) {
      DPRINTF("error: lAddElemHost called to field which is no lHostT type\n");
      CRITICAL(MSG_CULL_ADDELEMHOSTERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   if (!*lpp) {
      /* ensure existence of a str list in ep */
      *lpp = lCreateList("", dp);
   }

   /* add new host str element to sublist */
   sep = lCreateElem(dp);
   lSetPosHost(sep, pos, (lHost) str);
   lAppendElem(*lpp, sep);
   DRETURN(sep);
}

/**
 * @brief Removes an element from a sublist
 *
 * This function removes an element specified by a string field
 * nm and the string str supposed to be in the sublist snm of the
 * element ep.
 *
 * @param ep element
 * @param nm field id
 * @param str string
 * @param snm field id of a sublist of ep
 *
 * @return 1 element was found and removed 0 in case of an error
 */
int lDelSubStr(lListElem *ep, int nm, const char *str, int snm) {
   DENTER(CULL_LAYER);

   int ret, sublist_pos;

   /* get position of sublist in ep */
   sublist_pos = lGetPosViaElem(ep, snm, SGE_DO_ABORT);

   ret = lDelElemStr(&(ep->cont[sublist_pos].glp), nm, str);

   DRETURN(ret);
}

/**
 * @brief Removes element specified by a string field nm
 *
 * This function removes an element from the list referenced by
 * lpp, which is identified by the field nm and the string str
 *
 * @param lpp list reference
 * @param nm field id
 * @param str string
 *
 * @return 1 if the element was found and removed 0 in case of an error
 */
int lDelElemStr(lList **lpp, int nm, const char *str) {
   DENTER(CULL_LAYER);

   lListElem *ep;

   if (!lpp || !str) {
      DPRINTF("error: nullptr ptr passed to lDelElemStr\n");
      DRETURN(0);
   }

   /* empty list ? */
   if (!*lpp) {
      DRETURN(1);
   }

   /* seek element */
   ep = lGetElemStrRW(*lpp, nm, str);
   if (ep) {
      lRemoveElem(*lpp, &ep);
      if (lGetNumberOfElem(*lpp) == 0) {
         lFreeList(lpp);
      }

      DRETURN(1);
   }

   DRETURN(0);
}

/**
 * @brief Returns element specified by a string field nm
 *
 * @param ep the element
 * @param nm the field
 * @param str the value to look for
 * @param snm the field of the sub-element to compare
 * @return the matching element, or nullptr when there is none
 */
lListElem *lGetSubStrRW(const lListElem *ep, int nm, const char *str, int snm) {
   DENTER(CULL_LAYER);

   int sublist_pos;
   lListElem *ret = nullptr;

   if (ep != nullptr) {
      /* get position of sublist in ep */
      sublist_pos = lGetPosViaElem(ep, snm, SGE_DO_ABORT);

      ret = lGetElemStrRW(ep->cont[sublist_pos].glp, nm, str);
   }

   DRETURN(ret);
}

/**
 * @brief Returns the sub-element whose string field matches, read only
 *
 * @param ep the element
 * @param nm the field holding the sub-list
 * @param str the value to look for
 * @param snm the field of the sub-element to compare
 * @return the matching element, or nullptr when there is none
 */
const lListElem *
lGetSubStr(const lListElem *ep, int nm, const char *str, int snm) {
   return lGetSubStrRW(ep, nm, str, snm);
}

/**
 * @brief Returns element specified by a string field nm
 *
 * @param lp the list
 * @param nm the field
 * @param str the value to look for
 * @return the matching element, or nullptr when there is none
 */
lListElem *lGetElemStrRW(const lList *lp, int nm, const char *str) {
   DENTER(CULL_LAYER);

   const void *iterator = nullptr;
   lListElem *ret = nullptr;
   ret = lGetElemStrFirstRW(lp, nm, str, &iterator);
   DRETURN(ret);
}

/**
 * @brief Returns the element whose string field matches, read only
 *
 * @param lp the list
 * @param nm the field
 * @param str the value to look for
 * @return the matching element, or nullptr when there is none
 */
const lListElem *lGetElemStr(const lList *lp, int nm, const char *str) {
   return lGetElemStrRW(lp, nm, str);
}


/**
 * @brief Find first element with a certain string
 *
 * Returns the first element within 'lp' where the attribute
 * with field name id 'nm' is equivalent with 'str'. 'iterator'
 * will be filled with context information which will make it
 * possible to use 'iterator' with lGetElemStrNext() to get
 * the next element.
 *
 * @param lp list
 * @param nm field name id
 * @param str string to be compared
 * @param iterator iterator
 *
 * @return first element or nullptr
 */
lListElem *lGetElemStrFirstRW(const lList *lp, int nm, const char *str, const void **iterator) {
   DENTER(CULL_LAYER);

   int pos;
   int data_type;
   const lDescr *listDescriptor;


   if (!str) {
      DPRINTF("error: nullptr ptr passed to lGetElemStrFirst\n");
      DRETURN(nullptr);
   }

   /* empty list ? */
   if (!lp) {
      DRETURN(nullptr);
   }

   listDescriptor = lGetListDescr(lp);

   /* get position of nm in sdp */
   pos = lGetPosInDescr(listDescriptor, nm);

   /* run time type checking */
   if (pos < 0) {
      CRITICAL(MSG_CULL_XNOTFOUNDINELEMENT_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   data_type = lGetPosType(listDescriptor, pos);
   if (data_type != lStringT) {
      CRITICAL(MSG_CULL_GETELEMSTRERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   *iterator = nullptr;

   if (lp->descr[pos].ht != nullptr) {
      /* hash access */
      lListElem *ep = cull_hash_first(lp->descr[pos].ht, str, mt_is_unique(lp->descr[pos].mt), iterator);
      DRETURN(ep);
   } else {
      /* seek for element */
      for_each_rw_lv(ep, lp) {
         const char *s = lGetPosString(ep, pos);
         if (s && !strcmp(s, str)) {
            *iterator = ep;
            DRETURN(ep);
         }
      }
   }

   DRETURN(nullptr);
}

/**
 * @brief Returns the first element whose string field matches, starting an iteration, read only
 *
 * @param lp the list
 * @param nm the field
 * @param str the value to look for
 * @param iterator iteration state, carried between calls
 * @return the matching element, or nullptr when there is none
 */
const lListElem *lGetElemStrFirst(const lList *lp, int nm, const char *str, const void **iterator) {
   return lGetElemStrFirstRW(lp, nm, str, iterator);
}

/**
 * @brief Get next element with a certain string
 *
 * Returns a element within list 'lp' where the attribute with
 * field name id 'nm' is equivalent with 'str'. The function
 * uses 'iterator' as input. 'iterator' containes context
 * information which where fillen in in a previous call of
 * lGetElemStrFirst().
 *
 * @param lp list
 * @param nm string field name id
 * @param str string
 * @param iterator iterator
 *
 * @return next element or nullptr
 */
lListElem *lGetElemStrNextRW(const lList *lp, int nm, const char *str, const void **iterator) {
   DENTER(CULL_LAYER);

   lListElem *ep;
   int pos, data_type;
   const lDescr *listDescriptor;


   if (*iterator == nullptr) {
      return nullptr;
   }

   if (!str) {
      DPRINTF("error: nullptr ptr passed to lGetElemStr\n");
      DRETURN(nullptr);
   }

   /* empty list ? */
   if (!lp) {
      DRETURN(nullptr);
   }

   listDescriptor = lGetListDescr(lp);

   /* get position of nm in sdp */
   pos = lGetPosInDescr(listDescriptor, nm);
   /* run time type checking */
   if (pos < 0) {
      CRITICAL(MSG_CULL_XNOTFOUNDINELEMENT_S, lNm2Str(nm));
      DRETURN(nullptr);
   }
   data_type = lGetPosType(listDescriptor, pos);
   if (data_type != lStringT) {
      DPRINTF("error: lGetElemStrNext called to field which is no lStringT type\n");
      DRETURN(nullptr);
   }

   if (lp->descr[pos].ht != nullptr) {
      /* hash access */
      ep = cull_hash_next(lp->descr[pos].ht, iterator);
      DRETURN(ep);
   } else {
      /* seek for element */
      for (ep = ((lListElem *) *iterator)->next; ep; ep = ep->next) {
         const char *s = lGetPosString(ep, pos);
         if (s && !strcmp(s, str)) {
            *iterator = ep;
            DRETURN(ep);
         }
      }
   }

   *iterator = nullptr;
   DRETURN(nullptr);
}

/**
 * @brief Returns the next element whose string field matches, continuing an iteration, read only
 *
 * @param lp the list
 * @param nm the field
 * @param str the value to look for
 * @param iterator iteration state, carried between calls
 * @return the matching element, or nullptr when there is none
 */
const lListElem *lGetElemStrNext(const lList *lp, int nm, const char *str, const void **iterator) {
   return lGetElemStrNextRW(lp, nm, str, iterator);
}

/**
 * @brief Returns element specified by a wildcard
 *
 * @param lp the list
 * @param nm the field
 * @param str the value to look for
 * @return the matching element, or nullptr when there is none
 */
lListElem *lGetElemStrLikeRW(const lList *lp, int nm, const char *str) {
   DENTER(CULL_LAYER);

   int pos;
   const char *s;
   int data_type;
   size_t str_pos = 0;
   const lDescr *listDescriptor;


   if (!str) {
      DPRINTF("error: nullptr ptr passed to lGetElemStr\n");
      DRETURN(nullptr);
   }

   /* empty list ? */
   if (!lp) {
      DRETURN(nullptr);
   }

   /* get position of nm in sdp */
   listDescriptor = lGetListDescr(lp);
   pos = lGetPosInDescr(listDescriptor, nm);
   /* run time type checking */
   if (pos < 0) {
      CRITICAL(MSG_CULL_XNOTFOUNDINELEMENT_S, lNm2Str(nm));
      DRETURN(nullptr);
   }
   data_type = lGetPosType(listDescriptor, pos);
   if (data_type != lStringT) {
      CRITICAL(MSG_CULL_GETELEMSTRERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   /* seek for element */
   str_pos = strlen(str) - 1;
   for_each_rw_lv(ep, lp) {
      s = lGetPosString(ep, pos);
      if (s && (!strcmp(s, str) || (str[str_pos] == '*' && !strncmp(s, str, str_pos)))) {
         DRETURN(ep);
      }
   }

   DRETURN(nullptr);
}

/**
 * @brief Returns the element whose string field matches a pattern, read only
 *
 * @param lp the list
 * @param nm the field
 * @param str the value to look for
 * @return the matching element, or nullptr when there is none
 */
const lListElem *lGetElemStrLike(const lList *lp, int nm, const char *str) {
   return lGetElemStrLikeRW(lp, nm, str);
}

/**
 * @brief Adds ulong to the ulong sublist of element ep
 *
 * This function adds a new element into the sublist snm of the
 * element ep. The field nm of the added element will get the
 * initial value val.
 *
 * @param ep element
 * @param nm field which will get value val
 * @param val initial value for nm
 * @param snm sublist within ep where the element will be added
 * @param dp Type of the new element (e.g. JB_Type)
 *
 * @return nullptr in case of error or the pointer to the new element
 */
lListElem *lAddSubUlong(lListElem *ep, int nm, lUlong val, int snm,
                        const lDescr *dp) {
   DENTER(CULL_LAYER);

   lListElem *ret;
   int sublist_pos;

   if (!ep) {
      DPRINTF("error: nullptr ptr passed to lAddSubUlong\n");
      DRETURN(nullptr);
   }

   if (!(ep->descr)) {
      DPRINTF("nullptr descriptor in element not allowed !!!");
      ocs::TerminationManager::trigger_abort();
   }

   /* run time type checking */
   if ((sublist_pos = lGetPosViaElem(ep, snm, SGE_NO_ABORT)) < 0) {
      CRITICAL(MSG_CULL_ADDSUBULONGERRORXRUNTIMETYPE_S, lNm2Str(snm));
      DRETURN(nullptr);
   }

   ret = lAddElemUlong(&(ep->cont[sublist_pos].glp), nm, val, dp);

   DRETURN(ret);
}

/**
 * @brief Adds a ulong to the ulong list
 *
 * Adds an new element to a list lpp where one field nm within
 * the new element gets an initial value val
 *
 * @param lpp list
 * @param nm field in the new element which will get value val
 * @param val initial value for nm
 * @param dp type of the list (e.g. JB_Type)
 *
 * @return nullptr on error or pointer to the added element
 */
lListElem *lAddElemUlong(lList **lpp, int nm, lUlong val, const lDescr *dp) {
   DENTER(CULL_LAYER);

   lListElem *sep;
   int pos;

   if (!lpp || !dp) {
      DPRINTF("error: nullptr ptr passed to lAddElemUlong\n");
      DRETURN(nullptr);
   }

   /* get position of nm in sdp */
   pos = lGetPosInDescr(dp, nm);

   /* run time type checking */
   if (pos < 0) {
      CRITICAL(MSG_CULL_ADDELEMULONGERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   if (!*lpp) {
      /* ensure existence of a val list in ep */
      *lpp = lCreateList("ulong_sublist", dp);
   }

   /* add new host val element to sublist */
   sep = lCreateElem(dp);
   lSetPosUlong(sep, pos, val);
   lAppendElem(*lpp, sep);

   DRETURN(sep);
}

/**
 * @brief Removes an element from a sublist
 *
 * This function removes an element specified by a ulong field nm
 * and the ulong val supposed to be in the sublist snm of the
 * element ep
 *
 * @param ep element
 * @param nm field id
 * @param val value
 * @param snm field id of the sublist in ep
 *
 * @return 1 element was found and removed 0 in case of an error
 */
int lDelSubUlong(lListElem *ep, int nm, lUlong val, int snm) {
   DENTER(CULL_LAYER);

   int ret, sublist_pos;

   /* get position of sublist in ep */
   sublist_pos = lGetPosViaElem(ep, snm, SGE_DO_ABORT);

   ret = lDelElemUlong(&(ep->cont[sublist_pos].glp), nm, val);

   DRETURN(ret);
}

/**
 * @brief Removes elem specified by a ulong field nm
 *
 * This function removes an element specified by a ulong field nm
 * with the value val from the list referenced by lpp.
 *
 * @param lpp reference to a list
 * @param nm field id
 * @param val value if nm
 *
 * @return 1 element was found and removed 0 an error occurred
 */
int lDelElemUlong(lList **lpp, int nm, lUlong val) {
   DENTER(CULL_LAYER);

   lListElem *ep;

   if (!lpp || !val) {
      DPRINTF("error: nullptr ptr passed to lDelElemUlong\n");
      DRETURN(0);
   }

   /* empty list ? */
   if (!*lpp) {
      DRETURN(1);
   }

   /* seek element */
   ep = lGetElemUlongRW(*lpp, nm, val);
   if (ep) {
      lRemoveElem(*lpp, &ep);
      if (lGetNumberOfElem(*lpp) == 0) {
         lFreeList(lpp);
      }
   }

   DRETURN(1);
}

/**
 * @brief Element specified by a ulong field nm
 *
 * returns an element specified by a ulong field nm an the ulong
 * value val from the sublist snm of the element ep
 *
 * @param ep element pointer
 * @param nm field id which is part of a sublist element of ep
 * @param val unsigned long value
 * @param snm field id of a list which is part of ep
 *
 * @return nullptr if element was not found or in case of an error otherwise pointer to the element
 */
lListElem *lGetSubUlongRW(const lListElem *ep, int nm, lUlong val, int snm) {
   DENTER(CULL_LAYER);

   int sublist_pos;
   lListElem *ret;

   /* get position of sublist in ep */
   sublist_pos = lGetPosViaElem(ep, snm, SGE_DO_ABORT);

   ret = lGetElemUlongRW(ep->cont[sublist_pos].glp, nm, val);

   DRETURN(ret);
}

/**
 * @brief Returns the sub-element whose 32 bit field matches, read only
 *
 * @param ep the element
 * @param nm the field
 * @param val the value to look for
 * @param snm the field of the sub-element to compare
 * @return the matching element, or nullptr when there is none
 */
const lListElem *lGetSubUlong(const lListElem *ep, int nm, lUlong val, int snm) {
   return lGetSubUlongRW(ep, nm, val, snm);
}

/**
 * @brief Returns element specified by a ulong field nm
 *
 * @param lp the list
 * @param nm the field
 * @param val the value to look for
 * @return the matching element, or nullptr when there is none
 */
lListElem *lGetElemUlongRW(const lList *lp, int nm, lUlong val) {
   const void *iterator = nullptr;
   return lGetElemUlongFirstRW(lp, nm, val, &iterator);
}

/**
 * @brief Returns the element whose 32 bit field matches, read only
 *
 * @param lp the list
 * @param nm the field
 * @param val the value to look for
 * @return the matching element, or nullptr when there is none
 */
const lListElem *lGetElemUlong(const lList *lp, int nm, lUlong val) {
   return lGetElemUlongRW(lp, nm, val);
}

/**
 * @brief Find first ulong within a list
 *
 * Return the first element of list 'lp' where the attribute
 * with field name id 'nm' is equivalent with 'val'. Context
 * information will be stored in 'iterator'. 'iterator' might
 * be used in lGetElemUlongNext() to get the next element.
 *
 * @param lp list
 * @param nm ulong field anme id
 * @param val ulong value
 * @param iterator iterator
 *
 * @return element or nullptr
 */
lListElem *lGetElemUlongFirstRW(const lList *lp, int nm, lUlong val, const void **iterator) {
   DENTER(CULL_LAYER);
   int pos;


   /* empty list ? */
   if (!lp) {
      DRETURN(nullptr);
   }

   /* get position of nm in sdp */
   pos = lGetPosInDescr(lGetListDescr(lp), nm);

   /* run time type checking */
   if (pos < 0) {
      CRITICAL(MSG_CULL_GETELEMULONGERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   *iterator = nullptr;

   if (lp->descr[pos].ht != nullptr) {
      /* hash access */
      lListElem *ep = cull_hash_first(lp->descr[pos].ht, &val, mt_is_unique(lp->descr[pos].mt), iterator);
      DRETURN(ep);
   } else {
      /* seek for element */
      for_each_rw_lv(ep, lp) {
         lUlong s = lGetPosUlong(ep, pos);
         if (s == val) {
            *iterator = ep;
            DRETURN(ep);
         }
      }
   }

   DRETURN(nullptr);
}

/**
 * @brief Returns the first element whose 32 bit field matches, starting an iteration, read only
 *
 * @param lp the list
 * @param nm the field
 * @param val the value to look for
 * @param iterator iteration state, carried between calls
 * @return the matching element, or nullptr when there is none
 */
const lListElem *lGetElemUlongFirst(const lList *lp, int nm, lUlong val, const void **iterator) {
   return lGetElemUlongFirstRW(lp, nm, val, iterator);
}

/**
 * @brief Find next ulong element within a list
 *
 * This function might be used after a call to lGetElemUlongFirst().
 * It expects 'iterator' to contain context information which
 * makes it possible to find the next element within list 'lp'
 * where the attribute with field name id 'nm' is equivalent with
 * 'val'.
 *
 * @param lp list
 * @param nm ulong field name id
 * @param val value
 * @param iterator iterator
 *
 * @return next element or nullptr
 */
lListElem *lGetElemUlongNextRW(const lList *lp, int nm, lUlong val, const void **iterator) {
   DENTER(CULL_LAYER);

   lListElem *ep;
   int pos;

   if (*iterator == nullptr) {
      return nullptr;
   }

   /* get position of nm in sdp */
   pos = lGetPosInDescr(lGetListDescr(lp), nm);

   /* run time type checking */
   if (pos < 0) {
      CRITICAL(MSG_CULL_GETELEMULONGERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   if (lp->descr[pos].ht != nullptr) {
      /* hash access */
      ep = cull_hash_next(lp->descr[pos].ht, iterator);
      DRETURN(ep);
   } else {
      /* seek for element */
      for (ep = ((lListElem *) *iterator)->next; ep; ep = ep->next) {
         lUlong s = lGetPosUlong(ep, pos);
         if (s == val) {
            *iterator = ep;
            DRETURN(ep);
         }
      }
   }

   *iterator = nullptr;
   DRETURN(nullptr);
}

/**
 * @brief Returns the next element whose 32 bit field matches, continuing an iteration, read only
 *
 * @param lp the list
 * @param nm the field
 * @param val the value to look for
 * @param iterator iteration state, carried between calls
 * @return the matching element, or nullptr when there is none
 */
const lListElem *lGetElemUlongNext(const lList *lp, int nm, lUlong val, const void **iterator) {
   return lGetElemUlongNextRW(lp, nm, val, iterator);
}

/**
 * @brief Adds ulong64 to the ulong64 sublist of element ep
 *
 * This function adds a new element into the sublist snm of the
 * element ep. The field nm of the added element will get the
 * initial value val.
 *
 * @param ep element
 * @param nm field which will get value val
 * @param val initial value for nm
 * @param snm sublist within ep where the element will be added
 * @param dp Type of the new element (e.g. JB_Type)
 *
 * @return nullptr in case of error or the pointer to the new element
 */
lListElem *lAddSubUlong64(lListElem *ep, int nm, lUlong64 val, int snm,
                          const lDescr *dp) {
   DENTER(CULL_LAYER);

   lListElem *ret;
   int sublist_pos;

   if (!ep) {
      DPRINTF("error: nullptr ptr passed to lAddSubUlong64\n");
      DRETURN(nullptr);
   }

   if (!(ep->descr)) {
      DPRINTF("nullptr descriptor in element not allowed !!!");
      ocs::TerminationManager::trigger_abort();
   }

   /* run time type checking */
   if ((sublist_pos = lGetPosViaElem(ep, snm, SGE_NO_ABORT)) < 0) {
      CRITICAL(MSG_CULL_ADDSUBULONG64ERRORXRUNTIMETYPE_S, lNm2Str(snm));
      DRETURN(nullptr);
   }

   ret = lAddElemUlong64(&(ep->cont[sublist_pos].glp), nm, val, dp);

   DRETURN(ret);
}

/**
 * @brief Adds a ulong64 to the ulong64 list
 *
 * Adds an new element to a list lpp where one field nm within
 * the new element gets an initial value val
 *
 * @param lpp list
 * @param nm field in the new element which will get value val
 * @param val initial value for nm
 * @param dp type of the list (e.g. JB_Type)
 *
 * @return nullptr on error or pointer to the added element
 */
lListElem *lAddElemUlong64(lList **lpp, int nm, lUlong64 val, const lDescr *dp) {
   DENTER(CULL_LAYER);

   lListElem *sep;
   int pos;

   if (!lpp || !dp) {
      DPRINTF("error: nullptr ptr passed to lAddElemUlong64\n");
      DRETURN(nullptr);
   }

   /* get position of nm in sdp */
   pos = lGetPosInDescr(dp, nm);

   /* run time type checking */
   if (pos < 0) {
      CRITICAL(MSG_CULL_ADDELEMULONG64ERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   if (!*lpp) {
      /* ensure existence of a val list in ep */
      *lpp = lCreateList("ulong64_sublist", dp);
   }

   /* add new host val element to sublist */
   sep = lCreateElem(dp);
   lSetPosUlong64(sep, pos, val);
   lAppendElem(*lpp, sep);

   DRETURN(sep);
}

/**
 * @brief Removes an element from a sublist
 *
 * This function removes an element specified by a ulong field nm
 * and the ulong64 val supposed to be in the sublist snm of the
 * element ep
 *
 * @param ep element
 * @param nm field id
 * @param val value
 * @param snm field id of the sublist in ep
 *
 * @return 1 element was found and removed 0 in case of an error
 */
int lDelSubUlong64(lListElem *ep, int nm, lUlong64 val, int snm) {
   DENTER(CULL_LAYER);

   int ret, sublist_pos;

   /* get position of sublist in ep */
   sublist_pos = lGetPosViaElem(ep, snm, SGE_DO_ABORT);

   ret = lDelElemUlong64(&(ep->cont[sublist_pos].glp), nm, val);

   DRETURN(ret);
}

/**
 * @brief Removes elem specified by a ulong64 field nm
 *
 * This function removes an element specified by a ulong64 field nm
 * with the value val from the list referenced by lpp.
 *
 * @param lpp reference to a list
 * @param nm field id
 * @param val value if nm
 *
 * @return 1 element was found and removed 0 an error occurred
 */
int lDelElemUlong64(lList **lpp, int nm, lUlong64 val) {
   DENTER(CULL_LAYER);

   lListElem *ep;

   if (!lpp || !val) {
      DPRINTF("error: nullptr ptr passed to lDelElemUlong64\n");
      DRETURN(0);
   }

   /* empty list ? */
   if (!*lpp) {
      DRETURN(1);
   }

   /* seek element */
   ep = lGetElemUlong64RW(*lpp, nm, val);
   if (ep) {
      lRemoveElem(*lpp, &ep);
      if (lGetNumberOfElem(*lpp) == 0) {
         lFreeList(lpp);
      }
   }

   DRETURN(1);
}

/**
 * @brief Element specified by a ulong64 field nm
 *
 * returns an element specified by a ulong64 field nm an the ulong64
 * value val from the sublist snm of the element ep
 *
 * @param ep element pointer
 * @param nm field id which is part of a sublist element of ep
 * @param val unsigned long value
 * @param snm field id of a list which is part of ep
 *
 * @return nullptr if element was not found or in case of an error otherwise pointer to the element
 */
lListElem *lGetSubUlong64RW(const lListElem *ep, int nm, lUlong64 val, int snm) {
   DENTER(CULL_LAYER);

   int sublist_pos;
   lListElem *ret;

   /* get position of sublist in ep */
   sublist_pos = lGetPosViaElem(ep, snm, SGE_DO_ABORT);

   ret = lGetElemUlong64RW(ep->cont[sublist_pos].glp, nm, val);

   DRETURN(ret);
}

/**
 * @brief Returns the sub-element whose 64 bit field matches, read only
 *
 * @param ep the element
 * @param nm the field
 * @param val the value to look for
 * @param snm the field of the sub-element to compare
 * @return the matching element, or nullptr when there is none
 */
const lListElem *lGetSubUlong64(const lListElem *ep, int nm, lUlong64 val, int snm) {
   return lGetSubUlong64RW(ep, nm, val, snm);
}

/**
 * @brief Returns element specified by a ulong64 field nm
 *
 * @param lp the list
 * @param nm the field
 * @param val the value to look for
 * @return the matching element, or nullptr when there is none
 */
lListElem *lGetElemUlong64RW(const lList *lp, int nm, lUlong64 val) {
   const void *iterator = nullptr;
   return lGetElemUlong64FirstRW(lp, nm, val, &iterator);
}

/**
 * @brief Returns the element whose 64 bit field matches, read only
 *
 * @param lp the list
 * @param nm the field
 * @param val the value to look for
 * @return the matching element, or nullptr when there is none
 */
const lListElem *lGetElemUlong64(const lList *lp, int nm, lUlong64 val) {
   return lGetElemUlong64RW(lp, nm, val);
}

/**
 * @brief Find first ulong64 within a list
 *
 * Return the first element of list 'lp' where the attribute
 * with field name id 'nm' is equivalent with 'val'. Context
 * information will be stored in 'iterator'. 'iterator' might
 * be used in lGetElemUlong64Next() to get the next element.
 *
 * @param lp list
 * @param nm ulong64 field name id
 * @param val ulong64 value
 * @param iterator iterator
 *
 * @return element or nullptr
 */
lListElem *lGetElemUlong64FirstRW(const lList *lp, int nm, lUlong64 val, const void **iterator) {
   DENTER(CULL_LAYER);
   int pos;


   /* empty list ? */
   if (!lp) {
      DRETURN(nullptr);
   }

   /* get position of nm in sdp */
   pos = lGetPosInDescr(lGetListDescr(lp), nm);

   /* run time type checking */
   if (pos < 0) {
      CRITICAL(MSG_CULL_GETELEMULONG64ERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   *iterator = nullptr;

   if (lp->descr[pos].ht != nullptr) {
      /* hash access */
      lListElem *ep = cull_hash_first(lp->descr[pos].ht, &val, mt_is_unique(lp->descr[pos].mt), iterator);
      DRETURN(ep);
   } else {
      /* seek for element */
      for_each_rw_lv(ep, lp) {
         lUlong64 s = lGetPosUlong64(ep, pos);
         if (s == val) {
            *iterator = ep;
            DRETURN(ep);
         }
      }
   }

   DRETURN(nullptr);
}

/**
 * @brief Returns the first element whose 64 bit field matches, starting an iteration, read only
 *
 * @param lp the list
 * @param nm the field
 * @param val the value to look for
 * @param iterator iteration state, carried between calls
 * @return the matching element, or nullptr when there is none
 */
const lListElem *lGetElemUlong64First(const lList *lp, int nm, lUlong64 val, const void **iterator) {
   return lGetElemUlong64FirstRW(lp, nm, val, iterator);
}

/**
 * @brief Find next ulong64 element within a list
 *
 * This function might be used after a call to lGetElemUlong64First().
 * It expects 'iterator' to contain context information which
 * makes it possible to find the next element within list 'lp'
 * where the attribute with field name id 'nm' is equivalent with
 * 'val'.
 *
 * @param lp list
 * @param nm ulong64 field name id
 * @param val value
 * @param iterator iterator
 *
 * @return next element or nullptr
 */
lListElem *lGetElemUlong64NextRW(const lList *lp, int nm, lUlong64 val, const void **iterator) {
   DENTER(CULL_LAYER);

   lListElem *ep;
   int pos;

   if (*iterator == nullptr) {
      return nullptr;
   }

   /* get position of nm in sdp */
   pos = lGetPosInDescr(lGetListDescr(lp), nm);

   /* run time type checking */
   if (pos < 0) {
      CRITICAL(MSG_CULL_GETELEMULONG64ERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   if (lp->descr[pos].ht != nullptr) {
      /* hash access */
      ep = cull_hash_next(lp->descr[pos].ht, iterator);
      DRETURN(ep);
   } else {
      /* seek for element */
      for (ep = ((lListElem *) *iterator)->next; ep; ep = ep->next) {
         lUlong64 s = lGetPosUlong64(ep, pos);
         if (s == val) {
            *iterator = ep;
            DRETURN(ep);
         }
      }
   }

   *iterator = nullptr;
   DRETURN(nullptr);
}

/**
 * @brief Returns the next element whose 64 bit field matches, continuing an iteration, read only
 *
 * @param lp the list
 * @param nm the field
 * @param val the value to look for
 * @param iterator iteration state, carried between calls
 * @return the matching element, or nullptr when there is none
 */
const lListElem *lGetElemUlong64Next(const lList *lp, int nm, lUlong64 val, const void **iterator) {
   return lGetElemUlong64NextRW(lp, nm, val, iterator);
}

/**
 * @brief Returns elem specified by a string field nm
 *
 * @param ep the element
 * @param nm the field
 * @param str the value to look for
 * @param snm the field of the sub-element to compare
 * @return the matching element, or nullptr when there is none
 */
lListElem *lGetSubCaseStr(const lListElem *ep, int nm, const char *str,
                          int snm) {
   DENTER(CULL_LAYER);

   int sublist_pos;
   lListElem *ret;

   /* get position of sublist in ep */
   sublist_pos = lGetPosViaElem(ep, snm, SGE_DO_ABORT);

   ret = lGetElemCaseStrRW(ep->cont[sublist_pos].glp, nm, str);

   DRETURN(ret);
}

/**
 * @brief Returns element specified by a string field
 *
 * @param lp the list
 * @param nm the field
 * @param str the value to look for
 * @return the matching element, or nullptr when there is none
 */
lListElem *lGetElemCaseStrRW(const lList *lp, int nm, const char *str) {
   DENTER(CULL_LAYER);
   int pos;
   const char *s;
   int data_type;
   const lDescr *listDescriptor;

   if (!str) {
      DPRINTF("error: nullptr ptr passed to lGetElemCaseStr\n");
      DRETURN(nullptr);
   }

   /* empty list ? */
   if (!lp) {
      DRETURN(nullptr);
   }

   listDescriptor = lGetListDescr(lp);

   /* get position of nm in sdp */
   pos = lGetPosInDescr(listDescriptor, nm);

   /* run time type checking */
   if (pos < 0) {
      CRITICAL(MSG_CULL_GETELEMCASESTRERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   data_type = lGetPosType(listDescriptor, pos);
   if (data_type != lStringT) {
      DPRINTF(":::::::::::::::: lGetElemCaseStr - data type is not lStringT !!! :::::::");
      CRITICAL(MSG_CULL_GETELEMCASESTRERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   /* seek for element */
   for_each_rw_lv(ep, lp) {
      s = lGetPosString(ep, pos);
      if (s && !SGE_STRCASECMP(s, str)) {
         DRETURN(ep);
      }
   }

   DRETURN(nullptr);
}

/**
 * @brief Returns the element whose string field matches, ignoring case, read only
 *
 * @param lp the list
 * @param nm the field
 * @param str the value to look for
 * @return the matching element, or nullptr when there is none
 */
const lListElem *lGetElemCaseStr(const lList *lp, int nm, const char *str) {
   return lGetElemCaseStrRW(lp, nm, str);
}

/**
 * @brief Returns an element specified by a hostname
 *
 * @param lp the list
 * @param nm the field
 * @param str the value to look for
 * @return the matching element, or nullptr when there is none
 */
lListElem *lGetElemHostRW(const lList *lp, int nm, const char *str) {
   const void *iterator = nullptr;
   return lGetElemHostFirstRW(lp, nm, str, &iterator);
}

/**
 * @brief Returns the element whose host field matches, read only
 *
 * @param lp the list
 * @param nm the field
 * @param str the value to look for
 * @return the matching element, or nullptr when there is none
 */
const lListElem *lGetElemHost(const lList *lp, int nm, const char *str) {
   return lGetElemHostRW(lp, nm, str);
}

/**
 * @brief LGetElemHostFirst for hostnames
 *
 * lGetElemHostFirst for hostnames
 *
 * @param lp list
 * @param nm hostname field id
 * @param str hostname
 * @param iterator iterator
 *
 * @return element or nullptr
 */
lListElem *lGetElemHostFirstRW(const lList *lp, int nm, const char *str, const void **iterator) {
   DENTER(TOP_LAYER);

   int pos;
   int data_type;
   const lDescr *listDescriptor = nullptr;
   char uhost[CL_MAXHOSTNAMELEN + 1];
   char cmphost[CL_MAXHOSTNAMELEN + 1];
   const char *s = nullptr;

   /* check for null pointers */
   if ((str == nullptr) || (lp == nullptr)) {
      DPRINTF("error: nullptr ptr passed to lGetElemHostFirst\n");
      DRETURN(nullptr);
   }

   /* run time type checking */
   listDescriptor = lGetListDescr(lp);
   pos = lGetPosInDescr(listDescriptor, nm);
   data_type = lGetPosType(listDescriptor, pos);
   if ((pos < 0) || (data_type != lHostT)) {
      if (data_type != lHostT) {
         DPRINTF(":::::::::::::::: lGetElemHostFirst - data type is not lHostT !!! :::::::\n");
      }
      CRITICAL(MSG_CULL_GETELEMHOSTERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   *iterator = nullptr;
   if (lp->descr[pos].ht != nullptr) {
      /* we have a hash table */
      char host_key[CL_MAXHOSTNAMELEN + 1];
      sge_hostcpy(host_key, str);
      sge_strtoupper(host_key, CL_MAXHOSTNAMELEN);
      lListElem *ep = cull_hash_first(lp->descr[pos].ht, host_key, mt_is_unique(lp->descr[pos].mt), iterator);
      DRETURN(ep);
   } else {
      /* expensive host search algorithm */

      /* copy searched hostname */
      sge_hostcpy(uhost, str);

      /* sequence search */
      for_each_rw_lv(ep, lp) {
         s = lGetPosHost(ep, pos);
         if (s != nullptr) {
            sge_hostcpy(cmphost, s);
            if (!SGE_STRCASECMP(cmphost, uhost)) {
               *iterator = ep;
               DRETURN(ep);
            }
         }
      }
   }

   DRETURN(nullptr);
}

/**
 * @brief Returns the first element whose host field matches, starting an iteration, read only
 *
 * @param lp the list
 * @param nm the field
 * @param str the value to look for
 * @param iterator iteration state, carried between calls
 * @return the matching element, or nullptr when there is none
 */
const lListElem *lGetElemHostFirst(const lList *lp, int nm, const char *str, const void **iterator) {
   return lGetElemHostFirstRW(lp, nm, str, iterator);
}

/**
 * @brief LGetElemHostNext() for hostnames
 *
 * lGetElemHostNext() for hostnames
 *
 * @param lp list
 * @param nm hostname field id
 * @param str hostname
 * @param iterator iterator
 *
 * @return element or nullptr
 */
lListElem *lGetElemHostNextRW(const lList *lp, int nm, const char *str, const void **iterator) {
   DENTER(TOP_LAYER);

   int pos;
   lListElem *ep = nullptr;
   const lDescr *listDescriptor = nullptr;
   char uhost[CL_MAXHOSTNAMELEN + 1];
   char cmphost[CL_MAXHOSTNAMELEN + 1];
   const char *s = nullptr;

   /* check for null *iterator and */
   /* check for null pointers */
   if ((str == nullptr) || (lp == nullptr) || (*iterator == nullptr)) {
      DPRINTF("error: nullptr ptr passed to lGetElemHostNext\n");
      DRETURN(nullptr);
   }

   /* run time type checking */
   listDescriptor = lGetListDescr(lp);
   pos = lGetPosInDescr(listDescriptor, nm);
   if (pos < 0) {
      CRITICAL(MSG_CULL_GETELEMHOSTERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   if (lp->descr[pos].ht != nullptr) {
      /* we have a hash table */
      ep = cull_hash_next(lp->descr[pos].ht, iterator);
      DRETURN(ep);
   } else {
      /* expensive host search algorithm */

      /* copy searched hostname */
      sge_hostcpy(uhost, str);

      /* sequence search */
      for (ep = ((lListElem *) *iterator)->next; ep; ep = ep->next) {
         s = lGetPosHost(ep, pos);
         if (s != nullptr) {
            sge_hostcpy(cmphost, s);
            if (!SGE_STRCASECMP(cmphost, uhost)) {
               *iterator = ep;
               DRETURN(ep);
            }
         }
      }
   }
   *iterator = nullptr;

   DRETURN(nullptr);
}

/**
 * @brief Returns the next element whose host field matches, continuing an iteration, read only
 *
 * @param lp the list
 * @param nm the field
 * @param str the value to look for
 * @param iterator iteration state, carried between calls
 * @return the matching element, or nullptr when there is none
 */
const lListElem *lGetElemHostNext(const lList *lp, int nm, const char *str, const void **iterator) {
   return lGetElemHostNextRW(lp, nm, str, iterator);
}

/**
 * @brief Returns elem specified by a string field nm
 *
 * @param ep the element
 * @param nm the field
 * @param str the value to look for
 * @param snm the field of the sub-element to compare
 * @return the matching element, or nullptr when there is none
 */
lListElem *lGetSubHostRW(const lListElem *ep, int nm, const char *str, int snm) {
   DENTER(CULL_LAYER);

   int sublist_pos;
   lListElem *ret;

   /* get position of sublist in ep */
   sublist_pos = lGetPosViaElem(ep, snm, SGE_DO_ABORT);

   ret = lGetElemHostRW(ep->cont[sublist_pos].glp, nm, str);

   DRETURN(ret);
}

/**
 * @brief Returns the sub-element whose host field matches, read only
 *
 * @param ep the element
 * @param nm the field
 * @param str the value to look for
 * @param snm the field of the sub-element to compare
 * @return the matching element, or nullptr when there is none
 */
const lListElem *lGetSubHost(const lListElem *ep, int nm, const char *str, int snm) {
   return lGetSubHostRW(ep, nm, str, snm);
}

/**
 * @brief Removes elem specified by a lHostT field nm
 *
 * removes an element specified by a string field nm and the
 * hostname str from the list referenced by lpp.
 * If it is the last element within lpp the list itself will be
 * deleted.
 *
 * @param lpp list
 * @param nm field id
 * @param str string
 *
 * @return 1 if the host element was found and removed 0 in case of an error
 */
int lDelElemHost(lList **lpp, int nm, const char *str) {
   DENTER(CULL_LAYER);

   lListElem *ep;

   if (!lpp || !str) {
      DPRINTF("error: nullptr ptr passed to lDelElemHost\n");
      DRETURN(0);
   }

   /* empty list ? */
   if (!*lpp) {
      DRETURN(1);
   }

   /* seek elemtent */
   ep = lGetElemHostRW(*lpp, nm, str);
   if (ep) {
      lRemoveElem(*lpp, &ep);
      if (lGetNumberOfElem(*lpp) == 0) {
         lFreeList(lpp);
      }
      DRETURN(1);
   }

   DRETURN(0);
}

/**
 * @brief Returns name at position
 *
 * Returns the name at specified position in a descriptor array. The
 * Position must be inside the valid range of the descriptor. Returns
 * NoName if descriptor is nullptr or pos < 0.
 *
 * @param dp Descriptor
 * @param pos Position
 *
 * @return Name
 */
int lGetPosName(const lDescr *dp, int pos) {

   if (!dp) {
      LERROR(LEDESCRNULL);
      return (int) NoName;
   }
   if (pos < 0) {
      return (int) NoName;
   }
   return dp[pos].nm;
}

/**
 * @brief Are all bits of a mask set in a 32 bit field?
 *
 * @param ep the element
 * @param nm the field
 * @param bitmask the bits required to be set
 * @return true when every bit of @p bitmask is set in the field
 */
bool lMatchUlongBitMask(lListElem *ep, int nm, uint32_t bitmask) {
   DENTER(CULL_BASIS_LAYER);

   int pos = lGetPosViaElem(ep, nm, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lUlongT)
      incompatibleType2(MSG_CULL_GETULONG_WRONGTYPEFORFIELDXY_SS,
                        lNm2Str(nm), multitypes[mt_get_type(ep->descr[pos].mt)]);

   DRETURN((ep->cont[pos].ul & bitmask) > 0 ? true : false);
}

/**
 * @brief Set bits in a 32 bit field
 *
 * @param ep the element
 * @param nm the field
 * @param bitmask the bits to set
 * @return 0 on success, -1 when @p ep is nullptr
 */
int lOrUlongBitMask(lListElem *ep, int nm, uint32_t bitmask) {
   DENTER(CULL_BASIS_LAYER);

   if (ep == nullptr) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   int pos = lGetPosViaElem(ep, nm, SGE_NO_ABORT);
   if (pos < 0) {
      DPRINTF(("!!!!!!!!!! lSetUlongBit(): %s not found in element !!!!!!!!!!\n", lNm2Str(name)));
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lUlongT) {
      incompatibleType2(MSG_CULL_SETULONG_WRONGTYPEFORFIELDXY_SS, lNm2Str(nm),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
      DRETURN(-1);
   }

   ep->cont[pos].ul |= bitmask;

   DRETURN(0);
}
/**
 * @brief Keep only the bits of a mask in a 32 bit field
 *
 * @param ep the element
 * @param nm the field
 * @param bitmask the bits to keep
 * @return 0 on success, -1 when @p ep is nullptr
 */
int lAndUlongBitMask(lListElem *ep, int nm, uint32_t bitmask) {
   DENTER(CULL_BASIS_LAYER);

   if (ep == nullptr) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   int pos = lGetPosViaElem(ep, nm, SGE_NO_ABORT);
   if (pos < 0) {
      DPRINTF(("!!!!!!!!!! lSetUlongBit(): %s not found in element !!!!!!!!!!\n", lNm2Str(name)));
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lUlongT) {
      incompatibleType2(MSG_CULL_SETULONG_WRONGTYPEFORFIELDXY_SS, lNm2Str(nm),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
      DRETURN(-1);
   }

   ep->cont[pos].ul &= bitmask;

   DRETURN(0);
}
/**
 * @brief Clear bits in a 32 bit field
 *
 * @param ep the element
 * @param nm the field
 * @param bitmask the bits to clear
 * @return 0 on success, -1 when @p ep is nullptr
 */
int lClearUlongBitMask(lListElem *ep, int nm, uint32_t bitmask) {
   DENTER(CULL_BASIS_LAYER);

   if (ep == nullptr) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   int pos = lGetPosViaElem(ep, nm, SGE_NO_ABORT);
   if (pos < 0) {
      DPRINTF(("!!!!!!!!!! lSetUlongBit(): %s not found in element !!!!!!!!!!\n", lNm2Str(name)));
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lUlongT) {
      incompatibleType2(MSG_CULL_SETULONG_WRONGTYPEFORFIELDXY_SS, lNm2Str(nm),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
      DRETURN(-1);
   }

   ep->cont[pos].ul &= ~bitmask;

   DRETURN(0);

}

