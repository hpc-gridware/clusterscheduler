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

#pragma once

/** @file
 * @brief Macros to define object types by hand, for the module tests of the list library
 *
 * The product types are generated from the object model (sgeobj/tom); only the module tests define their own
 * types, with LISTDEF/LISTEND and the field macros below, and their field name tables with NAMEDEF.
 */

#include "tom/ocs_Types.h"
#include "tom/ocs_FieldTraits.h"

/*
 * An object type is a static descriptor block: a cull_descr_header followed by the
 * fields and the terminating #lEndT entry. LISTDEF/LISTEND build it at compile
 * time as an inline variable, so every translation unit sees the same block and
 * its contents are constant expressions. The descriptor of a type points at its first field.
 */
#define LISTDEF(name) inline constexpr auto name##_blk = ocs::tom::descr_make({           ///< opens an object type definition; ends with #LISTEND
#define LISTEND(name) {NoName, lEndT}}); inline constexpr const lDescr *name = name##_blk.e; ///< closes the object type definition opened with #LISTDEF
#define SGE_INT(name,flags)         { name, lIntT    | flags }, /* don't use it, not implemented on gdi level */ ///< declares an int (not supported at GDI level) field
#define SGE_HOST(name,flags)        { name, lHostT   | flags },                 ///< declares a host name field
#define SGE_STRING(name,flags)      { name, lStringT | flags },                 ///< declares a string field
#define SGE_DOUBLE(name,flags)      { name, lDoubleT | flags },                 ///< declares a double field
#define SGE_LONG(name,flags)        { name, lLongT   | flags },                 ///< declares a signed long field
#define SGE_ULONG(name,flags)       { name, lUlongT  | flags },                 ///< declares a 32 bit unsigned field
#define SGE_ULONG64(name,flags)     { name, lUlong64T  | flags },               ///< declares a 64 bit unsigned field
#define SGE_BOOL(name,flags)        { name, lBoolT   | flags },                 ///< declares a boolean field
#define SGE_LIST(name,type,flags)   { name, lListT   | flags },                 ///< declares a sub-list of another object type field
#define SGE_OBJECT(name,type,flags) { name, lObjectT | flags },                 ///< declares a single sub-object field
#define SGE_REF(name,type,flags)    { name, lRefT    | flags },                 ///< declares an opaque pointer field

/*
 * The field name tables of the object types written by hand in the module
 * tests; the field names of the generated types come from the object model
 * (tom/ocs_Model.h).
 */
#ifdef __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__
#define NAMEDEF( name ) const char *name[] = {                                           ///< opens the field name table matching a #LISTDEF
#define NAME( name ) name ,                                                              ///< one entry of a #NAMEDEF table
#define NAMEEND    };                                                                    ///< closes a #NAMEDEF table
#else
#define NAMEDEF(name) extern const char *name[];                                         ///< declares a field name table defined elsewhere
#define NAME(name)                                                                       ///< one entry of a forward declared #NAMEDEF table
#define NAMEEND                                                                          ///< closes a forward declared name table
#endif
