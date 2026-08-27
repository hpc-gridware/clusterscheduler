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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/       

/** @file
 * @brief The attribute list to spool, per object type
 */

/** @brief Upper bound on the length of any field list here
 *
 * Guaranteed to be larger than the longest static or built list, so a caller
 * can size a buffer with it instead of counting first.
 */
#define MAX_NUM_FIELDS 60
   
/** @name Object types whose field list never varies
 *
 * Each is a #spooling_field array terminated by a `NoName` entry, ready to
 * pass to #spool_flatfile_write_object or #spool_flatfile_read_object.
 * @{
 */
extern spooling_field CAL_fields[];    ///< Calendar
extern spooling_field CAT_fields[];    ///< Job category - the fields are `CT_*`, only the array is named `CAT_`
extern spooling_field CK_fields[];     ///< Checkpointing environment
extern spooling_field CE_fields[];     ///< Complex entry
extern spooling_field HGRP_fields[];   ///< Host group
extern spooling_field US_fields[];     ///< User set
extern spooling_field SC_fields[];     ///< Scheduler configuration
extern spooling_field CQ_fields[];     ///< Cluster queue
extern spooling_field CU_fields[];     ///< @warning Declared here, defined nowhere and used nowhere in either repository
extern spooling_field AR_fields[];     ///< Advance reservation
extern spooling_field PE_fields[];     ///< Parallel environment
extern spooling_field RL_fields[];     ///< RBAC role
extern spooling_field RQS_fields[];    ///< Resource quota set
/** @} */

/** @name Object types whose field list depends on the caller
 *
 * These build a #spooling_field array on the heap, which the caller frees
 * with #spool_free_spooling_fields.
 *
 * The recurring `spool` flag is the reason they exist: an object's spool file
 * and the text a user edits under `qconf -m*` are not the same set of
 * attributes. Accumulated usage, ticket state and timestamps have to survive
 * a qmaster restart, so they go into the spool file, but showing them in an
 * editable configuration would invite a user to change values the qmaster
 * owns. `spool == true` adds them, `false` leaves them out.
 * @{
 */
spooling_field *sge_build_PR_field_list(bool spool);

spooling_field *sge_build_UU_field_list(bool spool);

spooling_field *sge_build_STN_field_list(bool spool, bool recurse);

spooling_field *sge_build_STN_json_field_list();

spooling_field *sge_build_EH_field_list(bool spool, bool to_stdout,
                                            bool history);

spooling_field *sge_build_CONF_field_list(bool spool_config);

spooling_field *sge_build_CQ_field_list(bool with_hostlist);

spooling_field *sge_build_QU_field_list(bool to_stdout, bool to_file);
/** @} */
