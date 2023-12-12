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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>
#include <pthread.h>

#include "uti/sge_mtutil.h"
#include "uti/sge_rmon.h"
#include "uti/setup_path.h"
#include "uti/sge_log.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_prog.h"
#include "uti/sge_stdio.h"
#include "uti/sge_string.h"
#include "uti/sge_parse_args.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/config.h"
#include "sgeobj/sge_conf.h"

#include "gdi/sge_gdi.h"
#include "gdi/sge_gdi_ctx.h"
#include "gdi/sge_qtcsh.h"

#include "msg_common.h"


/* module global variables */
static pthread_mutex_t qtask_mutex = PTHREAD_MUTEX_INITIALIZER;
static lList *task_config = NULL;
static int mode_verbose = 0;

static int init_qtask_config(sge_gdi_ctx_class_t *ctx, lList **alpp, print_func_t ostream) {
   struct passwd *pwd;
   char fname[SGE_PATH_MAX + 1];
   char buffer[10000];
   FILE *fp;
   lList *clp_cluster = NULL, *clp_user = NULL;
   lListElem *nxt, *cep_dest, *cep, *next;
   const char *task_name;
   struct passwd pw_struct;
   char *pw_buffer;
   size_t pw_buffer_size;
   const char* user_name = ctx->get_username(ctx);
   const char* cell_root = ctx->get_cell_root(ctx);

   /* cell global settings */
   sprintf(fname, "%s/common/qtask", cell_root);

   if (!(fp = fopen(fname, "r")) && errno != ENOENT) {
      SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_SGETEXT_CANT_OPEN_SS, fname, strerror(errno)));
      answer_list_add(alpp, SGE_EVENT, STATUS_EDISK, ANSWER_QUALITY_ERROR);
      (*ostream)("%s", SGE_EVENT);
      goto Error;
   }
   if (fp) {
      /* read in config file */
      if (read_config_list(fp, &clp_cluster, alpp, CF_Type, CF_name, CF_value,
                        CF_sublist, NULL, RCL_NO_VALUE, buffer, sizeof(buffer)-1)) {
         FCLOSE(fp);
         goto Error;
      }
      FCLOSE(fp);
   }

   /* skip tasknames containing '/' */
   nxt = lFirstRW(clp_cluster);
   while ((cep=nxt)) {
      nxt = lNextRW(cep);
      if (strrchr(lGetString(cep, CF_name), '/')) 
         lRemoveElem(clp_cluster, &cep);

   }

   pw_buffer_size = get_pw_buffer_size();
   pw_buffer = sge_malloc(pw_buffer_size);
   pwd = sge_getpwnam_r(user_name, &pw_struct, pw_buffer, pw_buffer_size);
   
   /* user settings */
   if (pwd == NULL) {
      SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_USER_INVALIDNAMEX_S , user_name));
      answer_list_add(alpp, SGE_EVENT, STATUS_ENOSUCHUSER, ANSWER_QUALITY_ERROR);
      (*ostream)("%s", SGE_EVENT);
      goto Error;
   }
   if (!pwd->pw_dir) {
      SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_USER_NOHOMEDIRFORUSERX_S , user_name));
      answer_list_add(alpp, SGE_EVENT, STATUS_EDISK, ANSWER_QUALITY_ERROR);
      (*ostream)("%s", SGE_EVENT);
      goto Error;
   }
   sprintf(fname, "%s/.qtask", pwd->pw_dir);
   
   if (!(fp = fopen(fname, "r")) && errno != ENOENT) {
      SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_SGETEXT_CANT_OPEN_SS, fname, strerror(errno)));
      answer_list_add(alpp, SGE_EVENT, STATUS_EDISK, ANSWER_QUALITY_ERROR);
      (*ostream)("%s", SGE_EVENT);
      goto Error;
   }
   if (fp) {
      /* read in config file */
      if (read_config_list(fp, &clp_user, alpp, CF_Type, CF_name, CF_value,
                           CF_sublist, NULL, RCL_NO_VALUE, buffer, sizeof(buffer)-1)) {
         FCLOSE(fp);
         goto Error;
      }
      FCLOSE(fp);
   }

   /* skip tasknames containing '/' */
   nxt = lFirstRW(clp_user);
   while ((cep=nxt)) {
      nxt = lNextRW(cep);
      if (strrchr(lGetString(cep, CF_name), '/')) 
         lRemoveElem(clp_user, &cep);
   }

   /* merge contents of user list into cluster list */
   next = lFirstRW(clp_user);
   while ((cep=next)) {
      char *ro_task_name;
      next = lNextRW(cep);
      task_name = lGetString(cep, CF_name);
   
      /* build task name with leading '!' for search operation */
      ro_task_name = (char *)malloc(strlen(task_name) + 2);
      ro_task_name[0] = '!';
      strcpy(&ro_task_name[1], task_name);

      if ((cep_dest=lGetElemStrRW(clp_cluster, CF_name, ro_task_name))) {
         /* do not override cluster global task entry */
         lRemoveElem(clp_user, &cep);
      } else if ((cep_dest=lGetElemStrRW(clp_cluster, CF_name, task_name))) {
         /* override cluster global task entry */
         lSetString(cep_dest, CF_value, lGetString(cep, CF_value));
         lRemoveElem(clp_user, &cep);
      } else {
         /* no entry in cluster global task list 
            use entry from user task list */
         lDechainElem(clp_user, cep);
         if (!clp_cluster) 
            clp_cluster = lCreateList("cluster config", CF_Type);
         lAppendElem(clp_cluster, cep); 
      }

      sge_free(&ro_task_name);
   }
   lFreeList(&clp_user);

   
   lFreeList(&task_config);
   task_config = clp_cluster;

   /* remove leading '!' from command names */
   for_each (cep, clp_cluster) {
      task_name = lGetString(cep, CF_name);
      if (task_name[0] == '!') {
         char *t = (char *)malloc(strlen(task_name));
         strcpy(t, &task_name[1]);
         lSetString(cep, CF_name, t);
         sge_free(&t);
      }
   }

   sge_free(&pw_buffer);
   return 0;

FCLOSE_ERROR:
Error:
   lFreeList(&clp_cluster);
   lFreeList(&clp_user);
   sge_free(&pw_buffer);
   return -1;
}

/****** QTCSH/sge_get_qtask_args() *********************************************
*  NAME
*     sge_get_qtask_args() -- get args for a qtask entry
*
*  SYNOPSIS
*     char** sge_get_qtask_args(void *ctx, char *taskname, lList **answer_list)
*
*  FUNCTION
*     This function reads the qtask files and returns an array of args for the
*     given qtask entry.  Calling this function will initialize the qtask
*     framework, if it has not already been initialized.
*
*  INPUTS
*     void *ctx           - the communication context (sge_gdi_ctx_class_t *)
*     char *taskname      - The name of the entry for which to look in the qtask
*                           files
*     lList **answer_list - For returning error information
*
*  RESULT
*     char **           A NULL-terminated array of args for the given qtask
*                       entry
*
*  NOTES
*     MT-NOTE: sge_get_qtask_args() is MT safe with respect to itself, but it is
*              not thread safe to use this function in conjuction with the
*              init_qtask_config() function or accessing the
*              task_config global variable.
*
*******************************************************************************/
char **sge_get_qtask_args(void *context, char *taskname, lList **answer_list)
{
   const char *value = NULL; 
   int num_args = 0;
   const lListElem *task = NULL;
   char** args = NULL;
   sge_gdi_ctx_class_t *ctx = (sge_gdi_ctx_class_t *)context;
   
   DENTER (TOP_LAYER, "sge_get_qtask_args");
   
   if (mode_verbose) {
      fprintf(stderr, "sge_get_qtask_args(taskname = %s)\n", taskname);
   }

   /* If the task_config has not been filled yet, fill it.  We call
    * init_qtask_config() because we don't need to setup
    * the GDI.  We just need the qtask arguments. */
   /* We lock this part because multi-threaded DRMAA apps can have problems
    * here.  Once we're past this part, qtask_config's read-only, so we don't
    * have any more problems. */
   sge_mutex_lock("qtask_mutex", SGE_FUNC, __LINE__, &qtask_mutex);
    
   if (task_config == NULL) {
      /* Just using printf here since we don't really have an exciting function
       * like xprintf to pass in.  This was really meant for use with qtsch. */
      if (init_qtask_config(ctx, answer_list, (print_func_t)printf) != 0) {
         sge_mutex_unlock("qtask_mutex", SGE_FUNC, __LINE__, &qtask_mutex);
         DEXIT;
         return args;
      }
   }

   sge_mutex_unlock("qtask_mutex", SGE_FUNC, __LINE__, &qtask_mutex);
   
   task = lGetElemStr(task_config, CF_name, taskname);
   
   if (task == NULL) {
      DEXIT;
      return args;
   }
  
   value = lGetString(task, CF_value);
   
   if (value != NULL) {
      num_args = sge_quick_count_num_args(value);
   }
   
   args = (char **)malloc(sizeof(char *) * (num_args + 1));   
   memset(args, 0, sizeof(char *) * (num_args + 1));
   sge_parse_args (value, args);
   
   DEXIT;
   return args;
}

