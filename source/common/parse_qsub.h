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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Interface of the qsub-style command line parser
 */

#include "sgeobj/cull/sge_boundaries.h"
#include "cull/cull.h"

/*
 * defines for SPA_occurrence
 */
#define BIT_SPA_OCC_NONE               0x00000000L   ///< the option did not appear
#define BIT_SPA_OCC_NOARG              0x00000001L   ///< the option appeared without an argument
#define BIT_SPA_OCC_ARG                0x00000002L   ///< the option appeared with an argument

/*
** defines for pseudo-arguments
*/
#define STR_PSEUDO_JOBID       "jobid"       ///< pseudo-argument: the job id operand
#define STR_PSEUDO_SCRIPT      "script"      ///< pseudo-argument: the job script name
#define STR_PSEUDO_JOBARG      "jobarg"      ///< pseudo-argument: one argument to the job script
#define STR_PSEUDO_SCRIPTLEN   "scriptlen"   ///< pseudo-argument: length of an inline script
#define STR_PSEUDO_SCRIPTPTR   "scriptptr"   ///< pseudo-argument: the inline script itself

/*
** flags
*/
#define FLG_USE_PSEUDOS 1   ///< the operands become the pseudo-arguments above rather than an error
#define FLG_QALTER      2   ///< parse for `qalter`, where options that only make sense at submit time are rejected


/* I've added a -wd option to cull_parse_job_parameter() to deal with the
 * DRMAA_WD attribute.  It makes sense to me that since -wd exists and is
 * handled by cull_parse_job_parameter() that -cwd should just become an alias
 * for -wd.  Code to do that is ifdef'ed out below just in case we decide
 * it's a good idea. */
#if 0
/*
** marker to indicate that a -wd was originally a -cwd
*/
#define SGE_HOME_DIRECTORY "$$HOME$$"   ///< marks a `-wd` that was originally a `-cwd`, so it can still be resolved relative to the home directory
#endif

lList *cull_parse_cmdline(uint32_t prog_number, const char **arg_list, char **envp, lList **pcmdline, uint32_t flags);

char *reroot_path(lListElem* pjob, const char *path, lList **alpp);
