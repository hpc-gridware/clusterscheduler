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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "cull/cull_list.h"
#include "uti/sge_dstring.h"

#include "gdi/ocs_gdi_Packet.h"

#include "sgeobj/cull/sge_event_EV_L.h"
#include "sgeobj/cull/sge_event_EVS_L.h"
#include "sgeobj/cull/sge_event_ET_L.h"

/* event master request types */
typedef enum {
   EVR_ADD_EVC = 0,
   EVR_MOD_EVC,
   EVR_DEL_EVC,
   EVR_ADD_EVENT,
   EVR_ACK_EVENT
} evm_request_t;

/* documentation see libs/evc/sge_event_client.c */
typedef enum {
   EV_ID_INVALID = -1,
   EV_ID_ANY = 0,                   /* qmaster will give the ev a unique id */
   EV_ID_SCHEDD = 1,                /* schedd registers at qmaster */
   EV_ID_EVENT_MIRROR_LISTENER = 2, // event mirror thread registers as event client
   EV_ID_EVENT_MIRROR_READER = 3,   // event mirror thread registers as event client
   EV_ID_FIRST_DYNAMIC = 11         /* first id given by qmaster for EV_ID_ANY registration */
}ev_registration_id;

/*-------------------------------------------*/
/* data structurs for the local event client */
/*-------------------------------------------*/

/** 
 *   this is the definition for the update function that is used by process
 *   internal event clients / mirrors. 
 **/
typedef void (*event_client_update_func_t)(
   u_long32 id,                /* event client id */
   lList **alpp,               /* answer list */
   lList *event_list,          /* list of new events stored in the report list */
   void *arg                   ///< argument passed via sge_mirror_initialize
);

/**
 * The event client modification function. Part of the sge_event_master
 *
 * SEE ALSO
 *   EventMaster/sge_mod_event_client
 */
typedef int (*evm_mod_func_t)(
   lListElem *clio,  /* the new event client structure with a set update_func */
   lList** alpp,     /* a answer list */
   char* ruser,      /* calling user */
   char* rhost       /* calling host */
);

/**
 * The event client add function in the sge_event_master
 *
 * SEE ALSO
 *  EventMaster/sge_add_event_client_local
 **/
typedef int (*evm_add_func_t)(
   const ocs::gdi::Packet *packet,
   lListElem *clio,                        /* the new event client */
   lList **alpp,                           /* the answer list */
   lList **eclpp,                          /* list with added event client elem */
   event_client_update_func_t update_func, /* the event client update_func */
   void *update_func_arg                   /* additional argument passed to update_func */
);

/**
 * The event client remove function in the sge_event_master
 *
 * SEE ALSO
 *  EventMaster/sge_removce_event_client
 **/
typedef void (*evm_remove_func_t) (
   u_long32 aClientID               /* the event client id to remove */
);

/* documentation see libs/evc/sge_event_client.c */
/* #define EV_NO_FLUSH -1 */

#define EV_NOT_SUBSCRIBED false 
#define EV_SUBSCRIBED true
#define EV_FLUSHED true
#define EV_NOT_FLUSHED false
#define EV_MAX_FLUSH 0x3f
#define EV_NO_FLUSH (-1)

/* documentation see libs/evc/sge_event_client.c */

typedef enum {
   EV_BUSY_NO_HANDLING = 0,
   EV_BUSY_UNTIL_ACK,
   EV_BUSY_UNTIL_RELEASED
} ev_busy_handling;

typedef enum {
   EV_subscribing = 0,
   EV_connected,
   EV_closing,
   EV_terminated
} ev_state_handling;

/* documentation see libs/evc/sge_event_client.c       */
/*                                                     */
/* If this enum is changed, one has to be aware of the */
/* the following arrays in libs/evm/sge_event_master.c */
/*    block_events                                     */
/*    total_update_events                              */
/*    EVENT_LIST                                       */
/*    FIELD_LIST                                       */
/*    SOURCE_LIST                                      */
/* They might have to be addapted as well.             */

typedef enum {
   sgeE_ALL_EVENTS,                 /*impl. and tested, - = not available */

   sgeE_ADMINHOST_LIST,             /* send admin host list at registration */
   sgeE_ADMINHOST_ADD,              /* event add admin host */
   sgeE_ADMINHOST_DEL,              /* event delete admin host */
   sgeE_ADMINHOST_MOD,              /* event modify admin host */

   sgeE_CALENDAR_LIST,              /* send calendar list at registration */
   sgeE_CALENDAR_ADD,               /* event add calendar */
   sgeE_CALENDAR_DEL,               /* event delete calendar */
   sgeE_CALENDAR_MOD,               /* event modify calendar */

   sgeE_CKPT_LIST,                  /* send ckpt list at registration */
   sgeE_CKPT_ADD,                   /* event add ckpt */
   sgeE_CKPT_DEL,                   /* event delete ckpt */
   sgeE_CKPT_MOD,                   /* event modify ckpt */

   sgeE_CENTRY_LIST,                /* send complex list at registration */
   sgeE_CENTRY_ADD,                 /* event add complex */
   sgeE_CENTRY_DEL,                 /* event delete complex */
   sgeE_CENTRY_MOD,                 /* event modify complex */

   sgeE_CONFIG_LIST,                /* send config list at registration */
   sgeE_CONFIG_ADD,                 /* event add config */
   sgeE_CONFIG_DEL,                 /* event delete config */
   sgeE_CONFIG_MOD,                 /* event modify config */

   sgeE_EXECHOST_LIST,              /* send exec host list at registration */
   sgeE_EXECHOST_ADD,               /* event add exec host */
   sgeE_EXECHOST_DEL,               /* event delete exec host */
   sgeE_EXECHOST_MOD,               /* event modify exec host */

   sgeE_JATASK_ADD,                 /* event add array job task */
   sgeE_JATASK_DEL,                 /* event delete array job task */
   sgeE_JATASK_MOD,                 /* event modify array job task */

   sgeE_PETASK_ADD,                 /* event add a new pe task */
   sgeE_PETASK_MOD,                 /* event add a new pe task */
   sgeE_PETASK_DEL,                 /* event delete a pe task */

   sgeE_JOB_LIST,                   /* send job list at registration */
   sgeE_JOB_ADD,                    /* event job add (new job) */
   sgeE_JOB_DEL,                    /* event job delete */
   sgeE_JOB_MOD,                    /* event job modify */
   sgeE_JOB_MOD_SCHED_PRIORITY,     /* event job modify priority */
   sgeE_JOB_USAGE,                  /* event job online usage */
   sgeE_JOB_FINAL_USAGE,            /* event job final usage report after job end */
   sgeE_JOB_FINISH,                 /* job finally finished or aborted (user view) */

   sgeE_JOB_SCHEDD_INFO_LIST,       /* send job schedd info list at registration */
   sgeE_JOB_SCHEDD_INFO_ADD,        /* event jobs schedd info added */
   sgeE_JOB_SCHEDD_INFO_DEL,        /* event jobs schedd info deleted */
   sgeE_JOB_SCHEDD_INFO_MOD,        /* event jobs schedd info modified */

   sgeE_MANAGER_LIST,               /* send manager list at registration */
   sgeE_MANAGER_ADD,                /* event add manager */
   sgeE_MANAGER_DEL,                /* event delete manager */
   sgeE_MANAGER_MOD,                /* event modify manager */

   sgeE_OPERATOR_LIST,              /* send operator list at registration */
   sgeE_OPERATOR_ADD,               /* event add operator */
   sgeE_OPERATOR_DEL,               /* event delete operator */
   sgeE_OPERATOR_MOD,               /* event modify operator */

   sgeE_NEW_SHARETREE,              /* replace possibly existing share tree */

   sgeE_PE_LIST,                    /* send pe list at registration */
   sgeE_PE_ADD,                     /* event pe add */
   sgeE_PE_DEL,                     /* event pe delete */
   sgeE_PE_MOD,                     /* event pe modify */

   sgeE_PROJECT_LIST,               /* send project list at registration */
   sgeE_PROJECT_ADD,                /* event project add */
   sgeE_PROJECT_DEL,                /* event project delete */
   sgeE_PROJECT_MOD,                /* event project modify */

   sgeE_QMASTER_GOES_DOWN,          /* qmaster notifies all event clients, before it exits */

   sgeE_CQUEUE_LIST,                /* send cluster queue list at registration */
   sgeE_CQUEUE_ADD,                 /* event cluster queue add */
   sgeE_CQUEUE_DEL,                 /* event cluster queue delete */
   sgeE_CQUEUE_MOD,                 /* event cluster queue modify */

   sgeE_QINSTANCE_ADD,              /* event queue instance add */
   sgeE_QINSTANCE_DEL,              /* event queue instance delete */
   sgeE_QINSTANCE_MOD,              /* event queue instance mod */
   sgeE_QINSTANCE_SOS,              /* event queue instance sos */
   sgeE_QINSTANCE_USOS,             /* event queue instance usos */

   sgeE_SCHED_CONF,                 /* replace existing (sge) scheduler configuration */

   sgeE_SCHEDDMONITOR,              /* trigger scheduling run */

   sgeE_SHUTDOWN,                   /* request shutdown of an event client */

   sgeE_SUBMITHOST_LIST,            /* send submit host list at registration */
   sgeE_SUBMITHOST_ADD,             /* event add submit host */
   sgeE_SUBMITHOST_DEL,             /* event delete submit host */
   sgeE_SUBMITHOST_MOD,             /* event modify submit host */

   sgeE_USER_LIST,                  /* send user list at registration */
   sgeE_USER_ADD,                   /* event user add */
   sgeE_USER_DEL,                   /* event user delete */
   sgeE_USER_MOD,                   /* event user modify */

   sgeE_USERSET_LIST,               /* send userset list at registration */
   sgeE_USERSET_ADD,                /* event userset add */
   sgeE_USERSET_DEL,                /* event userset delete */
   sgeE_USERSET_MOD,                /* event userset modify */

   sgeE_HGROUP_LIST,
   sgeE_HGROUP_ADD,
   sgeE_HGROUP_DEL,
   sgeE_HGROUP_MOD,

   sgeE_RQS_LIST,
   sgeE_RQS_ADD,
   sgeE_RQS_DEL,
   sgeE_RQS_MOD,

   sgeE_AR_LIST,
   sgeE_AR_ADD,
   sgeE_AR_DEL,
   sgeE_AR_MOD,

   sgeE_ACK_TIMEOUT,

   sgeE_CATEGORY_LIST,              // events for job categories
   sgeE_CATEGORY_ADD,
   sgeE_CATEGORY_DEL,
   sgeE_CATEGORY_MOD,

   sgeE_EVENTSIZE
} ev_event;

/**
 * The event client event ack function. Part of the sge_event_master
 *
 * SEE ALSO
 *   EventMaster/sge_handle_event_ack
 */
typedef bool (*evm_ack_func_t)(
   u_long32,         /* the event client id */
   u_long32          /* the last event to ack */
);

#define IS_TOTAL_UPDATE_EVENT(x) \
  (((x)==sgeE_ADMINHOST_LIST) || \
  ((x)==sgeE_CALENDAR_LIST) || \
  ((x)==sgeE_CKPT_LIST) || \
  ((x)==sgeE_CENTRY_LIST) || \
  ((x)==sgeE_CONFIG_LIST) || \
  ((x)==sgeE_EXECHOST_LIST) || \
  ((x)==sgeE_CATEGORY_LIST) || \
  ((x)==sgeE_JOB_LIST) || \
  ((x)==sgeE_JOB_SCHEDD_INFO_LIST) || \
  ((x)==sgeE_MANAGER_LIST) || \
  ((x)==sgeE_OPERATOR_LIST) || \
  ((x)==sgeE_PE_LIST) || \
  ((x)==sgeE_PROJECT_LIST) || \
  ((x)==sgeE_CQUEUE_LIST) || \
  ((x)==sgeE_SUBMITHOST_LIST) || \
  ((x)==sgeE_USER_LIST) || \
  ((x)==sgeE_USERSET_LIST) || \
  ((x)==sgeE_HGROUP_LIST) || \
  ((x)==sgeE_SHUTDOWN) || \
  ((x)==sgeE_QMASTER_GOES_DOWN) || \
  ((x)==sgeE_ACK_TIMEOUT))

const char *event_text(const lListElem *event, dstring *buffer);

bool event_client_verify(const lListElem *event_client, lList **answer_list, bool add);
