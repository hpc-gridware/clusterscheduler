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
 * @brief Mailing the administrator about job failures, without flooding them
 */

/*
** defines for admin mail handling of the job-related
** error states
*/
/** @name When a job failure state is worth mailing the administrator about
 *
 * A busy cluster can fail the same way thousands of times in a minute, so each
 * state carries a rule saying how often it may be reported.
 * @{
 */
#define BIT_ADM_ALWAYS   0   ///< Mail every occurrence
#define BIT_ADM_NEVER    1   ///< Never mail about this state
#define BIT_ADM_NEW_CONF 2   ///< Mail again once the configuration has changed
#define BIT_ADM_QCHANGE  4   ///< Mail again once the queue has changed
#define BIT_ADM_HOUR     8   ///< Mail at most once an hour
/** @} */

int adm_mail_reset(int state);
void job_related_adminmail(uint32_t progid, lListElem *jr, int is_array, const char *job_owner);
