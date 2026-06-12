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
 *  Portions of this software are Copyright (c) 2024-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "spool/sge_spooling_utilities.h"
#include "spool/flatfile/sge_flatfile.h"   /* CS-2313a: spool_flatfile_format */
#include "sgeobj/sge_daemonize.h"

/* CS-2313a: the -fmt selected serialization format (plain|json), shared with the
 * bespoke show paths in ocs_qconf_{cqueue,centry,rqs}.cc. */
extern spool_flatfile_format qconf_opt_format;

/* CS-2313a: fill the CE_valtype/CE_doubleval of an object's complex-value sublists
 * (e.g. QU_consumable_config_list, QU_load_thresholds) from the centry definitions
 * so the JSON writer can emit native numbers. No-op unless -fmt json. */
void qconf_json_fill_complex(lListElem *obj, const int *ce_fields, int n_fields);

typedef struct object_info_entry {
   ocs::gdi::Target target;
   const char *object_name;
   lDescr *cull_descriptor;
   const char *attribute_name;
   int nm_name;
   spooling_field *fields;
   const struct spool_flatfile_instr *instr;
   bool (*pre_gdi_function)(lList *list, lList **answer_list);
} object_info_entry;

int sge_parse_qconf(char **argv);
