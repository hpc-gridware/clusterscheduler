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
#include <cstring>

#include "comm/commlib.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_resource_utilization.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_userset.h"

#include "uti/sge.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "msg_sgeobjlib.h"
#include "msg_common.h"

#include "sge_host.h"

#include "ocs_TopologyString.h"

lListElem *
host_list_locate(const lList *host_list, const char *hostname) {
   lListElem *ret = nullptr;

   DENTER(TOP_LAYER);

   if (host_list != nullptr) {
      if (hostname != nullptr) {
         const lListElem *element = lFirst(host_list);

         if (element != nullptr) {
            int nm = NoName;

            if (object_has_type(element, EH_Type)) {
               nm = object_get_primary_key(EH_Type);
            } else if (object_has_type(element, AH_Type)) {
               nm = object_get_primary_key(AH_Type);
            } else if (object_has_type(element, SH_Type)) {
               nm = object_get_primary_key(SH_Type);
            }
            ret = lGetElemHostRW(host_list, nm, hostname);
         }
      } else {
         CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      }
   } else {
      /*
       * This is a normal case and no error
       * if e.g. someone tries to find a element at qmaster startup to 
       * check if a certain element should be added
       */
      ret = nullptr;
   }

   DRETURN(ret);
}

/****** sgeobj/host/host_is_referenced() **************************************
*  NAME
*     host_is_referenced() -- Is a given host referenced in other objects? 
*
*  SYNOPSIS
*     bool host_is_referenced(const lListElem *host, 
*                             lList **answer_list, 
*                             const lList *queue_list
*                             const lList *hgrp_list) 
*
*  FUNCTION
*     This function returns true if the given "host" is referenced
*     in a cqueue contained in "queue_list" or in a host group. 
*     If this is the case than a corresponding message will be added 
*     to the "answer_list". 
*
*  INPUTS
*     const lListElem *host   - EH_Type, AH_Type or SH_Type object 
*     lList **answer_list     - AN_Type list 
*     const lList *queue_list - CQ_Type list 
*     const lList *hgrp_list  - HGRP_Type list (Master list)
*
*  RESULT
*     int - true (1) or false (0) 
******************************************************************************/
bool host_is_referenced(const lListElem *host,
                        lList **answer_list,
                        const lList *queue_list,
                        const lList *hgrp_list) {
   bool ret = false;

   if (host != nullptr) {
      const lListElem *cqueue = nullptr;
      const lListElem *queue = nullptr;
      const char *hostname = nullptr;
      int nm = NoName;

      if (object_has_type(host, EH_Type)) {
         nm = object_get_primary_key(EH_Type);
      } else if (object_has_type(host, AH_Type)) {
         nm = object_get_primary_key(AH_Type);
      } else if (object_has_type(host, SH_Type)) {
         nm = object_get_primary_key(SH_Type);
      }
      hostname = lGetHost(host, nm);

      /* look at all the queue instances and figure out, if one still references
         the host we are looking for */
      for_each_ep(cqueue, queue_list) {
         queue = lGetSubHost(cqueue, QU_qhostname, hostname, CQ_qinstances);

         if (queue != nullptr) {
            const char *queuename = lGetString(cqueue, CQ_name);

            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_HOSTREFINQUEUE_SS, hostname, queuename);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret = true;
            break;
         }
      }

      /* if we have not found a reference yet, we keep looking in the host groups, if
         we have an exec host */
      if (!ret && object_has_type(host, EH_Type)) {
         const lListElem *hgrp_elem = nullptr;
         lList *host_list = nullptr;

         for_each_ep(hgrp_elem, hgrp_list) {
            hgroup_find_all_references(hgrp_elem, nullptr, hgrp_list, &host_list, nullptr);
            if (host_list != nullptr) {
               if (lGetElemHost(host_list, HR_name, hostname) != nullptr) {
                  const char *hgrp_name = lGetHost(hgrp_elem, HGRP_name);

                  snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_HOSTREFINHGRP_SS, hostname, hgrp_name);
                  answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);

                  ret = true;
                  break;
               }
               lFreeList(&host_list);
            }
         }
      }
   }
   return ret;
}

/****** sgeobj/host/host_get_load_value() *************************************
*  NAME
*     host_get_load_value() -- return a load value of an exec host
*
*  SYNOPSIS
*     const char* host_get_load_value(lListElem *host, const char *name) 
*
*  FUNCTION
*     Returns a certain load value for a certain host.
*
*  INPUTS
*     lListElem *host  - the host to query
*     const char *name - the name of the load value
*
*  RESULT
*     const char* - string describing the load value
*
*  EXAMPLE
*     lListElem *host = lGetElemHost(Master_Host_List, EH_name, "myhost");
*     const char *value = host_get_load_value(host, "np_load_avg");
*     printf("The load on host myhost is %s\n", value);
*
*******************************************************************************/
const char *host_get_load_value(lListElem *host, const char *name)
{
   const lListElem *load;
   const char *value = nullptr;

   if (host != nullptr) {
      load = lGetSubStr(host, HL_name, name, EH_load_list);
      if (load != nullptr) {
         value = lGetString(load, HL_value);
      }
   }
   return value;
}

/* MT-NOTE: sge_resolve_host() is MT safe */
int sge_resolve_host(lListElem *ep, int nm) {
   int pos;
   int ret = CL_RETVAL_OK;
   int dataType;
   char unique[CL_MAXHOSTNAMELEN];
   const char *hostname;

   DENTER(TOP_LAYER);

   memset(unique, 0, CL_MAXHOSTNAMELEN);

   if (ep == nullptr) {
      DRETURN(-1);
   }

   /* ep is no host element, if ep has no nm */
   if ((pos = lGetPosViaElem(ep, nm, SGE_NO_ABORT)) < 0) {
      DRETURN(-1);
   }

   dataType = lGetPosType(lGetElemDescr(ep), pos);
   switch (dataType) {
      case lStringT:
         hostname = lGetPosString(ep, pos);
         DPRINTF("!!!!!!! sge_resolve_host: WARNING call with old lStringT data type,\n");
         DPRINTF("!!!!!!! this data type should be replaced with lHostT data type in\n");
         DPRINTF("!!!!!!! the future! Nevertheless, just a warning! Function works fine!\n");
         break;

      case lHostT:
         hostname = lGetPosHost(ep, pos);
         break;

      default:
         hostname = nullptr;
         ret = CL_RETVAL_GETHOSTNAME_ERROR;
         break;
   }
   /* Check to find hostname only if it was not contained in expression */
   if (hostname != nullptr && !sge_is_expression(hostname)) {
      ret = sge_resolve_hostname(hostname, unique, nm);

      if (ret == CL_RETVAL_OK) {
         switch (dataType) {
            case lStringT:
               lSetPosString(ep, pos, unique);
               break;
            case lHostT:
               lSetPosHost(ep, pos, unique);
               break;
         }
      }
   }

   DRETURN(ret);
}

/* MT-NOTE: sge_resolve_hostname() is MT safe */
int sge_resolve_hostname(const char *hostname, char *unique, int nm) {
   int ret = CL_RETVAL_OK;

   DENTER(TOP_LAYER);

   if (hostname == nullptr) {
      DRETURN(CL_RETVAL_PARAMS);
   }

   /* 
    * these "special" names are resolved:
    *    "global", "unknown", "template")
    */
   switch (nm) {
      case CE_stringval:
         if (strcmp(hostname, SGE_UNKNOWN_NAME) != 0) {
            ret = getuniquehostname(hostname, unique, 0);
         } else {
            strcpy(unique, hostname);
         }

         break;
      case EH_name:
      case CONF_name:
         if ((strcmp(hostname, SGE_GLOBAL_NAME) != 0) &&
             (strcmp(hostname, SGE_TEMPLATE_NAME) != 0)) {
            ret = getuniquehostname(hostname, unique, 0);
         } else {
            strcpy(unique, hostname);
         }
         break;
      default:
         ret = getuniquehostname(hostname, unique, 0);
         break;
   }

   if (ret != CL_RETVAL_OK) {
      strncpy(unique, hostname, CL_MAXHOSTNAMELEN - 1);
   }

   DRETURN(ret);
}

bool
host_is_centry_referenced(const lListElem *this_elem, const lListElem *centry) {
   bool ret = false;

   DENTER(TOP_LAYER);

   if (this_elem != nullptr) {
      const char *name = lGetString(centry, CE_name);
      const lList *ce_values = lGetList(this_elem, EH_consumable_config_list);
      const lList *load_list = lGetList(this_elem, EH_load_list);
      const lList *rep_vars = lGetList(this_elem, EH_report_variables);

      /* 
       * centry may be referenced in 
       *    - complex_values
       *    - load_list
       *    - report_variables
       */
      if (lGetElemStr(ce_values, CE_name, name) != nullptr ||
          lGetElemStr(load_list, HL_name, name) != nullptr ||
          lGetElemStr(rep_vars, STU_name, name) != nullptr) {
         ret = true;
      }
   }

   DRETURN(ret);
}

bool
host_is_centry_a_complex_value(const lListElem *this_elem,
                               const lListElem *centry) {
   bool ret = false;

   DENTER(TOP_LAYER);

   if (this_elem != nullptr) {
      const char *name = lGetString(centry, CE_name);
      const lList *ce_values = lGetList(this_elem, EH_consumable_config_list);
      const lList *load_list = lGetList(this_elem, EH_load_list);

      /* 
       * centry may be referenced in 
       *    - complex_values
       *    - load_list
       */
      if (lGetElemStr(ce_values, CE_name, name) != nullptr ||
          lGetElemStr(load_list, HL_name, name) != nullptr) {
         ret = true;
      }
   }
   DRETURN(ret);
}

/****** sgeobj/host/host_list_merge() ******************************************
*  NAME
*     host_list_merge() -- merge global host settings into exec hosts
*
*  SYNOPSIS
*     bool 
*     host_list_merge(lList *this_list) 
*
*  FUNCTION
*     Merges settings from the global host to the exec hosts objects.
*     Currently this applies only to the report_variables attribute.
*
*  INPUTS
*     lList *this_list - the exec host list to work on
*
*  RESULT
*     bool - true on success, else false
*
*  NOTES
*     MT-NOTE: host_list_merge() is MT safe 
*
*  SEE ALSO
*     sgeobj/host/host_merge()
*******************************************************************************/
bool
host_list_merge(lList *this_list) {
   bool ret = true;

   DENTER(TOP_LAYER);

   if (this_list != nullptr) {
      lListElem *global_host = lGetElemHostRW(this_list, EH_name, SGE_GLOBAL_NAME);

      /* we merge global settings into host settings */
      if (global_host != nullptr) {
         lListElem *host;

         /* for the global host, merged_report_variables == report_variables */
         lSetList(global_host, EH_merged_report_variables, lCopyList("", lGetList(global_host, EH_report_variables)));

         /* do merge for all hosts except global */
         for_each_rw (host, this_list) {
            if (host != global_host) {
               /* on error continue, but return error status */
               if (!host_merge(host, global_host)) {
                  ret = false;
               }
            }
         }
      }
   }

   DRETURN(ret);
}

/****** sgeobj/host/host_merge() **********************************************
*  NAME
*     host_merge() -- merge global host settings into an exec host
*
*  SYNOPSIS
*     bool 
*     host_merge(lListElem *host, const lListElem *global_host) 
*
*  FUNCTION
*     Merges settings from the global host object into a specific exec host.
*     Use the global settings, if no host specific settings are done.
*     Currently this applies only to the report_variables attribute.
*
*  INPUTS
*     lListElem *host              - the host object to hold the merged config
*     const lListElem *global_host - the global host object
*
*  RESULT
*     bool - true on success, else false
*
*  NOTES
*     MT-NOTE: host_merge() is MT safe 
*
*  SEE ALSO
*     sgeobj/host/host_list_merge()
*******************************************************************************/
bool
host_merge(lListElem *host, const lListElem *global_host) {
   bool ret = true;

   DENTER(TOP_LAYER);

   if (host != nullptr && global_host != nullptr) {
      const lList *local_list = lGetList(host, EH_report_variables);

      /* if we have a local list: use this one */
      if (local_list != nullptr && lGetNumberOfElem(local_list) != 0) {
         lSetList(host, EH_merged_report_variables, lCopyList("", local_list));
      } else {
         const lList *global_list;

         global_list = lGetList(global_host, EH_report_variables);
         /* if we have no local list, but a global one, use this one */
         if (global_list != nullptr && lGetNumberOfElem(global_list) != 0) {
            lSetList(host, EH_merged_report_variables, lCopyList("", global_list));
         } else {
            /* if no report variables are configured in local and global object,
             * delete the merged list.
             */
            lSetList(host, EH_merged_report_variables, nullptr);
         }
      }
   }

   DRETURN(ret);
}

/**
 * @brief debit a certain RSMAP id from a host
 *
 * @param host          the host to do debiting on
 * @param ce_name       name of the RSMAP complex variable
 * @param resl          the resource map object to be debited
 * @param slots         the number of slots for which we do debiting (can be negativ in undebiting)
 * @param just_check    when != nullptr we just check if debiting would be successful and return the result here
 * @return the number of modifications done (0 or 1)
 */
int
host_debit_rsmap(lListElem *host, const char *ce_name, const lListElem *resl, int slots, bool *just_check) {
   int mods = 0;

   lList *resource_utilization = lGetListRW(host, EH_resource_utilization);
   lListElem *resource = lGetElemStrRW(resource_utilization, RUE_name, ce_name);

   const char *id = lGetString(resl, RESL_value);
   u_long32 amount = lGetUlong(resl, RESL_amount);

   lListElem *utilized_id = lGetSubStrRW(resource, RESL_value, id, RUE_utilized_now_resource_map_list);
   if (utilized_id == nullptr) {
      utilized_id = lAddSubStr(resource, RESL_value, id, RUE_utilized_now_resource_map_list, RESL_Type);
   }
   if (utilized_id != nullptr) {
      if (just_check != nullptr) {
         // only check if there is enough resources free, no booking
         // only when slots > 0? It is never called with slots < 0 and just_check
         const lListElem *complex_value = lGetSubStr(host, CE_name, ce_name, EH_consumable_config_list);
         const lListElem *configured_id = lGetSubStr(complex_value, RESL_value, id, CE_resource_map_list);
         u_long32 configured_amount = lGetUlong(configured_id, RESL_amount);
         u_long32 utilized_amount = lGetUlong(utilized_id, RESL_amount);
         if (configured_amount - utilized_amount - amount < 0) {
            // not enough available of this id
            *just_check = false;
         }
      } else {
         if (slots < 0) {
            // undebit
            lAddUlong(utilized_id, RESL_amount, -amount);
         } else {
            // debit
            lAddUlong(utilized_id, RESL_amount, amount);
         }
      }
      mods++;
   }

   return mods;
}

int
host_debit_binding(lListElem *host, const char *ce_name, const lListElem *resl, int slots, bool *just_check) {
   DENTER(TOP_LAYER);

   // we book cores/threads in the resource utilization of slots
   lList *resource_utilization = lGetListRW(host, EH_resource_utilization);
   lListElem *resource = lGetElemStrRW(resource_utilization, RUE_name, ce_name);

   // add resource utilization entry if it does not exist yet
   if (resource == nullptr) {
      resource = lAddSubStr(host, RUE_name, ce_name, EH_resource_utilization, RUE_Type);
      lSetString(resource, RUE_utilized_now_binding_inuse, nullptr);
   }

   // what is currently booked and what should be booked additionally
   const char *binding_to_use = lGetString(resl, ST_name);
   const char *binding_in_use = lGetString(resource, RUE_utilized_now_binding_inuse);

#if 1
   const char *action = slots > 0 ? "add" : "del";
   if (binding_in_use != nullptr) {
      ocs::TopologyString topo_in_use(binding_in_use);
      DPRINTF("host_debit_binding: before %s\n", topo_in_use.to_product_topology_string().c_str());
   } else {
      DPRINTF("host_debit_binding: before NONE\n");
   }
   if (binding_to_use != nullptr) {
      ocs::TopologyString topo_to_use(binding_to_use);
      DPRINTF("host_debit_binding: %s    %s\n", action, topo_to_use.to_product_topology_string().c_str());
   }
#endif

   // just check if the requested change makes sense then return
   int mods = 0;
   if (just_check != nullptr) {
      if (binding_in_use == nullptr || strcmp(binding_to_use, binding_in_use) != 0) {
         *just_check = true;
         mods++;
      } else {
         *just_check = false;
      }
      DRETURN(mods);
   }

   // add/remove all cores/threads that are set in binding_to_use to binding_in_use
   ocs::TopologyString topo_now;
   const char *now_str = lGetString(resource, RUE_utilized_now_binding_inuse);
   if (now_str) {
      topo_now = ocs::TopologyString(now_str);
      ocs::TopologyString topo_to_use(binding_to_use);
      ocs::TopologyString::elem_mark_nodes_as_used_or_unused(resource, RUE_utilized_now_binding_inuse,
                                                            topo_now, topo_to_use, slots > 0);
   } else {
      topo_now = ocs::TopologyString(binding_to_use);
      ocs::TopologyString::elem_mark_nodes_as_used_or_unused(resource, RUE_utilized_now_binding_inuse,
                                                            topo_now, topo_now, slots > 0);
   }
   DPRINTF("host_debit_binding: after  %s\n", topo_now.to_product_topology_string().c_str());

   mods++;
   DRETURN(mods);
}

bool
host_do_per_host_booking(const char **last_hostname, const char *hostname)
{
   bool ret;

   if (sge_strnullcmp(*last_hostname, hostname) == 0) {
      // we already booked to this host - do not repeat
      ret = false;
   } else {
      // we did not yet book to this host, it is a new one
      ret = true;
      *last_hostname = hostname;
   }

   return ret;
}

/** @brief Check if a host is visible to the user
 *
 * Manager see all hosts as well as all users. Only in the "department view" mode users would not see those hosts
 * where they have no access to.
 *
 * @param hep the host to check
 * @param is_manager true if the user is a manager
 * @param dept_view true if the user has department view
 * @param acl_list the list of ACLs
 * @return true if the host is visible
 */
bool
host_is_visible(const lListElem *hep, bool is_manager, bool dept_view, const lList *acl_list) {
   bool host_visible = true;

   if (!is_manager && dept_view) {
      const char *username = component_get_username();
      const char *groupname = component_get_groupname();

      int amount;
      ocs_grp_elem_t *grp_array;
      component_get_supplementray_groups(&amount, &grp_array);
      lList *grp_list = grp_list_array2list(amount, grp_array);

      host_visible = sge_has_access_(username, groupname, grp_list, lGetList(hep, EH_acl), lGetList(hep, EH_xacl), acl_list);
      lFreeList(&grp_list);
   }
   return host_visible;
}
