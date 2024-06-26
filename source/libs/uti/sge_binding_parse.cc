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
 *  Portions of this software are Copyright (c) 2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/* this code is used by shepherd */
#include <cctype>

#include "uti/sge_binding_parse.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_dstring.h"
#include "uti/sge_string.h"

binding_type_t binding_type_to_enum(const char *parameter) {
   binding_type_t type = BINDING_TYPE_NONE;

   if (strstr(parameter, "env") != nullptr) {
      type = BINDING_TYPE_ENV;
   } else if (strstr(parameter, "pe") != nullptr) {
      type = BINDING_TYPE_PE;
   } else if (strstr(parameter, "set") != nullptr) {
      type = BINDING_TYPE_SET;
   }

   return type;
}

bool
binding_type_to_string(binding_type_t type, dstring *string) {
   bool ret = true;

   if (string != nullptr) {
      switch (type) {
         case BINDING_TYPE_SET:
            sge_dstring_append(string, "set");
            break;
         case BINDING_TYPE_PE:
            sge_dstring_append(string, "pe");
            break;
         case BINDING_TYPE_ENV:
            sge_dstring_append(string, "env");
            break;
         default:
            sge_dstring_append(string, "unknown");
      }
   }
   return ret;
}

/****** sge_binding_hlp/binding_striding_parse_step_size() *************************
*  NAME
*     binding_striding_parse_step_size() -- Parses the step size out of the "striding" query. 
*
*  SYNOPSIS
*     int binding_striding_parse_step_size(const char* parameter) 
*
*  FUNCTION
*     Parses the step size for the core binding strategy "striding" out of the 
*     query.
* 
*     The query string is expected to have following syntax: 
*    
*           "striding:<amount>:<stepsize>[:<socket>,<core>]"
*
*  INPUTS
*     const char* parameter - Points to the string with the query. 
*
*  RESULT
*     int - Returns the step size or -1 when it could not been parsed. 
*
*  NOTES
*     MT-NOTE: binding_striding_parse_step_size() is NOT MT safe 
*
*  SEE ALSO
*     ???/???
*******************************************************************************/
bool binding_explicit_extract_sockets_cores(const char *parameter,
                                            int **list_of_sockets, int *samount, int **list_of_cores, int *camount) {
   /* string representation of a socket number */
   char *socket = nullptr;
   /* string representation of a core number */
   char *core = nullptr;
   bool do_endlessly = true;

   /* no sockets and no cores at the beginning */
   *samount = 0;
   *camount = 0;

   if (list_of_sockets == nullptr || list_of_cores == nullptr || *list_of_sockets != nullptr
       || *list_of_cores != nullptr) {
      /* we expect nullptr pointers because we allocate memory within the function */
      return false;
   }

   /* check if the prefix of the parameter is correct */
   if (strstr(parameter, "explicit:") == nullptr) {
      return false;
   }

   if (sge_strtok(parameter, ":") != nullptr) {

      /* first socket,core is mandatory */
      if ((socket = sge_strtok(nullptr, ",")) == nullptr) {
         /* we have no first socket number */
         return false;
      }
      if ((core = sge_strtok(nullptr, ":")) == nullptr) {
         /* we have no first core number */
         return false;
      }

      /* adding first socket,core pair */
      *samount = *camount = 1;
      *list_of_sockets = (int *)realloc(*list_of_sockets, (*samount) * sizeof(int));
      *list_of_cores = (int *)realloc(*list_of_cores, (*camount) * sizeof(int));
      (*list_of_sockets)[0] = atoi(socket);
      (*list_of_cores)[0] = atoi(core);

      while (do_endlessly) {
         /* get socket number */
         if ((socket = sge_strtok(nullptr, ",")) == nullptr || (isdigit(*socket) == 0)) {
            break;
         }

         /* we have a socket therefore we need a core number */
         if ((core = sge_strtok(nullptr, ":")) == nullptr || (isdigit(*core) == 0)) {
            /* missing core number */
            sge_free(list_of_sockets);
            sge_free(list_of_cores);
            return false;
         }

         /* adding the next <socket>,<core> tuple */
         (*samount)++;
         (*camount)++;
         (*list_of_sockets) = (int *)realloc(*list_of_sockets, (*samount) * sizeof(int));
         (*list_of_cores) = (int *)realloc(*list_of_cores, (*camount) * sizeof(int));
         (*list_of_sockets)[*samount - 1] = atoi(socket);
         (*list_of_cores)[*camount - 1] = atoi(core);
      }        /* we try to continue with the next socket if possible */
      /* if "S" or "s" is found this is because the binding string
         in config file is parsed and the topology used by the job "SccScc"
         is followed */

   } else {
      /* this should not be reachable because of the pre-check */
      return false;
   }

   return true;
}

bool
binding_printf_explicit_sockets_cores(dstring *string, int *socket_array, int sockets,
                                      int *core_array, int cores) {
   bool ret = true;

   if (string != nullptr && socket_array != nullptr && core_array != nullptr && sockets == cores) {
      int i;
      bool first_line = true;

      for (i = 0; i < sockets; i++) {
         if (first_line) {
            sge_dstring_append(string, "explicit:");
            first_line = false;
         } else {
            sge_dstring_append_char(string, ':');
         }
         sge_dstring_sprintf_append(string, "%d,%d", socket_array[i], core_array[i]);
      }
   }

   return ret;
}



