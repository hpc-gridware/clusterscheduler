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

#include "uti/sge_prog.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_error_class.h"
#include "uti/sge_profiling.h"
#include "uti/sge_uidgid.h"

#include "comm/commlib.h"

#include "gdi/sge_gdi.h"
#include "gdi/sge_gdi_packet_type.h"

#include "sge.h"

typedef struct sge_gdi_ctx_class_str sge_gdi_ctx_class_t; 

struct sge_gdi_ctx_class_str {
   bool (*sge_gdi_packet_execute)        (sge_gdi_ctx_class_t* ctx, lList **answer_list,
                                          sge_gdi_packet_class_t *packet);
   bool (*sge_gdi_packet_wait_for_result)(sge_gdi_ctx_class_t* ctx, lList **answer_list,
                                          sge_gdi_packet_class_t **packet_handle, lList **malpp);
   bool (*gdi_wait)                      (sge_gdi_ctx_class_t* ctx, lList **alpp, lList **malpp,
                                          state_gdi_multi *state);
   lList* (*gdi)                         (sge_gdi_ctx_class_t *thiz, u_long32 target, 
                                          u_long32 cmd, lList **lp, lCondition *where, 
                                          lEnumeration *what);
   int (*gdi_multi)                      (sge_gdi_ctx_class_t* ctx, lList **alpp, int mode, 
                                          u_long32 target, u_long32 cmd, lList **lp, 
                                          lCondition *cp, lEnumeration *enp, 
                                          state_gdi_multi *state, bool do_copy);
  
   int (*prepare_enroll)(sge_gdi_ctx_class_t *thiz);
   int (*connect)(sge_gdi_ctx_class_t *thiz);
   int (*is_alive)(sge_gdi_ctx_class_t *thiz);
   lList* (*tsm)(sge_gdi_ctx_class_t *thiz, const char *schedd_name, const char *cell);
   lList* (*kill)(sge_gdi_ctx_class_t *thiz, lList *id_list, const char *cell, u_long32 option_flags, u_long32 action_flag);

   const char* (*get_master)(sge_gdi_ctx_class_t *thiz, bool reread);

   /* credentials */
   const char* (*get_private_key)(sge_gdi_ctx_class_t *thiz);
   const char* (*get_certificate)(sge_gdi_ctx_class_t *thiz);

   /* commlib */
   cl_com_handle_t* (*get_com_handle)(sge_gdi_ctx_class_t *thiz);
   
   /* dump current settings */
   void (*dprintf)(sge_gdi_ctx_class_t *thiz);
};
