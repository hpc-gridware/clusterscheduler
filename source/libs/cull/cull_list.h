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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The public cull types and the list and element API
 */

#include <cstdio>

#include <cinttypes>
#include "cull/cull_hashP.h"
#include "cull/pack.h"
#include "uti/sge_dstring.h"

#define NoName -1 ///< field name meaning "no field"; terminates a descriptor

typedef struct _lDescr lDescr;             ///< one field of an object type; see @ref _lDescr
typedef struct _lNameSpace lNameSpace;     ///< a block of field names; see @ref _lNameSpace
typedef struct _lList lList;               ///< a cull list; see @ref _lList
typedef struct _lListElem lListElem;       ///< one element of a cull list; see @ref _lListElem
typedef struct _lCondition lCondition;     ///< a selection condition, built with `lWhere()`
typedef struct _lEnumeration lEnumeration; ///< a field selection, built with `lWhat()`
typedef union _lMultiType lMultiType;      ///< the value of one field, in any cull type
typedef struct _lSortOrder lSortOrder;     ///< a sort specification, built with `lParseSortOrder()`
/// Argument of a where condition
typedef struct _WhereArg WhereArg, *WhereArgList; ///< a pointer to a @ref _WhereArg

typedef float lFloat;        ///< cull single precision float field
typedef double lDouble;      ///< cull double precision float field
typedef uint32_t lUlong;     ///< cull 32 bit unsigned field
typedef uint64_t lUlong64;   ///< cull 64 bit unsigned field
typedef long lLong;          ///< cull signed long field
typedef char lChar;          ///< cull single character field
typedef bool lBool;          ///< cull boolean field
typedef int lInt;            ///< cull integer field; not supported at GDI level
typedef char *lString;       ///< cull string field, owned by the element
typedef char *lHost;         ///< cull host name field, owned by the element
typedef lListElem *lObject;  ///< cull field holding a single sub-object
typedef void *lRef;          ///< cull field holding an opaque pointer; never spooled

/* IF YOU CHANGE THIS ENUM, CHANGE cull_multitype.c/multitypes[] */
/**
 * @brief The data type of a cull field
 *
 * @note If you change this enum, change `multitypes[]` in cull_multitype.cc
 *       to match — the array is indexed by these values.
 */
enum _enum_lMultiType {
   lEndT,      ///< marks the end of a descriptor; not a real type
   lDoubleT,   ///< @ref lDouble
   lUlongT,    ///< @ref lUlong
   lLongT,     ///< @ref lLong
   lBoolT,     ///< @ref lBool
   lIntT,      ///< @ref lInt
   lStringT,   ///< @ref lString
   lListT,     ///< a sub-list, @ref lList
   lObjectT,   ///< a single sub-object, @ref lObject
   lRefT,      ///< @ref lRef
   lHostT,     ///< @ref lHost
   lUlong64T   ///< @ref lUlong64
};

/* flags for the field definition
 * reserve 8 bit for data types (currently only 4 bit in use) */
/**
 * @addtogroup cull_field_attributes
 * @{
 */
#define CULL_DEFAULT       0x00000000 ///< no special settings, default behaviour
#define CULL_PRIMARY_KEY   0x00000100 ///< the field is part of the primary key; implies neither uniqueness nor hashing
#define CULL_HASH          0x00000200 ///< build a hash table on this field; non unique unless #CULL_UNIQUE is also given
#define CULL_UNIQUE        0x00000400 ///< the value must be unique across a list; currently only used when defining hash tables
#define CULL_NO_TRANSFER   0x00000800 ///< the value stays inside the process; it is neither packed nor delivered as event payload
#define CULL_CONFIGURE     0x00001000 ///< the field may be changed by configuration functions; not yet implemented
#define CULL_SPOOL         0x00002000 ///< the field is written when the object is spooled
#define CULL_SUBLIST       0x00010000 ///< the field is spooled when the type appears as a subtype of another, where fewer fields are written
#define CULL_SPOOL_PROJECT 0x00020000 ///< deprecated?
#define CULL_SPOOL_USER    0x00040000 ///< deprecated?
#define CULL_UNUSED1       0x00080000 ///< free for reuse
#define CULL_UNUSED2       0x00100000 ///< free for reuse
#define CULL_IS_REDUCED    0x00200000 ///< the descriptor is a reduced one, holding only part of the type's fields
/** @} */

#define BASIC_UNIT 50 ///< growth unit of a descriptor; don't touch
#define MAX_DESCR_SIZE  (4*BASIC_UNIT) ///< largest number of fields an object type may have

#ifdef __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#define LISTDEF( name ) lDescr name[] = {                                                ///< opens an object type definition; ends with #LISTEND
#define LISTEND {NoName, lEndT, nullptr}};                                               ///< closes an object type definition opened with #LISTDEF
#define SGE_INT(name,flags)         { name, lIntT    | flags, nullptr }, /* don't use it, not implemented on gdi level */ ///< declares an int (not supported at GDI level) field
#define SGE_HOST(name,flags)        { name, lHostT   | flags, nullptr },                 ///< declares a host name field
#define SGE_STRING(name,flags)      { name, lStringT | flags, nullptr },                 ///< declares a string field
#define SGE_DOUBLE(name,flags)      { name, lDoubleT | flags, nullptr },                 ///< declares a double field
#define SGE_LONG(name,flags)        { name, lLongT   | flags, nullptr },                 ///< declares a signed long field
#define SGE_ULONG(name,flags)       { name, lUlongT  | flags, nullptr },                 ///< declares a 32 bit unsigned field
#define SGE_ULONG64(name,flags)     { name, lUlong64T  | flags, nullptr },               ///< declares a 64 bit unsigned field
#define SGE_BOOL(name,flags)        { name, lBoolT   | flags, nullptr },                 ///< declares a boolean field
#define SGE_LIST(name,type,flags)   { name, lListT   | flags, nullptr },                 ///< declares a sub-list of another object type field
#define SGE_MAP(name,type,flags)   { name, lListT   | flags, nullptr },                  ///< declares a sub-list used as a key/value map field
#define SGE_MAPLIST(name,type,flags)   { name, lListT   | flags, nullptr },              ///< declares a sub-list used as a key/value-list map field
#define SGE_OBJECT(name,type,flags) { name, lObjectT | flags, nullptr },                 ///< declares a single sub-object field
#define SGE_REF(name,type,flags)    { name, lRefT    | flags, nullptr },                 ///< declares an opaque pointer field

#define DERIVED_LISTDEF(name,parent) lDescr *name = parent                               ///< opens a type that reuses another type's fields
#define DERIVED_LISTEND ;                                                                ///< closes a #DERIVED_LISTDEF
#define SGE_INT_D(name,flags,def)         { name, lIntT    | flags, nullptr },           ///< declares an int (not supported at GDI level) field, with a default value
#define SGE_HOST_D(name,flags,def)        { name, lHostT   | flags, nullptr },           ///< declares a host name field, with a default value
#define SGE_STRING_D(name,flags,def)      { name, lStringT | flags, nullptr },           ///< declares a string field, with a default value
#define SGE_DOUBLE_D(name,flags,def)      { name, lDoubleT | flags, nullptr },           ///< declares a double field, with a default value
#define SGE_LONG_D(name,flags,def)        { name, lLongT   | flags, nullptr },           ///< declares a signed long field, with a default value
#define SGE_ULONG_D(name,flags,def)       { name, lUlongT  | flags, nullptr },           ///< declares a 32 bit unsigned field, with a default value
#define SGE_ULONG64_D(name,flags,def)     { name, lUlong64T  | flags, nullptr },         ///< declares a 64 bit unsigned field, with a default value
#define SGE_BOOL_D(name,flags,def)        { name, lBoolT   | flags, nullptr },           ///< declares a boolean field, with a default value
#define SGE_LIST_D(name,type,flags,def)   { name, lListT   | flags, nullptr },           ///< declares a sub-list of another object type field, with a default value
#define SGE_MAP_D(name,type,flags,defkey,keyvalue,jgdi_keyname,jgdi_valuename)   { name, lListT   | flags, nullptr}, ///< declares a sub-list used as a key/value map field, with a default value
#define SGE_MAPLIST_D(name,type,flags,defkey,defvalue,jgdi_keyname,jgdi_valuename)   { name, lListT   | flags, nullptr}, ///< declares a sub-list used as a key/value-list map field, with a default value
#define SGE_OBJECT_D(name,type,flags,def) { name, lObjectT | flags, nullptr },           ///< declares a single sub-object field, with a default value
#define SGE_REF_D(name,type,flags,def)    { name, lRefT    | flags, nullptr },           ///< declares an opaque pointer field, with a default value

/* 
 * For lists, objects and references the type of the subordinate object(s) 
 * must be specified.
 * If multiple types are thinkable or non cull data types are referenced,
 * use the following define CULL_ANY_SUBTYPE as type
 */
#define CULL_ANY_SUBTYPE 0

#define NAMEDEF( name ) const char *name[] = {                                           ///< opens the field name table matching a #LISTDEF
#define NAME( name ) name ,                                                              ///< one entry of a #NAMEDEF table
#define NAMEEND    };                                                                    ///< closes a #NAMEDEF table
#else

#define LISTDEF(name) extern lDescr name[];                                              ///< declares an object type defined in another translation unit
#define LISTEND                                                                          ///< closes a forward declared object type
#define DERIVED_LISTDEF(name, parent) extern lDescr *name                                ///< declares a derived type defined elsewhere
#define DERIVED_LISTEND ;                                                                ///< closes a forward declared #DERIVED_LISTDEF

#define SGE_INT(name, flags)                                                             ///< declares an int (not supported at GDI level) field
#define SGE_HOST(name, flags)                                                            ///< declares a host name field
#define SGE_STRING(name, flags)                                                          ///< declares a string field
#define SGE_FLOAT(name, flags)                                                           ///< declares a float field
#define SGE_DOUBLE(name, flags)                                                          ///< declares a double field
#define SGE_CHAR(name, flags)                                                            ///< declares a single character field
#define SGE_LONG(name, flags)                                                            ///< declares a signed long field
#define SGE_ULONG(name, flags)                                                           ///< declares a 32 bit unsigned field
#define SGE_ULONG64(name, flags)                                                         ///< declares a 64 bit unsigned field
#define SGE_BOOL(name, flags)                                                            ///< declares a boolean field
#define SGE_LIST(name, type, flags)                                                      ///< declares a sub-list of another object type field
#define SGE_MAP(name, type, flags)                                                       ///< declares a sub-list used as a key/value map field
#define SGE_MAPLIST(name, type, flags)                                                   ///< declares a sub-list used as a key/value-list map field
#define SGE_OBJECT(name, type, flags)                                                    ///< declares a single sub-object field
#define SGE_REF(name, type, flags)                                                       ///< declares an opaque pointer field

#define SGE_INT_D(name, flags, def)                                                      ///< declares an int (not supported at GDI level) field, with a default value
#define SGE_HOST_D(name, flags, def)                                                     ///< declares a host name field, with a default value
#define SGE_STRING_D(name, flags, def)                                                   ///< declares a string field, with a default value
#define SGE_FLOAT_D(name, flags, def)                                                    ///< declares a float field, with a default value
#define SGE_DOUBLE_D(name, flags, def)                                                   ///< declares a double field, with a default value
#define SGE_CHAR_D(name, flags, def)                                                     ///< declares a single character field, with a default value
#define SGE_LONG_D(name, flags, def)                                                     ///< declares a signed long field, with a default value
#define SGE_ULONG_D(name, flags, def)                                                    ///< declares a 32 bit unsigned field, with a default value
#define SGE_ULONG64_D(name, flags, def)                                                  ///< declares a 64 bit unsigned field, with a default value
#define SGE_BOOL_D(name, flags, def)                                                     ///< declares a boolean field, with a default value
#define SGE_LIST_D(name, type, flags, def)                                               ///< declares a sub-list of another object type field, with a default value
#define SGE_MAP_D(name, type, flags, defkey, keyvalue, jgdi_keyname, jgdi_valuename)     ///< declares a sub-list used as a key/value map field, with a default value
#define SGE_MAPLIST_D(name, type, flags, defkey, defvalue, jgdi_keyname, jgdi_valuename) ///< declares a sub-list used as a key/value-list map field, with a default value
#define SGE_OBJECT_D(name, type, flags, def)                                             ///< declares a single sub-object field, with a default value
#define SGE_REF_D(name, type, flags, def)                                                ///< declares an opaque pointer field, with a default value

#define NAMEDEF(name) extern const char *name[];                                         ///< declares a field name table defined elsewhere
#define NAME(name)                                                                       ///< one entry of a forward declared #NAMEDEF table
#define NAMEEND                                                                          ///< closes a forward declared name table
#endif

/**
 * @brief One block of field names, mapping field numbers to their names
 *
 * Field numbers are allocated in blocks, one per object type, so a number can
 * be resolved to a name by finding the block whose range contains it.
 */
struct _lNameSpace {
   int lower;          ///< lowest field number in this block
   int size;           ///< how many names the block holds
   const char **namev; ///< the names, indexed by `number - lower`
   lDescr *descr;      ///< descriptor of the object type the block belongs to
};

/**
 * @brief One field of an object type
 *
 * An object type is described by an array of these, terminated by a field
 * whose type is #lEndT. The array is what `LISTDEF`/`LISTEND` build.
 */
struct _lDescr {
   int nm;         ///< the field number, or #NoName in the terminating entry
   int mt;         ///< the field's type and attributes: a @ref _enum_lMultiType or-ed with @ref cull_field_attributes flags
   cull_htable ht; ///< hash table over this field, when #CULL_HASH is set; otherwise nullptr
};

/* LIST SPECIFIC FUNCTIONS */
const char *lGetListName(const lList *lp);

const lDescr *lGetListDescr(const lList *lp);

uint32_t lGetNumberOfElem(const lList *lp);

uint32_t lGetNumberOfRemainingElem(const lListElem *ep);

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

lList *lCopyListHash(const char *name, const lList *src, bool hash, bool skip_no_transfer = false);

lListElem *lCopyElem(const lListElem *src);

lListElem *lCopyElemHash(const lListElem *src, bool isHash, bool skip_no_transfer = false);

int lModifyWhat(lListElem *dst, const lListElem *src, const lEnumeration *enp);

int
lCopyElemPartialPack(lListElem *dst, int *jp, const lListElem *src,
                     const lEnumeration *ep, bool isHash, sge_pack_buffer *pb,
                     bool skip_no_transfer = false);

int
lCopySwitchPack(const lListElem *sep, lListElem *dep, int src_idx, int dst_idx,
                bool isHash, lEnumeration *ep, sge_pack_buffer *pb,
                bool skip_no_transfer = false);

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

#define mt_get_type(mt) ((mt) & 0x000000FF) ///< the @ref _enum_lMultiType out of a field's flags
#define mt_do_hashing(mt) (((mt) & CULL_HASH) ? true : false) ///< is #CULL_HASH set on this field?
#define mt_is_unique(mt) (((mt) & CULL_UNIQUE) ? true : false) ///< is #CULL_UNIQUE set on this field?
#define mt_do_transfer(mt) (((mt) & CULL_NO_TRANSFER) ? false : true) ///< may the value of this field leave the process? (#CULL_NO_TRANSFER not set)

#define for_each_ep(ep, lp) for (ep=lFirst(lp);ep;ep=lNext(ep))                          ///< walk a list front to back, read only
#define for_each_rev(ep, lp) for (ep=lLast(lp);ep;ep=lPrev(ep))                          ///< walk a list back to front, read only
#define for_each_rw(ep, lp) for (ep=lFirstRW(lp);ep;ep=lNextRW(ep))                      ///< walk a list front to back, elements modifiable
#define for_each_rev_rw(ep, lp) for (ep=lLastRW(lp);ep;ep=lPrevRW(ep))                   ///< walk a list back to front, elements modifiable
// same macros as the ones without _lv (local variable) suffix with the difference that the
// object variable does not have to be declared before the loop, but is declared in
// the for loop itself. This allows to use the same variable name in code after the loop
#define for_each_ep_lv(ep, lp) for (const lListElem *ep=lFirst(lp);ep;ep=lNext(ep))      ///< like #for_each_ep, declaring @p ep in the loop itself
#define for_each_rw_lv(ep, lp) for (lListElem *ep=lFirstRW(lp);ep;ep=lNextRW(ep))        ///< like #for_each_rw, declaring @p ep in the loop itself
