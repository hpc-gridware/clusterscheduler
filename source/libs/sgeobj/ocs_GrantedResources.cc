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
 *  Portions of this software are Copyright (c) 2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>
#include <sstream>

#include "uti/ocs_Systemd.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"
#include "uti/sge_log.h"

#include "ocs_GrantedResources.h"
#include "ocs_TopologyString.h"
#include "sgeobj/sge_grantedres.h"
#include "sgeobj/sge_str.h"

#include "uti/sge.h"

std::string
ocs::GrantedResources::to_string(const lList *granted_resources) {
   DENTER(TOP_LAYER);

   // NONE
   if (granted_resources == nullptr) {
      DRETURN(NONE_STR);
   }

   // <hostname>=<topo>, ...
   std::stringstream ss;
   const void *iterator = nullptr;
   const lListElem *next = lGetElemStrFirst(granted_resources, GRU_name, SGE_ATTR_SLOTS, &iterator);
   const lListElem *curr;
   bool add_separator = false;
   while ((curr = next) != nullptr) {
      next = lGetElemStrNext(granted_resources, GRU_name, SGE_ATTR_SLOTS, &iterator);

      const char *hostname = lGetHost(curr, GRU_host);
      const lList *binding_in_use_list = lGetList(curr, GRU_binding_inuse);
      const lListElem *binding_in_use;
      for_each_ep(binding_in_use, binding_in_use_list) {
         TopologyString topo_in_use(lGetString(binding_in_use, ST_name));
         ss << (add_separator ? "," : "") << hostname << "=" << topo_in_use.to_product_topology_string();
         add_separator = true;
      }
   }
   DRETURN(ss.str());
}

bool
ocs::GrantedResources::add_binding_touse(lList **granted_resources_list, const char *host_name, const lList *binding_touse_list) {
   DENTER(TOP_LAYER);
   if (binding_touse_list != nullptr) {
      DPRINTF("  ==> gru_list_add_binding_touse:\n");
      DPRINTF("   -> adding new GRU with binding information: %d element(s)\n", lGetNumberOfElem(binding_touse_list));

      // @todo CS-731: is the GRU_name correct here? can we use slots? JG suggests a new complex "binding"
      lListElem *gru = lAddElemStr(granted_resources_list, GRU_name, SGE_ATTR_SLOTS, GRU_Type);
      lSetHost(gru, GRU_host, host_name);
      lSetUlong(gru, GRU_type, GRU_BINDING_TYPE);
      lSetList(gru, GRU_binding_inuse, lCopyList("binding_to_use", binding_touse_list));
   }
   DRETURN(true);
}

