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
 * @brief Working out where a job's output goes on this host
 */
#include <pwd.h>
#include <cstring>
#include <cerrno>

#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_spool.h"
#include "uti/sge_string.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_unistd.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_job.h"

#include "get_path.h"
#include "msg_execd.h"

static int
getHomeDir(dstring *dstr_exp_path, const char *user) {
   DENTER(TOP_LAYER);

   struct passwd *pwd;
   struct passwd pw_struct;
   char *buffer;
   int size;

   size = get_pw_buffer_size();
   buffer = sge_malloc(size);

   pwd = sge_getpwnam_r(user, &pw_struct, buffer, size);
   if (pwd == nullptr) {
      ERROR(MSG_EXECD_INVALIDUSERNAME_S, user);
      sge_free(&buffer);
      DRETURN(0);
   }
   if (pwd->pw_dir == nullptr) {
      ERROR(MSG_EXECD_NOHOMEDIR_S, user);
      sge_free(&buffer);
      DRETURN(0);
   }

   sge_dstring_append(dstr_exp_path, pwd->pw_dir);

   sge_free(&buffer);
   DRETURN(1);
}


/** @brief Resolve one of a job's paths into a concrete filename
 *
 * Picks the entry that applies to this host, expands the pseudo variables, and
 * appends the job's own name when what the user gave is a directory.
 *
 * @param qualified_hostname this host, as the cluster knows it
 * @param lp the path list the user submitted
 * @param cwd the job's working directory, for a relative path
 * @param owner the job owner, for the home directory
 * @param job_name the job's name
 * @param job_number the job id
 * @param ja_task_number the array task id
 * @param type which path this is, an `SGE_STD*` value
 * @param[out] pathstr receives the result
 * @param pathstr_len size of that buffer
 * @return 0 on success
 */
int sge_get_path(const char *qualified_hostname, const lList *lp, const char *cwd, const char *owner,
                 const char *job_name, uint32_t job_number,
                 uint32_t ja_task_number, int type,
                 char *pathstr, size_t pathstr_len) {
   DENTER(TOP_LAYER);
   const lListElem *ep = nullptr;
   const char *path = nullptr, *host = nullptr;
   char exp_path_buf[SGE_PATH_MAX];
   dstring dstr_exp_path;

   sge_dstring_init(&dstr_exp_path, exp_path_buf, sizeof(exp_path_buf));
   *pathstr = '\0';

   /*
    * check if there's a path for this host
    */
   ep = lGetElemHost(lp, PN_host, qualified_hostname);
   if (ep != nullptr) {
      path = expand_path(&dstr_exp_path, lGetString(ep, PN_path), job_number, ja_task_number, job_name, owner, qualified_hostname);
      host = lGetHost(ep, PN_host);
   } else {
      /* 
       * hostname: wasn't set, look for a default 
       */
      for_each_ep(ep, lp) {
         path = expand_path(&dstr_exp_path, lGetString(ep, PN_path), job_number, ja_task_number, job_name, owner, qualified_hostname);
         host = lGetHost(ep, PN_host);
         if (host == nullptr) {
            break;
         }
      }
   }

   /*
    * prepend cwd to path
    */
   if (path && path[0]!='\0' && path[0] != '/') {
      /* got relative path from -e/-o */
      snprintf(pathstr, pathstr_len, "%s/%s", cwd, path);
   } else if (path && path[0]!='\0' ) { 
      /* got absolute path from -e/-o */
      sge_strlcpy(pathstr, path, pathstr_len);
   } else if (type == SGE_STDIN) {
      sge_strlcpy(pathstr, "/dev/null", pathstr_len);
   } else if (type != SGE_SHELL) {
      /* no -e/-o directive (but not for shells) */
      sge_strlcpy(pathstr, cwd, pathstr_len);
   }

   DRETURN(0);
}

/**
 * @brief Retrieve the file staging host and path
 *
 * Retrieves the file staging host and path from the
 * job list element.
 *
 * @param lp pointer to the path sublist
 * @param[out] fs_host buffer to hold the host name
 * @param fs_host_len size of that buffer
 * @param[out] fs_path buffer to hold the file path
 * @param fs_path_len size of that buffer
 *
 * @return Is file staging enabled for this (stdin/stdout/stderr) path sublist?
 */
bool sge_get_fs_path(const lList *lp, char *fs_host, size_t fs_host_len, char *fs_path, size_t fs_path_len) {
   DENTER(TOP_LAYER);

   const lListElem* ep;
   bool       bFileStaging=false;

   if(lp && (ep=lFirst(lp))) {
      bFileStaging = (bool)lGetBool(ep, PN_file_staging);
 
      if( bFileStaging ) {
         if(lGetHost(ep, PN_file_host)) {
            sge_strlcpy(fs_host, lGetHost(ep, PN_file_host), fs_host_len);
         }
         if(lGetString(ep, PN_path)) {
            sge_strlcpy(fs_path, lGetString(ep, PN_path), fs_path_len);
         }
      }
   }
   DRETURN(bFileStaging);
}

const char *
/** @brief Expand the pseudo variables a user may put in a path
 *
 * `$HOME`, `$USER`, `$JOB_ID`, `$JOB_NAME`, `$HOSTNAME` and `$TASK_ID`.
 *
 * @param[out] dstr_exp_path receives the expanded path
 * @param in_path the path as the user gave it
 * @param job_id the job
 * @param ja_task_id the array task
 * @param job_name the job's name
 * @param user the job owner
 * @param host this host
 * @return the expanded path
 */
expand_path(dstring *dstr_exp_path, const char *in_path, uint32_t job_id, uint32_t ja_task_id,
            const char *job_name, const char *user, const char *host) {
   DENTER(TOP_LAYER);

   char *t;
   const char *s;
   char tmp[255];
   
   sge_dstring_clear(dstr_exp_path);

   if (in_path) {
      s = in_path;
      /*
      ** handle ~/ and ~username/
      */
      if (s[0] == '~') {
         t = strchr((char *)s, '/');
         strncpy(tmp, s+1, t-s+1);
         strcat(tmp, "");
         if (!strcmp(tmp, "")) {
            if (!getHomeDir(dstr_exp_path, user)) {
               DRETURN(nullptr);
            }
            s = s + 2;
         } else if (!getHomeDir(dstr_exp_path, tmp)) {
            s = t;
         }
      }
      t = strchr((char *)s, '$');
      while (t) {
         sge_dstring_nappend(dstr_exp_path, s, t-s);
         s = t;
         if (!strncmp(t, "$HOME", sizeof("$HOME") - 1)) {
            if (!getHomeDir(dstr_exp_path, user)) {
               DRETURN(nullptr);
            }
            s = t + sizeof("$HOME") - 1;
         }
         if (!strncmp(t, "$JOB_ID", sizeof("$JOB_ID") - 1)) {
            sge_dstring_sprintf_append(dstr_exp_path, sge_u32, job_id);
            s = t + sizeof("$JOB_ID") - 1;
         }
         if (ja_task_id) {
            if (!strncmp(t, "$TASK_ID", sizeof("$TASK_ID") - 1)) {
               sge_dstring_sprintf_append(dstr_exp_path, sge_u32, ja_task_id);
               s = t + sizeof("$TASK_ID") - 1;
            }
         }
         if (!strncmp(t, "$JOB_NAME", sizeof("$JOB_NAME") - 1)) {
            sge_dstring_sprintf_append(dstr_exp_path, "%s", job_name);
            s = t + sizeof("$JOB_NAME") - 1;
         }
         if (!strncmp(t, "$USER", sizeof("$USER") - 1)) {
            sge_dstring_sprintf_append(dstr_exp_path, "%s", user);
            s = t + sizeof("$USER") - 1;
         }
         if (!strncmp(t, "$HOSTNAME", sizeof("$HOSTNAME") - 1)) {
            sge_dstring_sprintf_append(dstr_exp_path, "%s", host);
            s = t + sizeof("$HOSTNAME") - 1;
         }
         if (*s == '$')  {
            sge_dstring_append_char(dstr_exp_path, *s);
            s++;
         }
         t = strchr((char *)s, '$');
      }
      sge_dstring_append(dstr_exp_path, s);
   }

   DRETURN(sge_dstring_get_string(dstr_exp_path));
}

/**
 * @brief Create a jatask's active job directory
 *
 * Creates the active jobs sub directory for a job array task.
 * If it already exists (because the task has been restarted)
 * and the execd is configured to keep the active job directories
 * (execd_param KEEP_ACTIVE), it is renamed to `old_name`.n, where
 * n is a number from 0 to 9. If a job is restarted more than 10 times,
 * the old active job dir will be removed and only the first 10 be kept.
 *
 * @param job job object
 * @param ja_task ja task object
 * @param err_str optional buffer to hold error strings. If it is nullptr, errors are output.
 *
 * @return the path of the jobs/jatasks active job directory, or nullptr if the function call failed.
 *
 * @see `sge_get_active_job_file_path()`, #sge_make_pe_task_active_dir
 */
const char *sge_make_ja_task_active_dir(const lListElem *job, const lListElem *ja_task, dstring *err_str) {
   DENTER(TOP_LAYER);

   static dstring path_buffer = DSTRING_INIT;
   const char *path;
   int result;


   if (err_str != nullptr) {
      sge_dstring_clear(err_str);
   }
   
   if (job == nullptr || ja_task == nullptr) {
      DRETURN(nullptr);
   }

   /* build path to active dir */
   path = sge_get_active_job_file_path(&path_buffer,
                                       lGetUlong(job, JB_job_number), 
                                       lGetUlong(ja_task, JAT_task_number), 
                                       nullptr, nullptr);

   /* try to create it */
   result = sge_mkdir(path, 0755, false, false);
   if (result == -1) {
      /* if it already exists and keep_active: try to rename it */
      if (errno == EEXIST && mconf_get_keep_active() != KEEP_ACTIVE_FALSE && lGetUlong(ja_task, JAT_job_restarted) > 0) {
         dstring new_path = DSTRING_INIT;
         int success = 0;

         for (int i = 0; i < 10; i++) {
            sge_dstring_sprintf(&new_path, "%s.%d", path, i);
            if (rename(path, sge_dstring_get_string(&new_path)) == 0) {
               success = 1;
               break;
            } 
         }
         
         /* if it couldn't be renamed: try to remove it */
         if (success == 0) {
            dstring error_string;
            char error_string_buffer[MAX_STRING_SIZE];

            sge_dstring_init(&error_string, error_string_buffer, sizeof(error_string_buffer));

            DPRINTF("could not rename old active job dir " SFN " - removing it\n", path);

            if (sge_rmdir(path, &error_string)) {
               if (err_str != nullptr) {
                  sge_dstring_sprintf(err_str, MSG_FILE_RMDIR_SS, path, sge_dstring_get_string(&error_string));
               } else {
                  ERROR(MSG_FILE_RMDIR_SS, path, SGE_EVENT);
                  DRETURN(nullptr);
               }
            }
         }

         sge_dstring_free(&new_path);

         result = sge_mkdir(path, 0755, false, false);
      }
   }   

   if (result == -1) {
      /* error creating directory */
      if (err_str != nullptr) {
         sge_dstring_sprintf(err_str, MSG_FILE_CREATEDIR_SS, path, strerror(errno));
      } else {
         ERROR(MSG_FILE_CREATEDIR_SS, path, strerror(errno));
      }
      DRETURN(nullptr);
   }

   DRETURN(path);
}

/**
 * @brief Create a petask's active job directory
 *
 * Creates the active job sub directory for a pe task.
 *
 * @param job the job object
 * @param ja_task the ja task object
 * @param pe_task the pe task object
 * @param err_str optional buffer to hold error strings. If it is nullptr, errors are output.
 *
 * @return the path of the jobs/jatasks active job directory, or nullptr if the function call failed.
 *
 * @see `sge_get_active_job_file_path()`, #sge_make_ja_task_active_dir
 */
const char *sge_make_pe_task_active_dir(const lListElem *job, const lListElem *ja_task, const lListElem *pe_task, dstring *err_str) {
   DENTER(TOP_LAYER);

   static dstring path_buffer = DSTRING_INIT;
   const char *path;

   if(err_str != nullptr) {
      sge_dstring_clear(err_str);
   }
  
   if(job == nullptr || ja_task == nullptr || pe_task == nullptr) {
      DRETURN(nullptr);
   }

   /* build path to active dir */
   path = sge_get_active_job_file_path(&path_buffer,
                                       lGetUlong(job, JB_job_number), 
                                       lGetUlong(ja_task, JAT_task_number), 
                                       lGetString(pe_task, PET_id), 
                                       nullptr);

   /* try to create it */
   if (sge_mkdir(path, 0755, false, false) == -1) {
      /* error creating directory */
      if(err_str != nullptr) {
         sge_dstring_sprintf(err_str, MSG_FILE_CREATEDIR_SS, path, strerror(errno));
      } else {
         ERROR(MSG_FILE_CREATEDIR_SS, path, strerror(errno));
      }
      DRETURN(nullptr);
   }

   DRETURN(path);
}
