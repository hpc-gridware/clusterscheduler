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
 ************************************************************************/
/* Portions of this code are Copyright (c) 2011 Univa Corporation. */
/*___INFO__MARK_END__*/

#include <cstdio>

#include "basis_types.h"
#include "cull/cull_hashP.h"
#include "cull/pack.h"
#include "uti/sge_dstring.h"

#define NoName -1

typedef struct _lDescr lDescr;
typedef struct _lNameSpace lNameSpace;
typedef struct _lList lList;
typedef struct _lListElem lListElem;
typedef struct _lCondition lCondition;
typedef struct _lEnumeration lEnumeration;
typedef union _lMultiType lMultiType;
typedef struct _lSortOrder lSortOrder;
typedef struct _WhereArg WhereArg, *WhereArgList;

typedef float lFloat;
typedef double lDouble;
typedef u_long32 lUlong;
typedef u_long64 lUlong64;
typedef long lLong;
typedef char lChar;
typedef bool lBool;
typedef int lInt;
typedef char *lString;
typedef char *lHost;
typedef lListElem *lObject;
typedef void *lRef;

/* IF YOU CHANGE THIS ENUM, CHANGE cull_multitype.c/multitypes[] */
enum _enum_lMultiType {
   lEndT,                       /* This is the end of the descriptor */
   lFloatT,
   lDoubleT,
   lUlongT,
   lLongT,
   lCharT,
   lBoolT,
   lIntT,
   lStringT,
   lListT,
   lObjectT,
   lRefT,
   lHostT,
   lUlong64T
};

/* flags for the field definition 
 * reserve 8 bit for data types (currently only 4 bit in use)
 * see doc header cull/list/-Field_Attributes in cull_list.c for details
 */
#define CULL_DEFAULT       0x00000000
#define CULL_PRIMARY_KEY   0x00000100
#define CULL_HASH          0x00000200
#define CULL_UNIQUE        0x00000400
#define CULL_UNUSED0       0x00000800
#define CULL_CONFIGURE     0x00001000
#define CULL_SPOOL         0x00002000
#define CULL_SUBLIST       0x00010000
#define CULL_SPOOL_PROJECT 0x00020000
#define CULL_SPOOL_USER    0x00040000
#define CULL_UNUSED1       0x00080000
#define CULL_UNUSED2       0x00100000
#define CULL_IS_REDUCED    0x00200000

#define BASIC_UNIT 50         /* Don't touch */
#define MAX_DESCR_SIZE  (4*BASIC_UNIT)

#ifdef __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#define LISTDEF( name ) lDescr name[] = {
#define LISTEND {NoName, lEndT, nullptr}};

#define SGE_INT(name,flags)         { name, lIntT    | flags, nullptr }, /* don't use it, not implemented on gdi level */
#define SGE_HOST(name,flags)        { name, lHostT   | flags, nullptr },
#define SGE_STRING(name,flags)      { name, lStringT | flags, nullptr },
#define SGE_FLOAT(name,flags)       { name, lFloatT  | flags, nullptr },
#define SGE_DOUBLE(name,flags)      { name, lDoubleT | flags, nullptr },
#define SGE_CHAR(name,flags)        { name, lCharT   | flags, nullptr },
#define SGE_LONG(name,flags)        { name, lLongT   | flags, nullptr },
#define SGE_ULONG(name,flags)       { name, lUlongT  | flags, nullptr },
#define SGE_ULONG64(name,flags)     { name, lUlong64T  | flags, nullptr },
#define SGE_BOOL(name,flags)        { name, lBoolT   | flags, nullptr },
#define SGE_LIST(name,type,flags)   { name, lListT   | flags, nullptr },
#define SGE_MAP(name,type,flags)   { name, lListT   | flags, nullptr },
#define SGE_MAPLIST(name,type,flags)   { name, lListT   | flags, nullptr },
#define SGE_OBJECT(name,type,flags) { name, lObjectT | flags, nullptr },
#define SGE_REF(name,type,flags)    { name, lRefT    | flags, nullptr },

#define DERIVED_LISTDEF(name,parent) lDescr *name = parent
#define DERIVED_LISTEND ; 

#define SGE_INT_D(name,flags,def)         { name, lIntT    | flags, nullptr },
#define SGE_HOST_D(name,flags,def)        { name, lHostT   | flags, nullptr },
#define SGE_STRING_D(name,flags,def)      { name, lStringT | flags, nullptr },
#define SGE_FLOAT_D(name,flags,def)       { name, lFloatT  | flags, nullptr },
#define SGE_DOUBLE_D(name,flags,def)      { name, lDoubleT | flags, nullptr },
#define SGE_CHAR_D(name,flags,def)        { name, lCharT   | flags, nullptr },
#define SGE_LONG_D(name,flags,def)        { name, lLongT   | flags, nullptr },
#define SGE_ULONG_D(name,flags,def)       { name, lUlongT  | flags, nullptr },
#define SGE_ULONG64_D(name,flags,def)     { name, lUlong64T  | flags, nullptr },
#define SGE_BOOL_D(name,flags,def)        { name, lBoolT   | flags, nullptr },
#define SGE_LIST_D(name,type,flags,def)   { name, lListT   | flags, nullptr },
#define SGE_MAP_D(name,type,flags,defkey,keyvalue,jgdi_keyname,jgdi_valuename)   { name, lListT   | flags, nullptr},
#define SGE_MAPLIST_D(name,type,flags,defkey,defvalue,jgdi_keyname,jgdi_valuename)   { name, lListT   | flags, nullptr},
#define SGE_OBJECT_D(name,type,flags,def) { name, lObjectT | flags, nullptr },
#define SGE_REF_D(name,type,flags,def)    { name, lRefT    | flags, nullptr },

/* 
 * For lists, objects and references the type of the subordinate object(s) 
 * must be specified.
 * If multiple types are thinkable or non cull data types are referenced,
 * use the following define CULL_ANY_SUBTYPE as type
 */
#define CULL_ANY_SUBTYPE 0

#define NAMEDEF( name ) const char *name[] = {
#define NAME( name ) name ,
#define NAMEEND    };

#else

#define LISTDEF(name) extern lDescr name[];
#define LISTEND

#define DERIVED_LISTDEF(name, parent) extern lDescr *name
#define DERIVED_LISTEND ;

#define SGE_INT(name, flags)
#define SGE_HOST(name, flags)
#define SGE_STRING(name, flags)
#define SGE_FLOAT(name, flags)
#define SGE_DOUBLE(name, flags)
#define SGE_CHAR(name, flags)
#define SGE_LONG(name, flags)
#define SGE_ULONG(name, flags)
#define SGE_ULONG64(name, flags)
#define SGE_BOOL(name, flags)
#define SGE_LIST(name, type, flags)
#define SGE_MAP(name, type, flags)
#define SGE_MAPLIST(name, type, flags)
#define SGE_OBJECT(name, type, flags)
#define SGE_REF(name, type, flags)

#define SGE_INT_D(name, flags, def)
#define SGE_HOST_D(name, flags, def)
#define SGE_STRING_D(name, flags, def)
#define SGE_FLOAT_D(name, flags, def)
#define SGE_DOUBLE_D(name, flags, def)
#define SGE_CHAR_D(name, flags, def)
#define SGE_LONG_D(name, flags, def)
#define SGE_ULONG_D(name, flags, def)
#define SGE_ULONG64_D(name, flags, def)
#define SGE_BOOL_D(name, flags, def)
#define SGE_LIST_D(name, type, flags, def)
#define SGE_MAP_D(name, type, flags, defkey, keyvalue, jgdi_keyname, jgdi_valuename)
#define SGE_MAPLIST_D(name, type, flags, defkey, defvalue, jgdi_keyname, jgdi_valuename)
#define SGE_OBJECT_D(name, type, flags, def)
#define SGE_REF_D(name, type, flags, def)

#define NAMEDEF(name) extern const char *name[];
#define NAME(name)
#define NAMEEND

#endif

struct _lNameSpace {
   int lower;
   int size;
   const char **namev;
   lDescr *descr;
};

struct _lDescr {
   int nm;                             /* name */
   int mt;                             /* multitype information */
   cull_htable ht;
};

/* LIST SPECIFIC FUNCTIONS */
const char *lGetListName(const lList *lp);

const lDescr *lGetListDescr(const lList *lp);

u_long32 lGetNumberOfElem(const lList *lp);

u_long32 lGetNumberOfRemainingElem(const lListElem *ep);

int lGetElemIndex(const lListElem *ep, const lList *lp);

const lDescr *lGetElemDescr(const lListElem *ep);

void lWriteElem(const lListElem *ep);

void lWriteElemTo(const lListElem *ep, FILE *fp);

void lWriteElemToStr(const lListElem *ep, dstring *buffer);

void lWriteList(const lList *lp);

void lWriteListTo(const lList *lp, FILE *fp);

void lWriteListToStr(const lList *lp, dstring *buffer);

lListElem *lCreateElem(const lDescr *dp);

lList *lCreateList(const char *listname, const lDescr *descr);

lList *lCreateListHash(const char *listname, const lDescr *descr, bool hash);

lList *lCreateElemList(const char *listname, const lDescr *descr, int nr_elem);

void lFreeElem(lListElem **ep);

void lFreeList(lList **ilp);

int lAddList(lList *lp0, lList **lp1);

int lAppendList(lList *lp0, lList *lp1);

int lOverrideStrList(lList *lp0, lList *lp1, int nm, const char *str);

lList *lAddSubList(lListElem *ep, int nm, lList *to_add);

int lCompListDescr(const lDescr *dp0, const lDescr *dp1);

lList *lCopyList(const char *name, const lList *src);

lList *lCopyListHash(const char *name, const lList *src, bool hash);

lListElem *lCopyElem(const lListElem *src);

lListElem *lCopyElemHash(const lListElem *src, bool isHash);

int lModifyWhat(lListElem *dst, const lListElem *src, const lEnumeration *enp);

int
lCopyElemPartialPack(lListElem *dst, int *jp, const lListElem *src,
                     const lEnumeration *ep, bool isHash, sge_pack_buffer *pb);

int
lCopySwitchPack(const lListElem *sep, lListElem *dep, int src_idx, int dst_idx,
                bool isHash, lEnumeration *ep, sge_pack_buffer *pb);

int lAppendElem(lList *lp, lListElem *ep);

lListElem *lDechainElem(lList *lp, lListElem *ep);

void lDechainList(lList *source, lList **target, lListElem *ep);

lListElem *lDechainObject(lListElem *parent, int name);

int lRemoveElem(lList *lp, lListElem **ep);

int lInsertElem(lList *lp, lListElem *ep, lListElem *new_elem);

int lPSortList(lList *lp, const char *fmt, ...);

int lSortList(lList *lp, const lSortOrder *sp);

int lUniqStr(lList *lp, int keyfield);

int lUniqHost(lList *lp, int keyfield);

lListElem *lFirstRW(const lList *lp);

const lListElem *lFirst(const lList *lp);

lListElem *lLastRW(const lList *lp);

const lListElem *lLast(const lList *lp);

lListElem *lNextRW(const lListElem *ep);

const lListElem *lNext(const lListElem *ep);

lListElem *lPrevRW(const lListElem *ep);

const lListElem *lPrev(const lListElem *ep);

lListElem *lFindNextRW(const lListElem *ep, const lCondition *cp);

lListElem *lFindPrevRW(const lListElem *ep, const lCondition *cp);

lListElem *lFindFirstRW(const lList *lp, const lCondition *cp);

lListElem *lFindLastRW(const lList *lp, const lCondition *cp);

#define mt_get_type(mt) ((mt) & 0x000000FF)
#define mt_do_hashing(mt) (((mt) & CULL_HASH) ? true : false)
#define mt_is_unique(mt) (((mt) & CULL_UNIQUE) ? true : false)

bool lListElem_is_pos_changed(const lListElem *ep, int pos);

bool lListElem_is_changed(const lListElem *ep);

bool lList_clear_changed_info(lList *lp);

bool lListElem_clear_changed_info(lListElem *lp);

#define for_each_ep(ep, lp) for (ep=lFirst(lp);ep;ep=lNext(ep))
#define for_each_rev(ep, lp) for (ep=lLast(lp);ep;ep=lPrev(ep))
#define for_each_rw(ep, lp) for (ep=lFirstRW(lp);ep;ep=lNextRW(ep))
#define for_each_rev_rw(ep, lp) for (ep=lLastRW(lp);ep;ep=lPrevRW(ep))

#define for_each_where(ep, lp, cp) \
   for (ep=lFindFirstRW(lp,cp);ep;ep=lFindNextRW(ep,cp))
#define for_each_where_rev(ep, lp, cp) \
   for (ep=lFindLast(lp,cp);ep;ep=lFindPrev(ep,cp))
