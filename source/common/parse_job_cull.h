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
 * @brief Interface of the command line to job object conversion
 */

/*
** flags for parse_script_file
*/

#define FLG_HIGHER_PRIOR         0    ///< options from the script win over options on the command line
#define FLG_LOWER_PRIOR          1    ///< options on the command line win over options in the script
#define FLG_USE_NO_PSEUDOS       2    ///< do not turn the operands into pseudo-arguments
#define FLG_DONT_ADD_SCRIPT      4    ///< do not add the script itself to the option list
#define FLG_IGNORE_EMBEDED_OPTS  8    ///< ignore the `#$` lines in the script
#define FLG_IGN_NO_FILE          16   ///< a missing script file is not an error  

/** @brief The directive prefix used when the job did not ask for another one
 *
 * `"#$"` - the marker that makes a comment line in a job script an option line.
 */
extern const char *default_prefix;

lList *cull_parse_job_parameter(uint32_t uid, const char *username, const char *cell_root, const char *unqualified_hostname,
                                const char *qualified_hostname, lList *cmdline, lListElem **pjob, uint32_t *sync_options);
lList *parse_script_file(uint32_t prog_number, const char *script_file, const char *directive_prefix, lList **option_list_ref, char **envp, uint32_t flags);
