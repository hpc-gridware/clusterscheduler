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

#include "cull/cull.h"

#include "sgeobj/sge_usage.h"
#include "sgeobj/sge_host.h"

/****** sgeobj/usage/usage_list_get_ulong_usage() *****************************
*  NAME
*     usage_list_get_ulong_usage() -- return ulong usage value
*
*  SYNOPSIS
*     u_long32 
*     usage_list_get_ulong_usage(const lList *usage_list, const char *name, 
*                                u_long32 def) 
*
*  FUNCTION
*     Searches a usage object with the given name in the given usage 
*     list. If such an element is found, returns the value of the 
*     usage object as u_long32 value.
*     If no such element is found, return the given default value.
*
*  INPUTS
*     const lList *usage_list - the usage list
*     const char *name        - name of the element to search
*     u_long32 def            - default value
*
*  RESULT
*     u_long32 - value of found object or default
*
*  SEE ALSO
*     gdi/usage/usage_list_get_double_usage()
*******************************************************************************/
u_long32
usage_list_get_ulong_usage(const lList *usage_list, const char *name,
                           u_long32 def)
{
   const lListElem *ep = lGetElemStr(usage_list, UA_name, name);
   if (ep != nullptr) {
      return (u_long32)lGetDouble(ep, UA_value);
   } else {
      return def;
   }
}

u_long64
usage_list_get_ulong64_usage(const lList *usage_list, const char *name,
                             u_long64 def)
{
   const lListElem *ep = lGetElemStr(usage_list, UA_name, name);
   if (ep != nullptr) {
      return (u_long64)lGetDouble(ep, UA_value);
   } else {
      return def;
   }
}

/****** sgeobj/usage/usage_list_get_double_usage() ****************************
*  NAME
*     usage_list_get_double_usage() -- return double usage value
*
*  SYNOPSIS
*     double
*     usage_list_get_double_usage(const lList *usage_list, const char *name, 
*                                 double def) 
*
*  FUNCTION
*     Searches a usage object with the given name in the given usage 
*     list. If such an element is found, returns the value of the 
*     usage object as double value.
*     If no such element is found, return the given default value.
*
*  INPUTS
*     const lList *usage_list - the usage list
*     const char *name        - name of the element to search
*     double def              - default value
*
*  RESULT
*     double - value of found object or default
*
*  SEE ALSO
*     gdi/usage/usage_list_get_ulong_usage()
*******************************************************************************/
double
usage_list_get_double_usage(const lList *usage_list, const char *name,
                            double def)
{
   const lListElem *ep = lGetElemStr(usage_list, UA_name, name);
   if(ep != nullptr) {
      return lGetDouble(ep, UA_value);
   } else {
      return def;
   }
}

/****** sgeobj/usage/usage_list_set_ulong_usage() ******************************
*  NAME
*     usage_list_set_ulong_usage() -- create/update a usage record
*
*  SYNOPSIS
*     void
*     usage_list_set_ulong_usage(lList *usage_list, const char *name, 
*                                u_long32 value) 
*
*  FUNCTION
*     Updates the value of a usage record. If no usage record exists with the
*     given name in usage_list, a new record is created.
*
*  INPUTS
*     lList *usage_list - list containing the usage record to update
*     const char *name  - name of the usage record to update
*     u_long32 value    - the new value
*
*  NOTES
*     MT-NOTE: usage_list_set_ulong_usage() is MT safe 
*
*  SEE ALSO
*     sgeobj/usage/usage_list_set_double_usage()
*     sgeobj/usage/usage_list_get_ulong_usage()
*     sgeobj/usage/usage_list_get_double_usage()
*******************************************************************************/
void
usage_list_set_ulong_usage(lList *usage_list, const char *name, u_long32 value)
{
   usage_list_set_double_usage(usage_list, name, value);
}

void
usage_list_set_ulong64_usage(lList *usage_list, const char *name, u_long64 value)
{
   usage_list_set_double_usage(usage_list, name, value);
}

/****** sgeobj/usage/usage_list_set_double_usage() ******************************
*  NAME
*     usage_list_set_double_usage() -- create/update a usage record
*
*  SYNOPSIS
*     void
*     usage_list_set_double_usage(lList *usage_list, const char *name, 
*                                 double value) 
*
*  FUNCTION
*     Updates the value of a usage record. If no usage record exists with the
*     given name in usage_list, a new record is created.
*
*  INPUTS
*     lList *usage_list - list containing the usage record to update
*     const char *name  - name of the usage record to update
*     double value      - the new value
*
*  NOTES
*     MT-NOTE: usage_list_set_double_usage() is MT safe 
*
*  SEE ALSO
*     sgeobj/usage/usage_list_set_ulong_usage()
*     sgeobj/usage/usage_list_get_ulong_usage()
*     sgeobj/usage/usage_list_get_double_usage()
*******************************************************************************/
void
usage_list_set_double_usage(lList *usage_list, const char *name, double value)
{
   lListElem *ep = lGetElemStrRW(usage_list, UA_name, name);
   if (ep == nullptr) {
      ep = lAddElemStr(&usage_list, UA_name, name, UA_Type);
   }

   lSetDouble(ep, UA_value, value);
}

/****** sge_usage/usage_list_sum() *********************************************
*  NAME
*     usage_list_sum() -- sum up usage of two lists
*
*  SYNOPSIS
*     void 
*     usage_list_sum(lList *usage_list, const lList *add_usage_list) 
*
*  FUNCTION
*     Add the usage reported in add_usage_list to usage_list.
*     Summing up of usage will only be done for certain attributes:
*        - cpu
*        - io
*        - iow
*        - mem
*        - vmem
*        - maxvmem
*        - all ru_* attributes (see man getrusage.2)
*
*  INPUTS
*     lList *usage_list           - the usage list to contain all usage
*     const lList *add_usage_list - usage to add to usage_list
*
*  NOTES
*     MT-NOTE: usage_list_sum() is MT safe 
*******************************************************************************/
void
usage_list_sum(lList *usage_list, const lList *add_usage_list)
{
   const lListElem *usage;

   for_each_ep(usage, add_usage_list) {
      const char *name = lGetString(usage, UA_name);
      /* Sum up all usage attributes. */
      if (strcmp(name, USAGE_ATTR_CPU) == 0 ||
          strcmp(name, USAGE_ATTR_IO) == 0 ||
          strcmp(name, USAGE_ATTR_IOW) == 0 ||
          strcmp(name, USAGE_ATTR_VMEM) == 0 ||
          strcmp(name, USAGE_ATTR_RSS) == 0 ||
          strcmp(name, USAGE_ATTR_MEM) == 0 ||
          strncmp(name, "acct_", 5) == 0 ||
          strncmp(name, "ru_", 3) == 0) {
         lListElem *sum = lGetElemStrRW(usage_list, UA_name, name);
         if (sum == nullptr) {
            lAppendElem(usage_list, lCopyElem(usage));
         } else {
            lAddDouble(sum, UA_value, lGetDouble(usage, UA_value));
         }
      }
   }
}

/* if the scaled usage list does not yet exist, it is created and returned */
lList *scale_usage(
const lList *scaling,     /* HS_Type */
const lList *prev_usage,  /* HS_Type */
lList *scaled_usage /* UA_Type */
) {
   const lListElem *sep;
   lListElem *ep;
   const lListElem *prev;

   if (!scaling) {
      return nullptr;
   }

   if (scaled_usage == nullptr) {
      scaled_usage = lCreateList("usage", UA_Type);
   }

   for_each_rw (ep, scaled_usage) {
      if ((sep=lGetElemStr(scaling, HS_name, lGetString(ep, UA_name)))) {
         lSetDouble(ep, UA_value, lGetDouble(ep, UA_value) * lGetDouble(sep, HS_value));
      }
   }

   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_CPU)) != nullptr) {
      if ((ep=lGetElemStrRW(scaled_usage, UA_name, USAGE_ATTR_CPU))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }
   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_IO)) != nullptr) {
      if ((ep=lGetElemStrRW(scaled_usage, UA_name, USAGE_ATTR_IO))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }
   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_IOW)) != nullptr) {
      if ((ep=lGetElemStrRW(scaled_usage, UA_name, USAGE_ATTR_IOW))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }
   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_VMEM)) != nullptr) {
      if ((ep=lGetElemStrRW(scaled_usage, UA_name, USAGE_ATTR_VMEM))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }
   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_MAXVMEM)) != nullptr) {
      if ((ep=lGetElemStrRW(scaled_usage, UA_name, USAGE_ATTR_MAXVMEM))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }
   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_RSS)) != nullptr) {
      if ((ep=lGetElemStrRW(scaled_usage, UA_name, USAGE_ATTR_RSS))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }
   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_MAXRSS)) != nullptr) {
      if ((ep=lGetElemStrRW(scaled_usage, UA_name, USAGE_ATTR_MAXRSS))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }
   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_MEM)) != nullptr) {
      if ((ep=lGetElemStrRW(scaled_usage, UA_name, USAGE_ATTR_MEM))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }

   return scaled_usage;
}
