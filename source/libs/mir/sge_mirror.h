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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The event mirror: a local copy of the master lists, kept current
 *
 * A component that needs the master's object lists - a scheduler, a proxy, a
 * monitoring tool - subscribes to the event client interface and applies every
 * event to its own copy. This layer sits on top of that interface and does the
 * applying, so the component only says *what* it wants mirrored.
 *
 * Mirroring can be restricted to certain event or object types, and a callback
 * can be installed per type to do something beyond the pure mirroring.
 *
 * @see sge_mirror.cc
 * @see @ref evc
 */

#include "cull/cull.h"

#include "sgeobj/sge_object.h"
#include "sgeobj/sge_event.h"

#include "evc/sge_event_client.h"

/// What an event asks the mirror to do
typedef enum {
   SGE_EMA_LIST = 1, ///< the whole master list has been sent; used at initialization
   SGE_EMA_ADD,      ///< a new object has been created
   SGE_EMA_MOD,      ///< an object has been modified
   SGE_EMA_DEL,      ///< an object has been deleted
   SGE_EMA_TRIGGER   ///< a certain action has been triggered, e.g. a scheduling run or a shutdown
} sge_event_action;

/// What a callback tells the mirror to do with the event it was given
typedef enum {
   SGE_EMA_FAILURE = 0, ///< the processing of events is stopped
   SGE_EMA_OK = 1,      ///< everything is fine
   SGE_EMA_IGNORE = 2   ///< no further processing for this event is done
} sge_callback_result;

/**
 * @brief Called for every event of a type the component installed it for
 *
 * Runs *before* the mirror applies the event, so the callback still sees the
 * old state and can stop the event from being applied at all - see
 * @ref sge_callback_result.
 *
 * @param evc the event client the event arrived on
 * @param type the object type the event is about
 * @param action what the event asks for
 * @param event the event element
 * @param clientdata whatever was passed when the callback was installed
 * @return whether the mirror should apply the event
 */
typedef sge_callback_result (*sge_mirror_callback)(sge_evc_class_t *evc,
                                                   sge_object_type type, 
                                                   sge_event_action action, 
                                                   lListElem *event, 
                                                   void *clientdata);

/// What most of the event mirror functions return
typedef enum {
   SGE_EM_OK = 0,          ///< action performed successfully
   SGE_EM_NOT_INITIALIZED, ///< the interface is not yet initialized
   
   SGE_EM_BAD_ARG,         ///< some input parameter was incorrect
   SGE_EM_TIMEOUT,         ///< a timeout occurred
   
   SGE_EM_DUPLICATE_KEY,   ///< an object should be added, but one with the same unique identifier already exists
   SGE_EM_KEY_NOT_FOUND,   ///< an object with the given key was not found

   SGE_EM_CALLBACK_FAILED, ///< a callback function failed

   SGE_EM_PROCESS_ERRORS,  ///< an error occurred during event processing

   SGE_EM_LAST_ERRNO       ///< not an error; the number of values above
} sge_mirror_error;

/* Initialization - Shutdown */
sge_mirror_error 
sge_mirror_initialize(sge_evc_class_t *evc, event_client_update_func_t update_func, evm_mod_func_t mod_func,
                      evm_add_func_t add_func, evm_remove_func_t remove_func, evm_ack_func_t ack_func, void *arg);

sge_mirror_error sge_mirror_shutdown(sge_evc_class_t *evc);

/* Subscription */
sge_mirror_error sge_mirror_subscribe(sge_evc_class_t *evc, sge_object_type type,
                                      sge_mirror_callback callback_before, 
                                      sge_mirror_callback callback_after, 
                                      void *client_data,
                                      const lCondition *where, const lEnumeration *what);

sge_mirror_error sge_mirror_unsubscribe(sge_evc_class_t *evc,
                                        sge_object_type type);

/* True if 'type' was subscribed with a where-filter, i.e. its local master list
 * is an intentional subset. Handlers that look up a parent object (e.g. the job a
 * ja-task belongs to) use this to tell a legitimate "not mirrored here" skip from
 * a real "missing object" inconsistency. */
bool sge_mirror_type_is_partial(sge_object_type type);

/* Event Processing */

sge_mirror_error
sge_mirror_process_event_list(sge_evc_class_t *evc, lList *event_list);

sge_mirror_error 
sge_mirror_process_events(sge_evc_class_t *evc);

sge_mirror_error 
sge_mirror_update_master_list(lList **list, const lDescr *list_descr,
                              lListElem *ep, const char *key, 
                              sge_event_action action, lListElem *event);

sge_mirror_error 
sge_mirror_update_master_list_host_key(lList **list, const lDescr *list_descr, 
                                       int key_nm, const char *key, 
                                       sge_event_action action, 
                                       lListElem *event);

sge_mirror_error 
sge_mirror_update_master_list_str_key(lList **list, const lDescr *list_descr, 
                                      int key_nm, const char *key, 
                                      sge_event_action action, 
                                      lListElem *event);

/* Error Handling */
const char *sge_mirror_strerror(sge_mirror_error num);
