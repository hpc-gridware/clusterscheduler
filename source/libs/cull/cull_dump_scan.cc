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
#include <cstdio>
#include <cstring>
#include <cctype>

/* do not compile in monitoring code */
#ifndef NO_SGE_COMPILE_DEBUG
#define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_dstring.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_string.h"

#include "cull/cull_dump_scan.h"
#include "cull/cull_listP.h"
#include "cull/cull_multitypeP.h"
#include "cull/cull_lerrnoP.h"

#include "basis_types.h"

#define READ_LINE_LENGHT MAX_STRING_SIZE

#define INDENT_STRING      "   "

static int space_comment(char *s);

static int fGetLine(FILE *fp, char *line, int max_line);

static int fGetBra(FILE *fp);

static int fGetKet(FILE *fp);

static int fGetDescr(FILE *fp, lDescr *dp);

static int fGetInt(FILE *fp, lInt *value);

static int fGetUlong(FILE *fp, lUlong *value);

static int fGetUlong64(FILE *fp, lUlong64 *value);

static int fGetString(FILE *fp, lString *value);

static int fGetHost(FILE *fp, lHost *value);

static int fGetFloat(FILE *fp, lFloat *value);

static int fGetDouble(FILE *fp, lDouble *value);

static int fGetLong(FILE *fp, lLong *value);

static int fGetChar(FILE *fp, lChar *value);

static int fGetBool(FILE *fp, lBool *value);

static int fGetList(FILE *fp, lList **value);

static int fGetObject(FILE *fp, lListElem **value);

/****** cull/dump_scan/lDumpDescr() ****************************************
*  NAME
*     lDumpDescr() -- Write a descriptor (for debugging purpose)
*
*  SYNOPSIS
*     int lDumpDescr(FILE *fp, const lDescr *dp, int indent) 
*
*  FUNCTION
*     Write a descriptor (for debugging purpose) 
*
*  INPUTS
*     FILE *fp         - file pointer 
*     const lDescr *dp - descriptor 
*     int indent       -  
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error
******************************************************************************/
int lDumpDescr(FILE *fp, const lDescr *dp, int indent) {
   int i, ret = ~EOF;
   char space[256];

   DENTER(CULL_LAYER);

   space[0] = '\0';
   for (i = 0; i < indent; i++)
      strcat(space, INDENT_STRING);

   if (!fp) {
      LERROR(LEFILENULL);
      DRETURN(-1);
   }
   ret = fprintf(fp, "%s{ /* DESCR BEGIN */\n", space);

   if (!dp) {
      LERROR(LEDESCRNULL);
      DRETURN(-1);
   }
   ret = fprintf(fp, "%s/* NUMBER OF DESCR FIELDS */ %d\n", space,
                 lCountDescr(dp));

   for (i = 0; mt_get_type(dp[i].mt) != lEndT && ret != EOF; i++) {
      ret = fprintf(fp, "%s/* %-20.20s */ { %d, %d }\n", space,
                    lNm2Str(dp[i].nm), dp[i].nm, dp[i].mt);
   }

   ret = fprintf(fp, "%s} /* DESCR END */\n", space);

   DRETURN((ret == EOF) ? -1 : 0);
}

/****** cull/dump_scan/lUndumpDescr() ****************************************
*  NAME
*     lUndumpDescr() -- Read a descriptor from file (debug) 
*
*  SYNOPSIS
*     lDescr* lUndumpDescr(FILE *fp) 
*
*  FUNCTION
*     Read a descriptor from file (for debugging purposes) 
*
*  INPUTS
*     FILE *fp - file stream 
*
*  RESULT
*     lDescr* - descriptor 
*******************************************************************************/
lDescr *lUndumpDescr(FILE *fp) {
   int n, i;
   lDescr *dp = nullptr;

   DENTER(CULL_LAYER);

   if (!fp) {
      LERROR(LEFILENULL);
      DRETURN(nullptr);
   }

   /* read bra */
   if (fGetBra(fp)) {
      printf("bra is missing\n");
      LERROR(LESYNTAX);
      DRETURN(nullptr);
   }

   /* read Descriptor Count */
   if (fGetInt(fp, &n)) {
      printf("reading integer from dump file failed\n");
      LERROR(LEFIELDREAD);
      DRETURN(nullptr);
   }

   if (!(dp = (lDescr *) sge_malloc(sizeof(lDescr) * (n + 1)))) {
      LERROR(LEMALLOC);
      DRETURN(nullptr);
   }

   for (i = 0; i < n; i++) {
      /* read descriptor */
      if (fGetDescr(fp, &(dp[i]))) {
         LERROR(LEFGETDESCR);
         DRETURN(nullptr);
      }
   }
   dp[i].nm = NoName;
   dp[i].mt = lEndT;
   dp[i].ht = nullptr;

   /* read ket */
   if (fGetKet(fp)) {
      printf("ket is missing");
      sge_free(&dp);
      LERROR(LESYNTAX);
      DRETURN(nullptr);
   }

   DRETURN(dp);
}

/****** cull/dump_scan/lDumpElem() ********************************************
*  NAME
*     lDumpElem() -- Dump a given element into a file 
*
*  SYNOPSIS
*     int lDumpElem(const char *fname, const lListElem *ep, int indent) 
*
*  FUNCTION
*     Dump a given element into a file 
*
*  INPUTS
*     const char *fname   - filename 
*     const lListElem *ep - element 
*     int indent          - 
*
*  RESULT
*     int - error state
*        -1 - Error
*         0 - OK
*
*  NOTES
*     MT-NOTE: lDumpElem() is not MT safe
******************************************************************************/
int lDumpElem(const char *fname, const lListElem *ep, int indent) {
   int ret;
   FILE *fp;

   fp = fopen(fname, "w");
   if (fp != nullptr) {
      ret = lDumpElemFp(fp, ep, indent);
      FCLOSE(fp);
   } else {
      LERROR(LEOPEN);
      ret = -1;
   }
   return ret;
   FCLOSE_ERROR:
   LERROR(LECLOSE);
   return -1;
}

/****** cull/dump_scan/lDumpElemFp() ******************************************
*  NAME
*     lDumpElemFp() -- Dump a given element into FILE stream 
*
*  SYNOPSIS
*     int lDumpElemFp(FILE *fp, const lListElem *ep, int indent) 
*
*  FUNCTION
*     Dump a given element into FILE stream
*
*  INPUTS
*     FILE *fp            - file stream 
*     const lListElem *ep - element 
*     int indent          - 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error 
*
*  NOTES
*     MT-NOTE: lDumpElemFp() is not MT safe
******************************************************************************/
int lDumpElemFp(FILE *fp, const lListElem *ep, int indent) {
   int i, ret = ~EOF;
   lList *tlp;
   lListElem *tep;
   char space[256];
   const char *str;
   dstring dstr = DSTRING_INIT;

   DENTER(CULL_LAYER);

   space[0] = '\0';
   for (i = 0; i < indent; i++)
      strcat(space, INDENT_STRING);

   if (!fp) {
      LERROR(LEFILENULL);
      DRETURN(-1);
   }
   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   ret = fprintf(fp, "%s{ \n", space);
   for (i = 0, ret = 0; ep->descr[i].nm != NoName && ret != EOF; i++) {
      char *tok = nullptr;

      switch (mt_get_type(ep->descr[i].mt)) {
         case lIntT:
            ret = fprintf(fp, "%s/* %-20.20s */ %d\n",
                          space, lNm2Str(ep->descr[i].nm), lGetPosInt(ep, i));
            break;
         case lUlongT:
            ret = fprintf(fp, "%s/* %-20.20s */ " sge_u32 "\n",
                          space, lNm2Str(ep->descr[i].nm), lGetPosUlong(ep, i));
            break;
         case lUlong64T:
            ret = fprintf(fp, "%s/* %-20.20s */ " sge_u64 "\n",
                          space, lNm2Str(ep->descr[i].nm), lGetPosUlong64(ep, i));
            break;
         case lStringT:
            str = lGetPosString(ep, i);
            /* quote " inside str */
            if ((tok = sge_strtok(str, "\"")) != nullptr) {
               sge_dstring_append(&dstr, tok);
               while ((tok = sge_strtok(nullptr, "\"")) != nullptr) {
                  sge_dstring_append(&dstr, "\\\"");
                  sge_dstring_append(&dstr, tok);
               }
            }
            str = sge_dstring_get_string(&dstr);
            ret = fprintf(fp, "%s/* %-20.20s */ \"%s\"\n",
                          space, lNm2Str(ep->descr[i].nm), str != nullptr ? str : "");
            sge_dstring_clear(&dstr);
            break;
         case lHostT:
            str = lGetPosHost(ep, i);
            ret = fprintf(fp, "%s/* %-20.20s */ \"%s\"\n",
                          space, lNm2Str(ep->descr[i].nm), str != nullptr ? str : "");
            break;
         case lFloatT:
            ret = fprintf(fp, "%s/* %-20.20s */ %f\n",
                          space, lNm2Str(ep->descr[i].nm), lGetPosFloat(ep, i));
            break;
         case lDoubleT:
            ret = fprintf(fp, "%s/* %-20.20s */ %f\n",
                          space, lNm2Str(ep->descr[i].nm), lGetPosDouble(ep, i));
            break;
         case lLongT:
            ret = fprintf(fp, "%s/* %-20.20s */%ld \n",
                          space, lNm2Str(ep->descr[i].nm), lGetPosLong(ep, i));
            break;
         case lCharT:
            ret = fprintf(fp, "%s/* %-20.20s */ %c\n",
                          space, lNm2Str(ep->descr[i].nm), lGetPosChar(ep, i));
            break;
         case lBoolT:
            ret = fprintf(fp, "%s/* %-20.20s */ %d\n",
                          space, lNm2Str(ep->descr[i].nm), lGetPosBool(ep, i));
            break;
         case lRefT:
            ret = fprintf(fp, "%s/* %-20.20s */ %ld\n",
                          space, lNm2Str(ep->descr[i].nm), (long) lGetPosRef(ep, i));
            break;
         case lObjectT:
            if ((tep = lGetPosObject(ep, i)) == nullptr)
               ret = fprintf(fp, "%s/* %-20.20s */ none\n",
                             space, lNm2Str(ep->descr[i].nm));
            else {
               ret = fprintf(fp, "%s/* %-20.20s */ object\n",
                             space, lNm2Str(ep->descr[i].nm));
               if (ret != EOF)
                  ret = lDumpObject(fp, tep, indent + 1);
            }
            break;
         case lListT:
            if ((tlp = lGetPosList(ep, i)) == nullptr)
               ret = fprintf(fp, "%s/* %-20.20s */ empty\n",
                             space, lNm2Str(ep->descr[i].nm));
            else {
               ret = fprintf(fp, "%s/* %-20.20s */ full\n",
                             space, lNm2Str(ep->descr[i].nm));
               if (ret != EOF)
                  ret = lDumpList(fp, tlp, indent + 1);
            }
            break;
      }
   }
   sge_dstring_free(&dstr);

   ret = fprintf(fp, "%s}\n", space);

   DRETURN((ret == EOF) ? -1 : 0);
}

/****** cull/dump_scan/lDumpObject() ********************************************
*  NAME
*     lDumpObject() -- Writes an object to a FILE stream
*
*  SYNOPSIS
*     int lDumpObject(FILE *fp, const lListElem *ep, int indent) 
*
*  FUNCTION
*     Writes an object to a FILE stream. 
*
*  INPUTS
*     FILE *fp             - file stream 
*     const lListElem *ep  - object 
*     int indent           - 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error
*******************************************************************************/
int lDumpObject(FILE *fp, const lListElem *ep, int indent) {
   int i, ret = ~EOF;

   char space[256];

   DENTER(CULL_LAYER);

   space[0] = '\0';
   for (i = 0; i < indent; i++)
      strcat(space, INDENT_STRING);

   if (!fp) {
      LERROR(LEFILENULL);
      DRETURN(-1);
   }
   if (!ep) {
      LERROR(LEELEMNULL);
      DRETURN(-1);
   }

   ret = fprintf(fp, "%s{ /* OBJECT BEGIN */\n", space);

   ret = lDumpDescr(fp, ep->descr, indent);

   ret = lDumpElemFp(fp, ep, indent);

   ret = fprintf(fp, "%s} /* OBJECT END */\n", space);

   DRETURN((ret == EOF) ? -1 : 0);

}

/****** cull/dump_scan/lDumpList() ********************************************
*  NAME
*     lDumpList() -- Writes a list to a FILE stream
*
*  SYNOPSIS
*     int lDumpList(FILE *fp, const lList *lp, int indent) 
*
*  FUNCTION
*     Writes a list to a FILE stream. 
*
*  INPUTS
*     FILE *fp        - file stream 
*     const lList *lp - list 
*     int indent      - 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error
*
*  NOTES
*     MT-NOTE: lDumpList() is not MT safe
*******************************************************************************/
int lDumpList(FILE *fp, const lList *lp, int indent) {
   const lListElem *ep;
   int i, ret = ~EOF;

   char space[256];

   DENTER(CULL_LAYER);

   space[0] = '\0';
   for (i = 0; i < indent; i++)
      strcat(space, INDENT_STRING);

   if (!fp) {
      LERROR(LEFILENULL);
      DRETURN(-1);
   }
   if (!lp) {
      LERROR(LELISTNULL);
      DRETURN(-1);
   }

   ret = fprintf(fp, "%s{ /* LIST BEGIN */\n", space);

   ret = fprintf(fp, "%s/* LISTNAME               */ \"%s\"\n", space,
                 lGetListName(lp));
   ret = fprintf(fp, "%s/* NUMBER OF ELEMENTS     */ " sge_uu32 "\n", space,
                 lGetNumberOfElem(lp));

   ret = lDumpDescr(fp, lGetListDescr(lp), indent);

   for (ep = lFirst(lp); ep && ret != EOF; ep = lNext(ep))
      ret = lDumpElemFp(fp, ep, indent);

   ret = fprintf(fp, "%s} /* LIST END */\n", space);

   DRETURN((ret == EOF) ? -1 : 0);

}

/****** cull/dump_scan/lUndumpElem() ******************************************
*  NAME
*     lUndumpElem() -- Read element from FILE stream 
*
*  SYNOPSIS
*     lListElem* lUndumpElem(FILE *fp, const lDescr *dp) 
*
*  FUNCTION
*     Read element from FILE stream 
*
*  INPUTS
*     FILE *fp         - file stream 
*     const lDescr *dp - descriptor 
*
*  RESULT
*     lListElem* - Read element 
******************************************************************************/
lListElem *lUndumpElem(const char *fname, const lDescr *dp) {
   lListElem *ep = nullptr;
   FILE *fp;

   DENTER(CULL_LAYER);

   fp = fopen(fname, "r");
   if (fp == nullptr) {
      LERROR(LEOPEN);
   } else {
      ep = lUndumpElemFp(fp, dp);
   }

   DRETURN(ep);
}

/****** cull/dump_scan/lUndumpElemFp() ******************************************
*  NAME
*     lUndumpElemFp() -- Read element from FILE stream 
*
*  SYNOPSIS
*     lListElem* lUndumpElemFp(FILE *fp, const lDescr *dp) 
*
*  FUNCTION
*     Read element from FILE stream 
*
*  INPUTS
*     FILE *fp         - file stream 
*     const lDescr *dp - descriptor 
*
*  RESULT
*     lListElem* - Read element 
******************************************************************************/
lListElem *lUndumpElemFp(FILE *fp, const lDescr *dp) {
   lListElem *ep;
   int n, i;
   int ret = 0;
   char *str;
   u_long32 dummy;

   DENTER(CULL_LAYER);

   if (!fp) {
      LERROR(LEFILENULL);
      DRETURN(nullptr);
   }
   if (!dp) {
      LERROR(LEDESCRNULL);
      DRETURN(nullptr);
   }
   if (!(ep = lCreateElem(dp))) {
      LERROR(LECREATEELEM);
      DRETURN(nullptr);
   }

   if ((n = lCountDescr(dp)) <= 0) {
      LERROR(LECOUNTDESCR);
      lFreeElem(&ep);
      DRETURN(nullptr);
   }

   /* read bra */
   if (fGetBra(fp)) {
      printf("bra is missing\n");
      LERROR(LESYNTAX);
      lFreeElem(&ep);
      DRETURN(nullptr);
   }

   for (i = 0; i < n && ret == 0; i++) {
      switch (mt_get_type(dp[i].mt)) {
         case lIntT:
            ret = fGetInt(fp, &(ep->cont[i].i));
            break;
         case lUlongT:
            ret = fGetUlong(fp, &(ep->cont[i].ul));
            break;
         case lUlong64T:
            ret = fGetUlong64(fp, &(ep->cont[i].ul64));
            break;
         case lStringT:
            ret = fGetString(fp, &str);
            if (ret == 0) {
               lSetPosString(ep, i, str);
               sge_free(&str);             /* fGetString strdup's */
            }
            break;
         case lHostT:
            ret = fGetHost(fp, &str);
            if (ret == 0) {
               lSetPosHost(ep, i, str);
               sge_free(&str);             /* fGetHost strdup's */
            }
            break;
         case lFloatT:
            ret = fGetFloat(fp, &(ep->cont[i].fl));
            break;
         case lDoubleT:
            ret = fGetDouble(fp, &(ep->cont[i].db));
            break;
         case lLongT:
            ret = fGetLong(fp, &(ep->cont[i].l));
            break;
         case lCharT:
            ret = fGetChar(fp, &(ep->cont[i].c));
            break;
         case lBoolT:
            ret = fGetBool(fp, &(ep->cont[i].b));
            break;
         case lRefT:
            /* we will not undump references! But we have to skip the line! */
            ret = fGetUlong(fp, &dummy);
            ep->cont[i].ref = nullptr;
            break;
         case lObjectT:
            ret = fGetObject(fp, &(ep->cont[i].obj));
            break;
         case lListT:
            ret = fGetList(fp, &(ep->cont[i].glp));
            break;
         default:
            lFreeElem(&ep);
            unknownType("lUndumpElemFp");
      }
   }

   /* error handling for loop */
   if (ret != 0) {
      lFreeElem(&ep);
      LERROR(LEFIELDREAD);
      DRETURN(nullptr);
   }

   /* read ket */
   if (fGetKet(fp)) {
      lFreeElem(&ep);
      printf("ket is missing\n");
      LERROR(LESYNTAX);
      DRETURN(nullptr);
   }

   DRETURN(ep);
}

/****** cull/dump_scan/lUndumpObject() ******************************************
*  NAME
*     lUndumpObject() -- Reads a by lDumpList dumped dump 
*
*  SYNOPSIS
*     lListElem* lUndumpObject(FILE *fp) 
*
*  FUNCTION
*     Reads a by lDumpList dumped dump into the memory. 
*
*  INPUTS
*     FILE *fp         - file pointer 
*
*  RESULT
*     lListElem* - Read list element
*
*  NOTES
*
******************************************************************************/
lListElem *lUndumpObject(FILE *fp) {
   lListElem *ep;
   lDescr *dp = nullptr;

   DENTER(CULL_LAYER);

   if (!fp) {
      LERROR(LEFILENULL);
      DRETURN(nullptr);
   }
   /* read bra */
   if (fGetBra(fp)) {
      printf("bra is missing\n");
      LERROR(LESYNTAX);
      DRETURN(nullptr);
   }

   /* read Descriptor from file */
   if ((dp = lUndumpDescr(fp)) == nullptr) {
      LERROR(LEFGETDESCR);
      DRETURN(nullptr);
   }

   if (lCountDescr(dp) <= 0) {
      LERROR(LECOUNTDESCR);
      sge_free(&dp);
      DRETURN(nullptr);
   }

   if ((ep = lUndumpElemFp(fp, dp)) == nullptr) {
      LERROR(LEUNDUMPELEM);
      sge_free(&dp);
      DRETURN(nullptr);
   }

   sge_free(&dp);

   /* read ket */
   if (fGetKet(fp)) {
      lFreeElem(&ep);
      printf("ket is missing\n");
      LERROR(LESYNTAX);
      DRETURN(nullptr);
   }

   DRETURN(ep);
}

/****** cull/dump_scan/lUndumpList() ******************************************
*  NAME
*     lUndumpList() -- Reads a by lDumpList dumped dump 
*
*  SYNOPSIS
*     lList* lUndumpList(FILE *fp, const char *name, const lDescr *dp) 
*
*  FUNCTION
*     Reads a by lDumpList dumped dump into the memory. 
*
*  INPUTS
*     FILE *fp         - file pointer 
*     const char *name - new name of list or nullptr if the old name in the
*                        dumpfile should be used as listname 
*     const lDescr *dp - new list descriptor or nullptr if the old list
*                        descriptor should be used as list descriptor 
*
*  RESULT
*     lList* - Read list 
*
*  NOTES
*     Actually a type/name matching is only performed for the list
*     itself and not for its sublists.
*     If an implementation of changed sublist descriptors is desired
*     we can probably use the following syntax for lUndumpList.
*     lList* lUndumpList(fp, name, formatstring, ...)
*     with formatstring like "%T(%I -> %T(%I->%T))" and the varargs 
*     list: ".....", lDescr1, fieldname1, lDescr2, fieldname2, lDescr3
*     or write a wrapper around lUndumpList which parses this format and 
*     hands over the varargs list to lUndumpList
******************************************************************************/
lList *lUndumpList(FILE *fp, const char *name, const lDescr *dp) {
   lList *lp = nullptr;
   lListElem *fep, *ep;
   lDescr *fdp = nullptr;
   int i, j, nelem, n, k;
   int *found;
   char *oldname;

   DENTER(CULL_LAYER);

   if (!fp) {
      LERROR(LEFILENULL);
      DRETURN(nullptr);
   }

   /* read bra */
   if (fGetBra(fp)) {
      printf("bra is missing\n");
      LERROR(LESYNTAX);
      DRETURN(nullptr);
   }
   /* read listname */
   if (fGetString(fp, &oldname)) {
      printf("fGetString failed\n");
      LERROR(LEFIELDREAD);
      DRETURN(nullptr);
   }

   /* read number of elems */
   if (fGetInt(fp, &nelem)) {
      printf("fGetInt failed\n");
      LERROR(LEFIELDREAD);
      DRETURN(nullptr);
   }

   /* read Descriptor from file */
   if (!(fdp = lUndumpDescr(fp))) {
      LERROR(LEFGETDESCR);
      DRETURN(nullptr);
   }

   if (!dp)                     /* dp is nullptr, use lDescr from dumpfile */
      dp = fdp;

   /* use old name (from file) if name is nullptr */
   if (!(lp = lCreateList((name) ? name : oldname, dp))) {
      sge_free(&fdp);
      LERROR(LECREATELIST);
      DRETURN(nullptr);
   }
   sge_free(&oldname);               /* fGetString strdup's */

   if ((n = lCountDescr(dp)) <= 0) {
      LERROR(LECOUNTDESCR);
      sge_free(&fdp);
      lFreeList(&lp);
      DRETURN(nullptr);
   }

   if (!(found = (int *) sge_malloc(sizeof(int) * n))) {
      LERROR(LEMALLOC);
      sge_free(&fdp);
      lFreeList(&lp);
      DRETURN(nullptr);
   }

   /* Initialize found array */
   for (i = 0; i < n; i++)
      found[i] = -1;

   /* Here warnings are displayed if there are additional or missing fields */
   for (j = 0; fdp[j].nm != NoName; j++) {
      for (i = 0; i < n; i++) {
         if (dp[i].nm == fdp[j].nm &&
             dp[i].mt == fdp[j].mt) {
            if (found[i] != -1)
                    DPRINTF(("lUndumpList: field %s found twice\n",
                            lNm2Str(dp[i].nm)));
            found[i] = j;
            break;
         }
      }
      if (i == n)
              DPRINTF(("lUndumpList: field %s not needed\n", lNm2Str(fdp[j].nm)));
   }

   for (i = 0; i < n; i++)
      if (found[i] == -1)
              DPRINTF(("lUndumpList: field %s not found\n", lNm2Str(dp[i].nm)));

   /* LOOP OVER THE LIST ELEMENTS */
   for (k = 0; k < nelem; k++) {
      if (!(fep = lUndumpElemFp(fp, fdp))) {
         LERROR(LEUNDUMPELEM);
         lFreeList(&lp);
         sge_free(&found);
         sge_free(&fdp);
         DRETURN(nullptr);
      }

      if (!(ep = lCreateElem(dp))) {
         lFreeList(&lp);
         sge_free(&found);
         sge_free(&fdp);
         LERROR(LECREATEELEM);
         DRETURN(nullptr);
      }

      for (i = 0; i < n; i++) {
         if (found[i] == -1) {
            continue;
         } else if (lCopySwitchPack(fep, ep, found[i], i, true, nullptr, nullptr) == -1) {
            lFreeList(&lp);
            lFreeElem(&ep);
            sge_free(&found);
            sge_free(&fdp);
            LERROR(LECOPYSWITCH);
            DRETURN(nullptr);
         }
      }
      lFreeElem(&fep);
      if (lAppendElem(lp, ep) == -1) {
         lFreeList(&lp);
         lFreeElem(&ep);
         sge_free(&found);
         sge_free(&fdp);
         LERROR(LEAPPENDELEM);
         DRETURN(nullptr);
      }

   }

   /* read ket */
   if (fGetKet(fp)) {
      lFreeList(&lp);
      printf("ket is missing\n");
      LERROR(LESYNTAX);
   }

   sge_free(&found);
   sge_free(&fdp);
   DRETURN(lp);
}

static int space_comment(char *s) {
   char *p, *t;

   DENTER(CULL_LAYER);

   while ((t = strstr(s, "/*"))) {
      if (!(p = strstr(t + 2, "*/"))) {
         DRETURN(-1);
      }
      while (t < p + 2)
         *t++ = ' ';
   }
   DRETURN(0);

}

static int fGetLine(FILE *fp, char *line, int max_line) {
   DENTER(CULL_LAYER);

   if (!fp) {
      LERROR(LEFILENULL);
      DRETURN(-1);
   }

   if (!(fgets(line, max_line, fp))) {
      LERROR(LEFGETS);
      DRETURN(-1);
   }
   if (space_comment(line)) {
      LERROR(LESPACECOMMENT);
      DRETURN(-1);
   }

   DRETURN(0);
}

static int fGetBra(FILE *fp) {
   char s[READ_LINE_LENGHT + 1];

   DENTER(CULL_LAYER);

   if (fGetLine(fp, s, READ_LINE_LENGHT)) {
      LERROR(LEFGETLINE);
      DRETURN(-1);
   }

   DRETURN(strstr(s, "{") ? 0 : -1);
}

static int fGetKet(FILE *fp) {
   char s[READ_LINE_LENGHT + 1];

   DENTER(CULL_LAYER);

   if (fGetLine(fp, s, READ_LINE_LENGHT)) {
      LERROR(LEFGETLINE);
      DRETURN(-1);
   }

   DRETURN(strstr(s, "}") ? 0 : -1);
}

static int fGetDescr(FILE *fp, lDescr *dp) {
   char s[READ_LINE_LENGHT + 1];
   int mt, nm;
   char bra[2], comma[2], ket[2];

   DENTER(CULL_LAYER);

   if (!fp) {
      LERROR(LEFILENULL);
      DRETURN(-1);
   }

   if (!dp) {
      LERROR(LEDESCRNULL);
      DRETURN(-1);
   }

   if (fGetLine(fp, s, READ_LINE_LENGHT)) {
      LERROR(LEFGETLINE);
      DRETURN(-1);
   }

   /* 
      We use this strange form of scanf to skip the 
      white space at the beginning. scanf is magic isn't it?
    */
   if (sscanf(s, "%1s %d %1s %d %1s", bra, &nm, comma, &mt, ket) != 5) {
      LERROR(LESSCANF);
      DRETURN(-1);
   }

   if (bra[0] != '{' || comma[0] != ',' || ket[0] != '}') {
      LERROR(LESYNTAX);
      DRETURN(-1);
   }

   dp->nm = nm;
   dp->mt = mt;
   dp->ht = nullptr;

   DRETURN(0);
}

static int fGetInt(FILE *fp, int *ip) {
   char s[READ_LINE_LENGHT + 1];

   DENTER(CULL_LAYER);

   if (!fp) {
      LERROR(LEFILENULL);
      DRETURN(-1);
   }

   if (fGetLine(fp, s, READ_LINE_LENGHT)) {
      LERROR(LEFGETLINE);
      DRETURN(-1);
   }

   if (sscanf(s, "%d", ip) != 1) {
      LERROR(LESSCANF);
      DRETURN(-1);
   }

   DRETURN(0);
}

static int fGetUlong(FILE *fp, lUlong *up) {
   char s[READ_LINE_LENGHT + 1];

   DENTER(CULL_LAYER);

   if (!fp) {
      LERROR(LEFILENULL);
      DRETURN(-1);
   }

   if (fGetLine(fp, s, READ_LINE_LENGHT)) {
      LERROR(LEFGETLINE);
      DRETURN(-1);
   }

   if (sscanf(s, sge_uu32, up) != 1) {
      LERROR(LESSCANF);
      DRETURN(-1);
   }

   DRETURN(0);
}

static int fGetUlong64(FILE *fp, lUlong64 *up) {
   char s[READ_LINE_LENGHT + 1];

   DENTER(CULL_LAYER);

   if (!fp) {
      LERROR(LEFILENULL);
      DRETURN(-1);
   }

   if (fGetLine(fp, s, READ_LINE_LENGHT)) {
      LERROR(LEFGETLINE);
      DRETURN(-1);
   }

   if (sscanf(s, sge_u64, up) != 1) {
      LERROR(LESSCANF);
      DRETURN(-1);
   }

   DRETURN(0);
}

static int fGetString(FILE *fp, lString *tp) {
   int i, j;
   char line[READ_LINE_LENGHT + 1];
   dstring sp = DSTRING_INIT;
   const char *s;

   DENTER(CULL_LAYER);

   if (!fp) {
      LERROR(LEFILENULL);
      DRETURN(-1);
   }
   if (fGetLine(fp, line, READ_LINE_LENGHT)) {
      LERROR(LEFGETLINE);
      DRETURN(-1);
   }
   s = line;

   while (isspace((int) *s)) {
      s++;
   }
   if (*s++ != '"') {
      LERROR(LESYNTAX);
      DRETURN(-1);
   }
   for (i = 0; s[i] != '\0' && s[i] != '"'; i++) {
      if (s[i] == '\\') {
         i++;
      }
      sge_dstring_append_char(&sp, s[i]);
   }
   if (s[i] != '"') {
      bool done = false;
      /* String is diveded by a newline */
      while (!done) {
         if (fGetLine(fp, line, READ_LINE_LENGHT)) {
            sge_dstring_free(&sp);
            LERROR(LEFGETLINE);
            DRETURN(-1);
         }
         s = line;
         for (j = 0; s[j] != '\0' && s[j] != '"'; j++, i++) {
            sge_dstring_append_char(&sp, s[j]);
         }
         if (s[j] == '"') {
            done = true;
            break;
         }
      }
   }

   s = sge_dstring_get_string(&sp);
   if (s == nullptr) {
      *tp = strdup("");
   } else {
      *tp = strdup(s);
   }

   sge_dstring_free(&sp);

   if (!(*tp)) {
      LERROR(LESTRDUP);
      DRETURN(-1);
   }

   DRETURN(0);
}

static int fGetHost(FILE *fp, lHost *tp) {
   int i;
   char line[READ_LINE_LENGHT + 1];
   char sp[READ_LINE_LENGHT + 1];
   char *s;

   DENTER(CULL_LAYER);

   if (!fp) {
      LERROR(LEFILENULL);
      DRETURN(-1);
   }

   if (fGetLine(fp, line, READ_LINE_LENGHT)) {
      LERROR(LEFGETLINE);
      DRETURN(-1);
   }
   s = line;

   while (isspace((int) *s))
      s++;
   if (*s++ != '"') {
      LERROR(LESYNTAX);
      DRETURN(-1);
   }
   for (i = 0; s[i] != '\0' && s[i] != '"'; i++)
      sp[i] = s[i];
   if (s[i] != '"') {
      LERROR(LESYNTAX);
      DRETURN(-1);
   }
   sp[i] = '\0';

   if (!(*tp = strdup(sp))) {
      LERROR(LESTRDUP);
      DRETURN(-1);
   }

   DRETURN(0);
}

static int fGetFloat(FILE *fp, lFloat *flp) {
   char s[READ_LINE_LENGHT + 1];

   DENTER(CULL_LAYER);

   if (!fp) {
      LERROR(LEFILENULL);
      DRETURN(-1);
   }

   if (fGetLine(fp, s, READ_LINE_LENGHT)) {
      LERROR(LEFGETLINE);
      DRETURN(-1);
   }

   if (sscanf(s, "%f", flp) != 1) {
      LERROR(LESSCANF);
      DRETURN(-1);
   }

   DRETURN(0);
}

static int fGetDouble(FILE *fp, lDouble *dp) {
   char s[READ_LINE_LENGHT + 1];

   DENTER(CULL_LAYER);

   if (!fp) {
      LERROR(LEFILENULL);
      DRETURN(-1);
   }

   if (fGetLine(fp, s, READ_LINE_LENGHT)) {
      LERROR(LEFGETLINE);
      DRETURN(-1);
   }

   if (sscanf(s, "%lf", dp) != 1) {
      LERROR(LESSCANF);
      DRETURN(-1);
   }

   DRETURN(0);
}

static int fGetLong(FILE *fp, lLong *lp) {
   char s[READ_LINE_LENGHT + 1];

   DENTER(CULL_LAYER);

   if (!fp) {
      LERROR(LEFILENULL);
      DRETURN(-1);
   }

   if (fGetLine(fp, s, READ_LINE_LENGHT)) {
      LERROR(LEFGETLINE);
      DRETURN(-1);
   }

   if (sscanf(s, "%ld", lp) != 1) {
      LERROR(LESSCANF);
      DRETURN(-1);
   }

   DRETURN(0);
}

static int fGetChar(FILE *fp, lChar *cp) {
   char s[READ_LINE_LENGHT + 1];

   DENTER(CULL_LAYER);

   if (!fp) {
      LERROR(LEFILENULL);
      DRETURN(-1);
   }

   if (fGetLine(fp, s, READ_LINE_LENGHT)) {
      LERROR(LEFGETLINE);
      DRETURN(-1);
   }

   if (sscanf(s, "%c", cp) != 1) {
      LERROR(LESSCANF);
      DRETURN(-1);
   }

   DRETURN(0);
}

static int fGetBool(FILE *fp, lBool *cp) {
   char s[READ_LINE_LENGHT + 1];
   int i = 0;

   DENTER(CULL_LAYER);

   if (!fp) {
      LERROR(LEFILENULL);
      DRETURN(-1);
   }

   if (fGetLine(fp, s, READ_LINE_LENGHT)) {
      LERROR(LEFGETLINE);
      DRETURN(-1);
   }

   if (sscanf(s, "%d", &i) != 1) {
      LERROR(LESSCANF);
      DRETURN(-1);
   }

   *cp = i;

   DRETURN(0);
}

static int fGetList(FILE *fp, lList **lpp) {
   char s[READ_LINE_LENGHT + 1];

   DENTER(CULL_LAYER);

   if (fp == nullptr) {
      LERROR(LEFILENULL);
      DRETURN(-1);
   }

   if (fGetLine(fp, s, READ_LINE_LENGHT)) {
      LERROR(LEFGETLINE);
      DRETURN(-1);
   }

   if (strstr(s, "empty") != nullptr)
      *lpp = nullptr;              /* empty sublist */
   else {
/*
      if (strstr(s, "full") == 0) {
         LERROR(LESYNTAX);
         DRETURN(-1);
      }
*/
      if ((*lpp = lUndumpList(fp, nullptr, nullptr)) == nullptr) {
         LERROR(LEUNDUMPLIST);
         DRETURN(-1);
      }
   }

   DRETURN(0);
}

static int fGetObject(FILE *fp, lListElem **epp) {
   char s[READ_LINE_LENGHT + 1];

   DENTER(CULL_LAYER);

   if (fp == nullptr) {
      LERROR(LEFILENULL);
      DRETURN(-1);
   }

   if (fGetLine(fp, s, READ_LINE_LENGHT)) {
      LERROR(LEFGETLINE);
      DRETURN(-1);
   }

   if (strstr(s, "none") != nullptr)
      *epp = nullptr;              /* no object stored */
   else {
      if (strstr(s, "object") == 0) {
         LERROR(LESYNTAX);
         DRETURN(-1);
      }

      if ((*epp = lUndumpObject(fp)) == nullptr) {
         LERROR(LEUNDUMPELEM);
         DRETURN(-1);
      }
      (*epp)->status = OBJECT_ELEM;
   }

   DRETURN(0);
}
