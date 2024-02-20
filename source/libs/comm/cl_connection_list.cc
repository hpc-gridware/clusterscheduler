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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cerrno>
#include <cstring>
#include <sys/time.h>
#include <cstdlib>
#include <arpa/inet.h>

#include "comm/cl_connection_list.h"
#include "comm/cl_app_message_queue.h"
#include "comm/cl_message_list.h"
#include "comm/cl_communication.h"
#include "comm/lists/cl_util.h"

int cl_connection_list_setup(cl_raw_list_t **list_p, char *list_name, int enable_locking, bool create_hash) {
   cl_connection_list_data_t *ldata = nullptr;
   int ret_val;
   ldata = (cl_connection_list_data_t *) sge_malloc(sizeof(cl_connection_list_data_t));
   if (ldata == nullptr) {
      return CL_RETVAL_MALLOC;
   }
   ldata->last_nr_of_descriptors = 0;
   ldata->select_not_called_count = 0;

   ret_val = cl_raw_list_setup(list_p, list_name, enable_locking);
   if (ret_val != CL_RETVAL_OK) {
      sge_free(&ldata);
      return ret_val;
   }

   /* create hashtable */
   if (create_hash == true) {
      ldata->r_ht = sge_htable_create(4, dup_func_string, hash_func_string, hash_compare_string);
      if (ldata->r_ht == nullptr) {
         cl_raw_list_cleanup(list_p);
         sge_free(&ldata);
         return CL_RETVAL_MALLOC;
      }
      CL_LOG_INT(CL_LOG_INFO, "created hash table with size =", 4);
   } else {
      ldata->r_ht = nullptr;
      CL_LOG(CL_LOG_INFO, "created NO hash table!");
   }

   /* set private list data */
   (*list_p)->list_data = ldata;
   return ret_val;
}

int cl_connection_list_cleanup(cl_raw_list_t **list_p) {
   cl_connection_list_data_t *ldata = nullptr;


   if (list_p == nullptr || *list_p == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   /* clean list private data */
   ldata = (cl_connection_list_data_t *)(*list_p)->list_data;
   (*list_p)->list_data = nullptr;
   if (ldata != nullptr) {
      if (ldata->r_ht != nullptr) {
         sge_htable_destroy(ldata->r_ht);
      }
      sge_free(&ldata);
   }

   return cl_raw_list_cleanup(list_p);
}

char *cl_create_endpoint_string(cl_com_endpoint_t *endpoint) {
   char help[2048]; /* max length of endpoint name is 256 */
   if (endpoint == nullptr) {
      return nullptr;
   }
   snprintf(help, 2048, "%lu%s%lu", (unsigned long) endpoint->addr.s_addr, endpoint->comp_name, endpoint->comp_id);
   return strdup(help);
}

int cl_connection_list_append_connection(cl_raw_list_t *list_p, cl_com_connection_t *connection, int do_lock) {
   int ret_val;
   cl_connection_list_elem_t *new_elem = nullptr;
   cl_connection_list_data_t *ldata;

   if (connection == nullptr || list_p == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   ldata = (cl_connection_list_data_t *)list_p->list_data;

   /* add new element list */
   new_elem = (cl_connection_list_elem_t *) sge_malloc(sizeof(cl_connection_list_elem_t));
   if (new_elem == nullptr) {
      return CL_RETVAL_MALLOC;
   }

   new_elem->connection = connection;

   /* lock the list */
   if (do_lock != 0) {
      if ((ret_val = cl_raw_list_lock(list_p)) != CL_RETVAL_OK) {
         return ret_val;
      }
   }

   new_elem->raw_elem = cl_raw_list_append_elem(list_p, (void *) new_elem);
   if (new_elem->raw_elem == nullptr) {
      if (do_lock != 0) {
         cl_raw_list_unlock(list_p);
      }
      sge_free(&new_elem);
      return CL_RETVAL_MALLOC;
   }

   /* insert into hash */
   if (connection->remote != nullptr) {
      /*
       * The endpoint is added to the hash if its endpoint data is already filled.
       * This is the case for all connections which are opened by the local handle (application).
       * For remote client connections (which are accepted by local handle) this is not the case.
       * Client connections are added when the endpoint is resolved (CRM message is generated).
       */
      if (ldata->r_ht != nullptr) {
         if (connection->remote->hash_id != nullptr) {
            sge_htable_store(ldata->r_ht, connection->remote->hash_id, new_elem);
         }
      }
   }

   /* unlock the thread list */
   if (do_lock != 0) {
      if ((ret_val = cl_raw_list_unlock(list_p)) != CL_RETVAL_OK) {
         return ret_val;
      }
   }
   return CL_RETVAL_OK;
}

int cl_connection_list_remove_connection(cl_raw_list_t *list_p, cl_com_connection_t *connection, int do_lock) {
   int function_return = CL_RETVAL_CONNECTION_NOT_FOUND;
   int ret_val = CL_RETVAL_OK;
   int is_do_free = 0;
   cl_connection_list_elem_t *elem = nullptr;
   cl_connection_list_data_t *ldata = nullptr;

   if (list_p == nullptr || connection == nullptr) {
      return CL_RETVAL_PARAMS;
   }
   /* lock list */
   if (do_lock != 0) {
      if ((ret_val = cl_raw_list_lock(list_p)) != CL_RETVAL_OK) {
         return ret_val;
      }
   }

   ldata = (cl_connection_list_data_t *)list_p->list_data;
   if (ldata->r_ht != nullptr && connection->remote != nullptr && connection->remote->hash_id != nullptr) {
      if (sge_htable_lookup(ldata->r_ht, connection->remote->hash_id, (const void **) &elem) == True) {
         /* found matching element */
         cl_raw_list_remove_elem(list_p, elem->raw_elem);
         is_do_free = 1;
         function_return = CL_RETVAL_OK;
         sge_htable_delete(ldata->r_ht, connection->remote->hash_id);
      }
   } else {
      /* Search without having hash table */
      CL_LOG(CL_LOG_INFO, "no hash table available, searching sequencial");
      elem = cl_connection_list_get_first_elem(list_p);
      while (elem != nullptr) {
         if (elem->connection == connection) {
            /* found matching element */
            cl_raw_list_remove_elem(list_p, elem->raw_elem);
            is_do_free = 1;
            function_return = CL_RETVAL_OK;
            break;
         }
         elem = cl_connection_list_get_next_elem(elem);
      }
   }

   /* unlock the thread list */
   if (do_lock != 0) {
      if ((ret_val = cl_raw_list_unlock(list_p)) != CL_RETVAL_OK) {
         if (is_do_free == 1) {
            sge_free(&elem);
         }
         return ret_val;
      }
   }

   if (is_do_free == 1) {
      sge_free(&elem);
   }

   return function_return;
}

int cl_connection_list_destroy_connections_to_close(cl_com_handle_t *handle) {
   int ret_val = CL_RETVAL_OK;
   bool timeout_flag = cl_com_get_ignore_timeouts_flag();
   cl_connection_list_elem_t *elem = nullptr;
   cl_connection_list_elem_t *act_elem = nullptr;
   cl_com_connection_t *connection = nullptr;
   cl_raw_list_t *delete_connections = nullptr;
   struct timeval now;

   if (handle == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   if (handle->connection_list == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   /* lock list */
   if ((ret_val = cl_raw_list_lock(handle->connection_list)) != CL_RETVAL_OK) {
      return ret_val;
   }

   gettimeofday(&now, nullptr);
   elem = cl_connection_list_get_first_elem(handle->connection_list);
   while (elem != nullptr) {
      act_elem = elem;
      elem = cl_connection_list_get_next_elem(elem);
      connection = act_elem->connection;
      if (connection->data_flow_type == CL_CM_CT_MESSAGE) {
         if (connection->connection_state == CL_CONNECTED &&
             connection->connection_sub_state != CL_COM_WORK) {

            /*
             * This is for connections which are not in in CL_COM_WORK state. Connections
             * will be closed when they reach close connection timeout ...
             */
            if (connection->connection_close_time.tv_sec == 0) {
               /* there is no timeout set, set connection close time for this connection */
               gettimeofday(&(connection->connection_close_time), nullptr);
               connection->connection_close_time.tv_sec += handle->close_connection_timeout;
            }

            if (cl_raw_list_get_elem_count(connection->received_message_list) == 0 &&
                cl_raw_list_get_elem_count(connection->send_message_list) == 0 &&
                connection->connection_sub_state == CL_COM_DONE) {
               connection->connection_state = CL_CLOSING;
               connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
               CL_LOG(CL_LOG_INFO, "setting connection state to close this connection");
            } else {
               if (connection->connection_close_time.tv_sec <= now.tv_sec || timeout_flag == true) {
                  CL_LOG(CL_LOG_ERROR, "close connection timeout - shutdown of connection");
                  connection->connection_state = CL_CLOSING;
                  connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                  CL_LOG(CL_LOG_INFO, "setting connection state to close this connection");
               } else {
                  CL_LOG(CL_LOG_INFO, "waiting for connection close handshake");
               }
            }
         }
      }

      /* check connection timeout time */
      if (connection->connection_state == CL_CONNECTED ||
          connection->connection_state == CL_OPENING ||
          connection->connection_state == CL_CONNECTING) {
         if (connection->last_transfer_time.tv_sec + handle->connection_timeout <= now.tv_sec) {
            switch (connection->data_flow_type) {
               case CL_CM_CT_MESSAGE: {
                  CL_LOG(CL_LOG_WARNING, "got connection transfer timeout ...");
                  if (connection->connection_state == CL_CONNECTED) {
                     if (connection->was_opened == true) {
                        CL_LOG(CL_LOG_WARNING, "client connection ignores connection transfer timeout");
                     } else {
                        CL_LOG_STR_STR_INT(CL_LOG_WARNING, "connection timeout! Shutdown connection to:",
                                           connection->remote->comp_host,
                                           connection->remote->comp_name,
                                           (int) connection->remote->comp_id);
                        connection->connection_state = CL_CLOSING;
                        connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                     }
                  } else {
                     CL_LOG(CL_LOG_WARNING, "closing unconnected connection object");
                     connection->connection_state = CL_CLOSING;
                     connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                  }
               }
                  break;
               case CL_CM_CT_STREAM: {
                  CL_LOG(CL_LOG_INFO, "ignore connection transfer timeout for stream connection");
                  if (connection->remote != nullptr) {
                     CL_LOG_STR(CL_LOG_INFO, "component host:", connection->remote->comp_host);
                     CL_LOG_STR(CL_LOG_INFO, "component name:", connection->remote->comp_name);
                     CL_LOG_INT(CL_LOG_INFO, "component id:  ", (int) connection->remote->comp_id);
                  }
               }
                  break;
               case CL_CM_CT_UNDEFINED: {
                  CL_LOG(CL_LOG_WARNING, "got connection transfer timeout for undefined connection type");
                  if (connection->local != nullptr) {
                     if (connection->local->comp_host != nullptr) {
                        CL_LOG_STR(CL_LOG_WARNING, "closing local connection object", connection->local->comp_host);
                     }
                     if (connection->local->comp_name != nullptr) {
                        CL_LOG_STR(CL_LOG_WARNING, "component name:", connection->local->comp_name);
                     }
                     CL_LOG_INT(CL_LOG_WARNING, "component id:  ", (int) connection->local->comp_id);
                  } else {
                     CL_LOG(CL_LOG_WARNING, "removing undefined connection object");
                  }
                  connection->connection_state = CL_CLOSING;
                  connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
               }
                  break;
            }
         }
      }

      /*
       * This code will do the cleanup for connections in CL_CLOSING state. Connections are dechained
       * and removed after unlocking the connection list
       */
      if (connection->connection_state == CL_CLOSING) {
         cl_connection_list_data_t *ldata = nullptr;
         if (connection->connection_sub_state == CL_COM_DO_SHUTDOWN) {
            ret_val = cl_com_connection_complete_shutdown(connection);
            if (ret_val != CL_RETVAL_OK) {
               if (connection->connection_close_time.tv_sec <= now.tv_sec ||
                   timeout_flag == true) {
                  CL_LOG(CL_LOG_ERROR, "close connection timeout - skipping another connection shutdown");
                  connection->connection_sub_state = CL_COM_SHUTDOWN_DONE;
               } else {
                  if (ret_val == CL_RETVAL_UNCOMPLETE_READ || ret_val == CL_RETVAL_UNCOMPLETE_WRITE) {
                     CL_LOG_STR(CL_LOG_INFO, "cl_com_connection_complete_shutdown() returned:",
                                cl_get_error_text(ret_val));
                     continue; /* skip this connection this time */
                  }
                  CL_LOG_STR(CL_LOG_ERROR, "skipping another connection shutdown, last one returned:",
                             cl_get_error_text(ret_val));
               }
            }
            connection->connection_sub_state = CL_COM_SHUTDOWN_DONE;
         }

         if (connection->is_read_selected == true || connection->is_write_selected == true) {
            /* 
             * If a connection is in selected read/write mode we should not delete the
             * connection since some functions have a cached connection pointer to this 
             * connections and release the connection list lock.
             */
            CL_LOG(CL_LOG_INFO, "connection is selected, will not remove now!");
            continue; /* skip this connection this time */
         }

         /* We can remove and delete this connection */
         if (delete_connections == nullptr) {
            if (cl_connection_list_setup(&delete_connections, "delete_connections", 0, false) != CL_RETVAL_OK) {
               continue; /* skip this connection this time */
            }
         }

         handle->statistic->bytes_sent += connection->statistic->bytes_sent;
         handle->statistic->bytes_received += connection->statistic->bytes_received;
         handle->statistic->real_bytes_sent += connection->statistic->real_bytes_sent;
         handle->statistic->real_bytes_received += connection->statistic->real_bytes_received;

         cl_raw_list_dechain_elem(handle->connection_list, act_elem->raw_elem);

         if (connection->remote != nullptr) {
            ldata = (cl_connection_list_data_t *)handle->connection_list->list_data;
            if (ldata->r_ht != nullptr && connection->remote != nullptr && connection->remote->hash_id != nullptr) {
               sge_htable_delete(ldata->r_ht, connection->remote->hash_id);
            }
         }

         cl_raw_list_append_dechained_elem(delete_connections, act_elem->raw_elem);
      }
   }

   /* unlock list */
   if ((ret_val = cl_raw_list_unlock(handle->connection_list)) != CL_RETVAL_OK) {
      CL_LOG(CL_LOG_ERROR, "error unlocking list");
   }

   /* ATTENTION: This code is executed without having the connection list lock !!! */
   if (delete_connections != nullptr) {
      cl_com_message_t *message = nullptr;
      cl_message_list_elem_t *message_list_elem = nullptr;

      pthread_mutex_lock(handle->messages_ready_mutex);
      cl_raw_list_lock(handle->received_message_queue);
      while ((elem = cl_connection_list_get_first_elem(delete_connections)) != nullptr) {
         /* get connection from elem */
         connection = elem->connection;

         cl_raw_list_lock(connection->received_message_list);
         while ((message_list_elem = cl_message_list_get_first_elem(connection->received_message_list)) != nullptr) {
            message = message_list_elem->message;
            if (message->message_state == CL_MS_READY) {
               /* decrease counter for ready messages */
               handle->messages_ready_for_read = handle->messages_ready_for_read - 1;
               cl_app_message_queue_remove(handle->received_message_queue, connection, 0, false);
            }
            CL_LOG(CL_LOG_ERROR, "deleting unread message for connection");
#if 0
            /* only debugging ... */
            if (message->message_length != 0) {
               CL_LOG_INT(CL_LOG_ERROR,"connection sub_state:",connection->connection_sub_state);
               CL_LOG(CL_LOG_ERROR,"deleting unread message for connection");
               CL_LOG_INT(CL_LOG_ERROR,"message state is:", (int)message->message_state);
               CL_LOG_INT(CL_LOG_ERROR,"message length:", (int)message->message_length);
               CL_LOG_INT(CL_LOG_ERROR,"message df:", (int)message->message_df);
               CL_LOG_INT(CL_LOG_ERROR,"message mat:", (int)message->message_mat);
            }
#endif

            cl_raw_list_remove_elem(connection->received_message_list, message_list_elem->raw_elem);
            sge_free(&message_list_elem);
            cl_com_free_message(&message);
         }
         cl_raw_list_unlock(connection->received_message_list);

         /* remove all messages from send_message_list */
         cl_raw_list_lock(connection->send_message_list);
         while ((message_list_elem = cl_message_list_get_first_elem(connection->send_message_list)) != nullptr) {
            message = message_list_elem->message;
            CL_LOG(CL_LOG_ERROR, "deleting unsend message for connection");
            cl_raw_list_remove_elem(connection->send_message_list, message_list_elem->raw_elem);
            sge_free(&message_list_elem);
            cl_com_free_message(&message);
         }
         cl_raw_list_unlock(connection->send_message_list);

         /* remove elem from delete_connections list */
         cl_raw_list_remove_elem(delete_connections, elem->raw_elem);
         sge_free(&elem);

         /* close connection and free stuff */
         if ((ret_val = cl_com_close_connection(&connection)) != CL_RETVAL_OK) {
            CL_LOG(CL_LOG_ERROR, "error closing connection");
         }
      }
      cl_raw_list_unlock(handle->received_message_queue);
      pthread_mutex_unlock(handle->messages_ready_mutex);

      cl_connection_list_cleanup(&delete_connections);
   }
   return ret_val;
}

cl_connection_list_elem_t *cl_connection_list_get_first_elem(cl_raw_list_t *list_p) {
   cl_raw_list_elem_t *raw_elem = cl_raw_list_get_first_elem(list_p);
   if (raw_elem) {
      return (cl_connection_list_elem_t *) raw_elem->data;
   }
   return nullptr;
}

cl_connection_list_elem_t *cl_connection_list_get_least_elem(cl_raw_list_t *list_p) {
   cl_raw_list_elem_t *raw_elem = cl_raw_list_get_least_elem(list_p);
   if (raw_elem) {
      return (cl_connection_list_elem_t *) raw_elem->data;
   }
   return nullptr;
}

cl_connection_list_elem_t *cl_connection_list_get_next_elem(cl_connection_list_elem_t *elem) {  /* CR check */

   cl_raw_list_elem_t *next_raw_elem = nullptr;

   if (elem != nullptr) {
      cl_raw_list_elem_t *raw_elem = elem->raw_elem;
      next_raw_elem = cl_raw_list_get_next_elem(raw_elem);
      if (next_raw_elem) {
         return (cl_connection_list_elem_t *) next_raw_elem->data;
      }
   }
   return nullptr;
}

cl_connection_list_elem_t *cl_connection_list_get_last_elem(cl_connection_list_elem_t *elem) {  /* CR check */

   cl_raw_list_elem_t *last_raw_elem = nullptr;

   if (elem != nullptr) {
      cl_raw_list_elem_t *raw_elem = elem->raw_elem;
      last_raw_elem = cl_raw_list_get_last_elem(raw_elem);
      if (last_raw_elem) {
         return (cl_connection_list_elem_t *) last_raw_elem->data;
      }
   }
   return nullptr;
}

cl_connection_list_elem_t *cl_connection_list_get_elem_endpoint(cl_raw_list_t *list_p, cl_com_endpoint_t *endpoint) {
   cl_connection_list_elem_t *elem = nullptr;

   if (list_p != nullptr && endpoint != nullptr) {
      cl_connection_list_data_t *ldata = nullptr;
      ldata = (cl_connection_list_data_t *)list_p->list_data;
      if (ldata->r_ht != nullptr && endpoint->hash_id != nullptr) {
         if (sge_htable_lookup(ldata->r_ht, endpoint->hash_id, (const void **) &elem) == True) {
            return elem;
         }
      } else {
         /* Search without having hash table */
         CL_LOG(CL_LOG_INFO, "no hash table available, searching sequential");
         elem = cl_connection_list_get_first_elem(list_p);
         while (elem != nullptr) {
            if (elem->connection != nullptr) {
               if (cl_com_compare_endpoints(elem->connection->remote, endpoint) == 1) {
                  /* found matching element */
                  return elem;
               }
            }
            elem = cl_connection_list_get_next_elem(elem);
         }
      }
   }
   return nullptr;
}
