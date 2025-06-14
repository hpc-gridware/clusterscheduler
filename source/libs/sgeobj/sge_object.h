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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/       

#include "uti/sge_dstring.h"

#include "cull/cull.h"
#include "cull/cull_list.h"

#include "sgeobj/cull/sge_all_listsL.h"

/****** sgeobj/object/--Object-Handling ***************************************
*
*  NAME
*     Object Handling -- utilities for sgeobj object access
*
*  FUNCTION
*     This module provides utility functions for accessing CULL 
*     objects, e.g. getting a string representation for fields, 
*     setting field contents from string representation etc.
*
*  NOTES
*
*  SEE ALSO
*     sgeobj/Object/object_has_type()
*     sgeobj/Object/object_get_type()
*     sgeobj/Object/object_get_subtype()
*     sgeobj/Object/object_get_primary_key()
*     sgeobj/Object/object_get_name_prefix()
*     sgeobj/Object/object_append_field_to_dstring()
*     sgeobj/Object/object_parse_field_from_string()
*     sgeobj/Object/ocs::DataStore::get_master_list()
*     sgeobj/Object/object_type_get_name()
*     sgeobj/Object/object_type_get_descr()
*     sgeobj/Object/object_type_get_key_nm()
******************************************************************************/

#define NULL_OUT_NONE(ep, nm) \
   if (lGetString(ep, nm) != nullptr && strcasecmp(lGetString(ep, nm), "none") == 0) { \
      lSetString(ep, nm, nullptr); \
   }

/****** sgeobj/object/--Object-Typedefs ***************************************
*
*  NAME
*     Object-Typedefs -- typedefs for generic object handling
*
*  SYNOPSIS
*     The enumeration sge_object_type defines different object and 
*     message types.
*
*     The following types are defined:
*        SGE_TYPE_ADMINHOST
*        SGE_TYPE_CALENDAR
*        SGE_TYPE_CKPT
*        SGE_TYPE_CONFIG
*        SGE_TYPE_EXECHOST
*        SGE_TYPE_JATASK
*        SGE_TYPE_PETASK
*        SGE_TYPE_JOB
*        SGE_TYPE_JOB_SCHEDD_INFO
*        SGE_TYPE_MANAGER
*        SGE_TYPE_OPERATOR
*        SGE_TYPE_SHARETREE
*        SGE_TYPE_PE
*        SGE_TYPE_PROJECT
*        SGE_TYPE_CQUEUE
*        SGE_TYPE_QINSTANCE
*        SGE_TYPE_SCHEDD_CONF
*        SGE_TYPE_SCHEDD_MONITOR
*        SGE_TYPE_SHUTDOWN
*        SGE_TYPE_MARK_4_REGISTRATION
*        SGE_TYPE_SUBMITHOST
*        SGE_TYPE_USER
*        SGE_TYPE_USERSET
*        SGE_TYPE_CUSER
*        SGE_TYPE_CENTRY   
*        SGE_TYPE_ZOMBIE
*        SGE_TYPE_SUSER
*        SGE_TYPE_RQS
*        SGE_TYPE_AR
*        SGE_TYPE_JOBSCRIPT
*
*     If usermapping is enabled, an additional object type is defined:
*        SGE_TYPE_HGROUP
*  
*     The last value defined as obect type is SGE_TYPE_ALL. 
*****************************************************************************/
typedef enum {
   SGE_TYPE_FIRST = 0,
   SGE_TYPE_ADMINHOST = SGE_TYPE_FIRST,
   SGE_TYPE_CALENDAR, // 1
   SGE_TYPE_CKPT,
   SGE_TYPE_CONFIG,
   SGE_TYPE_EXECHOST,
   SGE_TYPE_JATASK, // 5
   SGE_TYPE_PETASK,
   SGE_TYPE_JOB,
   SGE_TYPE_JOB_SCHEDD_INFO,
   SGE_TYPE_MANAGER,
   SGE_TYPE_OPERATOR, // 10
   SGE_TYPE_SHARETREE,
   SGE_TYPE_PE,
   SGE_TYPE_PROJECT,
   SGE_TYPE_CQUEUE,
   SGE_TYPE_QINSTANCE, // 15
   SGE_TYPE_SCHEDD_CONF,
   SGE_TYPE_SCHEDD_MONITOR,
   SGE_TYPE_SHUTDOWN,
   SGE_TYPE_MARK_4_REGISTRATION,
   SGE_TYPE_SUBMITHOST, // 20
   SGE_TYPE_USER,
   SGE_TYPE_USERSET,
   SGE_TYPE_HGROUP,
   SGE_TYPE_CENTRY,
   SGE_TYPE_ZOMBIE, // 25
   SGE_TYPE_SUSER,
   SGE_TYPE_RQS,
   SGE_TYPE_AR,
   SGE_TYPE_JOBSCRIPT,
   SGE_TYPE_CATEGORY, // 30


   /*
    * Don't forget to edit
    *
    *    'mirror_base' in libs/mir/sge_mirror.c
    *    'object_base' in libs/sgeobj/sge_object.c
    *    'table_base' in libs/spool/sge_spooling_database.c
    *
    *    'sge_mirror_unsubscribe_internal' libs/mir/sge_mirror.c
    *    'sge_mirror_subscribe_internal' libs/mir/sge_mirror.c
    * if something is changed here!
    */

   SGE_TYPE_ALL,            /* must be the second to the last entry */
   SGE_TYPE_NONE            /* this must the last entry */
} sge_object_type;


/* Datastructure for internal storage of object/message related information */
typedef struct {
   const char *type_name;                 /* type name, e.g. "JOB"      */
   lDescr *descr;                         /* descriptor, e.g. JB_Type       */
   const int key_nm;                      /* nm of key attribute        */
} object_description;

const char *
object_type_get_name(sge_object_type type);

sge_object_type 
object_name_get_type(const char *name);

const lDescr *
object_type_get_descr(sge_object_type type);

int
object_type_get_key_nm(sge_object_type type);

/* JG: TODO: rename to object_has_descr, make function object_has_type 
             and call this function where possible */
bool 
object_has_type(const lListElem *object, const lDescr *descr);

/* JG: TODO: rename to object_get_type_descr, check all calls, if possible pass sge_object_type */
const lDescr *
object_get_type(const lListElem *object);

const lDescr *
object_get_subtype(int nm);

int 
object_get_primary_key(const lDescr *descr);

const char *
object_get_name(const lDescr *descr);

const char *
object_get_name_prefix(const lDescr *descr, dstring *buffer);

const char *
object_append_field_to_dstring(const lListElem *object, lList **answer_list, dstring *buffer, int nm, char string_quotes);
bool 
object_parse_field_from_string(lListElem *object, lList **answer_list, int nm, const char *value);

void
object_delete_range_id(lListElem *object, lList **answer_list, int rnm, u_long32 id);

int 
object_set_range_id(lListElem *object, int rnm, u_long32 start, u_long32 end, u_long32 step);

bool
object_parse_bool_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_parse_ulong32_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_parse_ulong64_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_parse_int_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_parse_char_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_parse_long_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_parse_double_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_parse_float_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_parse_time_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_parse_mem_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_parse_inter_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_parse_list_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string, const lDescr *descr, int nm);

bool
object_parse_celist_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_parse_solist_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_parse_qtlist_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_set_any_type(lListElem *this_elem, int name, void *value);

bool
object_replace_any_type(lListElem *this_elem, int name, lListElem *org_elem);

void
object_get_any_type(const lListElem *this_elem, int name, void *value);

bool  
object_has_differences(const lListElem *this_elem, lList **answer_list, const lListElem *old_elem);

bool
object_list_has_differences(const lList *this_elem, lList **answer_list, const lList *old_elem);

bool
object_unpack_elem_verify(lList **answer_list, sge_pack_buffer *pb, lListElem **epp, const lDescr *descr);

bool
object_list_verify_cull(const lList *lp, const lDescr *descr);

bool
object_verify_cull(const lListElem *ep, const lDescr *descr);

bool
object_verify_ulong_not_null(const lListElem *ep, lList **answer_list, int nm);
bool
object_verify_ulong64_not_null(const lListElem *ep, lList **answer_list, int nm);

bool
object_verify_ulong_null(const lListElem *ep, lList **answer_list, int nm);
bool
object_verify_ulong64_null(const lListElem *ep, lList **answer_list, int nm);

bool
object_verify_double_null(const lListElem *ep, lList **answer_list, int nm);

bool
object_verify_string_not_null(const lListElem *ep, lList **answer_list, int nm);

bool
object_verify_expression_syntax(const lListElem *ep, lList **answer_list);

int
object_verify_name(const lListElem *object, lList **answer_list, int name);

int
object_verify_pe_range(lList **alpp, const char *pe_name, lList *pe_range, const char *object_descr);

int
compress_ressources(lList **alpp, lList *rl, const char *object_descr );
