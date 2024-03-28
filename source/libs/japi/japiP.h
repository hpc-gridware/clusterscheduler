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
/*___INFO__MARK_END__*/

#include "cull/cull.h"

struct drmaa_job_template_s {
   lList *strings;        /* VA_Type  */
   lList *string_vectors; /* NSV_Type */
};

/* 
 * This iterator is returned by 
 *   drmaa_run_bulk_jobs()             - vector of vector of job ids
 */
struct drmaa_bulk_jobid_iterator_s {
   u_long32 jobid;
   int start;
   int end;
   int incr;
   /* next position of iterator */
   int next_pos;
};

/* 
 * This iterator is returned by 
 *   japi_get_vector_attribute()       - vector of attribute values
 *   japi_get_attribute_names()        - vector of attribute name 
 *   japi_get_vector_attribute_names() - vector of attribute name 
 *   japi_wait()                       - vector of rusage strings 
 */
struct drmaa_string_array_iterator_s {
   lList *strings;  /* STR_Type  */
   /* next position of iterator */
   lListElem *next_pos;
};

/*
 * Transparent use of two different iterators 
 */
enum { JAPI_ITERATOR_BULK_JOBS, JAPI_ITERATOR_STRINGS };
struct drmaa_attr_names_s {
   int iterator_type; 
   union {
      struct drmaa_bulk_jobid_iterator_s ji;
      struct drmaa_string_array_iterator_s si;
   } it;
};
struct drmaa_attr_values_s {
   int iterator_type; 
   union {
      struct drmaa_bulk_jobid_iterator_s ji;
      struct drmaa_string_array_iterator_s si;
   } it;
};
struct drmaa_job_ids_s {
   int iterator_type; 
   union {
      struct drmaa_bulk_jobid_iterator_s ji;
      struct drmaa_string_array_iterator_s si;
   } it;
};
