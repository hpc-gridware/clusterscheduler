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

#include "cull/cull_list.h"
#include "gdi/sge_gdi.h"
#include "sgeobj/sge_conf.h"

enum {
   TAG_NONE = 0,     /* usable e.g. as delimiter in a tag array */
   TAG_OLD_REQUEST,
   TAG_GDI_REQUEST,
   TAG_ACK_REQUEST,
   TAG_REPORT_REQUEST,
   TAG_FINISH_REQUEST,
   TAG_JOB_EXECUTION,
   TAG_SLAVE_ALLOW,
   TAG_CHANGE_TICKET,
   TAG_SIGJOB,
   TAG_SIGQUEUE,
   TAG_KILL_EXECD,
   TAG_NEW_FEATURES,     /*12*/
   TAG_GET_NEW_CONF,
   TAG_JOB_REPORT,              /* cull based job reports */
   TAG_TASK_EXIT,
   TAG_TASK_TID,
   TAG_EVENT_CLIENT_EXIT,
   TAG_FULL_LOAD_REPORT

#ifdef KERBEROS
   ,TAG_AUTH_FAILURE
#endif

};

typedef struct {
   char snd_host[CL_MAXHOSTLEN]; /* sender hostname; nullptr -> all              */
   char snd_name[CL_MAXHOSTLEN]; /* sender name (aka 'commproc'); nullptr -> all */
   u_short snd_id;            /* sender identifier; 0 -> all               */
   int tag;                   /* message tag; TAG_NONE -> all              */
   u_long32 request_mid;      /* message id of request                     */
   sge_pack_buffer buf;       /* message buffer                            */
} struct_msg_t;

lList
*sge_gdi2(u_long32 target, u_long32 cmd, lList **lpp, lCondition *cp, lEnumeration *enp);

int
sge_gdi2_multi(lList **alpp, int mode, u_long32 target, u_long32 cmd, lList **lp, lCondition *cp, lEnumeration *enp,
               state_gdi_multi *state, bool do_copy);

bool
sge_gdi2_wait(lList **alpp, lList **malpp, state_gdi_multi *state);

int sge_gdi2_get_any_request(char *rhost, char *commproc, u_short *id, sge_pack_buffer *pb, int *tag, int synchron,
                             u_long32 for_request_mid, u_long32 *mid);

int sge_gdi2_send_any_request(int synchron, u_long32 *mid, const char *rhost, const char *commproc, int id,
                              sge_pack_buffer *pb, int tag, u_long32 response_id, lList **alpp);

lList *gdi2_kill(lList *id_list, u_long32 action_flag);

lList *gdi2_tsm();

bool sge_gdi2_check_permission(lList **alpp, int option);

int
gdi2_send_message_pb(int synchron, const char *tocomproc, int toid, const char *tohost, int tag, sge_pack_buffer *pb,
                     u_long32 *mid);

int gdi2_receive_message(char *fromcommproc, u_short *fromid, char *fromhost, int *tag, char **buffer, u_long32 *buflen,
                         int synchron);

int gdi2_get_configuration(const char *config_name, lListElem **gepp, lListElem **lepp);

int gdi2_get_merged_configuration(lList **conf_list);

int gdi2_wait_for_conf(lList **conf_list);

int report_list_send(const lList *rlp, const char *rhost, const char *commproc, int id, int synchron);

int sge_gdi2_shutdown();

/* 
** commlib handler functions 
*/
#include "comm/lists/cl_list_types.h"

const char *sge_dump_message_tag(unsigned long tag);

typedef enum sge_gdi_stored_com_error_type {
   SGE_COM_ACCESS_DENIED = 101,
   SGE_COM_ENDPOINT_NOT_UNIQUE,
   SGE_COM_WAS_COMMUNICATION_ERROR
} sge_gdi_stored_com_error_t;

bool sge_get_com_error_flag(u_long32 progid, sge_gdi_stored_com_error_t error_type, bool reset_error_flag);

void general_communication_error(const cl_application_error_list_elem_t *commlib_error);

int gdi_log_flush_func(cl_raw_list_t *list_p);

bool
sge_gdi_extract_answer(lList **alpp, u_long32 cmd, u_long32 target, int id, lList *mal, lList **olpp);


void gdi2_default_exit_func(int i);

const char *
gdi3_get_act_master_host(bool reread);

int
sge_gdi_ctx_class_is_alive();