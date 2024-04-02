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

#include <sys/time.h>

#include "comm/cl_endpoint_list.h"
#include "comm/cl_commlib.h"

int cl_endpoint_list_setup(cl_raw_list_t **list_p,
                           const char *list_name,
                           long entry_life_time,
                           long refresh_interval,
                           bool create_hash) {

   int ret_val = CL_RETVAL_OK;
   struct timeval now;
   cl_endpoint_list_data_t *ldata = nullptr;

   ldata = (cl_endpoint_list_data_t *) sge_malloc(sizeof(cl_endpoint_list_data_t));
   if (ldata == nullptr) {
      return CL_RETVAL_MALLOC;
   }

   gettimeofday(&now, nullptr);

   ldata->entry_life_time = entry_life_time;
   ldata->refresh_interval = refresh_interval;
   ldata->last_refresh_time = now.tv_sec;


   if (ldata->entry_life_time == 0) {
      CL_LOG(CL_LOG_INFO, "using default value for entry_life_time");
      ldata->entry_life_time = CL_ENDPOINT_LIST_DEFAULT_LIFE_TIME;
   }

   if (ldata->refresh_interval == 0) {
      CL_LOG(CL_LOG_INFO, "using default value for refresh_interval");
      ldata->refresh_interval = CL_ENDPOINT_LIST_DEFAULT_REFRESH_TIME;
   }


   ret_val = cl_raw_list_setup(list_p, list_name, 1);
   if (ret_val != CL_RETVAL_OK) {
      sge_free(&ldata);
      return ret_val;
   }

   /* create hashtable */
   if (create_hash == true) {
      ldata->ht = sge_htable_create(4, dup_func_string, hash_func_string, hash_compare_string);
      if (ldata->ht == nullptr) {
         cl_raw_list_cleanup(list_p);
         sge_free(&ldata);
         return CL_RETVAL_MALLOC;
      }
      CL_LOG_INT(CL_LOG_INFO, "created hash table with size =", 4);
   } else {
      CL_LOG(CL_LOG_INFO, "created NO hash table!");
      ldata->ht = nullptr;
   }

   /* set private list data */
   (*list_p)->list_data = ldata;

   CL_LOG_INT(CL_LOG_INFO, "entry_life_time is: ", ldata->entry_life_time);
   CL_LOG_INT(CL_LOG_INFO, "refresh_interval is:", ldata->refresh_interval);

   return ret_val;
}

int cl_endpoint_list_set_entry_life_time(cl_raw_list_t *list_p, long entry_life_time) {
   cl_endpoint_list_data_t *ldata = nullptr;

   ldata = cl_endpoint_list_get_data(list_p);
   if (ldata != nullptr) {
      ldata->entry_life_time = entry_life_time;
      CL_LOG_INT(CL_LOG_INFO, "setting entry life time to", entry_life_time);
      return CL_RETVAL_OK;
   } else {
      CL_LOG(CL_LOG_ERROR, "can't set new entry_life_time");
   }
   return CL_RETVAL_PARAMS;
}

cl_endpoint_list_data_t *cl_endpoint_list_get_data(cl_raw_list_t *list_p) {

   cl_endpoint_list_data_t *ldata = nullptr;
   cl_raw_list_t *endpoint_list = nullptr;

   if (list_p == nullptr) {
      endpoint_list = cl_com_get_endpoint_list();
   } else {
      endpoint_list = list_p;
   }
   if (endpoint_list == nullptr) {
      CL_LOG(CL_LOG_WARNING, "no global endpoint_list");
      return nullptr;
   }

   ldata = (cl_endpoint_list_data_t *) endpoint_list->list_data;
   return ldata;
}

int cl_endpoint_list_cleanup(cl_raw_list_t **list_p) {
   cl_endpoint_list_data_t *ldata = nullptr;
   cl_endpoint_list_elem_t *elem = nullptr;

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
   while ((elem = cl_endpoint_list_get_first_elem(*list_p)) != nullptr) {
      cl_raw_list_remove_elem(*list_p, elem->raw_elem);
      cl_com_free_endpoint(&(elem->endpoint));
      sge_free(&elem);
   }
   cl_raw_list_unlock(*list_p);

   /* clean list private data */
   ldata = (cl_endpoint_list_data_t *)(*list_p)->list_data;
   if (ldata != nullptr) {
      if (ldata->ht != nullptr) {
         sge_htable_destroy(ldata->ht);
      }
      sge_free(&ldata);
   }
   (*list_p)->list_data = nullptr;

   return cl_raw_list_cleanup(list_p);
}

int cl_endpoint_list_define_endpoint(cl_raw_list_t *list_p, cl_com_endpoint_t *endpoint, int service_port,
                                     cl_xml_connection_autoclose_t autoclose, bool is_static) {

   int ret_val = CL_RETVAL_OK;
   struct timeval now;
   cl_com_endpoint_t *dup_endpoint = nullptr;
   cl_endpoint_list_elem_t *new_elem = nullptr;
   cl_endpoint_list_elem_t *elem = nullptr;

   if (endpoint == nullptr || list_p == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   /* lock the list and check for duplicate entry */
   if ((ret_val = cl_raw_list_lock(list_p)) != CL_RETVAL_OK) {
      return ret_val;
   }

   elem = cl_endpoint_list_get_elem_endpoint(list_p, endpoint);
   if (elem) {
      /* found matching endpoint */
      gettimeofday(&now, nullptr);
      elem->last_used = now.tv_sec;
      elem->service_port = service_port;
      elem->autoclose = autoclose;
      if (elem->is_static == true && is_static == false) {
         CL_LOG(CL_LOG_DEBUG, "can't set static element to non static");
      } else {
         elem->is_static = is_static;
      }

      /* unlock the list */
      if ((ret_val = cl_raw_list_unlock(list_p)) != CL_RETVAL_OK) {
         return ret_val;
      }
      return CL_RETVAL_OK;
   }

   /* unlock the list */
   if ((ret_val = cl_raw_list_unlock(list_p)) != CL_RETVAL_OK) {
      return ret_val;
   }

   /* create a copy of endpoint */
   dup_endpoint = cl_com_dup_endpoint(endpoint);
   if (dup_endpoint == nullptr) {
      return CL_RETVAL_MALLOC;
   }

   /* add new element list */
   new_elem = (cl_endpoint_list_elem_t *) sge_malloc(sizeof(cl_endpoint_list_elem_t));
   if (new_elem == nullptr) {
      cl_com_free_endpoint(&dup_endpoint);
      return CL_RETVAL_MALLOC;
   }

   gettimeofday(&now, nullptr);
   new_elem->endpoint = dup_endpoint;
   new_elem->service_port = service_port;
   new_elem->autoclose = autoclose;
   new_elem->is_static = is_static;
   new_elem->last_used = now.tv_sec;

   /* lock the list */
   if ((ret_val = cl_raw_list_lock(list_p)) != CL_RETVAL_OK) {
      return ret_val;
   }
   new_elem->raw_elem = cl_raw_list_append_elem(list_p, (void *) new_elem);
   if (new_elem->raw_elem == nullptr) {
      cl_raw_list_unlock(list_p);
      cl_com_free_endpoint(&dup_endpoint);
      sge_free(&new_elem);
      return CL_RETVAL_MALLOC;
   } else {
      cl_endpoint_list_data_t *ldata = (cl_endpoint_list_data_t *)list_p->list_data;
      if (ldata->ht != nullptr) {
         sge_htable_store(ldata->ht, dup_endpoint->hash_id, new_elem);
      }
   }

   /* unlock the list */
   if ((ret_val = cl_raw_list_unlock(list_p)) != CL_RETVAL_OK) {
      return ret_val;
   }

   return CL_RETVAL_OK;
}

int cl_endpoint_list_get_autoclose_mode(cl_raw_list_t *list_p, cl_com_endpoint_t *endpoint,
                                        cl_xml_connection_autoclose_t *autoclose) {
   int back = CL_RETVAL_UNKNOWN_ENDPOINT;
   int ret_val = CL_RETVAL_OK;
   cl_endpoint_list_elem_t *elem = nullptr;

   if (list_p == nullptr || endpoint == nullptr || autoclose == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   *autoclose = CL_CM_AC_UNDEFINED;

   /* lock list */
   if ((ret_val = cl_raw_list_lock(list_p)) != CL_RETVAL_OK) {
      return ret_val;
   }

   elem = cl_endpoint_list_get_elem_endpoint(list_p, endpoint);
   if (elem != nullptr) {
      /* found matching endpoint */
      back = CL_RETVAL_OK;
      CL_LOG_INT(CL_LOG_INFO, "setting autoclose to:", elem->autoclose);
      *autoclose = elem->autoclose;
   }

   /* unlock list */
   if ((ret_val = cl_raw_list_unlock(list_p)) != CL_RETVAL_OK) {
      return ret_val;
   }
   return back;
}

int cl_endpoint_list_get_service_port(cl_raw_list_t *list_p, cl_com_endpoint_t *endpoint, int *service_port) {
   int back = CL_RETVAL_UNKNOWN_ENDPOINT;
   int ret_val = CL_RETVAL_OK;
   cl_endpoint_list_elem_t *elem = nullptr;

   if (list_p == nullptr || endpoint == nullptr || service_port == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   *service_port = 0;

   /* lock list */
   if ((ret_val = cl_raw_list_lock(list_p)) != CL_RETVAL_OK) {
      return ret_val;
   }

   elem = cl_endpoint_list_get_elem_endpoint(list_p, endpoint);
   if (elem != nullptr) {
      /* found matching endpoint */
      back = CL_RETVAL_OK;
      *service_port = elem->service_port;
   }

   /* unlock list */
   if ((ret_val = cl_raw_list_unlock(list_p)) != CL_RETVAL_OK) {
      return ret_val;
   }
   return back;
}

int
cl_endpoint_list_get_last_touch_time(cl_raw_list_t *list_p, cl_com_endpoint_t *endpoint, unsigned long *touch_time) {

   int back = CL_RETVAL_UNKNOWN_ENDPOINT;
   int ret_val = CL_RETVAL_OK;
   cl_endpoint_list_elem_t *elem = nullptr;


   if (list_p == nullptr || endpoint == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   /* set time to 0 if endpoint not found, otherwise return last communication time */
   /* otherwise return error */
   if (touch_time) {
      *touch_time = 0;
   }

   /* lock list */
   if ((ret_val = cl_raw_list_lock(list_p)) != CL_RETVAL_OK) {
      return ret_val;
   }

   elem = cl_endpoint_list_get_elem_endpoint(list_p, endpoint);
   if (elem != nullptr) {
      /* found matching endpoint */
      back = CL_RETVAL_OK;
      CL_LOG_STR(CL_LOG_INFO, "found endpoint comp_host:", elem->endpoint->comp_host);
      if (touch_time) {
         *touch_time = elem->last_used;
      }
   }

   /* unlock list */
   if ((ret_val = cl_raw_list_unlock(list_p)) != CL_RETVAL_OK) {
      return ret_val;
   }
   return back;

}

int cl_endpoint_list_undefine_endpoint(cl_raw_list_t *list_p, cl_com_endpoint_t *endpoint) {
   int back = CL_RETVAL_UNKNOWN_ENDPOINT;
   int ret_val = CL_RETVAL_OK;
   cl_endpoint_list_elem_t *elem = nullptr;

   if (list_p == nullptr || endpoint == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   /* lock list */
   if ((ret_val = cl_raw_list_lock(list_p)) != CL_RETVAL_OK) {
      return ret_val;
   }

   elem = cl_endpoint_list_get_elem_endpoint(list_p, endpoint);
   if (elem && elem->is_static == false) {
      cl_endpoint_list_data_t *ldata = nullptr;

      cl_raw_list_remove_elem(list_p, elem->raw_elem);
      cl_com_free_endpoint(&(elem->endpoint));
      sge_free(&elem);

      ldata = (cl_endpoint_list_data_t *)list_p->list_data;
      if (ldata->ht != nullptr) {
         sge_htable_delete(ldata->ht, endpoint->hash_id);
      }
      back = CL_RETVAL_OK;
   }

   /* unlock list */
   if ((ret_val = cl_raw_list_unlock(list_p)) != CL_RETVAL_OK) {
      return ret_val;
   }
   return back;
}

cl_endpoint_list_elem_t *cl_endpoint_list_get_first_elem(cl_raw_list_t *list_p) {
   cl_raw_list_elem_t *raw_elem = cl_raw_list_get_first_elem(list_p);
   if (raw_elem) {
      return (cl_endpoint_list_elem_t *) raw_elem->data;
   }
   return nullptr;
}

cl_endpoint_list_elem_t *cl_endpoint_list_get_least_elem(cl_raw_list_t *list_p) {
   cl_raw_list_elem_t *raw_elem = cl_raw_list_get_least_elem(list_p);
   if (raw_elem) {
      return (cl_endpoint_list_elem_t *) raw_elem->data;
   }
   return nullptr;
}

cl_endpoint_list_elem_t *cl_endpoint_list_get_next_elem(cl_endpoint_list_elem_t *elem) {
   cl_raw_list_elem_t *next_raw_elem = nullptr;

   if (elem != nullptr) {
      cl_raw_list_elem_t *raw_elem = elem->raw_elem;
      next_raw_elem = cl_raw_list_get_next_elem(raw_elem);
      if (next_raw_elem) {
         return (cl_endpoint_list_elem_t *) next_raw_elem->data;
      }
   }
   return nullptr;
}

cl_endpoint_list_elem_t *cl_endpoint_list_get_last_elem(cl_endpoint_list_elem_t *elem) {
   cl_raw_list_elem_t *last_raw_elem = nullptr;

   if (elem != nullptr) {
      cl_raw_list_elem_t *raw_elem = elem->raw_elem;
      last_raw_elem = cl_raw_list_get_last_elem(raw_elem);
      if (last_raw_elem) {
         return (cl_endpoint_list_elem_t *) last_raw_elem->data;
      }
   }
   return nullptr;
}

cl_endpoint_list_elem_t *cl_endpoint_list_get_elem_endpoint(cl_raw_list_t *list_p, cl_com_endpoint_t *endpoint) {
   cl_endpoint_list_elem_t *elem = nullptr;

   if (endpoint != nullptr && list_p != nullptr) {
      cl_endpoint_list_data_t *ldata = nullptr;
      ldata = (cl_endpoint_list_data_t *)list_p->list_data;
      if (ldata->ht != nullptr) {
         if (sge_htable_lookup(ldata->ht, endpoint->hash_id, (const void **) &elem) == True) {
            return elem;
         }
      } else {
         /* Search without having hash table */
         CL_LOG(CL_LOG_INFO, "no hash table available, searching sequential");
         elem = cl_endpoint_list_get_first_elem(list_p);
         while (elem != nullptr) {
            if (cl_com_compare_endpoints(elem->endpoint, endpoint) == 1) {
               /* found matching element */
               return elem;
            }
            elem = cl_endpoint_list_get_next_elem(elem);
         }
      }
   }
   return nullptr;
}
