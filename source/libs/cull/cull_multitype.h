#pragma once
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

#include <cstdio>

#include "cull/cull_list.h"

#define SGE_NO_ABORT    0
#define SGE_DO_ABORT    1

#define for_each_attr(ATTR, LDP) for (ATTR = LDP->nm; ATTR != NoName; LDP++, ATTR = LDP->nm)

extern const char *multitypes[];

typedef int (*lCmpFunction)(lListElem *, lListElem *, int);

void lWriteDescrTo(const lDescr *dp, FILE *fp);

int _lGetPosInDescr(const lDescr *dp, int name);

int lGetPosInDescr(const lDescr *dp, int name);

int lGetPosType(const lDescr *dp, int pos);

int lCountDescr(const lDescr *dp);

lDescr *lCopyDescr(const lDescr *dp);

int lGetPosViaElem(const lListElem *element, int nm, int abort);

void lInit(const lNameSpace *namev);

const char *lNm2Str(int nm);

int lStr2Nm(const char *str);
int lStr2Nm(const char *str, const lNameSpace *ns);

const char *lMt2Str(int mt);

char **lGetPosStringRef(const lListElem *ep, int id);

char **lGetPosHostRef(const lListElem *ep, int id);

lList **lGetListRef(const lListElem *ep, int name);

int lGetType(const lDescr *dp, int nm);

int lGetPosName(const lDescr *dp, int pos);

int lXchgList(lListElem *ep, int name, lList **lpp);

int lXchgString(lListElem *ep, int name, char **str);

int lSwapList(lListElem *to, int to_nm, lListElem *from, int from_nm);

lInt lGetPosInt(const lListElem *ep, int id);

lUlong lGetPosUlong(const lListElem *ep, int id);

lUlong64 lGetPosUlong64(const lListElem *ep, int id);

const char *lGetPosString(const lListElem *ep, int id);

const char *lGetPosHost(const lListElem *ep, int id);

lList *lGetPosList(const lListElem *ep, int id);

lFloat lGetPosFloat(const lListElem *ep, int id);

lDouble lGetPosDouble(const lListElem *ep, int id);

lLong lGetPosLong(const lListElem *ep, int id);

lChar lGetPosChar(const lListElem *ep, int id);

lBool lGetPosBool(const lListElem *ep, int id);

lObject lGetPosObject(const lListElem *ep, int id);

lRef lGetPosRef(const lListElem *ep, int id);

int lSetPosInt(lListElem *ep, int pos, int value);

int lSetPosUlong(lListElem *ep, int pos, lUlong value);

int lSetPosUlong64(lListElem *ep, int pos, lUlong64 value);

int lSetPosString(lListElem *ep, int pos, const char *value);

int lSetPosHost(lListElem *ep, int pos, const char *value);

int lSetPosList(lListElem *ep, int pos, lList *value);

int lSetPosFloat(lListElem *ep, int pos, lFloat value);

int lSetPosDouble(lListElem *ep, int pos, lDouble value);

int lSetPosLong(lListElem *ep, int pos, lLong value);

int lSetPosChar(lListElem *ep, int pos, lChar value);

int lSetPosBool(lListElem *ep, int pos, lBool value);

int lSetPosObject(lListElem *ep, int pos, lListElem *value);

int lSetPosRef(lListElem *ep, int pos, lRef value);

lInt lGetInt(const lListElem *ep, int name);

lUlong lGetUlong(const lListElem *ep, int name);

lUlong64 lGetUlong64(const lListElem *ep, int name);

const char *lGetString(const lListElem *ep, int name);
const char *lGetStringNotNull(const lListElem *ep, int name);

const char *lGetHost(const lListElem *ep, int name);

lList *lGetListRW(const lListElem *ep, int name);

const lList *lGetList(const lListElem *ep, int name);

lList *lGetOrCreateList(lListElem *ep, int name, const char *list_name, const lDescr *descr);

lFloat lGetFloat(const lListElem *ep, int name);

lDouble lGetDouble(const lListElem *ep, int name);

lLong lGetLong(const lListElem *ep, int name);

lChar lGetChar(const lListElem *ep, int name);

lBool lGetBool(const lListElem *ep, int name);

lObject lGetObject(const lListElem *ep, int name);

lRef lGetRef(const lListElem *ep, int name);

int lSetInt(lListElem *ep, int name, int value);

int lSetUlong(lListElem *ep, int name, lUlong value);

int lSetUlong64(lListElem *ep, int name, lUlong64 value);

int lSetString(lListElem *ep, int name, const char *value);

int lSetHost(lListElem *ep, int name, const char *value);

int lSetList(lListElem *ep, int name, lList *value);

int lSetFloat(lListElem *ep, int name, lFloat value);

int lSetDouble(lListElem *ep, int name, lDouble value);

int lSetLong(lListElem *ep, int name, lLong value);

int lSetChar(lListElem *ep, int name, lChar value);

int lSetBool(lListElem *ep, int name, lBool value);

int lSetObject(lListElem *ep, int name, lListElem *value);

int lSetRef(lListElem *ep, int name, lRef value);

int lAddDouble(lListElem *ep, int name, lDouble offset);

int lAddUlong(lListElem *ep, int name, lUlong offset);

int lAddUlong64(lListElem *ep, int name, lUlong64 offset);

int intcmp(lInt i0, lInt i1);

int ulongcmp(lUlong u0, lUlong u1);

int bitmaskcmp(lUlong bm0, lUlong bm1);

int ulong64cmp(lUlong64 u0, lUlong64 u1);

int floatcmp(lFloat u0, lFloat u1);

int doublecmp(lDouble u0, lDouble u1);

int charcmp(lChar u0, lChar u1);

int boolcmp(lBool u0, lBool u1);

int longcmp(lLong u0, lLong u1);

int refcmp(lRef u0, lRef u1);

int incompatibleType(const char *str);

int incompatibleType2(const char *fmt, ...);

int unknownType(const char *str);


/* - -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -

   functions for lists with a char * as key

*/
lListElem *lAddElemStr(lList **lpp, int nm, const char *str, const lDescr *dp);

lListElem *lAddElemHost(lList **lpp, int nm, const char *str, const lDescr *dp);

int lDelElemStr(lList **lpp, int nm, const char *str);


lListElem *lGetElemStrRW(const lList *lp, int nm, const char *str);

const lListElem *lGetElemStr(const lList *lp, int nm, const char *str);

lListElem *lGetElemStrFirstRW(const lList *lp, int nm, const char *str, const void **iterator);

const lListElem *lGetElemStrFirst(const lList *lp, int nm, const char *str, const void **iterator);

lListElem *lGetElemStrNextRW(const lList *lp, int nm, const char *str, const void **iterator);

const lListElem *lGetElemStrNext(const lList *lp, int nm, const char *str, const void **iterator);

lListElem *lGetElemStrLikeRW(const lList *lp, int nm, const char *str);

const lListElem *lGetElemStrLike(const lList *lp, int nm, const char *str);


/* - -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -

   functions for sublists with a char * as key

*/
lListElem *lAddSubStr(lListElem *ep, int nm, const char *str, int snm, const lDescr *dp);

int lDelSubStr(lListElem *ep, int nm, const char *str, int snm);

lListElem *lGetSubStrRW(const lListElem *ep, int nm, const char *str, int snm);

const lListElem *lGetSubStr(const lListElem *ep, int nm, const char *str, int snm);

/* - -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- - 

   functions for lists with a ulong as key

*/
lListElem *lAddElemUlong(lList **lpp, int nm, lUlong val, const lDescr *dp);

int lDelElemUlong(lList **lpp, int nm, lUlong val);

lListElem *lGetElemUlongRW(const lList *lp, int nm, lUlong val);

const lListElem *lGetElemUlong(const lList *lp, int nm, lUlong val);

lListElem *lGetElemUlongFirstRW(const lList *lp, int nm, lUlong val, const void **iterator);

const lListElem *lGetElemUlongFirst(const lList *lp, int nm, lUlong val, const void **iterator);

lListElem *lGetElemUlongNextRW(const lList *lp, int nm, lUlong val, const void **iterator);

const lListElem *lGetElemUlongNext(const lList *lp, int nm, lUlong val, const void **iterator);

/* - -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- - 

   functions for sublists with a ulong as key

*/
lListElem *lAddSubUlong(lListElem *ep, int nm, lUlong val, int snm, const lDescr *dp);

int lDelSubUlong(lListElem *ep, int nm, lUlong val, int snm);

lListElem *lGetSubUlongRW(const lListElem *ep, int nm, lUlong val, int snm);

const lListElem *lGetSubUlong(const lListElem *ep, int nm, lUlong val, int snm);

/* - -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- - 

   functions for lists with a ulong64 as key

*/
lListElem *lAddElemUlong64(lList **lpp, int nm, lUlong64 val, const lDescr *dp);

int lDelElemUlong64(lList **lpp, int nm, lUlong64 val);

lListElem *lGetElemUlong64RW(const lList *lp, int nm, lUlong64 val);

lListElem *lGetElemUlong64FirstRW(const lList *lp, int nm, lUlong64 val, const void **iterator);

lListElem *lGetElemUlong64NextRW(const lList *lp, int nm, lUlong64 val, const void **iterator);

const lListElem *lGetElemUlong64(const lList *lp, int nm, lUlong64 val);

const lListElem *lGetElemUlong64First(const lList *lp, int nm, lUlong64 val, const void **iterator);

const lListElem *lGetElemUlong64Next(const lList *lp, int nm, lUlong64 val, const void **iterator);

/* - -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- - 

   functions for sublists with a ulong64 as key

*/
lListElem *lAddSubUlong64(lListElem *ep, int nm, lUlong64 val, int snm, const lDescr *dp);

int lDelSubUlong64(lListElem *ep, int nm, lUlong64 val, int snm);

lListElem *lGetSubUlong64RW(const lListElem *ep, int nm, lUlong64 val, int snm);

const lListElem *lGetSubUlong64(const lListElem *ep, int nm, lUlong64 val, int snm);

/* - -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -

   functions for lists with a case insensitive char * as key

*/
int lDelElemCaseStr(lList **lpp, int nm, const char *str);

lListElem *lGetElemCaseStrRW(const lList *lp, int nm, const char *str);

const lListElem *lGetElemCaseStr(const lList *lp, int nm, const char *str);

/* - -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -

   functions for sublists with a hostname as key

*/
lListElem *lAddSubHost(lListElem *ep, int nm, const char *str, int snm, const lDescr *dp);

int lDelElemHost(lList **lpp, int nm, const char *str);

lListElem *lGetSubHostRW(const lListElem *ep, int nm, const char *str, int snm);

const lListElem *lGetSubHost(const lListElem *ep, int nm, const char *str, int snm);

lListElem *lGetElemHostRW(const lList *lp, int nm, const char *str);

const lListElem *lGetElemHost(const lList *lp, int nm, const char *str);

lListElem *lGetElemHostFirstRW(const lList *lp, int nm, const char *str, const void **iterator);

const lListElem *lGetElemHostFirst(const lList *lp, int nm, const char *str, const void **iterator);

lListElem *lGetElemHostNextRW(const lList *lp, int nm, const char *str, const void **iterator);

const lListElem *lGetElemHostNext(const lList *lp, int nm, const char *str, const void **iterator);

// working with u_long32 as bitmasks
bool lMatchUlongBitMask(lListElem *ep, int nm, u_long32 bitmask);
int lAndUlongBitMask(lListElem *ep, int name, u_long32 bitmask);
int lOrUlongBitMask(lListElem *ep, int name, u_long32 bitmask);
int lClearUlongBitMask(lListElem *ep, int nm, u_long32 bitmask);
