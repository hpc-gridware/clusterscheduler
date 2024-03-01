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
 ************************************************************************/
/* Portions of this code are Copyright (c) 2011 Univa Corporation. */
/*___INFO__MARK_END__*/
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

/* do not compile in monitoring code */
#ifndef NO_SGE_COMPILE_DEBUG
#define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "cull/msg_cull.h"
#include "cull/cull_multitypeP.h"
#include "cull/cull_listP.h"
#include "cull/cull_whatP.h"
#include "cull/cull_lerrnoP.h"
#include "cull/cull_hash.h"

#ifdef OBSERVE
#  include "cull/cull_observe.h"
#endif

#define CULL_BASIS_LAYER CULL_LAYER

/* ---------- global variable --------------------------------- */

static const char *_lNm2Str(const lNameSpace *nsp, int nm);

static int _lStr2Nm(const lNameSpace *nsp, const char *str);


const char *multitypes[] =
        {
                "lEndT",
                "lFloatT",
                "lDoubleT",
                "lUlongT",
                "lLongT",
                "lCharT",
                "lBoolT",
                "lIntT",
                "lStringT",
                "lListT",
                "lObjectT",
                "lRefT",
                "lHostT",
                "lUlong64T"
        };

int incompatibleType(const char *str) {
   int i;

   DENTER(TOP_LAYER);

   for (i = 0; i < 5; i++)
           DPRINTF(("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"));
   DPRINTF(("incompatible type in function %s()\n", str));
   for (i = 0; i < 5; i++)
           DPRINTF(("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"));

   abort();
   DRETURN(-1);
}

int incompatibleType2(const char *fmt, ...) {
   va_list ap;
   char buf[MAX_STRING_SIZE];

   DENTER(TOP_LAYER);
   va_start(ap, fmt);
   vsnprintf(buf, sizeof(buf), fmt, ap);
   va_end(ap);

   CRITICAL(SFNMAX, buf);
   fprintf(stderr, SFNMAX, buf);

   abort();
   DRETURN(-1);
}

/* ------------------------------------------------------------ */
int unknownType(const char *str) {
   DENTER(CULL_LAYER);

   /* abort is used, so we don't free any memory; if you change this
      function please free your memory                      */
   DPRINTF(("Unknown Type in %s.\n", str));
   LERROR(LEUNKTYPE);
   DRETURN(-1);
   /* abort(); */
}

/****** cull/multitype/lGetPosViaElem() ****************************************
*  NAME
*     lGetPosViaElem() -- Get Position of name within element 
*
*  SYNOPSIS
*     int lGetPosViaElem(const lListElem *element, int name, int do_abort) 
*
*  FUNCTION
*     Get Position of field 'name' within 'element' 
*
*  INPUTS
*     const lListElem *element - element 
*     int name                 - field name id 
*     int do_abort             - call do_abort if do_abort=1
*
*  RESULT
*     int - position or -1 in case of an error
*******************************************************************************/
int lGetPosViaElem(const lListElem *element, int name, int do_abort) {
   int pos = -1;

   DENTER(CULL_BASIS_LAYER);

   if (!element) {
      if (do_abort) {
         CRITICAL(MSG_CULL_POINTER_NULLELEMENTFORX_S, lNm2Str(name));
         abort();
      }
      DRETURN(-1);
   }
   pos = lGetPosInDescr(element->descr, name);

   if (do_abort && (pos < 0)) {
      /* someone has called lGetPosViaElem() with invalid name */
      CRITICAL(MSG_CULL_XNOTFOUNDINELEMENT_S, lNm2Str(name));
      abort();
   }

   DRETURN(pos);
}

/****** cull/multitype/lMt2Str() **********************************************
*  NAME
*     lMt2Str() -- returns the string representation of a type id
*
*  SYNOPSIS
*     char* lMt2Str(int mt) 
*
*  FUNCTION
*     returns the string representation of a type id
*
*  INPUTS
*     int mt - multitype id (e.g. lStringT)
*
*  RESULT
*     char* - string representation of mt 
*  
******************************************************************************/
const char *lMt2Str(int mt) {
   if (mt >= 0 && mt < (int) (sizeof(multitypes) / sizeof(char *))) {
      return multitypes[mt];
   } else {
      return "unknown multitype";
   }
}

/****** cull/multitype/lNm2Str() **********************************************
*  NAME
*     lNm2Str() -- returns the string representation of a name id
*
*  SYNOPSIS
*     char* lNm2Str(int nm) 
*
*  FUNCTION
*     returns the string representation of a name id
*
*  INPUTS
*     int nm - name id (e.g. CONF_name)
*
*  RESULT
*     char* - string representation of id
*  
*  NOTE
*     JG: TODO: Implementation is not really efficient.
*               Could be improved by using a hash table that will be 
*               dynamically built as names are looked up.
******************************************************************************/
const char *lNm2Str(int nm) {
   const lNameSpace *nsp;
   char stack_noinit[50];
   const char *cp;
   const lNameSpace *ns;

   DENTER(CULL_BASIS_LAYER);

   if (!(ns = cull_state_get_name_space())) {
      DPRINTF(("name vector uninitialized !!\n"));
      goto Error;
   }

   for (nsp = ns; nsp->lower; nsp++) {
      if ((cp = _lNm2Str(nsp, nm))) {
         DRETURN(cp);
      }
   }

   Error:
   snprintf(stack_noinit, sizeof(stack_noinit), "Nameindex = %d", nm);
   cull_state_set_noinit(stack_noinit);
   LERROR(LENAMENOT);
   DRETURN(cull_state_get_noinit());
}

static const char *_lNm2Str(const lNameSpace *nsp, int nm) {
   DENTER(CULL_BASIS_LAYER);

   if (!nsp) {
      DPRINTF(("name vector uninitialized !!\n"));
      DRETURN(nullptr);
   }

   if (nm >= nsp->lower && nm < (nsp->lower + nsp->size)) {
      DRETURN(nsp->namev[nm - nsp->lower]);
   }

   DRETURN(nullptr);
}

/****** cull/multitype/lStr2Nm() **********************************************
*  NAME
*     lStr2Nm() -- Returns the int representation of a name 
*
*  SYNOPSIS
*     int lStr2Nm(const char *str) 
*
*  FUNCTION
*     Returns the int representation of a name 
*
*  INPUTS
*     const char *str - String 
*
*  RESULT
*     int - value
*
*  NOTE
*     JG: TODO: Highly inefficient implementation, does tons of strcmp.
*               Should have a hash table that will be extended whenever
*               a new name has to be resolved.
******************************************************************************/
int lStr2Nm(const char *str) {
   const lNameSpace *nsp, *ns;
   int ret;

   DENTER(CULL_BASIS_LAYER);

   if (!(ns = cull_state_get_name_space())) {
      DPRINTF(("name vector uninitialized !!\n"));
      DRETURN(NoName);
   }

   for (nsp = ns; nsp->lower; nsp++) {
      if ((ret = _lStr2Nm(nsp, str)) != NoName) {
         DPRINTF(("Name: %s Id: %d\n", str, ret));
         DRETURN(ret);
      }
   }

   LERROR(LENAMENOT);
   DRETURN(NoName);
}

static int _lStr2Nm(const lNameSpace *nsp, const char *str) {
   int i;
   int ret = NoName;
   int found = 0;

   DENTER(CULL_BASIS_LAYER);

   if (nsp) {
      for (i = 0; i < nsp->size; i++) {
         DPRINTF(("%d: %s\n", nsp->namev[i]));
         if (!strcmp(nsp->namev[i], str)) {
            found = 1;
            break;
         }
      }

      if (found)
         ret = nsp->lower + i;
      else
         ret = NoName;
   }

   DRETURN(ret);
}

/****** cull/multitype/lInit() ************************************************
*  NAME
*     lInit() -- Initialize the mechanism for lNm2Str() 
*
*  SYNOPSIS
*     void lInit(const lNameSpace *namev) 
*
*  FUNCTION
*     Initialize the mechanism for lNm2Str() 
*
*  INPUTS
*     const lNameSpace *namev - Namespace 
*******************************************************************************/
void lInit(const lNameSpace *namev) {
   cull_state_set_name_space(namev);
#ifdef OBSERVE
   lObserveInit();
#endif
}

/****** cull/multitype/lCountDescr() ****************************************
*  NAME
*     lCountDescr() -- Returns the size of a descriptor 
*
*  SYNOPSIS
*     int lCountDescr(const lDescr *dp) 
*
*  FUNCTION
*     Returns the size of a descriptor excluding lEndT Descr. 
*
*  INPUTS
*     const lDescr *dp - pointer to descriptor 
*
*  RESULT
*     int - size or -1 on error 
*
*  NOTES
*     MT-NOTE: lCountDescr() is MT safe
******************************************************************************/
int lCountDescr(const lDescr *dp) {
   const lDescr *p;

   DENTER(CULL_BASIS_LAYER);

   if (!dp) {
      LERROR(LEDESCRNULL);
      DRETURN(-1);
   }

   p = &dp[0];
   while (mt_get_type(p->mt) != lEndT)
      p++;

   DRETURN((p - &dp[0]));
}

/****** cull/multitype/lCopyDescr() *******************************************
*  NAME
*     lCopyDescr() -- Copys a descriptor 
*
*  SYNOPSIS
*     lDescr* lCopyDescr(const lDescr *dp) 
*
*  FUNCTION
*     Returns a pointer to a copied descriptor, has to be freed by 
*     the user. 
*
*  INPUTS
*     const lDescr *dp - descriptor 
*
*  RESULT
*     lDescr* - descriptor pointer or nullptr in case of error
******************************************************************************/
lDescr *lCopyDescr(const lDescr *dp) {
   int i;
   lDescr *new_descr = nullptr;

   DENTER(CULL_BASIS_LAYER);

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
   DPRINTF(("lCopyDescr failed\n"));
   DRETURN(nullptr);
}

/****** cull/multitype/lWriteDescrTo() ****************************************
*  NAME
*     lWriteDescrTo() -- Writes a descriptor (for debugging purpose) 
*
*  SYNOPSIS
*     void lWriteDescrTo(const lDescr *dp, FILE *fp) 
*
*  FUNCTION
*     Writes a descriptor (for debugging purpose) 
*
*  INPUTS
*     const lDescr *dp - descriptor 
*     FILE *fp         - output stream 
******************************************************************************/
void lWriteDescrTo(const lDescr *dp, FILE *fp) {
   int i;

   DENTER(CULL_LAYER);

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
         DPRINTF((format, dp[i].nm, lNm2Str(dp[i].nm), dp[i].mt, do_hash, is_hash));
      } else {
         fprintf(fp, format, dp[i].nm, lNm2Str(dp[i].nm), dp[i].mt, do_hash, is_hash);
      }
   }

   DRETURN_VOID;
}

/****** cull/multitype/_lGetPosInDescr() ***************************************
*  NAME
*     _lGetPosInDescr() -- Returns position of a name in a descriptor 
*
*  SYNOPSIS
*     int _lGetPosInDescr(const lDescr *dp, int name) 
*
*  FUNCTION
*     Returns position of a name in a descriptor array. Does a full search
*     in the descriptor even if the element is not a reduced element.
*
*  INPUTS
*     const lDescr *dp - descriptor 
*     int name         - namse 
*
*  RESULT
*     int - position or -1 if not found 
******************************************************************************/
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

/****** cull/multitype/lGetPosInDescr() ***************************************
*  NAME
*     lGetPosInDescr() -- Returns position of a name in a descriptor 
*
*  SYNOPSIS
*     int lGetPosInDescr(const lDescr *dp, int name) 
*
*  FUNCTION
*     Returns position of a name in a descriptor array 
*
*  INPUTS
*     const lDescr *dp - descriptor 
*     int name         - namse 
*
*  RESULT
*     int - position or -1 if not found 
******************************************************************************/
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

/****** cull/multitype/lGetPosType() ****************************************
*  NAME
*     lGetPosType() -- Returns type at position
*
*  SYNOPSIS
*     int lGetPosType(const lDescr *dp, int pos) 
*
*  FUNCTION
*     Returns the type at specified position in a descriptor array. The
*     Position must be inside the valid range of the descriptor. Returns
*     NoName if descriptor is nullptr or pos < 0.
*
*  INPUTS
*     const lDescr *dp - Descriptor 
*     int pos          - Position 
*
*  RESULT
*     int - Type 
******************************************************************************/
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

lList **lGetListRef(const lListElem *ep, int name) {
   int pos;

   DENTER(CULL_BASIS_LAYER);

   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lListT)
      incompatibleType("lGetPosListRef");

   DRETURN(&(ep->cont[pos].glp));
}

char **lGetPosStringRef(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (mt_get_type(ep->descr[pos].mt) != lStringT)
      incompatibleType("lGetPosStringRef");

   DRETURN(&(ep->cont[pos].str));
}

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

/****** cull/multitype/lGetPosInt() *******************************************
*  NAME
*     lGetPosInt() -- Returns the int value at position  
*
*  SYNOPSIS
*     lInt lGetPosInt(const lListElem *ep, int pos) 
*
*  FUNCTION
*     Returns the int value at position 'pos' 
*
*  INPUTS
*     const lListElem *ep - element pointer 
*     int pos             - position id 
*
*  RESULT
*     lInt - int
******************************************************************************/
lInt lGetPosInt(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (mt_get_type(ep->descr[pos].mt) != lIntT)
      incompatibleType("lGetPosInt");

   DRETURN((lInt) ep->cont[pos].i);
}

/****** cull/multitype/lGetInt() **********************************************
*  NAME
*     lGetInt() -- Returns the int value for field name 
*
*  SYNOPSIS
*     lInt lGetInt(const lListElem *ep, int name) 
*
*  FUNCTION
*     Returns the int value for field name 
*
*  INPUTS
*     const lListElem *ep - element 
*     int name            - field name id 
*
*  RESULT
*     lInt - int 
******************************************************************************/
lInt lGetInt(const lListElem *ep, int name) {
   int pos;
   DENTER(CULL_BASIS_LAYER);

   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lIntT)
      incompatibleType2(MSG_CULL_GETINT_WRONGTYPEFORFIELDXY_SS,
                        lNm2Str(name), multitypes[mt_get_type(ep->descr[pos].mt)]);

   DRETURN((lInt) ep->cont[pos].i);
}

/****** cull/multitype/lGetPosUlong() ****************************************
*  NAME
*     lGetPosUlong() -- Returns the ulong value at position pos 
*
*  SYNOPSIS
*     lUlong lGetPosUlong(const lListElem *ep, int pos) 
*
*  FUNCTION
*     Returns the ulong value at position pos 
*
*  INPUTS
*     const lListElem *ep - element 
*     int pos             - pos value 
*
*  RESULT
*     lUlong - ulong
******************************************************************************/
lUlong lGetPosUlong(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (pos < 0) {
      /* someone has called lGetPosUlong() */
      /* makro with an invalid nm        */
      CRITICAL(SFNMAX, MSG_CULL_GETPOSULONG_GOTINVALIDPOSITION);
      abort();
   }

   if (mt_get_type(ep->descr[pos].mt) != lUlongT)
      incompatibleType("lGetPosUlong");
   DRETURN((lUlong) ep->cont[pos].ul);
}

/****** cull/multitype/lGetUlong() ********************************************
*  NAME
*     lGetUlong() -- Return 'u_long32' value for specified fieldname 
*
*  SYNOPSIS
*     lUlong lGetUlong(const lListElem *ep, int name) 
*
*  FUNCTION
*     Return the content of the field specified by fieldname 'name' of 
*     list element 'ep'. The type of the field 'name' has to be of
*     type 'u_long32'.
*
*  INPUTS
*     const lListElem *ep - Pointer to list element 
*     int name            - field name 
*
*  RESULT
*     lUlong - u_long32 value
******************************************************************************/
lUlong lGetUlong(const lListElem *ep, int name) {
   int pos;
   DENTER(CULL_BASIS_LAYER);

   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lUlongT)
      incompatibleType2(MSG_CULL_GETULONG_WRONGTYPEFORFIELDXY_SS,
                        lNm2Str(name), multitypes[mt_get_type(ep->descr[pos].mt)]);

   DRETURN((lUlong) ep->cont[pos].ul);
}

/****** cull/multitype/lGetPosUlong64() **************************************
*  NAME
*     lGetPosUlong64() -- Returns the ulong64 value at position pos 
*
*  SYNOPSIS
*     lUlong64 lGetPosUlong64(const lListElem *ep, int pos) 
*
*  FUNCTION
*     Returns the ulong64 value at position pos 
*
*  INPUTS
*     const lListElem *ep - element 
*     int pos             - pos value 
*
*  RESULT
*     lUlong64 - ulong64
******************************************************************************/
lUlong64 lGetPosUlong64(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (pos < 0) {
      /* someone has called lGetPosUlong64() */
      /* makro with an invalid nm        */
      CRITICAL(SFNMAX, MSG_CULL_GETPOSULONG64_GOTINVALIDPOSITION);
      abort();
   }

   if (mt_get_type(ep->descr[pos].mt) != lUlong64T)
      incompatibleType("lGetPosUlong64");
   DRETURN((lUlong64) ep->cont[pos].ul64);
}

/****** cull/multitype/lGetUlong64() ******************************************
*  NAME
*     lGetUlong64() -- Return 'u_long64' value for specified fieldname 
*
*  SYNOPSIS
*     lUlong64 lGetUlong64(const lListElem *ep, int name) 
*
*  FUNCTION
*     Return the content of the field specified by fieldname 'name' of 
*     list element 'ep'. The type of the field 'name' has to be of
*     type 'u_long64'.
*
*  INPUTS
*     const lListElem *ep - Pointer to list element 
*     int name            - field name 
*
*  RESULT
*     lUlong64 - u_long64 value
******************************************************************************/
lUlong64 lGetUlong64(const lListElem *ep, int name) {
   int pos;
   DENTER(CULL_BASIS_LAYER);

   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lUlong64T)
      incompatibleType2(MSG_CULL_GETULONG64_WRONGTYPEFORFIELDXY_SS,
                        lNm2Str(name), multitypes[mt_get_type(ep->descr[pos].mt)]);

   DRETURN((lUlong64) ep->cont[pos].ul64);
}

/****** cull/multitype/lGetPosString() ****************************************
*  NAME
*     lGetPosString() -- Returns the string ptr value at position pos 
*
*  SYNOPSIS
*     const char* lGetPosString(const lListElem *ep, int pos) 
*
*  FUNCTION
*     Returns the char* value at position pos (runtime type checking) 
*
*  INPUTS
*     const lListElem *ep - element 
*     int pos             - pos value 
*
*  RESULT
*     const char* - string pointer 
*******************************************************************************/
const char *lGetPosString(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (pos < 0) {
      /* someone has called lGetString() */
      /* makro with an invalid nm        */
      DPRINTF(("!!!!!!!!!!!! lGetPosString() got an invalid pos !!!!!!!!!\n"));
      DRETURN(nullptr);
   }

   if (mt_get_type(ep->descr[pos].mt) != lStringT)
      incompatibleType("lGetPosString");

   DRETURN((lString) ep->cont[pos].str);
}

/****** cull/multitype/lGetPosHost() ******************************************
*  NAME
*     lGetPosHost() -- Returns the hostname value at position pos 
*
*  SYNOPSIS
*     const char* lGetPosHost(const lListElem *ep, int pos) 
*
*  FUNCTION
*     Returns the hostname value at position pos 
*
*  INPUTS
*     const lListElem *ep - element 
*     int pos             - position 
*
*  RESULT
*     const char* - Hostname  
******************************************************************************/
const char *lGetPosHost(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (pos < 0) {
      /* someone has called lGetString() */
      /* makro with an invalid nm        */
      DPRINTF(("!!!!!!!!!!!!!! lGetPosHost() got an invalid pos !!!!!!!!!\n"));
      DRETURN(nullptr);
   }

   if (mt_get_type(ep->descr[pos].mt) != lHostT)
      incompatibleType("lGetPosHost");
   DRETURN((lHost) ep->cont[pos].host);
}

/****** cull/multitype/lGetType() *********************************************
*  NAME
*     lGetType() -- Return type of field within descriptor 
*
*  SYNOPSIS
*     int lGetType(const lDescr *dp, int nm) 
*
*  FUNCTION
*     Return type of field within descriptor.
*
*  INPUTS
*     const lDescr *dp - descriptor 
*     int nm           - field name id 
*
*  RESULT
*     int - Type id or lEndT
******************************************************************************/
int lGetType(const lDescr *dp, int nm) {
   int pos;

   DENTER(CULL_BASIS_LAYER);

   pos = lGetPosInDescr(dp, nm);
   if (pos < 0) {
      DRETURN(lEndT);
   }

   DRETURN(mt_get_type(dp[pos].mt));
}

/****** cull/multitype/lGetString() ********************************************
*  NAME
*     lGetString() -- Return string for specified fieldname 
*
*  SYNOPSIS
*     const char *lGetString(const lListElem *ep, int name) 
*
*  FUNCTION
*     Return the content of the field specified by fieldname 'name' of 
*     list element 'ep'. The type of the field 'name' has to be of
*     type string.
*
*  INPUTS
*     const lListElem *ep - Pointer to list element 
*     int name            - field name 
*
*  RESULT
*     const char* - string pointer (no copy) 
******************************************************************************/
const char *lGetString(const lListElem *ep, int name) {
   int pos;
   DENTER(CULL_BASIS_LAYER);

   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lStringT) {
      incompatibleType2(MSG_CULL_GETSTRING_WRONGTYPEFORFILEDXY_SS,
                        lNm2Str(name), multitypes[mt_get_type(ep->descr[pos].mt)]);
   }

   DRETURN((lString) ep->cont[pos].str);
}

/****** cull/multitype/lGetHost() **********************************************
*  NAME
*     lGetHost() -- Return hostname string for specified field 
*
*  SYNOPSIS
*     const char* lGetHost(const lListElem *ep, int name) 
*
*  FUNCTION
*     This procedure returns the hostname string for the field name, 
*     but doesn't copy the string (runtime type checking)
*
*  INPUTS
*     const lListElem *ep - list element pointer
*     int name            - name of list element
*
*  RESULT
*     const char* - value of list entry 
*******************************************************************************/
const char *lGetHost(const lListElem *ep, int name) {
   int pos;
   DENTER(CULL_BASIS_LAYER);

   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lHostT)
      incompatibleType2(MSG_CULL_GETHOST_WRONGTYPEFORFILEDXY_SS,
                        lNm2Str(name), multitypes[mt_get_type(ep->descr[pos].mt)]);

   DRETURN((lHost) ep->cont[pos].host);
}

/****** cull/multitype/lGetPosObject() ******************************************
*  NAME
*     lGetPosObject() -- Returns the CULL object at position pos (no copy) 
*
*  SYNOPSIS
*     lList* lGetPosObject(const lListElem *ep, int pos) 
*
*  FUNCTION
*     Returns the CULL object (list element) at position pos (no copy) 
*
*  INPUTS
*     const lListElem *ep - element 
*     int pos             - pos value 
*
*  RESULT
*     lListElem* - CULL list element pointer
******************************************************************************/
lListElem *lGetPosObject(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (pos < 0) {
      /* someone has called lGetPosUlong() */
      /* makro with an invalid nm        */
      CRITICAL(SFNMAX, MSG_CULL_GETPOSOBJECT_GOTANINVALIDPOS);
      abort();
   }

   if (mt_get_type(ep->descr[pos].mt) != lObjectT)
      incompatibleType("lGetPosObject");

   DRETURN((lListElem *) ep->cont[pos].obj);
}

/****** cull/multitype/lGetPosList() ******************************************
*  NAME
*     lGetPosList() -- Returns the CULL list at position pos (no copy) 
*
*  SYNOPSIS
*     lList* lGetPosList(const lListElem *ep, int pos) 
*
*  FUNCTION
*     Returns the CULL list at position pos (no copy) 
*
*  INPUTS
*     const lListElem *ep - element 
*     int pos             - pos value 
*
*  RESULT
*     lList* - CULL list pointer
******************************************************************************/
lList *lGetPosList(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (pos < 0) {
      /* someone has called lGetPosUlong() */
      /* makro with an invalid nm        */
      CRITICAL(SFNMAX, MSG_CULL_GETPOSLIST_GOTANINVALIDPOS);
      abort();
   }

   if (mt_get_type(ep->descr[pos].mt) != lListT)
      incompatibleType("lGetPosList");

   DRETURN((lList *) ep->cont[pos].glp);
}

/****** cull/multitype/lGetObject() *********************************************
*  NAME
*     lGetObject() -- Returns the CULL object for a field name 
*
*  SYNOPSIS
*     lListElem* lGetObject(const lListElem *ep, int name) 
*
*  FUNCTION
*     Returns the CULL object for a field name 
*
*  INPUTS
*     const lListElem *ep - element 
*     int name            - field name value 
*
*  RESULT
*     lListElem* - CULL list element pointer 
******************************************************************************/
lListElem *lGetObject(const lListElem *ep, int name) {
   int pos;
   DENTER(CULL_BASIS_LAYER);

   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lObjectT)
      incompatibleType2(MSG_CULL_GETOBJECT_WRONGTYPEFORFIELDXY_SS,
                        lNm2Str(name), multitypes[mt_get_type(ep->descr[pos].mt)]);
   DRETURN((lListElem *) ep->cont[pos].obj);
}

/****** cull/multitype/lGetList() *********************************************
*  NAME
*     lGetList() -- Returns the CULL list for a field name 
*
*  SYNOPSIS
*     lList* lGetList(const lListElem *ep, int name) 
*
*  FUNCTION
*     Returns the CULL list for a field name 
*
*  INPUTS
*     const lListElem *ep - element 
*     int name            - field name value 
*
*  RESULT
*     lList* - CULL list pointer 
******************************************************************************/
lList *lGetListRW(const lListElem *ep, int name) {
   int pos;
   DENTER(CULL_BASIS_LAYER);

   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lListT) {
      incompatibleType2(MSG_CULL_GETLIST_WRONGTYPEFORFIELDXY_SS,
                        lNm2Str(name), multitypes[mt_get_type(ep->descr[pos].mt)]);
   }

   DRETURN((lList *) ep->cont[pos].glp);
}

const lList *lGetList(const lListElem *ep, int nm) {
   return lGetListRW(ep, nm);
}

/****** cull/multitype/lGetOrCreateList() **************************************
*  NAME
*     lGetOrCreateList() -- Returns the CULL list for a field name
*
*  SYNOPSIS
*     lList* 
*     lGetOrCreateList(lListElem *ep, int name, const char *list_name, 
*                      const lDescr *descr) 
*
*  FUNCTION
*     Returns the CULL list for a field name.
*     If the list does not yet exist, create it.
*
*  INPUTS
*     lListElem *ep         - element
*     int name              - field name value
*     const char *list_name - list name for list creation
*     const lDescr *descr   - descriptor for list creation
*
*  RESULT
*     lList* - CULL list pointer
*
*  NOTES
*     MT-NOTE: lGetOrCreateList() is MT safe 
*
*  SEE ALSO
*     cull/multitype/lGetList()
*     cull/list/lCreateList()
*******************************************************************************/
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

/****** cull/multitype/lGetPosFloat() *****************************************
*  NAME
*     lGetPosFloat() -- Returns the float value at position pos 
*
*  SYNOPSIS
*     lFloat lGetPosFloat(const lListElem *ep, int pos) 
*
*  FUNCTION
*     Returns the float value at position pos  
*
*  INPUTS
*     const lListElem *ep - element 
*     int pos             - position 
*
*  RESULT
*     lFloat - float 
******************************************************************************/
lFloat lGetPosFloat(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);
   if (mt_get_type(ep->descr[pos].mt) != lFloatT)
      incompatibleType("lGetPosFloat");
   DRETURN(ep->cont[pos].fl);
}

/****** cull/multitype/lGetFloat() ********************************************
*  NAME
*     lGetFloat() -- Returns float value for field name 
*
*  SYNOPSIS
*     lFloat lGetFloat(const lListElem *ep, int name) 
*
*  FUNCTION
*     Returns float value for field name 
*
*  INPUTS
*     const lListElem *ep - element 
*     int name            - field name  
*
*  RESULT
*     lFloat - float
******************************************************************************/
lFloat lGetFloat(const lListElem *ep, int name) {
   int pos;
   DENTER(CULL_BASIS_LAYER);

   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lFloatT)
      incompatibleType2(MSG_CULL_GETFLOAT_WRONGTYPEFORFIELDXY_SS, lNm2Str(name),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
   DRETURN(ep->cont[pos].fl);
}

/****** cull/multitype/lGetPosDouble() ****************************************
*  NAME
*     lGetPosDouble() -- Returns a double value at pos
*
*  SYNOPSIS
*     lDouble lGetPosDouble(const lListElem *ep, int pos) 
*
*  FUNCTION
*     Returns a double value at pos 
*
*  INPUTS
*     const lListElem *ep - element 
*     int pos             - pos 
*
*  RESULT
*     lDouble - double value 
*******************************************************************************/
lDouble lGetPosDouble(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);
   if (mt_get_type(ep->descr[pos].mt) != lDoubleT)
      incompatibleType("lGetPosDouble");
   DRETURN(ep->cont[pos].db);
}

/****** cull/multitype/lGetDouble() *******************************************
*  NAME
*     lGetDouble() -- Returns the double value for field name 
*
*  SYNOPSIS
*     lDouble lGetDouble(const lListElem *ep, int name) 
*
*  FUNCTION
*     Returns the double value for field name 
*
*  INPUTS
*     const lListElem *ep - element 
*     int name            - field name value 
*
*  RESULT
*     lDouble - double value 
******************************************************************************/
lDouble lGetDouble(const lListElem *ep, int name) {
   int pos;
   DENTER(CULL_BASIS_LAYER);

   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lDoubleT)
      incompatibleType2(MSG_CULL_GETDOUBLE_WRONGTYPEFORFIELDXY_SS, lNm2Str(name),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
   DRETURN(ep->cont[pos].db);
}

/****** cull/multitype/lGetPosLong() ****************************************
*  NAME
*     lGetPosLong() -- Returns the long value at position pos 
*
*  SYNOPSIS
*     lLong lGetPosLong(const lListElem *ep, int pos) 
*
*  FUNCTION
*     Returns the long value at position pos 
*
*  INPUTS
*     const lListElem *ep - element 
*     int pos             - position 
*
*  RESULT
*     lLong - long 
*******************************************************************************/
lLong lGetPosLong(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);
   if (mt_get_type(ep->descr[pos].mt) != lLongT)
      incompatibleType("lGetPosLong");
   DRETURN(ep->cont[pos].l);
}

/****** cull/multitype/lGetLong() *********************************************
*  NAME
*     lGetLong() -- Returns the long value for a field name 
*
*  SYNOPSIS
*     lLong lGetLong(const lListElem *ep, int name) 
*
*  FUNCTION
*     Returns the long value for a field name 
*
*  INPUTS
*     const lListElem *ep - element 
*     int name            - name 
*
*  RESULT
*     lLong - long 
******************************************************************************/
lLong lGetLong(const lListElem *ep, int name) {
   int pos;
   DENTER(CULL_BASIS_LAYER);
   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lLongT)
      incompatibleType2(MSG_CULL_GETLONG_WRONGTYPEFORFIELDXY_SS, lNm2Str(name),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
   DRETURN(ep->cont[pos].l);
}

/****** cull/multitype/lGetPosBool() ******************************************
*  NAME
*     lGetPosBool() -- Returns the boolean value at position pos 
*
*  SYNOPSIS
*     lChar lGetPosBool(const lListElem *ep, int pos) 
*
*  FUNCTION
*     Returns the boolean value at position pos 
*
*  INPUTS
*     const lListElem *ep - element 
*     int pos             - position 
*
*  RESULT
*     lBool - boolean 
******************************************************************************/
lBool lGetPosBool(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (mt_get_type(ep->descr[pos].mt) != lBoolT)
      incompatibleType("lGetPosBool");
   DRETURN(ep->cont[pos].b);
}

/****** cull/multitype/lGetBool() *********************************************
*  NAME
*     lGetBool() -- Returns the boolean value for a field name 
*
*  SYNOPSIS
*     lBool lGetBool(const lListElem *ep, int name) 
*
*  FUNCTION
*     Returns the boolean value for a field name 
*
*  INPUTS
*     const lListElem *ep - element 
*     int name            - field name 
*
*  RESULT
*     lBool - boolean
******************************************************************************/
lBool lGetBool(const lListElem *ep, int name) {
   int pos;
   DENTER(CULL_BASIS_LAYER);
   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lBoolT)
      incompatibleType2(MSG_CULL_GETBOOL_WRONGTYPEFORFIELDXY_SS, lNm2Str(name),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
   DRETURN(ep->cont[pos].b);
}

/****** cull/multitype/lGetPosChar() ******************************************
*  NAME
*     lGetPosChar() -- Returns the char value at position pos 
*
*  SYNOPSIS
*     lChar lGetPosChar(const lListElem *ep, int pos) 
*
*  FUNCTION
*     Returns the char value at position pos 
*
*  INPUTS
*     const lListElem *ep - element 
*     int pos             - position 
*
*  RESULT
*     lChar - character 
******************************************************************************/
lChar lGetPosChar(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);

   if (mt_get_type(ep->descr[pos].mt) != lCharT)
      incompatibleType("lGetPosChar");
   DRETURN(ep->cont[pos].c);
}

/****** cull/multitype/lGetChar() *********************************************
*  NAME
*     lGetChar() -- Returns the char value for a field name 
*
*  SYNOPSIS
*     lChar lGetChar(const lListElem *ep, int name) 
*
*  FUNCTION
*     Returns the char value for a field name 
*
*  INPUTS
*     const lListElem *ep - element 
*     int name            - field name 
*
*  RESULT
*     lChar - character
******************************************************************************/
lChar lGetChar(const lListElem *ep, int name) {
   int pos;
   DENTER(CULL_BASIS_LAYER);
   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lCharT)
      incompatibleType2(MSG_CULL_GETCHAR_WRONGTYPEFORFIELDXY_SS, lNm2Str(name),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
   DRETURN(ep->cont[pos].c);
}

/****** cull/multitype/lGetPosRef() *******************************************
*  NAME
*     lGetPosRef() -- Returns the reference at position pos 
*
*  SYNOPSIS
*     lRef lGetPosRef(const lListElem *ep, int pos) 
*
*  FUNCTION
*     Returns the reference at position pos 
*
*  INPUTS
*     const lListElem *ep - element 
*     int pos             - position 
*
*  RESULT
*     lRef - reference (pointer) 
******************************************************************************/
lRef lGetPosRef(const lListElem *ep, int pos) {
   DENTER(CULL_BASIS_LAYER);
   if (mt_get_type(ep->descr[pos].mt) != lRefT) {
      incompatibleType("lGetPosRef");
   }
   DRETURN(ep->cont[pos].ref);
}

/****** cull/multitype/lGetRef() **********************************************
*  NAME
*     lGetRef() -- Returns the character for a field name 
*
*  SYNOPSIS
*     lRef lGetRef(const lListElem *ep, int name) 
*
*  FUNCTION
*     Returns the character for a field name 
*
*  INPUTS
*     const lListElem *ep - element 
*     int name            - field name value 
*
*  RESULT
*     lRef - reference 
******************************************************************************/
lRef lGetRef(const lListElem *ep, int name) {
   int pos;
   DENTER(CULL_BASIS_LAYER);

   pos = lGetPosViaElem(ep, name, SGE_DO_ABORT);

   if (mt_get_type(ep->descr[pos].mt) != lRefT)
      incompatibleType2(MSG_CULL_GETREF_WRONGTYPEFORFIELDXY_SS, lNm2Str(name),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
   DRETURN(ep->cont[pos].ref);
}

/****** cull/multitype/lSetPosInt() ****************************************
*  NAME
*     lSetPosInt() -- Sets the int value 
*
*  SYNOPSIS
*     int lSetPosInt(lListElem *ep, int pos, int value) 
*
*  FUNCTION
*     Sets in the element 'ep' at position 'pos' the int 'value' 
*
*  INPUTS
*     lListElem *ep - element 
*     int pos             - position 
*     int value           - value 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error
******************************************************************************/
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

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/****** cull/multitype/lSetInt() **********************************************
*  NAME
*     lSetInt() -- Sets an int within an element 
*
*  SYNOPSIS
*     int lSetInt(lListElem *ep, int name, int value) 
*
*  FUNCTION
*     Sets an int within an element 
*
*  INPUTS
*     lListElem *ep - element 
*     int name      - field name id 
*     int value     - new value 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error
******************************************************************************/
int lSetInt(lListElem *ep, int name, int value) {
   int pos;
   DENTER(CULL_BASIS_LAYER);

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

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/****** cull/multitype/lSetPosUlong() *****************************************
*  NAME
*     lSetPosUlong() -- Get ulong at a certain position 
*
*  SYNOPSIS
*     int lSetPosUlong(lListElem *ep, int pos, lUlong value) 
*
*  FUNCTION
*     Get ulong at a certain position 
*
*  INPUTS
*     lListElem *ep - element 
*     int pos             - position 
*     lUlong value        - new value 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error
*******************************************************************************/
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

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/****** cull/multitype/lSetUlong() ********************************************
*  NAME
*     lSetUlong() -- Set ulong value at the given field name id 
*
*  SYNOPSIS
*     int lSetUlong(lListElem *ep, int name, lUlong value) 
*
*  FUNCTION
*     Set ulong value at the given field name id 
*
*  INPUTS
*     lListElem *ep - element 
*     int name      - field name id 
*     lUlong value  - new value 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error
******************************************************************************/
int lSetUlong(lListElem *ep, int name, lUlong value) {
   int pos;

   DENTER(CULL_BASIS_LAYER);

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

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/****** cull_multitype/lAddUlong() *********************************************
*  NAME
*     lAddUlong() -- Adds a lUlong offset to the lUlong field
*
*  SYNOPSIS
*     int lAddUlong(lListElem *ep, int name, lUlong offset) 
*
*  FUNCTION
*     The 'offset' is added to the lUlong field 'name' of
*     the CULL element 'ep'.
*
*  INPUTS
*     lListElem *ep - element
*     int name      - field name id
*     lUlong offset - the offset
*
*  RESULT
*     int - 
*
*  EXAMPLE
*     int - error state
*         0 - OK
*        -1 - Error
*******************************************************************************/
int lAddUlong(lListElem *ep, int name, lUlong offset) {
   int pos;

   DENTER(CULL_BASIS_LAYER);

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

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/****** cull/multitype/lSetUlong64() ******************************************
*  NAME
*     lSetUlong64() -- Set ulong value at the given field name id 
*
*  SYNOPSIS
*     int lSetUlong64(lListElem *ep, int name, lUlong64 value) 
*
*  FUNCTION
*     Set ulong64 value at the given field name id 
*
*  INPUTS
*     lListElem *ep  - element 
*     int name       - field name id 
*     lUlong64 value - new value 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error
******************************************************************************/
int lSetUlong64(lListElem *ep, int name, lUlong64 value) {
   int pos;

   DENTER(CULL_BASIS_LAYER);

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

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/****** cull_multitype/lAddUlong64() *******************************************
*  NAME
*     lAddUlong64() -- Adds a lUlong64 offset to the lUlong64 field
*
*  SYNOPSIS
*     int lAddUlong64(lListElem *ep, int name, lUlong64 offset) 
*
*  FUNCTION
*     The 'offset' is added to the lUlong64 field 'name' of
*     the CULL element 'ep'.
*
*  INPUTS
*     lListElem *ep   - element
*     int name        - field name id
*     lUlong64 offset - the offset
*
*  RESULT
*     int - 
*
*  EXAMPLE
*     int - error state
*         0 - OK
*        -1 - Error
*******************************************************************************/
int lAddUlong64(lListElem *ep, int name, lUlong64 offset) {
   int pos;

   DENTER(CULL_BASIS_LAYER);

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

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/****** cull/multitype/lSetPosUlong64() ***************************************
*  NAME
*     lSetPosUlong64() -- Get ulong64 at a certain position 
*
*  SYNOPSIS
*     int lSetPosUlong64(lListElem *ep, int pos, lUlong64 value) 
*
*  FUNCTION
*     Get ulong64 at a certain position 
*
*  INPUTS
*     lListElem *ep - element 
*     int pos             - position 
*     lUlong64 value      - new value 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error
*******************************************************************************/
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

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/****** cull/multitype/lSetPosString() ***************************************
*  NAME
*     lSetPosString() -- Sets the string at a certain position 
*
*  SYNOPSIS
*     int lSetPosString(lListElem *ep, int pos, const char *value) 
*
*  FUNCTION
*     Sets the string at a certain position. 
*
*  INPUTS
*     lListElem *ep - element 
*     int pos             - position 
*     const char *value   - string value 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error 
******************************************************************************/
int lSetPosString(lListElem *ep, int pos, const char *value) {
   char *str = nullptr;
   int changed;

   DENTER(CULL_BASIS_LAYER);

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

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/****** cull/multitype/lSetPosHost() ******************************************
*  NAME
*     lSetPosHost() -- Sets the hostname at a certain position
*
*  SYNOPSIS
*     int lSetPosHost(lListElem *ep, int pos, const char *value) 
*
*  FUNCTION
*     Sets the hostname at a certain position 
*
*  INPUTS
*     lListElem *ep - element 
*     int pos             - position 
*     const char *value   - new hostname 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error
*******************************************************************************/
int lSetPosHost(lListElem *ep, int pos, const char *value) {
   char *str = nullptr;
   int changed;

   DENTER(CULL_BASIS_LAYER);

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
         char host_key[CL_MAXHOSTLEN + 1];
         cull_hash_insert(ep, cull_hash_key(ep, pos, host_key),
                          ep->descr[pos].ht, mt_is_unique(ep->descr[pos].mt));
      }

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/****** cull/multitype/lSetString() *******************************************
*  NAME
*     lSetString() -- Sets the string at the given field name id 
*
*  SYNOPSIS
*     int lSetString(lListElem *ep, int name, const char *value) 
*
*  FUNCTION
*     Sets the string at the given field name id 
*
*  INPUTS
*     lListElem *ep     - element 
*     int name          - field name id
*     const char *value - new string 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error 
*******************************************************************************/
int lSetString(lListElem *ep, int name, const char *value) {
   char *str;
   int pos;
   int changed;

   DENTER(CULL_BASIS_LAYER);

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

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/****** cull/multitype/lSetHost() *********************************************
*  NAME
*     lSetHost() -- Set hostname for field name in element
*
*  SYNOPSIS
*     int lSetHost(lListElem *ep, int name, const char *value) 
*
*  FUNCTION
*     Sets in the element ep for field name the char * value.
*     Also duplicates the pointed to char array
*     (runtime type checking)
*
*
*  INPUTS
*     lListElem *ep     - list element pointer
*     int name          - name of list element (e.g. EH_name)
*     const char *value - new value for list element
*
*  RESULT
*     int - error state
*         -1 - Error 
*          0 - OK 
******************************************************************************/
int lSetHost(lListElem *ep, int name, const char *value) {
   char *str;
   int pos;
   int changed;

   DENTER(CULL_BASIS_LAYER);
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
         char host_key[CL_MAXHOSTLEN + 1];
         cull_hash_insert(ep, cull_hash_key(ep, pos, host_key),
                          ep->descr[pos].ht, mt_is_unique(ep->descr[pos].mt));
      }
      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }
   DRETURN(0);
}

/****** cull/multitype/lSetPosObject() ****************************************
*  NAME
*     lSetPosObject() -- Set list element at position pos 
*
*  SYNOPSIS
*     int lSetPosObject(lListElem *ep, int pos, lListElem *value) 
*
*  FUNCTION
*     Sets in the element 'ep' at position 'pos' the list element 'value'.
*     Doesn't copy the object. Does runtime type checking. 
*
*  INPUTS
*     lListElem *ep - element 
*     int pos             - position 
*     lListElem *value    - value 
*
*  RESULT
*     int - error state 
*         0 - OK
*        -1 - Error
*******************************************************************************/
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

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/****** cull/multitype/lSetPosList() ****************************************
*  NAME
*     lSetPosList() -- Set list at position pos 
*
*  SYNOPSIS
*     int lSetPosList(lListElem *ep, int pos, lList *value) 
*
*  FUNCTION
*     Sets in the element 'ep' at position 'pos' the lists 'value'.
*     Doesn't copy the list. Does runtime type checking. 
*
*  INPUTS
*     lListElem *ep - element 
*     int pos             - position 
*     lList *value        - value 
*
*  RESULT
*     int - error state 
*         0 - OK
*        -1 - Error
*******************************************************************************/
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

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/****** cull/multitype/lXchgString() ********************************************
*  NAME
*     lXchgList() -- Exchange field name value string pointer 
*
*  SYNOPSIS
*     int lXchgString(lListElem *ep, int name, char **str) 
*
*  FUNCTION
*     Exchange the string pointer, which has the given field name value. 
*
*  INPUTS
*     lListElem *ep - element 
*     int name      - field name value 
*     char **str   - pointer to a string
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error
******************************************************************************/
int lXchgString(lListElem *ep, int name, char **str) {
   int pos;
   char *tmp;

   DENTER(CULL_BASIS_LAYER);

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

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);

}

/****** cull/multitype/lXchgList() ********************************************
*  NAME
*     lXchgList() -- Exchange field name value list pointer 
*
*  SYNOPSIS
*     int lXchgList(lListElem *ep, int name, lList **lpp) 
*
*  FUNCTION
*     Exchange the list pointer which has the given field name value. 
*
*  INPUTS
*     lListElem *ep - element 
*     int name      - field name value 
*     lList **lpp   - pointer to CULL list 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error
******************************************************************************/
int lXchgList(lListElem *ep, int name, lList **lpp) {
   int pos;
   lList *tmp;

   DENTER(CULL_BASIS_LAYER);

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

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);

}

/****** cull/multitype/lSwapList() ********************************************
*  NAME
*     lSwapList() -- Exchange two lists within two elements
*
*  SYNOPSIS
*     int lSwapList(lListElem *to, int nm_to, lListElem *from, int nm_from) 
*
*  FUNCTION
*     Exchange two lists within two elements. 
*
*  INPUTS
*     lListElem *to   - element one 
*     int nm_to       - field name id of a list attribute of 'to' 
*     lListElem *from - element two 
*     int nm_from     - field name id of a list attribute of 'from' 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error 
******************************************************************************/
int lSwapList(lListElem *to, int nm_to, lListElem *from, int nm_from) {
   lList *tmp = nullptr;

   DENTER(CULL_BASIS_LAYER);

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

/****** cull/multitype/lSetObject() *********************************************
*  NAME
*     lSetObject() -- Sets a list at the given field name id 
*
*  SYNOPSIS
*     int lSetObject(lListElem *ep, int name, lList *value) 
*
*  FUNCTION
*     Sets a list at the given field name id. List will not be copyed.
*
*  INPUTS
*     lListElem *ep - element 
*     int name      - field name id 
*     lList *value  - new list pointer 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error 
******************************************************************************/
int lSetObject(lListElem *ep, int name, lListElem *value) {
   int pos;

   DENTER(CULL_BASIS_LAYER);

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
      value->status = OBJECT_ELEM;

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/****** cull/multitype/lSetList() *********************************************
*  NAME
*     lSetList() -- Sets a list at the given field name id 
*
*  SYNOPSIS
*     int lSetList(lListElem *ep, int name, lList *value) 
*
*  FUNCTION
*     Sets a list at the given field name id. List will not be copied.
*
*  INPUTS
*     lListElem *ep - element 
*     int name      - field name id 
*     lList *value  - new list pointer 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error 
*
*  NOTES
*     MT-NOTE: lAddSubList() is MT safe
******************************************************************************/
int lSetList(lListElem *ep, int name, lList *value) {
   int pos;

   DENTER(CULL_BASIS_LAYER);

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

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/****** cull/multitype/lSetPosFloat() *****************************************
*  NAME
*     lSetPosFloat() -- Set float value at given position 
*
*  SYNOPSIS
*     int lSetPosFloat(lListElem * ep, int pos, lFloat value) 
*
*  FUNCTION
*     Set float value at given position. 
*
*  INPUTS
*     lListElem * ep - element 
*     int pos              - position 
*     lFloat value         - new float value 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error 
******************************************************************************/
int lSetPosFloat(lListElem *ep, int pos, lFloat value) {
   DENTER(CULL_BASIS_LAYER);
   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   if (pos < 0) {
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lFloatT) {
      incompatibleType("lSetPosFloat");
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, false, lGetPosName(ep->descr, pos));
#endif

   if (value != ep->cont[pos].fl) {
      ep->cont[pos].fl = value;

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/****** cull/multitype/lSetFloat() ********************************************
*  NAME
*     lSetFloat() -- Set float value with given field name id 
*
*  SYNOPSIS
*     int lSetFloat(lListElem * ep, int name, lFloat value) 
*
*  FUNCTION
*     Set float value with given field name id. 
*
*  INPUTS
*     lListElem * ep - element 
*     int name       - field name id 
*     lFloat value   - new float value 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error 
******************************************************************************/
int lSetFloat(lListElem *ep, int name, lFloat value) {
   int pos;

   DENTER(CULL_BASIS_LAYER);
   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   pos = lGetPosViaElem(ep, name, SGE_NO_ABORT);
   if (pos < 0) {
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lFloatT) {
      incompatibleType2(MSG_CULL_SETFLOAT_WRONGTYPEFORFIELDXY_SS, lNm2Str(name),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, false, name);
#endif

   if (value != ep->cont[pos].fl) {
      ep->cont[pos].fl = value;

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/****** cull/multitype/lSetPosDouble() ****************************************
*  NAME
*     lSetPosDouble() -- Set double value at given position 
*
*  SYNOPSIS
*     int lSetPosDouble(lListElem *ep, int pos, lDouble value) 
*
*  FUNCTION
*     Set double value at given position. 
*
*  INPUTS
*     lListElem *ep - element 
*     int pos             - position 
*     lDouble value       - new double value 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error 
******************************************************************************/
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

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}


/****** cull/multitype/lSetDouble() *******************************************
*  NAME
*     lSetDouble() -- Set double value with given field name id 
*
*  SYNOPSIS
*     int lSetDouble(lListElem *ep, int name, lDouble value) 
*
*  FUNCTION
*     Set double value with given field name id 
*
*  INPUTS
*     lListElem *ep - element 
*     int name      - field name id 
*     lDouble value - new double value 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error 
*******************************************************************************/
int lSetDouble(lListElem *ep, int name, lDouble value) {
   int pos;

   DENTER(CULL_BASIS_LAYER);
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

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/****** cull_multitype/lAddDouble() ********************************************
*  NAME
*     lAddDouble() -- Adds a double offset to the double field
*
*  SYNOPSIS
*     lAddDouble(lListElem *ep, int name, lDouble offset) 
*
*  FUNCTION
*     The 'offset' is added to the double field 'name' of 
*     the CULL element 'ep'.
*
*  INPUTS
*     lListElem *ep  - element
*     int name       - field name id 
*     lDouble offset - the offset
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error 
*******************************************************************************/
int lAddDouble(lListElem *ep, int name, lDouble value) {
   int pos;

   DENTER(CULL_BASIS_LAYER);
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
      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}


/****** cull/multitype/lSetPosLong() ******************************************
*  NAME
*     lSetPosLong() -- Set long value at given position 
*
*  SYNOPSIS
*     int lSetPosLong(lListElem *ep, int pos, lLong value) 
*
*  FUNCTION
*     Set long value at given position. 
*
*  INPUTS
*     lListElem *ep - element 
*     int pos             - position 
*     lLong value         - new long value 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error 
******************************************************************************/
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

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/****** cull/multitype/lSetLong() *********************************************
*  NAME
*     lSetLong() -- Set long value with given field name id 
*
*  SYNOPSIS
*     int lSetLong(lListElem *ep, int name, lLong value) 
*
*  FUNCTION
*     Set long value with given field name id. 
*
*  INPUTS
*     lListElem *ep - element 
*     int name      - field name id 
*     lLong value   - value 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error 
******************************************************************************/
int lSetLong(lListElem *ep, int name, lLong value) {
   int pos;

   DENTER(CULL_BASIS_LAYER);
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

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/****** cull/multitype/lSetPosBool() ******************************************
*  NAME
*     lSetPosBool() -- Sets the character a the given position 
*
*  SYNOPSIS
*     int lSetPosBool(lListElem *ep, int pos, lBool value) 
*
*  FUNCTION
*     Sets the character a the given position. 
*
*  INPUTS
*     lListElem *ep - element 
*     int pos             - position 
*     lBool value         - value 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error 
******************************************************************************/
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

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/****** cull/multitype/lSetBool() *********************************************
*  NAME
*     lSetBool() -- Sets character with the given field name id 
*
*  SYNOPSIS
*     int lSetBool(lListElem * ep, int name, lBool value) 
*
*  FUNCTION
*     Sets character with the given field name id 
*
*  INPUTS
*     lListElem * ep - element 
*     int name       - field name id 
*     lBool value    - new character 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error 
******************************************************************************/
int lSetBool(lListElem *ep, int name, lBool value) {
   int pos;

   DENTER(CULL_BASIS_LAYER);
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

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/****** cull/multitype/lSetPosChar() ******************************************
*  NAME
*     lSetPosChar() -- Sets the character a the given position 
*
*  SYNOPSIS
*     int lSetPosChar(lListElem *ep, int pos, lChar value) 
*
*  FUNCTION
*     Sets the character a the given position. 
*
*  INPUTS
*     lListElem *ep - element 
*     int pos             - position 
*     lChar value         - value 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error 
******************************************************************************/
int lSetPosChar(lListElem *ep, int pos, lChar value) {
   DENTER(CULL_BASIS_LAYER);
   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   if (pos < 0) {
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lCharT) {
      incompatibleType("lSetPosChar");
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, false, lGetPosName(ep->descr, pos));
#endif

   if (value != ep->cont[pos].c) {
      ep->cont[pos].c = value;

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/****** cull/multitype/lSetChar() *********************************************
*  NAME
*     lSetChar() -- Sets character with the given field name id 
*
*  SYNOPSIS
*     int lSetChar(lListElem * ep, int name, lChar value) 
*
*  FUNCTION
*     Sets character with the given field name id 
*
*  INPUTS
*     lListElem * ep - element 
*     int name       - field name id 
*     lChar value    - new character 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error 
******************************************************************************/
int lSetChar(lListElem *ep, int name, lChar value) {
   int pos;

   DENTER(CULL_BASIS_LAYER);
   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   pos = lGetPosViaElem(ep, name, SGE_NO_ABORT);
   if (pos < 0) {
      LERROR(LENEGPOS);
      DRETURN(-1);
   }

   if (mt_get_type(ep->descr[pos].mt) != lCharT) {
      incompatibleType2(MSG_CULL_SETCHAR_WRONGTYPEFORFIELDXY_SS, lNm2Str(name),
                        multitypes[mt_get_type(ep->descr[pos].mt)]);
      DRETURN(-1);
   }

#ifdef OBSERVE
   lObserveChangeValue(ep, false, name);
#endif

   if (value != ep->cont[pos].c) {
      ep->cont[pos].c = value;

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/****** cull/multitype/lSetPosRef() *******************************************
*  NAME
*     lSetPosRef() -- Set pointer at given position 
*
*  SYNOPSIS
*     int lSetPosRef(lListElem * ep, int pos, lRef value) 
*
*  FUNCTION
*     Set pointer at given position 
*
*  INPUTS
*     lListElem * ep - element 
*     int pos              - position 
*     lRef value           - pointer 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error
******************************************************************************/
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

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/****** cull/multitype/lSetRef() **********************************************
*  NAME
*     lSetRef() -- Set pointer with the given field name id 
*
*  SYNOPSIS
*     int lSetRef(lListElem * ep, int name, lRef value) 
*
*  FUNCTION
*     Set pointer with the given field name id 
*
*  INPUTS
*     lListElem * ep - element 
*     int name       - field name id 
*     lRef value     - new pointer 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error 
******************************************************************************/
int lSetRef(lListElem *ep, int name, lRef value) {
   int pos;

   DENTER(CULL_BASIS_LAYER);
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

      /* remember that field changed */
      sge_bitfield_set(&(ep->changed), pos);
   }

   DRETURN(0);
}

/* ------------------------------------------------------------ 
   compares two int values i0 and i1 
   return values like strcmp
 */
int intcmp(int i0, int i1) {
   return i0 == i1 ? 0 : (i0 < i1 ? -1 : 1);
}

/* ------------------------------------------------------------ 
   compares two ulong values u0 and u1 
   return values like strcmp
 */
int ulongcmp(lUlong u0, lUlong u1) {
   return u0 == u1 ? 0 : (u0 < u1 ? -1 : 1);
}

int bitmaskcmp(lUlong bm0, lUlong bm1) {
   return ((bm0 & bm1) == bm1) ? 1 : 0;
}

/* ------------------------------------------------------------ 
   compares two ulong64 values u0 and u1 
   return values like strcmp
 */
int ulong64cmp(lUlong64 u0, lUlong64 u1) {
   return u0 == u1 ? 0 : (u0 < u1 ? -1 : 1);
}

/* ------------------------------------------------------------ 
   compares two lFloat values f0 and f1 
   return values like strcmp
 */
int floatcmp(lFloat f0, lFloat f1) {
   return f0 == f1 ? 0 : (f0 < f1 ? -1 : 1);
}

/* ------------------------------------------------------------ 
   compares two double values d0 and d1 
   return values like strcmp
 */
int doublecmp(lDouble d0, lDouble d1) {
   return d0 == d1 ? 0 : (d0 < d1 ? -1 : 1);
}

/* ------------------------------------------------------------ 
   compares two long values l0 and l1 
   return values like strcmp
 */
int longcmp(lLong l0, lLong l1) {
   return l0 == l1 ? 0 : (l0 < l1 ? -1 : 1);
}

/* ------------------------------------------------------------ 
   compares two bool values c0 and c1 
   return values like strcmp
 */
int boolcmp(lBool b0, lBool b1) {
   return b0 == b1 ? 0 : (b0 < b1 ? -1 : 1);
}

/* ------------------------------------------------------------ 
   compares two char values c0 and c1 
   return values like strcmp
 */
int charcmp(lChar c0, lChar c1) {
   return c0 == c1 ? 0 : (c0 < c1 ? -1 : 1);
}

/* ------------------------------------------------------------ 
   compares two lRef values c0 and c1 
   return values like strcmp
 */
int refcmp(lRef c0, lRef c1) {
   return c0 == c1 ? 0 : (c0 < c1 ? -1 : 1);
}

/****** cull/multitype/lAddSubStr() *******************************************
*  NAME
*     lAddSubStr() -- adds a string to the string sublist  
*
*  SYNOPSIS
*     lListElem* lAddSubStr(lListElem* ep, int nm, char* str, 
*                           int snm, lDescr* dp) 
*
*  FUNCTION
*     This function add a new element into a sublist snm of an 
*     element ep. The field nm of this added element will get the 
*     initial value specified with str. 
*
*  INPUTS
*     lListElem* ep - list element 
*     int nm        - field id contained in the element which 
*                     will be created
*     char* str     - initial value if nm 
*     int snm       - field id of the sublist within ep 
*     lDescr* dp    - Type of the new element 
*
*  RESULT
*     nullptr in case of error
*     otherwise pointer to the added element 
******************************************************************************/
lListElem *lAddSubStr(lListElem *ep, int nm, const char *str, int snm,
                      const lDescr *dp) {
   lListElem *ret;
   int sublist_pos;

   DENTER(CULL_LAYER);

   if (!ep) {
      DPRINTF(("error: nullptr ptr passed to lAddSubStr\n"));
      DRETURN(nullptr);
   }

   if (!(ep->descr)) {
      DPRINTF(("nullptr descriptor in element not allowed !!!"));
      abort();
   }

   /* run time type checking */
   if ((sublist_pos = lGetPosViaElem(ep, snm, SGE_NO_ABORT)) < 0) {
      CRITICAL(MSG_CULL_ADDSUBSTRERRORXRUNTIMETYPE_S, lNm2Str(snm));
      DRETURN(nullptr);
   }

   ret = lAddElemStr(&(ep->cont[sublist_pos].glp), nm, str, dp);

   /* remember that field changed */
   if (ret != nullptr) {
      sge_bitfield_set(&(ep->changed), sublist_pos);
   }

   DRETURN(ret);
}

/****** cull/multitype/lAddSubHost() ******************************************
*  NAME
*     lAddSubHost() -- adds a string to the string sublist  
*
*  SYNOPSIS
*     lListElem* lAddSubHost(lListElem* ep, int nm, char* str, 
*                            int snm, lDescr* dp) 
*
*  FUNCTION
*     This function add a new element into a sublist snm of an 
*     element ep. The field nm of this added element will get the 
*     initial value specified with str. 
*
*  INPUTS
*     lListElem* ep - list element 
*     int nm        - field id contained in the element which 
*                     will be created
*     char* str     - initial value if nm 
*     int snm       - field id of the sublist within ep 
*     lDescr* dp    - Type of the new element 
*
*  RESULT
*     nullptr in case of error
*     otherwise pointer to the added element 
******************************************************************************/
lListElem *lAddSubHost(lListElem *ep, int nm, const char *str, int snm,
                       const lDescr *dp) {
   lListElem *ret;
   int sublist_pos;

   DENTER(CULL_LAYER);

   if (!ep) {
      DPRINTF(("error: nullptr ptr passed to lAddSubHost\n"));
      DRETURN(nullptr);
   }

   if (!(ep->descr)) {
      DPRINTF(("nullptr descriptor in element not allowed !!!"));
      abort();
   }

   /* run time type checking */
   if ((sublist_pos = lGetPosViaElem(ep, snm, SGE_NO_ABORT)) < 0) {
      CRITICAL(MSG_CULL_ADDSUBHOSTERRORXRUNTIMETYPE_S, lNm2Str(snm));
      DRETURN(nullptr);
   }

   ret = lAddElemHost(&(ep->cont[sublist_pos].glp), nm, str, dp);

   /* remember that field changed */
   if (ret != nullptr) {
      sge_bitfield_set(&(ep->changed), sublist_pos);
   }

   DRETURN(ret);
}


/****** cull/multitype/lAddElemStr() ******************************************
*  NAME
*     lAddElemStr() -- adds a string to the string list  
*
*  SYNOPSIS
*     lListElem* lAddElemStr(lList **lpp, int nm, const char *str, 
*                            const lDescr *dp) 
*
*  FUNCTION
*     This function adds a new element of type dp to the list referenced
*     by lpp. The field nm will get the initial value str.
*
*  INPUTS
*     lList** lpp - list reference 
*     int nm      - field id 
*     char* str   - initial value 
*     lDescr* dp  - Type of the object which will be added  
*
*  RESULT
*     lListElem* - 
******************************************************************************/
lListElem *lAddElemStr(lList **lpp, int nm, const char *str, const lDescr *dp) {
   lListElem *sep;
   int pos;
   int data_type;

   DENTER(CULL_LAYER);

   if (!lpp || !str || !dp) {
      DPRINTF(("error: nullptr ptr passed to lAddElemStr\n"));
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
      DPRINTF(("error: lAddElemStr called to field which is no lStringT type\n"));
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

/****** cull/multitype/lAddElemHost() *****************************************
*  NAME
*     lAddElemHost() -- Adds a hostname to a hostname list 
*
*  SYNOPSIS
*     lListElem* lAddElemHost(lList **lpp, int nm, const char *str, 
*                             const lDescr *dp) 
*
*  FUNCTION
*     Adds a hostname to a hostname list 
*
*  INPUTS
*     lList **lpp      - list reference 
*     int nm           - hostname field id 
*     const char *str  - new hostname 
*     const lDescr *dp - descriptor of new element 
*
*  RESULT
*     lListElem* - new element or nullptr
******************************************************************************/
lListElem *lAddElemHost(lList **lpp, int nm, const char *str, const lDescr *dp) {
   lListElem *sep;
   int pos;
   int data_type;

   DENTER(CULL_LAYER);

   if (!lpp || !str || !dp) {
      DPRINTF(("error: nullptr ptr passed to lAddElemHost\n"));
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
      DPRINTF(("error: lAddElemHost called to field which is no lHostT type\n"));
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

/****** cull/multitype/lDelSubStr() *******************************************
*  NAME
*     lDelSubStr() -- removes an element from a sublist 
*
*  SYNOPSIS
*     int lDelSubStr(lListElem* ep, int nm, const char* str, int snm) 
*
*  FUNCTION
*     This function removes an element specified by a string field 
*     nm and the string str supposed to be in the sublist snm of the 
*     element ep.
*
*  INPUTS
*     lListElem* ep - element 
*     int nm        - field id 
*     const char* str     - string 
*     int snm       - field id of a sublist of ep
*
*  RESULT
*     1 element was found and removed
*     0 in case of an error 
******************************************************************************/
int lDelSubStr(lListElem *ep, int nm, const char *str, int snm) {
   int ret, sublist_pos;

   DENTER(CULL_LAYER);

   /* get position of sublist in ep */
   sublist_pos = lGetPosViaElem(ep, snm, SGE_DO_ABORT);

   ret = lDelElemStr(&(ep->cont[sublist_pos].glp), nm, str);

   /* remember that field changed */
   if (ret == 1) {
      sge_bitfield_set(&(ep->changed), sublist_pos);
   }

   DRETURN(ret);
}

/****** cull/multitype/lDelElemStr() ******************************************
*  NAME
*     lDelElemStr() -- removes element specified by a string field nm 
*
*  SYNOPSIS
*     int lDelElemStr(lList** lpp, int nm, const char* str) 
*
*  FUNCTION
*     This function removes an element from the list referenced by 
*     lpp, which is identified by the field nm and the string str 
*
*  INPUTS
*     lList** lpp - list reference 
*     int nm      - field id
*     const char* str   - string
*
*  RESULT
*     1 if the element was found and removed
*     0 in case of an error 
******************************************************************************/
int lDelElemStr(lList **lpp, int nm, const char *str) {
   lListElem *ep;

   DENTER(CULL_LAYER);

   if (!lpp || !str) {
      DPRINTF(("error: nullptr ptr passed to lDelElemStr\n"));
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

/****** cull/multitype/lGetSubStr() *******************************************
*  NAME
*     lGetSubStr() -- returns element specified by a string field nm 
*
*  SYNOPSIS
*     lListElem* lGetSubStr(const lListElem* ep, int nm, 
*                           const char* str, int snm) 
*
*  FUNCTION
*     returns an element specified by a string field nm and the 
*     string str from the sublist snm of the element ep 
*
*  INPUTS
*     const lListElem* ep - element pointer 
*     int nm              - field id contained in an sublist 
*                           element of ep 
*     const char* str     - string 
*     int snm             - field id contained in ep 
*
*  RESULT
*     nullptr if element was not found or in case of an error
*     otherwise pointer to an element
******************************************************************************/
lListElem *lGetSubStr(const lListElem *ep, int nm, const char *str, int snm) {
   int sublist_pos;
   lListElem *ret = nullptr;

   DENTER(CULL_LAYER);

   if (ep != nullptr) {
      /* get position of sublist in ep */
      sublist_pos = lGetPosViaElem(ep, snm, SGE_DO_ABORT);

      ret = lGetElemStrRW(ep->cont[sublist_pos].glp, nm, str);
   }

   DRETURN(ret);
}

/****** cull/multitype/lGetElemStr() ******************************************
*  NAME
*     lGetElemStr() -- returns element specified by a string field nm 
*
*  SYNOPSIS
*     lListElem* lGetElemStr(const lList* lp, int nm, const char* str) 
*
*  FUNCTION
*     returns an element specified by a string field nm from list lp 
*
*  INPUTS
*     const lList* lp - list 
*     int nm    - field id 
*     const char* str - value 
*
*  RESULT
*     nullptr when element was not found or if an error occured
*     otherwise pointer to element 
******************************************************************************/
lListElem *lGetElemStrRW(const lList *lp, int nm, const char *str) {
   const void *iterator = nullptr;
   lListElem *ret = nullptr;
   DENTER(CULL_LAYER);

   ret = lGetElemStrFirstRW(lp, nm, str, &iterator);
   DRETURN(ret);
}

const lListElem *lGetElemStr(const lList *lp, int nm, const char *str) {
   return lGetElemStrRW(lp, nm, str);
}


/****** cull/multitype/lGetElemStrFirst() *************************************
*  NAME
*     lGetElemStrFirst() -- Find first element with a certain string 
*
*  SYNOPSIS
*     lListElem* lGetElemStrFirst(const lList *lp, int nm, 
*                                 const char *str, const void **iterator) 
*
*  FUNCTION
*     Returns the first element within 'lp' where the attribute
*     with field name id 'nm' is equivalent with 'str'. 'iterator'
*     will be filled with context information which will make it 
*     possible to use 'iterator' with lGetElemStrNext() to get
*     the next element. 
*      
*
*  INPUTS
*     const lList *lp       - list 
*     int nm                - field name id 
*     const char *str       - string to be compared 
*     const void **iterator - iterator 
*
*  RESULT
*     lListElem* - first element or nullptr
******************************************************************************/
lListElem *lGetElemStrFirstRW(const lList *lp, int nm, const char *str, const void **iterator) {
   lListElem *ep;
   int pos;
   int data_type;
   const lDescr *listDescriptor;


   DENTER(CULL_LAYER);
   if (!str) {
      DPRINTF(("error: nullptr ptr passed to lGetElemStrFirst\n"));
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
      CRITICAL(MSG_CULL_GETELEMSTRERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   data_type = lGetPosType(listDescriptor, pos);
   if (data_type != lStringT) {
      DPRINTF(("error: lGetElemStrFirst called to field which is no lStringT type\n"));
      CRITICAL(MSG_CULL_GETELEMSTRERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   *iterator = nullptr;

   if (lp->descr[pos].ht != nullptr) {
      /* hash access */
      ep = cull_hash_first(lp->descr[pos].ht, str,
                           mt_is_unique(lp->descr[pos].mt), iterator);
      DRETURN(ep);
   } else {
      /* seek for element */
      for_each_rw(ep, lp) {
         const char *s = lGetPosString(ep, pos);
         if (s && !strcmp(s, str)) {
            *iterator = ep;
            DRETURN(ep);
         }
      }
   }

   DRETURN(nullptr);
}

const lListElem *lGetElemStrFirst(const lList *lp, int nm, const char *str, const void **iterator) {
   return lGetElemStrFirstRW(lp, nm, str, iterator);
}

/****** cull/multitype/lGetElemStrNext() **************************************
*  NAME
*     lGetElemStrNext() -- Get next element with a certain string 
*
*  SYNOPSIS
*     lListElem* lGetElemStrNext(const lList *lp, 
*                                int nm, 
*                                const char *str, 
*                                const void **iterator) 
*
*  FUNCTION
*     Returns a element within list 'lp' where the attribute with
*     field name id 'nm' is equivalent with 'str'. The function
*     uses 'iterator' as input. 'iterator' containes context
*     information which where fillen in in a previous call of
*     lGetElemStrFirst().
*
*  INPUTS
*     const lList *lp       - list 
*     int nm                - string field name id 
*     const char *str       - string 
*     const void **iterator - iterator 
*
*  RESULT
*     lListElem* - next element or nullptr
******************************************************************************/
lListElem *lGetElemStrNextRW(const lList *lp, int nm, const char *str, const void **iterator) {
   lListElem *ep;
   int pos, data_type;
   const lDescr *listDescriptor;


   DENTER(CULL_LAYER);

   if (*iterator == nullptr) {
      return nullptr;
   }

   if (!str) {
      DPRINTF(("error: nullptr ptr passed to lGetElemStr\n"));
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
      CRITICAL(MSG_CULL_GETELEMSTRERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }
   data_type = lGetPosType(listDescriptor, pos);
   if (data_type != lStringT) {
      DPRINTF(("error: lGetElemStrNext called to field which is no lStringT type\n"));
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

const lListElem *lGetElemStrNext(const lList *lp, int nm, const char *str, const void **iterator) {
   return lGetElemStrNextRW(lp, nm, str, iterator);
}

/****** cull/multitype/lGetElemStrLike() **************************************
*  NAME
*     lGetElemStrLike() -- returns element specified by a wildcard 
*
*  SYNOPSIS
*     lListElem* lGetElemStrLike(const lList* lp, int nm, 
*                                const char* str) 
*
*  FUNCTION
*     returns an element specified by a string field nm from the 
*     list lp and uses a trailing '*' as a wilcard, e.g. 'OAport' 
*     matches 'OA*' 
*
*  INPUTS
*     const lList* lp - list pointer 
*     int nm    - field id 
*     const char* str - wildcard string 
*
*  RESULT
*     nullptr if element was not found or in case of error
*     otherwise pointer to element 
******************************************************************************/
lListElem *lGetElemStrLikeRW(const lList *lp, int nm, const char *str) {
   lListElem *ep;
   int pos;
   const char *s;
   int data_type;
   size_t str_pos = 0;
   const lDescr *listDescriptor;


   DENTER(CULL_LAYER);
   if (!str) {
      DPRINTF(("error: nullptr ptr passed to lGetElemStr\n"));
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
      CRITICAL(MSG_CULL_GETELEMSTRERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }
   data_type = lGetPosType(listDescriptor, pos);
   if (data_type != lStringT) {
      DPRINTF(("error: lGetElemStrLike called to field which is no lStringT type\n"));
      CRITICAL(MSG_CULL_GETELEMSTRERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   /* seek for element */
   str_pos = strlen(str) - 1;
   for_each_rw(ep, lp) {
      s = lGetPosString(ep, pos);
      if (s && (!strcmp(s, str) ||
                (str[str_pos] == '*' && !strncmp(s, str, str_pos)))) {
         DRETURN(ep);
      }
   }

   DRETURN(nullptr);
}

const lListElem *lGetElemStrLike(const lList *lp, int nm, const char *str) {
   return lGetElemStrLikeRW(lp, nm, str);
}

/****** cull/multitype/lAddSubUlong() *****************************************
*  NAME
*     lAddSubUlong() -- adds ulong to the ulong sublist of element ep 
*
*  SYNOPSIS
*     lListElem* lAddSubUlong(lListElem* ep, int nm, lUlong val, 
*                             int snm, const lDescr* dp) 
*
*  FUNCTION
*     This function adds a new element into the sublist snm of the 
*     element ep. The field nm of the added element will get the 
*     initial value val. 
*
*  INPUTS
*     lListElem* ep       - element 
*     int nm              - field which will get value val 
*     lUlong val          - initial value for nm 
*     int snm             - sublist within ep where the element 
*                           will be added 
*     const lDescr* dp    - Type of the new element (e.g. JB_Type) 
*
*  RESULT
*     nullptr in case of error
*     or the pointer to the new element 
******************************************************************************/
lListElem *lAddSubUlong(lListElem *ep, int nm, lUlong val, int snm,
                        const lDescr *dp) {
   lListElem *ret;
   int sublist_pos;

   DENTER(CULL_LAYER);

   if (!ep) {
      DPRINTF(("error: nullptr ptr passed to lAddSubUlong\n"));
      DRETURN(nullptr);
   }

   if (!(ep->descr)) {
      DPRINTF(("nullptr descriptor in element not allowed !!!"));
      abort();
   }

   /* run time type checking */
   if ((sublist_pos = lGetPosViaElem(ep, snm, SGE_NO_ABORT)) < 0) {
      CRITICAL(MSG_CULL_ADDSUBULONGERRORXRUNTIMETYPE_S, lNm2Str(snm));
      DRETURN(nullptr);
   }

   ret = lAddElemUlong(&(ep->cont[sublist_pos].glp), nm, val, dp);

   /* remember that field changed */
   if (ret != nullptr) {
      sge_bitfield_set(&(ep->changed), sublist_pos);
   }

   DRETURN(ret);
}

/****** cull/multitype/lAddElemUlong() ****************************************
*  NAME
*     lAddElemUlong() -- adds a ulong to the ulong list 
*
*  SYNOPSIS
*     lListElem* lAddElemUlong(lList** lpp, int nm, lUlong val, 
*                              const lDescr* dp) 
*
*  FUNCTION
*     Adds an new element to a list lpp where one field nm within
*     the new element gets an initial value val 
*
*  INPUTS
*     lList** lpp       - list  
*     int nm            - field in the new element which will get 
*                         value val 
*     lUlong val        - initial value for nm 
*     const lDescr* dp  - type of the list (e.g. JB_Type) 
*
*  RESULT
*     nullptr on error
*     or pointer to the added element 
******************************************************************************/
lListElem *lAddElemUlong(lList **lpp, int nm, lUlong val, const lDescr *dp) {
   lListElem *sep;
   int pos;

   DENTER(CULL_LAYER);

   if (!lpp || !dp) {
      DPRINTF(("error: nullptr ptr passed to lAddElemUlong\n"));
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

/****** cull/multitype/lDelSubUlong() *****************************************
*  NAME
*     lDelSubUlong() -- removes an element from a sublist 
*
*  SYNOPSIS
*     int lDelSubUlong(lListElem* ep, int nm, lUlong val, int snm) 
*
*  FUNCTION
*     This function removes an element specified by a ulong field nm
*     and the ulong val supposed to be in the sublist snm of the 
*     element ep 
*
*  INPUTS
*     lListElem* ep - element 
*     int nm        - field id 
*     lUlong val    - value 
*     int snm       - field id of the sublist in ep 
*
*  RESULT
*     1 element was found and removed
*     0 in case of an error 
******************************************************************************/
int lDelSubUlong(lListElem *ep, int nm, lUlong val, int snm) {
   int ret, sublist_pos;

   DENTER(CULL_LAYER);

   /* get position of sublist in ep */
   sublist_pos = lGetPosViaElem(ep, snm, SGE_DO_ABORT);

   ret = lDelElemUlong(&(ep->cont[sublist_pos].glp), nm, val);

   /* remember that field changed */
   if (ret == 1) {
      sge_bitfield_set(&(ep->changed), sublist_pos);
   }

   DRETURN(ret);
}

/****** cull/multitype/lDelElemUlong() ****************************************
*  NAME
*     lDelElemUlong() -- removes elem specified by a ulong field nm 
*
*  SYNOPSIS
*     int lDelElemUlong(lList** lpp, int nm, lUlong val) 
*
*  FUNCTION
*     This function removes an element specified by a ulong field nm 
*     with the value val from the list referenced by lpp. 
*
*  INPUTS
*     lList** lpp - reference to a list 
*     int nm      - field id 
*     lUlong val  - value if nm 
*
*  RESULT
*     1 element was found and removed 
*     0 an error occured
******************************************************************************/
int lDelElemUlong(lList **lpp, int nm, lUlong val) {
   lListElem *ep;

   DENTER(CULL_LAYER);

   if (!lpp || !val) {
      DPRINTF(("error: nullptr ptr passed to lDelElemUlong\n"));
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

/****** cull/multitype/lGetSubUlong() *****************************************
*  NAME
*     lGetSubUlong() -- Element specified by a ulong field nm 
*
*  SYNOPSIS
*     lListElem* lGetSubUlong(const lListElem* ep, int nm, 
*                             lUlong val, int snm) 
*
*  FUNCTION
*     returns an element specified by a ulong field nm an the ulong 
*     value val from the sublist snm of the element ep 
*
*  INPUTS
*     const lListElem* ep - element pointer 
*     int nm              - field id which is part of a sublist 
*                           element of ep 
*     lUlong val          - unsigned long value 
*     int snm             - field id of a list which is part of ep 
*
*  RESULT
*     nullptr if element was not found or in case of an error
*     otherwise pointer to the element 
******************************************************************************/
lListElem *lGetSubUlong(const lListElem *ep, int nm, lUlong val, int snm) {
   int sublist_pos;
   lListElem *ret;

   DENTER(CULL_LAYER);

   /* get position of sublist in ep */
   sublist_pos = lGetPosViaElem(ep, snm, SGE_DO_ABORT);

   ret = lGetElemUlongRW(ep->cont[sublist_pos].glp, nm, val);

   DRETURN(ret);
}

/****** cull/multitype/lGetElemUlong() ****************************************
*  NAME
*     lGetElemUlong() -- returns element specified by a ulong field nm 
*
*  SYNOPSIS
*     lListElem* lGetElemUlong(const lList* lp, int nm, lUlong val) 
*
*  FUNCTION
*     returns an element specified by a ulong field nm an an ulong 
*     value val from list lp 
*
*  INPUTS
*     const lList* lp  - list pointer 
*     int nm     - field id 
*     lUlong val - unsigned long value 
*
*  RESULT
*    nullptr if element was not found or an error occured
*    otherwise pointer to element 
******************************************************************************/
lListElem *lGetElemUlongRW(const lList *lp, int nm, lUlong val) {
   const void *iterator = nullptr;
   return lGetElemUlongFirstRW(lp, nm, val, &iterator);
}

const lListElem *lGetElemUlong(const lList *lp, int nm, lUlong val) {
   return lGetElemUlongRW(lp, nm, val);
}

/****** cull/multitype/lGetElemUlongFirst() ***********************************
*  NAME
*     lGetElemUlongFirst() -- Find first ulong within a list 
*
*  SYNOPSIS
*     lListElem* lGetElemUlongFirst(const lList *lp, 
*                                   int nm, 
*                                   lUlong val, 
*                                   const void **iterator) 
*
*  FUNCTION
*     Return the first element of list 'lp' where the attribute
*     with field name id 'nm' is equivalent with 'val'. Context
*     information will be stored in 'iterator'. 'iterator' might
*     be used in lGetElemUlongNext() to get the next element.
*
*  INPUTS
*     const lList *lp       - list 
*     int nm                - ulong field anme id 
*     lUlong val            - ulong value 
*     const void **iterator - iterator 
*
*  RESULT
*     lListElem* - element or nullptr
******************************************************************************/
lListElem *lGetElemUlongFirstRW(const lList *lp, int nm, lUlong val, const void **iterator) {
   lListElem *ep = nullptr;
   int pos;

   DENTER(CULL_LAYER);

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
      ep = cull_hash_first(lp->descr[pos].ht, &val,
                           mt_is_unique(lp->descr[pos].mt), iterator);
      DRETURN(ep);
   } else {
      /* seek for element */
      for_each_rw(ep, lp) {
         lUlong s = lGetPosUlong(ep, pos);
         if (s == val) {
            *iterator = ep;
            DRETURN(ep);
         }
      }
   }

   DRETURN(nullptr);
}

const lListElem *lGetElemUlongFirst(const lList *lp, int nm, lUlong val, const void **iterator) {
   return lGetElemUlongFirstRW(lp, nm, val, iterator);
}

/****** cull/multitype/lGetElemUlongNext() ************************************
*  NAME
*     lGetElemUlongNext() -- Find next ulong element within a list 
*
*  SYNOPSIS
*     lListElem* lGetElemUlongNext(const lList *lp, 
*                                  int nm, 
*                                  lUlong val, 
*                                  const void **iterator) 
*
*  FUNCTION
*     This function might be used after a call to lGetElemUlongFirst().
*     It expects 'iterator' to contain context information which
*     makes it possible to find the next element within list 'lp'
*     where the attribute with field name id 'nm' is equivalent with
*     'val'. 
*
*  INPUTS
*     const lList *lp       - list 
*     int nm                - ulong field name id 
*     lUlong val            - value 
*     const void **iterator - iterator 
*
*  RESULT
*     lListElem* - next element or nullptr
******************************************************************************/
lListElem *lGetElemUlongNextRW(const lList *lp, int nm, lUlong val, const void **iterator) {
   lListElem *ep;
   int pos;

   DENTER(CULL_LAYER);

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

const lListElem *lGetElemUlongNext(const lList *lp, int nm, lUlong val, const void **iterator) {
   return lGetElemUlongNextRW(lp, nm, val, iterator);
}

/****** cull/multitype/lAddSubUlong64() ***************************************
*  NAME
*     lAddSubUlong64() -- adds ulong64 to the ulong64 sublist of element ep 
*
*  SYNOPSIS
*     lListElem* lAddSubUlong64(lListElem* ep, int nm, lUlong64 val, 
*                             int snm, const lDescr* dp) 
*
*  FUNCTION
*     This function adds a new element into the sublist snm of the 
*     element ep. The field nm of the added element will get the 
*     initial value val. 
*
*  INPUTS
*     lListElem* ep       - element 
*     int nm              - field which will get value val 
*     lUlong64 val        - initial value for nm 
*     int snm             - sublist within ep where the element 
*                           will be added 
*     const lDescr* dp    - Type of the new element (e.g. JB_Type) 
*
*  RESULT
*     nullptr in case of error
*     or the pointer to the new element 
******************************************************************************/
lListElem *lAddSubUlong64(lListElem *ep, int nm, lUlong64 val, int snm,
                          const lDescr *dp) {
   lListElem *ret;
   int sublist_pos;

   DENTER(CULL_LAYER);

   if (!ep) {
      DPRINTF(("error: nullptr ptr passed to lAddSubUlong64\n"));
      DRETURN(nullptr);
   }

   if (!(ep->descr)) {
      DPRINTF(("nullptr descriptor in element not allowed !!!"));
      abort();
   }

   /* run time type checking */
   if ((sublist_pos = lGetPosViaElem(ep, snm, SGE_NO_ABORT)) < 0) {
      CRITICAL(MSG_CULL_ADDSUBULONG64ERRORXRUNTIMETYPE_S, lNm2Str(snm));
      DRETURN(nullptr);
   }

   ret = lAddElemUlong64(&(ep->cont[sublist_pos].glp), nm, val, dp);

   /* remember that field changed */
   if (ret != nullptr) {
      sge_bitfield_set(&(ep->changed), sublist_pos);
   }

   DRETURN(ret);
}

/****** cull/multitype/lAddElemUlong64() **************************************
*  NAME
*     lAddElemUlong64() -- adds a ulong64 to the ulong64 list 
*
*  SYNOPSIS
*     lListElem* lAddElemUlong64(lList** lpp, int nm, lUlong64 val, 
*                              const lDescr* dp) 
*
*  FUNCTION
*     Adds an new element to a list lpp where one field nm within
*     the new element gets an initial value val 
*
*  INPUTS
*     lList** lpp       - list  
*     int nm            - field in the new element which will get 
*                         value val 
*     lUlong64 val      - initial value for nm 
*     const lDescr* dp  - type of the list (e.g. JB_Type) 
*
*  RESULT
*     nullptr on error
*     or pointer to the added element 
******************************************************************************/
lListElem *lAddElemUlong64(lList **lpp, int nm, lUlong64 val, const lDescr *dp) {
   lListElem *sep;
   int pos;

   DENTER(CULL_LAYER);

   if (!lpp || !dp) {
      DPRINTF(("error: nullptr ptr passed to lAddElemUlong64\n"));
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

/****** cull/multitype/lDelSubUlong64() ***************************************
*  NAME
*     lDelSubUlong64() -- removes an element from a sublist 
*
*  SYNOPSIS
*     int lDelSubUlong64(lListElem* ep, int nm, lUlong64 val, int snm) 
*
*  FUNCTION
*     This function removes an element specified by a ulong field nm
*     and the ulong64 val supposed to be in the sublist snm of the 
*     element ep 
*
*  INPUTS
*     lListElem* ep - element 
*     int nm        - field id 
*     lUlong64 val  - value 
*     int snm       - field id of the sublist in ep 
*
*  RESULT
*     1 element was found and removed
*     0 in case of an error 
******************************************************************************/
int lDelSubUlong64(lListElem *ep, int nm, lUlong64 val, int snm) {
   int ret, sublist_pos;

   DENTER(CULL_LAYER);

   /* get position of sublist in ep */
   sublist_pos = lGetPosViaElem(ep, snm, SGE_DO_ABORT);

   ret = lDelElemUlong64(&(ep->cont[sublist_pos].glp), nm, val);

   /* remember that field changed */
   if (ret == 1) {
      sge_bitfield_set(&(ep->changed), sublist_pos);
   }

   DRETURN(ret);
}

/****** cull/multitype/lDelElemUlong64() **************************************
*  NAME
*     lDelElemUlong64() -- removes elem specified by a ulong64 field nm 
*
*  SYNOPSIS
*     int lDelElemUlong64(lList** lpp, int nm, lUlong64 val) 
*
*  FUNCTION
*     This function removes an element specified by a ulong64 field nm 
*     with the value val from the list referenced by lpp. 
*
*  INPUTS
*     lList** lpp    - reference to a list 
*     int nm         - field id 
*     lUlong64 val   - value if nm 
*
*  RESULT
*     1 element was found and removed 
*     0 an error occured
******************************************************************************/
int lDelElemUlong64(lList **lpp, int nm, lUlong64 val) {
   lListElem *ep;

   DENTER(CULL_LAYER);

   if (!lpp || !val) {
      DPRINTF(("error: nullptr ptr passed to lDelElemUlong64\n"));
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

/****** cull/multitype/lGetSubUlong64() *****************************************
*  NAME
*     lGetSubUlong64() -- Element specified by a ulong64 field nm 
*
*  SYNOPSIS
*     lListElem* lGetSubUlong64(const lListElem* ep, int nm, 
*                             lUlong64 val, int snm) 
*
*  FUNCTION
*     returns an element specified by a ulong64 field nm an the ulong64
*     value val from the sublist snm of the element ep 
*
*  INPUTS
*     const lListElem* ep - element pointer 
*     int nm              - field id which is part of a sublist 
*                           element of ep 
*     lUlong64 val        - unsigned long value 
*     int snm             - field id of a list which is part of ep 
*
*  RESULT
*     nullptr if element was not found or in case of an error
*     otherwise pointer to the element 
******************************************************************************/
lListElem *lGetSubUlong64(const lListElem *ep, int nm, lUlong64 val, int snm) {
   int sublist_pos;
   lListElem *ret;

   DENTER(CULL_LAYER);

   /* get position of sublist in ep */
   sublist_pos = lGetPosViaElem(ep, snm, SGE_DO_ABORT);

   ret = lGetElemUlong64RW(ep->cont[sublist_pos].glp, nm, val);

   DRETURN(ret);
}

/****** cull/multitype/lGetElemUlong64() **************************************
*  NAME
*     lGetElemUlong64() -- returns element specified by a ulong64 field nm 
*
*  SYNOPSIS
*     lListElem* lGetElemUlong64(const lList* lp, int nm, lUlong64 val) 
*
*  FUNCTION
*     returns an element specified by a ulong64 field nm an an ulong64 
*     value val from list lp 
*
*  INPUTS
*     const lList* lp  - list pointer 
*     int nm     - field id 
*     lUlong64 val - unsigned long value 
*
*  RESULT
*    nullptr if element was not found or an error occured
*    otherwise pointer to element 
******************************************************************************/
lListElem *lGetElemUlong64RW(const lList *lp, int nm, lUlong64 val) {
   const void *iterator = nullptr;
   return lGetElemUlong64FirstRW(lp, nm, val, &iterator);
}

const lListElem *lGetElemUlong64(const lList *lp, int nm, lUlong64 val) {
   return lGetElemUlong64RW(lp, nm, val);
}

/****** cull/multitype/lGetElemUlong64First() *********************************
*  NAME
*     lGetElemUlong64First() -- Find first ulong64 within a list 
*
*  SYNOPSIS
*     lListElem* lGetElemUlong64First(const lList *lp, 
*                                   int nm, 
*                                   lUlong64 val, 
*                                   const void **iterator) 
*
*  FUNCTION
*     Return the first element of list 'lp' where the attribute
*     with field name id 'nm' is equivalent with 'val'. Context
*     information will be stored in 'iterator'. 'iterator' might
*     be used in lGetElemUlong64Next() to get the next element.
*
*  INPUTS
*     const lList *lp       - list 
*     int nm                - ulong64 field name id 
*     lUlong64 val          - ulong64 value 
*     const void **iterator - iterator 
*
*  RESULT
*     lListElem* - element or nullptr
******************************************************************************/
lListElem *lGetElemUlong64FirstRW(const lList *lp, int nm, lUlong64 val, const void **iterator) {
   lListElem *ep = nullptr;
   int pos;

   DENTER(CULL_LAYER);

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
      ep = cull_hash_first(lp->descr[pos].ht, &val,
                           mt_is_unique(lp->descr[pos].mt), iterator);
      DRETURN(ep);
   } else {
      /* seek for element */
      for_each_rw(ep, lp) {
         lUlong64 s = lGetPosUlong64(ep, pos);
         if (s == val) {
            *iterator = ep;
            DRETURN(ep);
         }
      }
   }

   DRETURN(nullptr);
}

const lListElem *lGetElemUlong64First(const lList *lp, int nm, lUlong64 val, const void **iterator) {
   return lGetElemUlong64FirstRW(lp, nm, val, iterator);
}

/****** cull/multitype/lGetElemUlong64Next() **********************************
*  NAME
*     lGetElemUlong64Next() -- Find next ulong64 element within a list 
*
*  SYNOPSIS
*     lListElem* lGetElemUlong64Next(const lList *lp, 
*                                    int nm, 
*                                    lUlong64 val, 
*                                    const void **iterator) 
*
*  FUNCTION
*     This function might be used after a call to lGetElemUlong64First().
*     It expects 'iterator' to contain context information which
*     makes it possible to find the next element within list 'lp'
*     where the attribute with field name id 'nm' is equivalent with
*     'val'. 
*
*  INPUTS
*     const lList *lp       - list 
*     int nm                - ulong64 field name id 
*     lUlong64 val          - value 
*     const void **iterator - iterator 
*
*  RESULT
*     lListElem* - next element or nullptr
******************************************************************************/
lListElem *lGetElemUlong64NextRW(const lList *lp, int nm, lUlong64 val, const void **iterator) {
   lListElem *ep;
   int pos;

   DENTER(CULL_LAYER);

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

const lListElem *lGetElemUlong64Next(const lList *lp, int nm, lUlong64 val, const void **iterator) {
   return lGetElemUlong64NextRW(lp, nm, val, iterator);
}

/****** cull/multitype/lDelSubCaseStr() ***************************************
*  NAME
*     lDelSubCaseStr() -- removes elem specified by a string field nm 
*
*  SYNOPSIS
*     int lDelSubCaseStr(lListElem* ep, int nm, const char* str, 
*                        int snm) 
*
*  FUNCTION
*     removes an element specified by a string field nm an a string 
*     str which is contained in the sublist snm of ep 
*
*  INPUTS
*     lListElem* ep       - element 
*     int nm              - field id 
*     const char* str     - string 
*     int snm             - filed id of the element within element 
*
*  RESULT
*     1 if the element was found an removed 
*     0 in case of error
******************************************************************************/
int lDelSubCaseStr(lListElem *ep, int nm, const char *str, int snm) {
   int ret, sublist_pos;

   DENTER(CULL_LAYER);

   /* get position of sublist in ep */
   sublist_pos = lGetPosViaElem(ep, snm, SGE_DO_ABORT);

   ret = lDelElemCaseStr(&(ep->cont[sublist_pos].glp), nm, str);

   /* remember that field changed */
   if (ret == 1) {
      sge_bitfield_set(&(ep->changed), sublist_pos);
   }

   DRETURN(ret);
}

/****** cull/multitype/lDelElemCaseStr() **************************************
*  NAME
*     lDelElemCaseStr() -- removes elem specified by a string field nm 
*
*  SYNOPSIS
*     int lDelElemCaseStr(lList** lpp, int nm, const char* str) 
*
*  FUNCTION
*     This function removes an element specified by nm and str from 
*     the list lpp. 
*     If the list does not contain elements after this operation, it 
*     will be deleted too.
*
*  INPUTS
*     lList** lpp       - list 
*     int nm            - field id of the element which 
*                         should be removed 
*     const char* str   - value of the attribute identified by nm 
*
*  RESULT
*     1 if the element was found and removed
*     0 in case of error 
******************************************************************************/
int lDelElemCaseStr(lList **lpp, int nm, const char *str) {
   lListElem *ep;

   DENTER(CULL_LAYER);

   if (!lpp || !str) {
      DPRINTF(("error: nullptr ptr passed to lDelElemCaseStr\n"));
      DRETURN(0);
   }

   /* empty list ? */
   if (!*lpp) {
      DRETURN(1);
   }

   /* seek elemtent */
   ep = lGetElemCaseStr(*lpp, nm, str);
   if (ep) {
      lRemoveElem(*lpp, &ep);
      if (lGetNumberOfElem(*lpp) == 0) {
         lFreeList(lpp);
      }
   }

   DRETURN(1);
}

/****** cull/multitype/lGetSubCaseStr() ***************************************
*  NAME
*     lGetSubCaseStr() -- returns elem specified by a string field nm 
*
*  SYNOPSIS
*     lListElem* lGetSubCaseStr(const lListElem* ep, int nm, 
*                               const char* str, int snm) 
*
*  FUNCTION
*     returns an element specified by a string field nm and a string 
*     str from a sublist snm of the element ep 
*
*  INPUTS
*     const lListElem* ep - element pointer 
*     int nm              - field within an element of the sublist 
*     const char* str     - string 
*     int snm             - field within ep which identifies the 
*                           sublist 
*
*  RESULT
*     nullptr if element was not found or in case of an error
*     otherwise pointer to element 
******************************************************************************/
lListElem *lGetSubCaseStr(const lListElem *ep, int nm, const char *str,
                          int snm) {
   int sublist_pos;
   lListElem *ret;

   DENTER(CULL_LAYER);

   /* get position of sublist in ep */
   sublist_pos = lGetPosViaElem(ep, snm, SGE_DO_ABORT);

   ret = lGetElemCaseStr(ep->cont[sublist_pos].glp, nm, str);

   DRETURN(ret);
}

/****** cull/multitype/lGetElemCaseStr() **************************************
*  NAME
*     lGetElemCaseStr() -- returns element specified by a string field 
*
*  SYNOPSIS
*     lListElem* lGetElemCaseStr(const lList* lp, int nm, 
*                                const char* str) 
*
*  FUNCTION
*     This functions returns an element specified by a string 
*     field nm and str from the list lp. 
*
*  INPUTS
*     const lList* lp - Pointer to a list 
*     int nm          - Constant specifying an attribute within an 
*                       element of lp 
*     const char* str - string 
*
*  RESULT
*     nullptr when element is not found or an error occured
*     otherwise the pointer to an element 
*
******************************************************************************/
lListElem *lGetElemCaseStr(const lList *lp, int nm, const char *str) {
   lListElem *ep;
   int pos;
   const char *s;
   int data_type;
   const lDescr *listDescriptor;

   DENTER(CULL_LAYER);
   if (!str) {
      DPRINTF(("error: nullptr ptr passed to lGetElemCaseStr\n"));
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
      DPRINTF((":::::::::::::::: lGetElemCaseStr - data type is not lStringT !!! :::::::"));
      CRITICAL(MSG_CULL_GETELEMCASESTRERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   /* seek for element */
   for_each_rw(ep, lp) {
      s = lGetPosString(ep, pos);
      if (s && !SGE_STRCASECMP(s, str)) {
         DRETURN(ep);
      }
   }

   DRETURN(nullptr);
}

/****** cull/multitype/lGetElemHost() *****************************************
*  NAME
*     lGetElemHost() -- returns an element specified by a hostname 
*
*  SYNOPSIS
*     lListElem* lGetElemHost(const lList* lp, int nm, const char* str) 
*
*  FUNCTION
*     returns an element specified by a string field nm and a hostname
*     from the list lp 
*
*  INPUTS
*     const lList* lp - Pointer to an element which contains a hostname 
*     int nm          - host field containing the hostname 
*     const char* str - hostname 
*
*  RESULT
*     nullptr when the list does not contain the element or in case of
*     error otherwise pointer to an element
******************************************************************************/
lListElem *lGetElemHostRW(const lList *lp, int nm, const char *str) {
   const void *iterator = nullptr;
   return lGetElemHostFirstRW(lp, nm, str, &iterator);
}

const lListElem *lGetElemHost(const lList *lp, int nm, const char *str) {
   return lGetElemHostRW(lp, nm, str);
}

/****** cull/multitype/lGetElemHostFirst() ************************************
*  NAME
*     lGetElemHostFirst() -- lGetElemHostFirst for hostnames 
*
*  SYNOPSIS
*     lListElem* lGetElemHostFirst(const lList *lp, int nm, const char *str, 
*                                  const void **iterator) 
*
*  FUNCTION
*     lGetElemHostFirst for hostnames 
*
*  INPUTS
*     const lList *lp       - list 
*     int nm                - hostname field id 
*     const char *str       - hostname 
*     const void **iterator - iterator 
*
*  RESULT
*     lListElem* - element or nullptr
******************************************************************************/
lListElem *lGetElemHostFirstRW(const lList *lp, int nm, const char *str, const void **iterator) {
   int pos;
   int data_type;
   lListElem *ep = nullptr;
   const lDescr *listDescriptor = nullptr;
   char uhost[CL_MAXHOSTLEN + 1];
   char cmphost[CL_MAXHOSTLEN + 1];
   const char *s = nullptr;

   DENTER(TOP_LAYER);

   /* check for null pointers */
   if ((str == nullptr) || (lp == nullptr)) {
      DPRINTF(("error: nullptr ptr passed to lGetElemHostFirst\n"));
      DRETURN(nullptr);
   }

   /* run time type checking */
   listDescriptor = lGetListDescr(lp);
   pos = lGetPosInDescr(listDescriptor, nm);
   data_type = lGetPosType(listDescriptor, pos);
   if ((pos < 0) || (data_type != lHostT)) {
      if (data_type != lHostT) {
         DPRINTF((":::::::::::::::: lGetElemHostFirst - data type is not lHostT !!! :::::::\n"));
      }
      CRITICAL(MSG_CULL_GETELEMHOSTERRORXRUNTIMETYPE_S, lNm2Str(nm));
      DRETURN(nullptr);
   }

   *iterator = nullptr;
   if (lp->descr[pos].ht != nullptr) {
      /* we have a hash table */
      char host_key[CL_MAXHOSTLEN + 1];
      sge_hostcpy(host_key, str);
      sge_strtoupper(host_key, CL_MAXHOSTLEN);
      ep = cull_hash_first(lp->descr[pos].ht, host_key,
                           mt_is_unique(lp->descr[pos].mt), iterator);
      DRETURN(ep);
   } else {
      /* expensive host search algorithm */

      /* copy searched hostname */
      sge_hostcpy(uhost, str);

      /* sequence search */
      for_each_rw(ep, lp) {
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

const lListElem *lGetElemHostFirst(const lList *lp, int nm, const char *str, const void **iterator) {
   return lGetElemHostFirstRW(lp, nm, str, iterator);
}

/****** cull/multitype/lGetElemHostNext() *************************************
*  NAME
*     lGetElemHostNext() -- lGetElemHostNext() for hostnames 
*
*  SYNOPSIS
*     lListElem* lGetElemHostNext(const lList *lp, 
*                                 int nm, 
*                                 const char *str, 
*                                 const void **iterator) 
*
*  FUNCTION
*     lGetElemHostNext() for hostnames 
*
*  INPUTS
*     const lList *lp       - list 
*     int nm                - hostname field id 
*     const char *str       - hostname 
*     const void **iterator - iterator 
*
*  RESULT
*     lListElem* - element or nullptr
******************************************************************************/
lListElem *lGetElemHostNextRW(const lList *lp, int nm, const char *str, const void **iterator) {
   int pos;
   lListElem *ep = nullptr;
   const lDescr *listDescriptor = nullptr;
   char uhost[CL_MAXHOSTLEN + 1];
   char cmphost[CL_MAXHOSTLEN + 1];
   const char *s = nullptr;

   DENTER(TOP_LAYER);

   /* check for null *iterator and */
   /* check for null pointers */
   if ((str == nullptr) || (lp == nullptr) || (*iterator == nullptr)) {
      DPRINTF(("error: nullptr ptr passed to lGetElemHostNext\n"));
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

const lListElem *lGetElemHostNext(const lList *lp, int nm, const char *str, const void **iterator) {
   return lGetElemHostNextRW(lp, nm, str, iterator);
}

/****** cull/multitype/lGetSubHost() ******************************************
*  NAME
*     lGetSubHost() -- returns elem specified by a string field nm 
*
*  SYNOPSIS
*     lListElem* lGetSubHost(const lListElem* ep, int nm, 
*                            const char* str, int snm) 
*
*  FUNCTION
*     returns an element specified by a string field nm and the 
*     hostname str from the sublist snm of the element ep 
*
*  INPUTS
*     const lListElem* ep - element pointer 
*     int nm              - field id within an sublist element of ep 
*     const char* str     - hostname 
*     int snm             - field id of a sublist in ep 
*
*  RESULT
*     nullptr if element was not found or in case of error
*     otherwise pointer to element 
******************************************************************************/
lListElem *lGetSubHostRW(const lListElem *ep, int nm, const char *str, int snm) {
   int sublist_pos;
   lListElem *ret;

   DENTER(CULL_LAYER);

   /* get position of sublist in ep */
   sublist_pos = lGetPosViaElem(ep, snm, SGE_DO_ABORT);

   ret = lGetElemHostRW(ep->cont[sublist_pos].glp, nm, str);

   DRETURN(ret);
}

const lListElem *lGetSubHost(const lListElem *ep, int nm, const char *str, int snm) {
   return lGetSubHostRW(ep, nm, str, snm);
}

/****** cull/multitype/lDelElemHost() ****************************************
*  NAME
*     lDelElemHost() -- removes elem specified by a lHostT field nm 
*
*  SYNOPSIS
*     int lDelElemHost(lList** lpp, int nm, const char* str) 
*
*  FUNCTION
*     removes an element specified by a string field nm and the 
*     hostname str from the list referenced by lpp.
*     If it is the last element within lpp the list itself will be 
*     deleted.
*
*  INPUTS
*     lList** lpp       - list 
*     int nm            - field id 
*     const char* str   - string  
*
*  RESULT
*     1 if the host element was found and removed 
*     0 in case of an error
******************************************************************************/
int lDelElemHost(lList **lpp, int nm, const char *str) {
   lListElem *ep;

   DENTER(CULL_LAYER);

   if (!lpp || !str) {
      DPRINTF(("error: nullptr ptr passed to lDelElemHost\n"));
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

/****** cull/multitype/lGetPosName() ****************************************
*  NAME
*     lGetPosName() -- Returns name at position
*
*  SYNOPSIS
*     int lGetPosName(const lDescr *dp, int pos) 
*
*  FUNCTION
*     Returns the name at specified position in a descriptor array. The
*     Position must be inside the valid range of the descriptor. Returns
*     NoName if descriptor is nullptr or pos < 0.
*
*  INPUTS
*     const lDescr *dp - Descriptor 
*     int pos          - Position 
*
*  RESULT
*     int - Name 
******************************************************************************/
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

