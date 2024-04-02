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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>
#include <cstdlib>
#include "comm/lists/cl_parameter_list.h"

#include "uti/sge_string.h"

int cl_parameter_list_setup(cl_raw_list_t **list_p, const char *list_name) {
   int ret_val = CL_RETVAL_OK;
   ret_val = cl_raw_list_setup(list_p, list_name, 1);
   return ret_val;
}

int cl_parameter_list_cleanup(cl_raw_list_t **list_p) {
   cl_parameter_list_elem_t *elem = nullptr;

   if (list_p == nullptr) {
      /* we expect an address of an pointer */
      return CL_RETVAL_PARAMS;
   }
   if (*list_p == nullptr) {
      /* we expect an initalized pointer */
      return CL_RETVAL_PARAMS;
   }

   /* delete all entries in list */
   cl_raw_list_lock(*list_p);
   while ((elem = cl_parameter_list_get_first_elem(*list_p)) != nullptr) {
      cl_raw_list_remove_elem(*list_p, elem->raw_elem);
      free(elem->parameter);
      free(elem->value);
      free(elem);
   }
   cl_raw_list_unlock(*list_p);
   return cl_raw_list_cleanup(list_p);
}

int cl_parameter_list_append_parameter(cl_raw_list_t *list_p, const char *parameter, const char *value, int lock_list) {

   int ret_val;
   cl_parameter_list_elem_t *new_elem = nullptr;

   if (parameter == nullptr || value == nullptr || list_p == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   /* lock the list */
   if (lock_list == 1) {
      if ((ret_val = cl_raw_list_lock(list_p)) != CL_RETVAL_OK) {
         return ret_val;
      }
   }

   /* add new element list */
   new_elem = (cl_parameter_list_elem_t *) sge_malloc(sizeof(cl_parameter_list_elem_t));

   if (new_elem == nullptr) {
      if (lock_list == 1) {
         cl_raw_list_unlock(list_p);
      }
      return CL_RETVAL_MALLOC;
   }

   new_elem->parameter = strdup(parameter);
   if (new_elem->parameter == nullptr) {
      free(new_elem);
      if (lock_list == 1) {
         cl_raw_list_unlock(list_p);
      }
      return CL_RETVAL_MALLOC;
   }

   new_elem->value = strdup(value);
   if (new_elem->value == nullptr) {
      free(new_elem->parameter);
      free(new_elem);
      if (lock_list == 1) {
         cl_raw_list_unlock(list_p);
      }
      return CL_RETVAL_MALLOC;
   }

   new_elem->raw_elem = cl_raw_list_append_elem(list_p, (void *) new_elem);
   if (new_elem->raw_elem == nullptr) {
      free(new_elem->parameter);
      free(new_elem->value);
      free(new_elem);
      if (lock_list == 1) {
         cl_raw_list_unlock(list_p);
      }
      return CL_RETVAL_MALLOC;
   }
   CL_LOG_STR(CL_LOG_INFO, "adding new parameter:", new_elem->parameter);
   CL_LOG_STR(CL_LOG_INFO, "value is            :", new_elem->value);

   /* unlock the list */
   if (lock_list == 1) {
      if ((ret_val = cl_raw_list_unlock(list_p)) != CL_RETVAL_OK) {
         return ret_val;
      }
   }
   return CL_RETVAL_OK;
}

int cl_parameter_list_remove_parameter(cl_raw_list_t *list_p, const char *parameter, int lock_list) {
   int ret_val = CL_RETVAL_OK;
   int function_return = CL_RETVAL_UNKNOWN_PARAMETER;
   cl_parameter_list_elem_t *elem = nullptr;

   if (list_p == nullptr || parameter == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   if (lock_list != 0) {
      /* lock list */
      if ((ret_val = cl_raw_list_lock(list_p)) != CL_RETVAL_OK) {
         return ret_val;
      }
   }

   elem = cl_parameter_list_get_first_elem(list_p);
   while (elem != nullptr) {
      if (strcmp(elem->parameter, parameter) == 0) {
         /* found matching element */
         cl_raw_list_remove_elem(list_p, elem->raw_elem);
         function_return = CL_RETVAL_OK;
         free(elem->parameter);
         free(elem->value);
         free(elem);
         elem = nullptr;
         break;
      }
      elem = cl_parameter_list_get_next_elem(elem);
   }

   if (lock_list != 0) {
      /* unlock list */
      if ((ret_val = cl_raw_list_unlock(list_p)) != CL_RETVAL_OK) {
         return ret_val;
      }
   }
   return function_return;
}

cl_parameter_list_elem_t *cl_parameter_list_get_first_elem(cl_raw_list_t *list_p) {
   cl_raw_list_elem_t *raw_elem = cl_raw_list_get_first_elem(list_p);

   if (raw_elem) {
      return (cl_parameter_list_elem_t *) raw_elem->data;
   }
   return nullptr;
}

cl_parameter_list_elem_t *cl_parameter_list_get_least_elem(cl_raw_list_t *list_p) {
   cl_raw_list_elem_t *raw_elem = cl_raw_list_get_least_elem(list_p);

   if (raw_elem) {
      return (cl_parameter_list_elem_t *) raw_elem->data;
   }
   return nullptr;
}

cl_parameter_list_elem_t *cl_parameter_list_get_next_elem(cl_parameter_list_elem_t *elem) {
   cl_raw_list_elem_t *next_raw_elem = nullptr;

   if (elem != nullptr) {
      cl_raw_list_elem_t *raw_elem = elem->raw_elem;
      next_raw_elem = cl_raw_list_get_next_elem(raw_elem);
      if (next_raw_elem) {
         return (cl_parameter_list_elem_t *) next_raw_elem->data;
      }
   }
   return nullptr;
}

cl_parameter_list_elem_t *cl_parameter_list_get_last_elem(cl_parameter_list_elem_t *elem) {
   cl_raw_list_elem_t *last_raw_elem = nullptr;

   if (elem != nullptr) {
      cl_raw_list_elem_t *raw_elem = elem->raw_elem;
      last_raw_elem = cl_raw_list_get_last_elem(raw_elem);
      if (last_raw_elem) {
         return (cl_parameter_list_elem_t *) last_raw_elem->data;
      }
   }
   return nullptr;
}

int cl_parameter_list_get_param_string(cl_raw_list_t *list_p, char **param_string, int lock_list) {
   cl_parameter_list_elem_t *elem = nullptr;
   cl_parameter_list_elem_t *next_elem = nullptr;
   cl_parameter_list_elem_t *first_elem = nullptr;
   int ret_val = CL_RETVAL_OK;
   size_t malloc_size = 0;

   if (list_p == nullptr || param_string == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   if (*param_string != nullptr) {
      return CL_RETVAL_PARAMS;
   }


   if (lock_list == 1) {
      /* lock list */
      if ((ret_val = cl_raw_list_lock(list_p)) != CL_RETVAL_OK) {
         return ret_val;
      }
   }

   /* If we have no elems, return empty malloced string */
   if (cl_raw_list_get_elem_count(list_p) == 0) {
      *param_string = strdup("");
      if (lock_list == 1) {
         if ((ret_val = cl_raw_list_unlock(list_p)) != CL_RETVAL_OK) {
            return ret_val;
         }
      }
      if (*param_string == nullptr) {
         return CL_RETVAL_MALLOC;
      }
      return CL_RETVAL_OK;
   }

   /* store first and last elem of list */
   first_elem = cl_parameter_list_get_first_elem(list_p);

   /* go over list and calculate string size:
    * each parameter value pair have an "=" and a ":"
    * the last entry has no ":" so we can simply add 2 bytes for each
    * parameter/value string.
    * The string termination value "0" is also recognized because
    * the last elem has no ":"
    */
   elem = first_elem;
   while (elem != nullptr) {
      malloc_size = malloc_size + strlen(elem->parameter) + strlen(elem->value) + 2;
      elem = cl_parameter_list_get_next_elem(elem);
   }

   /* malloc return string */
   *param_string = (char *)calloc(malloc_size, sizeof(char));
   if (*param_string == nullptr) {
      /*unlock parameter list*/
      if (lock_list == 1) {
         if ((ret_val = cl_raw_list_unlock(list_p)) != CL_RETVAL_OK) {
            return ret_val;
         }
      }
      return CL_RETVAL_MALLOC;
   }

   elem = first_elem;
   while (elem != nullptr) {
      next_elem = cl_parameter_list_get_next_elem(elem);
      if (next_elem == nullptr) {
         /* this is last elem! */
         /* we need no ":" at the end, because it's the last element*/
         sge_strlcat(*param_string, elem->parameter, strlen(elem->parameter));
         sge_strlcat(*param_string, "=", 1);
         sge_strlcat(*param_string, elem->value, strlen(elem->value));
      } else {
         sge_strlcat(*param_string, elem->parameter, strlen(elem->parameter));
         sge_strlcat(*param_string, "=", 1);
         sge_strlcat(*param_string, elem->value, strlen(elem->value));
         sge_strlcat(*param_string, ":", 1);
      }
      elem = next_elem;
   }

   /*unlock parameter list*/
   if (lock_list == 1) {
      if ((ret_val = cl_raw_list_unlock(list_p)) != CL_RETVAL_OK) {
         return ret_val;
      }
   }
   return CL_RETVAL_OK;
}
