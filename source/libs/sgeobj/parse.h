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
 * @brief Declarations for command line parsing
 *
 * @see parse.cc
 */

#include "sgeobj/cull/sge_parse_SPA_L.h"

/**
 * @name Group option constants
 *
 * The `-g` switch of the query clients decides how much detail is collapsed
 * into one output line.
 * @{
 */
#define GROUP_DEFAULT            0x00000000 ///< no grouping; one line per task
#define GROUP_NO_TASK_GROUPS     0x00000001 ///< do not collapse the tasks of an array job
#define GROUP_NO_PETASK_GROUPS   0x00000002 ///< do not collapse the tasks of a parallel job
#define GROUP_CQ_SUMMARY         0x00000004 ///< one line per cluster queue instead of per queue instance
/** @} */

char **parse_noopt(char **sp, const char *shortopt, const char *longopt, lList **ppcmdline, lList **alpp);

char **parse_until_next_opt(char **sp, const char *shortopt, const char *longopt, lList **ppcmdline, lList **alpp);

char **parse_until_next_opt2(char **sp, const char *shortopt, const char *longopt, lList **ppcmdline, lList **alpp);

char **parse_param(char **sp, const char *opt, lList **ppcmdline, lList **alpp);

lListElem *sge_add_arg(lList **popt_list, uint32_t opt_number,
                       uint32_t opt_type, const char *opt_switch,
                       const char *opt_switch_arg);

lListElem *sge_add_noarg(lList **popt_list, uint32_t opt_number, const char *opt_switch, const char *opt_switch_arg);

bool parse_multi_stringlist(lList **ppcmdline, const char *opt, lList **ppal, lList **ppdestlist, lDescr *type, int field);

bool parse_flag(lList **ppcmdline, const char *opt, lList **ppal, uint32_t *pflag);

int parse_string(lList **ppcmdline, const char *opt, lList **ppal, char **str);

int
parse_string_arg(lList **ppcmdline, const char *opt, lList **ppal, char **value);

int
parse_uint32_t(lList **ppcmdline, const char *opt, lList **ppal, uint32_t *value);

int   
parse_u_longlist(lList **ppcmdline, const char *opt, lList **ppal, lList **value);

bool parse_multi_jobtaskslist(lList **ppcmdline, const char *opt, lList **ppal, lList **ppdestlist, bool include_names, uint32_t action);

/**
 * @brief Render a mail recipient list back into the form a user writes
 *
 * @param head the `MR_Type` list to render
 * @param[out] mail_str receives the text
 * @param mail_str_len the size of `mail_str`
 * @return 0 on success, -1 when the text did not fit
 */
int sge_unparse_ma_list(lList *head, char *mail_str, unsigned int mail_str_len); 

uint32_t parse_group_options(lList *string_list, lList **anser_list);

bool sge_parse_bitfield_str(const char *str, const char *set_specifier[],
                           uint32_t *value, const char *name, lList **alpp,  bool none_allowed);
