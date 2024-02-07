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
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Host Configuration
*
* Contains configuration options for hosts (execution hosts but also a global configuration for the sge_qmaster).
* Host specific configurations inherit values from the global configuration.
* @todo there is an overlap with the exec host type (EH_Type), can this be unified?
*
*    SGE_HOST(CONF_name) - Host Name
*    Name of the host the configuration object refers to, or global for the global configuration.
*
*    SGE_ULONG(CONF_version) - Configuration Version
*    Each configuration object has a version number which is increased with every change.
*
*    SGE_LIST(CONF_entries) - Configuration Entries
*    A configuration consists of multiple configuration entries of CF_Type.
*
*/

enum {
   CONF_name = CONF_LOWERBOUND,
   CONF_version,
   CONF_entries
};

LISTDEF(CONF_Type)
   SGE_HOST(CONF_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL)
   SGE_ULONG(CONF_version, CULL_SPOOL)
   SGE_LIST(CONF_entries, CF_Type, CULL_SPOOL)
LISTEND

NAMEDEF(CONFN)
   NAME("CONF_name")
   NAME("CONF_version")
   NAME("CONF_entries")
NAMEEND

#define CONF_SIZE sizeof(CONFN)/sizeof(char *)


