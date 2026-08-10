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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Definition of the structs behind the opaque DRMAA data types
 *
 * `drmaa.h` hands the application pointers to `drmaa_job_template_t`,
 * `drmaa_attr_names_t`, `drmaa_attr_values_t` and `drmaa_job_ids_t` without
 * ever showing what they contain. This header contains those definitions and
 * is therefore private to the implementation - an application including it
 * would depend on details the DRMAA specification deliberately hides.
 *
 * The three opaque string vectors are the same layout: a discriminator plus a
 * union of the two iterator implementations, one over the job ids of a bulk
 * job and one over a cull list of strings.
 */

#include "cull/cull.h"

/** @brief The attributes of a job template, both scalar and vector ones */
struct drmaa_job_template_s {
   lList *strings;        ///< The scalar attributes as `VA_Type`, name and value
   lList *string_vectors; ///< The vector attributes as `NSV_Type`, name and list of values
};

/**
 * @brief Iterator over the job ids of a bulk job
 *
 * Returned by drmaa_run_bulk_jobs(). The ids are not stored, they are
 * computed from the task range while the iterator runs.
 */
struct drmaa_bulk_jobid_iterator_s {
   uint32_t jobid;   ///< The job id all tasks of the bulk job share
   int start;        ///< First task number of the range
   int end;          ///< Last task number of the range
   int incr;         ///< Step between two task numbers
   /** Task number the next call returns, past `end` once the iterator is exhausted */
   int next_pos;
};

/**
 * @brief Iterator over a list of strings
 *
 * Returned by japi_get_vector_attribute(), japi_get_attribute_names(),
 * japi_get_vector_attribute_names() and japi_wait() - the last one for the
 * rusage strings of a finished job.
 */
struct drmaa_string_array_iterator_s {
   lList *strings;  ///< The strings as `STR_Type`, owned by the iterator
   /** Element the next call returns, `nullptr` once the iterator is exhausted */
   lListElem *next_pos;
};

/**
 * @brief Discriminator of the union in the opaque string vectors
 */
enum {/** The `ji` member of the union is in use */JAPI_ITERATOR_BULK_JOBS,/** The `si` member of the union is in use */JAPI_ITERATOR_STRINGS };
/** @brief Opaque vector of attribute names */
struct drmaa_attr_names_s {
   int iterator_type;   ///< Which member of `it` is in use, see #JAPI_ITERATOR_STRINGS
   /** The iterator itself, in one of its two implementations */
   union {
      struct drmaa_bulk_jobid_iterator_s ji;
      struct drmaa_string_array_iterator_s si;
   } it;
};
/** @brief Opaque vector of attribute values */
struct drmaa_attr_values_s {
   int iterator_type;   ///< Which member of `it` is in use, see #JAPI_ITERATOR_STRINGS
   /** The iterator itself, in one of its two implementations */
   union {
      struct drmaa_bulk_jobid_iterator_s ji;
      struct drmaa_string_array_iterator_s si;
   } it;
};
/** @brief Opaque vector of job ids */
struct drmaa_job_ids_s {
   int iterator_type;   ///< Which member of `it` is in use, see #JAPI_ITERATOR_STRINGS
   /** The iterator itself, in one of its two implementations */
   union {
      struct drmaa_bulk_jobid_iterator_s ji;
      struct drmaa_string_array_iterator_s si;
   } it;
};
