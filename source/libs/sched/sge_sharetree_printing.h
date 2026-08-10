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

/** @file
 * @brief Rendering a share tree, for `qconf -sst` and friends
 *
 * The share tree of the fair share policy is a tree of users and projects
 * with configured shares and accumulated usage. This module renders it - as
 * plain text or as JSON - with the field selection and the delimiters the
 * caller asks for, which is how the same data feeds a human readable table
 * and a machine readable dump.
 */

#include "cull/cull.h"

#include "uti/sge_dstring.h"
#include "rapidjson/writer.h"

/** @brief How a share tree dump is formatted */
typedef struct {
   bool name_format;         ///< Print the node names rather than only the values
   bool format_times;        ///< Render times as dates instead of as seconds
   const char *delim;        ///< Delimiter between two fields
   const char *line_delim;   ///< Delimiter between two lines
   const char *rec_delim;    ///< Delimiter between two records
   const char *str_format;   ///< Format used for string values
   const char *field_names;  ///< Comma separated fields to print, nullptr for all
   const char *line_prefix;  ///< String put in front of every line
} format_t;

void 
print_hdr(dstring *out, const format_t *format);

void
sge_sharetree_print(dstring *out, rapidjson::StringBuffer *jsonBuffer, const lList *sharetree_in,
                    const lList *users, const lList *projects, const lList *usersets, bool group_nodes,
                    bool decay_usage, const char **names, const format_t *format);
