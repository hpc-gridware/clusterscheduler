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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cerrno>
#include <cstdlib>
#include <csignal>

#include "uti/ocs_Systemd.h"
#include "uti/sge_afsutil.h"
#include "uti/sge_arch.h"
#include "uti/sge_binding_hlp.h"
#include "uti/sge_bitfield.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_bootstrap_env.h"
#include "uti/sge_dstring.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_os.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_unistd.h"

#include "gdi/ocs_gdi_ClientBase.h"

#include "sgeobj/ocs_BindingExecd2Shepherd.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/ocs_Job.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_feature.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_var.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/ocs_Binding.h"
#include "sgeobj/sge_grantedres.h"
#include "sgeobj/sge_mailrec.h"
#include "sgeobj/sge_path_alias.h"
#include "sgeobj/sge_ulong.h"

#include "comm/commlib.h"

#include "get_path.h"
#include "sge_job_qmaster.h"
#include "tmpdir.h"
#include "exec_job.h"
#include "ocs_execd_systemd.h"
#include "mail.h"
#include "basis_types.h"
#include "pdc.h"
#include "job_report_execd.h"
#include "sge.h"
#include "symbols.h"
#include "msg_common.h"
#include "msg_execd.h"
#include "msg_daemons_common.h"

#if defined(SOLARIS)
#   include "sge_smf.h"
#endif

#define ENVIRONMENT_FILE "environment"
#define CONFIG_FILE "config"

static int ck_login_sh(char *shell);

static int get_nhosts(const lList *gdil_list);

/* from execd.c import the working dir of the execd */
extern char execd_spool_dir[SGE_PATH_MAX];

#if COMPILE_DC
#if defined(SOLARIS) || defined(LINUX) || defined(FREEBSD) || defined(FREEBSD)

/* local functions */
static int addgrpid_already_in_use(long);

static long get_next_addgrpid(lList *, long);

#endif
#endif

/* 
   in case of regular jobs the queue is the first entry in the gdil list of the job
   in case of tasks the queue is the appropriate entry in the gdil list of the slave job
*/

lListElem *responsible_queue(lListElem *jep, lListElem *jatep, lListElem *petep) {
   lListElem *master_q = nullptr;

   DENTER(TOP_LAYER);

   if (petep == nullptr) {
      master_q = lGetObject(lFirst(lGetList(jatep, JAT_granted_destin_identifier_list)), JG_queue);
   } else {
      const lListElem *pe_queue = lFirst(lGetList(petep, PET_granted_destin_identifier_list));
      master_q = lGetObject(lGetElemStr(lGetList(jatep, JAT_granted_destin_identifier_list),
                                        JG_qname, lGetString(pe_queue, JG_qname)), JG_queue);

   }

   DRETURN(master_q);
}

#if COMPILE_DC
#if defined(SOLARIS) || defined(LINUX) || defined(FREEBSD) || defined(DARWIN)

static long get_next_addgrpid(lList *rlp, long last_addgrpid) {
   const lListElem *rep;
   int take_next = 0;

   /* uninitialized => return first number in list */
   if (last_addgrpid == 0) {
      rep = lFirst(rlp);
      if (rep)
         return (lGetUlong(rep, RN_min));
      else
         return (0);
   }

   /* search range and return next id */
   for_each_ep(rep, rlp) {
      long min, max;

      min = lGetUlong(rep, RN_min);
      max = lGetUlong(rep, RN_max);
      if (take_next)
         return (min);
      else if (last_addgrpid >= min && last_addgrpid < max)
         return (++last_addgrpid);
      else if (last_addgrpid == max)
         take_next = 1;
   }

   /* not successful until now => take first number */
   rep = lFirst(rlp);
   if (rep)
      return (lGetUlong(rep, RN_min));

   return (0);
}

static int addgrpid_already_in_use(long add_grp_id) {
   const lListElem *job = nullptr;
   const lListElem *ja_task = nullptr;
   const lListElem *pe_task = nullptr;

   for_each_ep(job, *ocs::DataStore::get_master_list(SGE_TYPE_JOB)) {
      for_each_ep(ja_task, lGetList(job, JB_ja_tasks)) {
         const char *id = lGetString(ja_task, JAT_osjobid);
         if (id != nullptr && atol(id) == add_grp_id) {
            return 1;
         }

         for_each_ep(pe_task, lGetList(ja_task, JAT_task_list)) {
            id = lGetString(pe_task, PET_osjobid);
            if (id != nullptr && atol(id) == add_grp_id) {
               return 1;
            }
         }
      }
   }
   return 0;
}

#endif
#endif

static const char *
sge_exec_job_get_limit(dstring *dstr, int limit_nm, const char *limit_name, u_long32 type,
                       const lListElem *master_q, const lListElem *jatep, const lListElem *petep,
                       const char *qualified_hostname) {
   DENTER(TOP_LAYER);

   const char *ret = lGetString(master_q, limit_nm);
   // can not really happen but just to be sure
   if (ret == nullptr) {
      ret = "INFINITY";
   }

   DPRINTF("sge_exec_job_get_limit: limit_name=%s, limit=%s\n", limit_name, ret);

   // for sequential jobs we are done, for tightly integrated parallel jobs we need to check the pe settings
   // if the limit in the master queue is infinity there is no need to do calculations
   // if it is not infinity we need to check pe setting master_forks_slave and daemon_forks_slave
   // and sum up the limits of the (possibly multiple) queues
   const lListElem *pe = lGetObject(jatep, JAT_pe_object);
   if (pe != nullptr && lGetBool(pe, PE_control_slaves) &&
       strcasecmp(ret, "INFINITY") != 0) {
      DPRINTF("sge_exec_job_get_limit: we have a tightly integrated parallel job and limit is not infinity\n");
      if ((petep == nullptr && lGetBool(pe, PE_master_forks_slaves)) || // master task forks slaves
                               lGetBool(pe, PE_daemon_forks_slaves)) {  // one slave task forks slaves
         DPRINTF("sge_exec_job_get_limit: we need to sum up the limits\n");
         double limit = 0;
         const lList *gdil = lGetList(jatep, JAT_granted_destin_identifier_list);
         const void *iterator = nullptr;
         const lListElem *gdil_ep;
         const lListElem *next_gdil_ep = lGetElemHostFirst(gdil, JG_qhostname, qualified_hostname, &iterator);
         while ((gdil_ep = next_gdil_ep) != nullptr) {
            next_gdil_ep = lGetElemHostNext(gdil, JG_qhostname, qualified_hostname, &iterator);

            const lListElem *queue = lGetObject(gdil_ep, JG_queue);
            if (queue != nullptr) {
               const char *limit_str = lGetString(queue, limit_nm);
               if (limit_str != NULL) {
                  // if one of the queue instances has a limit of infinity the sum is infinity
                  if (strcasecmp(limit_str, "INFINITY") == 0) {
                     DPRINTF("sge_exec_job_get_limit: qinstance %s has infinity\n", lGetString(gdil_ep, JG_qname));
                     ret = "INFINITY";
                     break;
                  } else {
                     u_long32 slots = lGetUlong(gdil_ep, JG_slots);
                     double dbl;
                     parse_ulong_val(&dbl, nullptr, type, limit_str, nullptr, 0);
                     limit += dbl * slots;
                     DPRINTF("sge_exec_job_get_limit: qinstance %s has limit %s, slots " sge_u32 ", sum %f\n",
                             lGetString(gdil_ep, JG_qname), limit_str, slots, dbl * slots);
                  }
               }
            }
         } // end: loop over all gdil elements on this host
         double_print_to_dstring(limit, dstr, type);
         ret = sge_dstring_get_string(dstr);
         DPRINTF("sge_exec_job_get_limit: sum of limits %s\n", ret);
      } // end: we need to sum up the limits
   } // end: we have a tightly integrated pe job

   DRETURN(ret);
}

/************************************************************************
 part of execd. Setup job environment then start shepherd.

 If we encounter an error we return an error
 return -1==error 
        -2==general error (Halt queue)
        -3==general error (Halt job)
        err_str set to error string
        err_length size of err_str
 ************************************************************************/
int sge_exec_job(lListElem *jep, lListElem *jatep, lListElem *petep, char *err_str, int err_length) {
   int i;
   char ps_name[128];
   FILE *fp;
   u_long32 interval;
   struct passwd *pw;
   SGE_STRUCT_STAT buf;
   int used_slots, pe_slots = 0, host_slots = 0, nhosts = 0;
   static lList *processor_set = nullptr;
   const char *cp;
   char *shell;
   const char *cwd = nullptr;
   char cwd_out_buffer[SGE_PATH_MAX];
   dstring cwd_out;

   const lList *path_aliases = nullptr;
   char dce_wrapper_cmd[128];
   bool starting_shepherd_ok = true;

#if COMPILE_DC
#if defined(SOLARIS) || defined(LINUX) || defined(FREEBSD) || defined(DARWIN)
   static gid_t last_addgrpid;
#endif
#endif

   dstring active_dir;

   char shepherd_path[SGE_PATH_MAX] = "",
           coshepherd_path[SGE_PATH_MAX] = "",
           tmpdir[SGE_PATH_MAX] = "",
           active_dir_buffer[SGE_PATH_MAX] = "",
           shell_path[SGE_PATH_MAX] = "",
           stdout_path[SGE_PATH_MAX] = "",
           stderr_path[SGE_PATH_MAX] = "",
           stdin_path[SGE_PATH_MAX] = "",
           pe_stdout_path[SGE_PATH_MAX] = "",
           pe_stderr_path[SGE_PATH_MAX] = "",
           fs_stdin_host[SGE_PATH_MAX] = "\"\"",
           fs_stdin_path[SGE_PATH_MAX] = "",
           fs_stdout_host[SGE_PATH_MAX] = "\"\"",
           fs_stdout_path[SGE_PATH_MAX] = "",
           fs_stderr_host[SGE_PATH_MAX] = "\"\"",
           fs_stderr_path[SGE_PATH_MAX] = "";

   DSTRING_STATIC(dstr_fname, SGE_PATH_MAX);
   const char *str_fname;
   DSTRING_STATIC(dstr_script_file, SGE_PATH_MAX);
   const char *str_script_file = ""; // @todo initialization shouldn't be necessary, but compiler seems to find a path in which str_script_file is not initialized!

   char mail_str[1024];
   const char *shepherd_name;
   const lList *gdil;
   const lListElem *gdil_ep;
   lListElem *master_q;
   lListElem *ep;
   const lListElem *env;
   lList *environmentList = nullptr;
   const char *arch = sge_get_arch();
   const char *sge_root = bootstrap_get_sge_root();
   const char *qualified_hostname = component_get_qualified_hostname();
   const char *default_cell = bootstrap_get_sge_cell();
   const char *binary_path = bootstrap_get_binary_path();
   const char *admin_user = bootstrap_get_admin_user();
   const char *masterhost = ocs::gdi::ClientBase::gdi_get_act_master_host(false);
   bool csp_mode = false;
   sigset_t sigset, sigset_oset;
   struct passwd pw_struct;
   char *pw_buffer;
   size_t pw_buffer_size;

   int write_osjob_id = 1;

   const char *fs_stdin_file = "", *fs_stdout_file = "", *fs_stderr_file = "";

   bool bInputFileStaging, bOutputFileStaging, bErrorFileStaging;

   const char *processor_binding_strategy = nullptr;

   /* env var reflecting SGE_BINDING if set */
   char *sge_binding_environment = nullptr;
   /* string reflecting the logical socket,core pairs if "pe" is set */
   dstring core_binding_strategy_string = DSTRING_INIT;

   /* retrieve the job, jatask and petask id once */
   u_long32 job_id;
   u_long32 ja_task_id;
   const char *pe_task_id = nullptr;

   char *shell_start_mode = nullptr;
   char *pag_cmd = nullptr;
   char *notify_kill = nullptr;
   char *notify_susp = nullptr;
   char *shepherd_cmd = nullptr;
   char *set_token_cmd = nullptr;

   DENTER(TOP_LAYER);

   sge_dstring_init(&cwd_out, cwd_out_buffer, sizeof(cwd_out_buffer));
   sge_dstring_init(&active_dir, active_dir_buffer, sizeof(active_dir_buffer));

   SGE_ASSERT((jep));
   SGE_ASSERT((jatep));

   job_id = lGetUlong(jep, JB_job_number);


   ja_task_id = lGetUlong(jatep, JAT_task_number);
   gdil = lGetList(jatep, JAT_granted_destin_identifier_list);

   if (petep != nullptr) {
      pe_task_id = lGetString(petep, PET_id);
   }

   DPRINTF("job: " sge_u32 " jatask: " sge_u32 " petask: %s\n", job_id, ja_task_id, pe_task_id != nullptr ? pe_task_id : "none");

   master_q = responsible_queue(jep, jatep, petep);
   SGE_ASSERT((master_q));

   pw_buffer_size = get_pw_buffer_size();
   pw_buffer = sge_malloc(pw_buffer_size);
   pw = sge_getpwnam_r(lGetString(jep, JB_owner), &pw_struct, pw_buffer, pw_buffer_size);
   if (!pw) {
      snprintf(err_str, err_length, MSG_SYSTEM_GETPWNAMFAILED_S, lGetString(jep, JB_owner));
      sge_free(&pw_buffer);
      DRETURN(-3); /* error only relevant for this user */
   }

   sge_get_active_job_file_path(&active_dir, job_id, ja_task_id, pe_task_id, nullptr);

   umask(022);

   /* make tmpdir only when this is the first task that gets started
      in this queue instance. QU_job_slots_used holds actual number of used
      slots for this job in the queue */
   if (!(used_slots = qinstance_slots_used(master_q))) {
      if (!(sge_make_tmpdir(master_q, job_id, ja_task_id,
                            pw->pw_uid, pw->pw_gid, tmpdir, SGE_PATH_MAX))) {
         snprintf(err_str, err_length, SFNMAX, MSG_SYSTEM_CANTMAKETMPDIR);
         sge_free(&pw_buffer);
         DRETURN(-2);
      }
   } else {
      SGE_STRUCT_STAT statbuf;
      if (!(sge_get_tmpdir(master_q, job_id, ja_task_id, tmpdir, SGE_PATH_MAX))) {
         snprintf(err_str, err_length, SFNMAX, MSG_SYSTEM_CANTGETTMPDIR);
         sge_free(&pw_buffer);
         DRETURN(-2);
      }

      if (SGE_STAT(tmpdir, &statbuf)) {
         snprintf(err_str, err_length, MSG_SYSTEM_CANTOPENTMPDIR_S, tmpdir);
         sge_free(&pw_buffer);
         DRETURN(-2);
      }
   }

   /* increment used slots */
   DPRINTF("%s: used slots increased from %d to %d\n", lGetString(master_q, QU_full_name), used_slots, used_slots + 1);
   qinstance_set_slots_used(master_q, used_slots + 1);

   nhosts = get_nhosts(gdil);
   pe_slots = 0;
   for_each_ep(gdil_ep, gdil) {
      pe_slots += (int) lGetUlong(gdil_ep, JG_slots);
   }

   // Core Binding
   //
   // Linux: "set affinity" is used to bind the job to cores
   // - check, depending on the used topology, which cores can be used in order to fulfill the selected strategy.
   // - if strategy is not applicable or in case of errors "nullptr" is written to this line in the "config" file
   // - in case SGE_BINDING environment variable has to be set this is done in the shepherd itself and not here.
   //
   // Solaris: "processor sets" are used to bind the job to cores
   // - try to create processor set according to binding strategy and write processor set id to "binding" element in config file
   // - in case SGE_BINDING environment variable has to be setup instead of creating processor sets, this have to be done here
   //   (on Linux this is done from shepherd itself)
   char *rankfileinput = nullptr;
   if (mconf_get_enable_binding()) {

#if defined(OCS_HWLOC)
      ocs::BindingExecd2Shepherd::create_binding_strategy_string_linux(&core_binding_strategy_string, jep, &rankfileinput);
#elif defined(BINDING_SOLARIS)
      ocs::BindingExecd2Shepherd::create_binding_strategy_string_solaris(&core_binding_strategy_string, jep, err_str, err_length, &sge_binding_environment, &rankfileinput);
      if (sge_binding_environment != nullptr) {
         INFO("SGE_BINDING variable set: %s", sge_binding_environment);
      }
#endif
         
#if defined(OCS_HWLOC) || defined(BINDING_SOLARIS)
      if (sge_dstring_get_string(&core_binding_strategy_string) != nullptr
          && strcmp(sge_dstring_get_string(&core_binding_strategy_string), "nullptr") != 0) {
         INFO("core binding: %s", sge_dstring_get_string(&core_binding_strategy_string));

         dstring pseudo_usage = DSTRING_INIT;
         sge_dstring_sprintf(&pseudo_usage, "binding_inuse!%s",
                             binding_get_topology_for_job(sge_dstring_get_string(&core_binding_strategy_string)));

         lListElem *jr = get_job_report(job_id, ja_task_id, pe_task_id);
         add_usage(jr, sge_dstring_get_string(&pseudo_usage), nullptr, 0);
         flush_job_report(jr);
         sge_dstring_free(&pseudo_usage);
      }
#endif
   }
   if (rankfileinput != nullptr) {
      INFO("appended socket,core list to hostfile %s", rankfileinput);
   }

   /***************** write out sge host file ******************************/
   /* JG: TODO: create function write_pe_hostfile() */
   /* JG: TODO (254) use function sge_get_active_job.... */
   if (petep == nullptr) {
      DSTRING_STATIC(dstr_hostfilename, SGE_PATH_MAX);
      const char *str_hostfilename;
      lFreeList(&processor_set);

      str_hostfilename = sge_dstring_sprintf(&dstr_hostfilename, "%s/%s/%s", execd_spool_dir, active_dir_buffer,
                                             PE_HOSTFILE);
      fp = fopen(str_hostfilename, "w");
      if (!fp) {
         snprintf(err_str, err_length, MSG_FILE_NOOPEN_SS, str_hostfilename, strerror(errno));
         sge_free(&rankfileinput);
         sge_free(&pw_buffer);
         DRETURN(-2);
      }

      /*
         Get number of hosts 'nhosts' where the user got queues on

         The granted_destination_identifier_list holds
         on entry for each queue, not host. But each
         entry also contais the hosts name where the
         queue resides on.

         We need to combine the processor sets of all queues on this host.
         They need to get passed to shepherd
      */
      host_slots = 0;
      for_each_ep(gdil_ep, gdil) {
         int slots;
         lList *alp = nullptr;
         /* this is the processor set id in case when when using
            the processors feature */
         const char *q_set = nullptr;

         slots = (int) lGetUlong(gdil_ep, JG_slots);
         q_set = lGetString(gdil_ep, JG_processors);

         /* if job to core binding is used this appears in the fourth row
            and is more important than the processors configuration out
            of the queue since both should NOT be used together */

         if (rankfileinput != nullptr) {
            /* print job2core binding info */
            fprintf(fp, "%s %d %s %s\n",
                    lGetHost(gdil_ep, JG_qhostname),
                    slots,
                    lGetString(gdil_ep, JG_qname),
                    rankfileinput);
         } else {
            /* print processors info      */
            fprintf(fp, "%s %d %s %s\n",
                    lGetHost(gdil_ep, JG_qhostname),
                    slots,
                    lGetString(gdil_ep, JG_qname),
                    q_set ? q_set : "<nullptr>");
         }

         if (!sge_hostcmp(lGetHost(master_q, QU_qhostname), lGetHost(gdil_ep, JG_qhostname))) {
            host_slots += slots;
            if (q_set && strcasecmp(q_set, "UNDEFINED")) {
               range_list_parse_from_string(&processor_set, &alp, q_set,
                                            false, false, INF_ALLOWED);
               /* TODO: should we not print the answerlist in case of an error? */
               lFreeList(&alp);
            }
         }
      }

      FCLOSE(fp);
      sge_free(&rankfileinput);
   }
   /*************************** finished writing sge hostfile  ********/

   /********************** setup environment file ***************************/
   str_fname = sge_dstring_sprintf(&dstr_fname, "%s/%s/environment", execd_spool_dir, active_dir_buffer);
   fp = fopen(str_fname, "w");
   if (!fp) {
      snprintf(err_str, err_length, MSG_FILE_NOOPEN_SS, str_fname, strerror(errno));
      sge_free(&pw_buffer);
      DRETURN(-2);
   }


   /* write environment of job */
   var_list_copy_env_vars_and_value(&environmentList,
                                    lGetList(jep, JB_env_list));

   /* write environment of petask */
   if (petep != nullptr) {
      var_list_copy_env_vars_and_value(&environmentList,
                                       lGetList(petep, PET_environment));
   }

   {
      const lListElem *user_path;
      dstring buffer = DSTRING_INIT;

      if ((user_path = lGetElemStr(environmentList, VA_variable, "PATH"))) {
         sge_dstring_sprintf(&buffer, "%s:%s", tmpdir, lGetString(user_path, VA_value));
      } else {
         sge_dstring_sprintf(&buffer, "%s:%s", tmpdir, SGE_DEFAULT_PATH);
      }
      var_list_set_string(&environmentList, "PATH", sge_dstring_get_string(&buffer));
      sge_dstring_free(&buffer);
   }

   /* 1.) try to read cwd from pe task */
   if (petep != nullptr) {
      cwd = lGetString(petep, PET_cwd);
      path_aliases = lGetList(petep, PET_path_aliases);
   }

   /* 2.) try to read cwd from job */
   if (cwd == nullptr) {
      cwd = lGetString(jep, JB_cwd);
      path_aliases = lGetList(jep, JB_path_aliases);
   }

   /* 3.) if pe task or job set cwd: do path mapping */
   if (cwd != nullptr) {
      /* path aliasing only for cwd flag set */
      path_alias_list_get_path(path_aliases, nullptr,
                               cwd, qualified_hostname,
                               &cwd_out);
      cwd = sge_dstring_get_string(&cwd_out);
      var_list_set_string(&environmentList, "PWD", cwd);
   } else {
      /* 4.) if cwd not set in job: take users home dir
       *     copy the string as we want to free the pw buffer later
       */
      cwd = sge_dstring_copy_string(&cwd_out, pw->pw_dir);
   }

   {
      const char *reqname = petep == nullptr ? lGetString(jep, JB_job_name) : lGetString(petep, PET_name);
      if (reqname != nullptr) {
         var_list_set_string(&environmentList, "REQNAME", reqname);
      }
   }

   var_list_set_string(&environmentList, VAR_PREFIX "CELL", default_cell);

   var_list_set_string(&environmentList, "HOME", pw->pw_dir);
   var_list_set_string(&environmentList, "SHELL", pw->pw_shell);
   var_list_set_string(&environmentList, "USER", pw->pw_name);
   var_list_set_string(&environmentList, "LOGNAME", pw->pw_name);

   /* do not access pw->* from here on! */
   sge_free(&pw_buffer);

   if (sge_binding_environment != nullptr) {
      var_list_set_string(&environmentList, "SGE_BINDING", sge_binding_environment);
      sge_free(&sge_binding_environment);
   }

   /* new RSMAP resource map consumable feature */
   if (lGetList(jatep, JAT_granted_resources_list) != nullptr) {
      const lListElem *gr;

      /* now setting the granted resources list */
      for_each_ep (gr, lGetList(jatep, JAT_granted_resources_list)) {
         u_long32 gru_type = lGetUlong(gr, GRU_type);
         if (gru_type == GRU_HARD_REQUEST_TYPE || gru_type == GRU_RESOURCE_MAP_TYPE) {
            /* if the type is a hard resource request add it to the string */
            std::string str_hgr{"SGE_HGR_"};
            str_hgr += lGetString(gr, GRU_name);

            std::string id_buffer;
            if (gru_type == GRU_RESOURCE_MAP_TYPE) {
               granted_res_get_id_string(id_buffer, lGetList(gr, GRU_resource_map_list));
            } else {
               // @todo use function like double_print_*_to_dstring for formatting
               id_buffer = std::to_string(lGetDouble(gr, GRU_amount));
            }

            var_list_set_string(&environmentList,
                                str_hgr.c_str(),
                                id_buffer.c_str());
         }
      }
   }

   /*
    * Handling of script_file and JOB_NAME:
    * script_file: For batch jobs, it is the path to the spooled
    *              script file, for interactive jobs, a fixed string
    *              (macro), e.g. JOB_TYPE_STR_QSH
    * JOB_NAME:    If a name is passed in JB_job_name or PET_name, i
    *              this name is used.
    *              Otherwise basename(script_file) is used.
    */
   {
      u_long32 jb_now;
      const char *job_name;

      if (petep != nullptr) {
         jb_now = JOB_TYPE_QRSH;
         job_name = lGetString(petep, PET_name);
      } else {
         jb_now = lGetUlong(jep, JB_type);
         job_name = lGetString(jep, JB_job_name);
      }

      /* set script_file */
      JOB_TYPE_CLEAR_IMMEDIATE(jb_now);
      if (jb_now & JOB_TYPE_QSH) {
         str_script_file = sge_dstring_copy_string(&dstr_script_file, JOB_TYPE_STR_QSH);
      } else if (jb_now & JOB_TYPE_QLOGIN) {
         str_script_file = sge_dstring_copy_string(&dstr_script_file, JOB_TYPE_STR_QLOGIN);
      } else if (jb_now & JOB_TYPE_QRSH) {
         str_script_file = sge_dstring_copy_string(&dstr_script_file, JOB_TYPE_STR_QRSH);
      } else if (jb_now & JOB_TYPE_QRLOGIN) {
         str_script_file = sge_dstring_copy_string(&dstr_script_file, JOB_TYPE_STR_QRLOGIN);
      } else if (jb_now & JOB_TYPE_BINARY) {
         const char *sfile;

         sfile = lGetString(jep, JB_script_file);
         if (sfile != nullptr) {
            DSTRING_STATIC(dstr_script_file_out, SGE_PATH_MAX);
            path_alias_list_get_path(lGetList(jep, JB_path_aliases), nullptr,
                                     sfile, qualified_hostname,
                                     &dstr_script_file_out);
            str_script_file = sge_dstring_copy_dstring(&dstr_script_file, &dstr_script_file_out);
         }
      } else {
         if (lGetString(jep, JB_script_file) != nullptr) {
            /* JG: TODO: use some function to create path */
            str_script_file = sge_dstring_sprintf(&dstr_script_file, "%s/%s/" sge_u32, execd_spool_dir, EXEC_DIR,
                                                  job_id);
         } else {
            /*
             * This is an error that will be handled in shepherd.
             * When we implement binary submission (Issue #25), this case
             * might become valid and the binary to execute might be the
             * first argument to execute.
             */
            str_script_file = sge_dstring_copy_string(&dstr_script_file, "none");
         }
      }

      /* set JOB_NAME */
      if (job_name == nullptr) {
         job_name = sge_basename(str_script_file, PATH_SEPARATOR_CHAR);
      }

      var_list_set_string(&environmentList, "JOB_NAME", job_name);
   }

   {
      u_long32 type = lGetUlong(jep, JB_type);
      const char *var_name = "QRSH_COMMAND";

      if (!JOB_TYPE_IS_BINARY(type) && petep == nullptr) {
         const char *old_qrsh_command_s = nullptr;
         dstring old_qrsh_command = DSTRING_INIT;

         old_qrsh_command_s = var_list_get_string(environmentList, var_name);
         if (old_qrsh_command_s != nullptr) {
            char delim[2];
            const char *buffer;
            const char *token;
            int is_first_token = 1;
            dstring new_qrsh_command = DSTRING_INIT;

            snprintf(delim, sizeof(delim), "%c", 0xff);
            sge_dstring_copy_string(&old_qrsh_command, old_qrsh_command_s);
            buffer = sge_dstring_get_string(&old_qrsh_command);
            token = sge_strtok(buffer, delim);
            while (token != nullptr) {
               if (is_first_token) {
                  sge_dstring_sprintf(&new_qrsh_command, "%s/%s/" sge_u32,
                                      execd_spool_dir, EXEC_DIR, job_id);
                  is_first_token = 0;
               } else {
                  sge_dstring_append(&new_qrsh_command, delim);
                  sge_dstring_append(&new_qrsh_command, token);
               }
               token = sge_strtok(nullptr, delim);
            }
            var_list_set_string(&environmentList, var_name, sge_dstring_get_string(&new_qrsh_command));
            sge_dstring_free(&new_qrsh_command);
         }
         sge_dstring_free(&old_qrsh_command);
      } else {
         const char *sfile;

         sfile = var_list_get_string(environmentList, var_name);
         if (sfile != nullptr) {
            dstring dstr_script_file_out = DSTRING_INIT;

            path_alias_list_get_path(lGetList(jep, JB_path_aliases), nullptr,
                                     sfile, qualified_hostname,
                                     &dstr_script_file_out);
            var_list_set_string(&environmentList, var_name, sge_dstring_get_string(&dstr_script_file_out));
            sge_dstring_free(&dstr_script_file_out);
         }
      }
   }

   var_list_set_string(&environmentList, "JOB_SCRIPT", str_script_file);
   str_fname = sge_dstring_sprintf(&dstr_fname, "%s/%s", binary_path, arch);
   var_list_set_string(&environmentList, "SGE_BINARY_PATH", str_fname);

   /* JG: TODO (ENV): do we need REQNAME and REQUEST? */
   var_list_set_string(&environmentList, "REQUEST",
                       petep == nullptr ? lGetString(jep, JB_job_name) : lGetString(petep, PET_name));
   var_list_set_string(&environmentList, "HOSTNAME", lGetHost(master_q, QU_qhostname));
   var_list_set_string(&environmentList, "QUEUE", lGetString(master_q, QU_qname));

   var_list_set_uint32t(&environmentList, "JOB_ID", job_id);

   if (job_is_array(jep)) {
      u_long32 start, end, step;

      job_get_submit_task_ids(jep, &start, &end, &step);

      var_list_set_uint32t(&environmentList, VAR_PREFIX "TASK_ID", ja_task_id);
      var_list_set_uint32t(&environmentList, VAR_PREFIX "TASK_FIRST", start);
      var_list_set_uint32t(&environmentList, VAR_PREFIX "TASK_LAST", end);
      var_list_set_uint32t(&environmentList, VAR_PREFIX "TASK_STEPSIZE", step);
   } else {
      const char *udef = "undefined";

      var_list_set_string(&environmentList, VAR_PREFIX "TASK_ID", udef);
      var_list_set_string(&environmentList, VAR_PREFIX "TASK_FIRST", udef);
      var_list_set_string(&environmentList, VAR_PREFIX "TASK_LAST", udef);
      var_list_set_string(&environmentList, VAR_PREFIX "TASK_STEPSIZE", udef);
   }

   if (pe_task_id != nullptr) {
      var_list_set_string(&environmentList, VAR_PREFIX "PE_TASK_ID", pe_task_id);
   }

   var_list_set_string(&environmentList, "ENVIRONMENT", "BATCH");
   var_list_set_string(&environmentList, "ARC", arch);

   var_list_set_string(&environmentList, VAR_PREFIX "ARCH", arch);

   if ((cp = getenv("TZ")) && strlen(cp))
      var_list_set_string(&environmentList, "TZ", cp);

   if ((cp = getenv("SGE_QMASTER_PORT")) && strlen(cp))
      var_list_set_string(&environmentList, "SGE_QMASTER_PORT", cp);

   if ((cp = getenv("SGE_EXECD_PORT")) && strlen(cp))
      var_list_set_string(&environmentList, "SGE_EXECD_PORT", cp);

   var_list_set_string(&environmentList, VAR_PREFIX "ROOT", sge_root);

   var_list_set_int(&environmentList, "NQUEUES",
                    lGetNumberOfElem(gdil));
   var_list_set_int(&environmentList, "NSLOTS", pe_slots);
   var_list_set_int(&environmentList, "NHOSTS", nhosts);

   var_list_set_int(&environmentList, "RESTARTED", (int) lGetUlong(jatep, JAT_job_restarted));

   var_list_set_string(&environmentList, "TMPDIR", tmpdir);
   var_list_set_string(&environmentList, "TMP", tmpdir);

   var_list_set_string(&environmentList, VAR_PREFIX "ACCOUNT", (lGetString(jep, JB_account) ?
                                                                lGetString(jep, JB_account) : DEFAULT_ACCOUNT));

   sge_get_path(qualified_hostname, lGetList(jep, JB_shell_list), cwd,
                lGetString(jep, JB_owner),
                petep == nullptr ? lGetString(jep, JB_job_name) : lGetString(petep, PET_name),
                job_id,
                job_is_array(jep) ? ja_task_id : 0,
                SGE_SHELL, shell_path, SGE_PATH_MAX);

   if (shell_path[0] == 0) {
      strcpy(shell_path, lGetString(master_q, QU_shell));
   }
   var_list_set_string(&environmentList, "SHELL", shell_path);

   /* forward name of pe to job */
   if (lGetString(jatep, JAT_granted_pe) != nullptr) {
      DSTRING_STATIC(dstr_buffer, SGE_PATH_MAX);
      const char *str_buffer;
      const lListElem *pe;

      var_list_set_string(&environmentList, "PE", lGetString(jatep, JAT_granted_pe));
      /* forward PE_HOSTFILE only to master task */
      if (petep == nullptr) {
         str_buffer = sge_dstring_sprintf(&dstr_buffer, "%s/%s/%s", execd_spool_dir, active_dir_buffer, PE_HOSTFILE);
         var_list_set_string(&environmentList, "PE_HOSTFILE", str_buffer);
      }
      // for tightly integrated parallel jobs, also set the rsh_command SGE_RSH_COMMAND
      // it will be read by qrsh -inherit in the job script
      pe = lGetObject(jatep, JAT_pe_object);
      if (pe != nullptr && lGetBool(pe, PE_control_slaves)) {
         const char *mconf_string = mconf_get_rsh_command();
         if (mconf_string != nullptr && sge_strnullcasecmp(mconf_string, "none") != 0) {
            var_list_set_string(&environmentList, "SGE_RSH_COMMAND", mconf_string);
         } else {
            char default_buffer[SGE_PATH_MAX];
            dstring default_dstring;

            // @todo CS-1262 there is no utilbin/arch/rsh any more
            sge_dstring_init(&default_dstring, default_buffer, SGE_PATH_MAX);
            sge_dstring_sprintf(&default_dstring, "%s/utilbin/%s/rsh", sge_root, arch);
            var_list_set_string(&environmentList, "SGE_RSH_COMMAND", sge_dstring_get_string(&default_dstring));
         }
         sge_free(&mconf_string);

         /* transport the notify kill and susp signals to qrsh -inherit */
         if (mconf_get_notify_kill_type() == 0) {
            mconf_string = mconf_get_notify_kill();
            if (mconf_string != nullptr) {
               var_list_set_string(&environmentList, "SGE_NOTIFY_KILL_SIGNAL", mconf_string);
               sge_free(&mconf_string);
            }
         }
         if (mconf_get_notify_susp_type() == 0) {
            mconf_string = mconf_get_notify_susp();
            if (mconf_string != nullptr) {
               var_list_set_string(&environmentList, "SGE_NOTIFY_SUSP_SIGNAL", mconf_string);
               sge_free(&mconf_string);
            }
         }
      }
   }

   /* forward name of ckpt env to job */
   if ((ep = lGetObject(jep, JB_checkpoint_object))) {
      var_list_set_string(&environmentList, VAR_PREFIX "CKPT_ENV", lGetString(ep, CK_name));
      if (lGetString(ep, CK_ckpt_dir))
         var_list_set_string(&environmentList, VAR_PREFIX "CKPT_DIR", lGetString(ep, CK_ckpt_dir));
   }

   {
      DSTRING_STATIC(dstr_buffer, SGE_PATH_MAX);
      const char *str_buffer;

      str_buffer = sge_dstring_sprintf(&dstr_buffer, "%s/%s", execd_spool_dir, active_dir_buffer);
      var_list_set_string(&environmentList, VAR_PREFIX "JOB_SPOOL_DIR", str_buffer);
   }

   /* Bugfix: Issuezilla 1300
    * Because this change could break pre-existing installations, it's been
    * made optional. */
   if (mconf_get_set_lib_path()) {
      /* If we're supposed to be inheriting the environment from shell that
       * spawned the execd, we end up clobbering the lib path with the
       * following call because what we write out overwrites what gets
       * inherited.  To solve this, we have to set the inherited lib path
       * into the environmentList so that the following call finds it,
       * prepends to it, and write it out.
       * This is really completely backwards from the point of this bug fix,
       * but if we don't do this the resulting behavior is not what an
       * average user would expect.
       * This approach has the side effect that when inherit_env and
       * set_lib_path are both true, the lib path will very likely end up
       * containing two SGE lib entries because the shell that spawned the
       * execd mostly likely had the SGE lib set in its lib path.  The hassle
       * of checking through the lib path to see if the SGE lib is already
       * set is not worth it, in my opinion. */
      if (mconf_get_inherit_env()) {
         const char *lib_path_env = var_get_sharedlib_path_name();
         const char *lib_path = sge_getenv(lib_path_env);

         if (lib_path != nullptr) {
            var_list_set_string(&environmentList, lib_path_env, lib_path);
         }
      }
      var_list_set_sharedlib_path(&environmentList);
   }

   /* set final of variables whose value shall be replaced */
   var_list_copy_prefix_vars(&environmentList, environmentList,
                             VAR_PREFIX, "SGE_");

   /* set final of variables whose value shall not be replaced */
   var_list_copy_prefix_vars_undef(&environmentList, environmentList,
                                   VAR_PREFIX_NR, "SGE_");
   var_list_remove_prefix_vars(&environmentList, VAR_PREFIX);
   var_list_remove_prefix_vars(&environmentList, VAR_PREFIX_NR);

   var_list_dump_to_file(environmentList, fp);


   FCLOSE(fp);
   /*************************** finished writing environment *****************/

   /**************** write out config file ******************************/
   /* JG: TODO (254) use function sge_get_active_job.... */
   str_fname = sge_dstring_sprintf(&dstr_fname, "%s/config", active_dir_buffer);
   fp = fopen(str_fname, "w");
   if (!fp) {
      lFreeList(&environmentList);
      snprintf(err_str, err_length, MSG_FILE_NOOPEN_SS, str_fname, strerror(errno));
      DRETURN(-2);
   }

   // add sge_root and sge_cell to the config file
   // they can be used as special variables in prolog/epilog/pe/ckpt configuration
   fprintf(fp, "sge_root=%s\n", sge_root);
   fprintf(fp, "sge_cell=%s\n", default_cell);

#ifdef COMPILE_DC

#if defined(SOLARIS) || defined(LINUX) || defined(FREEBSD) || defined(DARWIN)

#if defined(LINUX)
   if (!sup_groups_in_proc()) {
      lFreeList(&environmentList);
      snprintf(err_str, err_length, SFNMAX, MSG_EXECD_NOSGID);
      FCLOSE(fp);
      DRETURN(-2);
   }
#endif

   // The usage_collection mode.
   {
      usage_collection_t usage_collection = mconf_get_usage_collection();
      fprintf(fp, "usage_collection=" sge_u32 "\n", usage_collection);
      if (petep == nullptr) {
         lSetUlong(jatep, JAT_usage_collection, usage_collection);
      } else {
         lSetUlong(petep, PET_usage_collection, usage_collection);
      }
   }

   // Set the additional group id.
   // When we are using systemd we do not need to set an additional group id - we use value 0.
   // Unless in hybrid usage collection mode where we get usage from both Systemd and via PDC.
   // And when we enabled killing by add_grp_id, we also need to set it.
   if (ocs::execd::execd_use_pdc_for_usage_collection() || mconf_get_enable_addgrp_kill()) {
      // parse range and create list
      lList *rlp = nullptr;
      lList *alp = nullptr;
      gid_t temp_id;
      char *gid_range = mconf_get_gid_range();
      DPRINTF("gid_range = %s\n", gid_range);
      range_list_parse_from_string(&rlp, &alp, gid_range,
                                   0, 0, INF_NOT_ALLOWED);
      sge_free(&gid_range);
      if (rlp == nullptr) {
         lFreeList(&alp);
         snprintf(err_str, err_length, SFNMAX, MSG_EXECD_NOPARSEGIDRANGE);
         lFreeList(&environmentList);
         FCLOSE(fp);
         DRETURN(-2);
      }

      /* search next add_grp_id */
      temp_id = last_addgrpid;
      last_addgrpid = get_next_addgrpid(rlp, last_addgrpid);
      while (addgrpid_already_in_use(last_addgrpid)) {
         last_addgrpid = get_next_addgrpid(rlp, last_addgrpid);
         if (temp_id == last_addgrpid) {
            snprintf(err_str, err_length, SFNMAX, MSG_EXECD_NOADDGID);
            lFreeList(&environmentList);
            FCLOSE(fp);
            DRETURN(-1);
         }
      }
      lFreeList(&rlp);
      lFreeList(&alp);
   } else {
      // We do not use PDC for usage collection nor do we kill by addrgp,
      // no need to set an additional group id.
      last_addgrpid = 0;
   }

   // write add_grp_id to config file and to the job-structure
   fprintf(fp, "add_grp_id=" gid_t_fmt "\n", last_addgrpid);
   {
      char str_id[256];
      snprintf(str_id, sizeof(str_id), gid_t_fmt, last_addgrpid);
      if (petep == nullptr) {
         lSetString(jatep, JAT_osjobid, str_id);
      } else {
         lSetString(petep, PET_osjobid, str_id);
      }
   }

   if (mconf_get_ignore_ngroups_max_limit()) {
      fprintf(fp, "skip_ngroups_max_silently=yes\n");
   }

#endif

#endif /*  COMPILE_DC */

   /* handle stdout/stderr */
   /* Setting stdin/stdout/stderr
    * These must point to the 'physical' files, so it must be the given
    * input/output/error-path when file staging is disabled and the
    * temp directory when file staging is enabled.
    */

   /* File Staging */

   /* When there is no path given, we will fill it later (in the shepherd) with
    * the default path. */

   bInputFileStaging = sge_get_fs_path(lGetList(jep, JB_stdin_path_list),
                                       fs_stdin_host, SGE_PATH_MAX, fs_stdin_path, SGE_PATH_MAX);

   bOutputFileStaging = sge_get_fs_path(lGetList(jep, JB_stdout_path_list),
                                        fs_stdout_host, SGE_PATH_MAX, fs_stdout_path, SGE_PATH_MAX);

   bErrorFileStaging = sge_get_fs_path(lGetList(jep, JB_stderr_path_list),
                                       fs_stderr_host, SGE_PATH_MAX, fs_stderr_path, SGE_PATH_MAX);


   /* The fs_stdin_tmp_path and stdin_path (and so on) will be set correctly
    * in the shepherd. But we need the 'standard' stdin_path (like it is without
    * file staging) later anyway, so we generate it here. */

   sge_get_path(qualified_hostname,
                lGetList(jep, JB_stdout_path_list), cwd,
                lGetString(jep, JB_owner),
                lGetString(jep, JB_job_name),
                job_id,
                job_is_array(jep) ? ja_task_id : 0,
                SGE_STDOUT, stdout_path, SGE_PATH_MAX);
   sge_get_path(qualified_hostname,
                lGetList(jep, JB_stderr_path_list), cwd,
                lGetString(jep, JB_owner),
                lGetString(jep, JB_job_name),
                job_id,
                job_is_array(jep) ? ja_task_id : 0,
                SGE_STDERR, stderr_path, SGE_PATH_MAX);
   sge_get_path(qualified_hostname,
                lGetList(jep, JB_stdin_path_list), cwd,
                lGetString(jep, JB_owner),
                lGetString(jep, JB_job_name),
                job_id,
                job_is_array(jep) ? ja_task_id : 0,
                SGE_STDIN, stdin_path, SGE_PATH_MAX);

   DPRINTF("fs_stdin_host=%s\n", fs_stdin_host);
   DPRINTF("fs_stdin_path=%s\n", fs_stdin_path);
   DPRINTF("fs_stdin_tmp_path=%s/%s\n", tmpdir, fs_stdin_file ? fs_stdin_file : "");
   DPRINTF("fs_stdin_file_staging=%d\n", bInputFileStaging);

   DPRINTF("fs_stdout_host=%s\n", fs_stdout_host);
   DPRINTF("fs_stdout_path=%s\n", fs_stdout_path);
   DPRINTF("fs_stdout_tmp_path=%s/%s\n", tmpdir, fs_stdout_file ? fs_stdout_file : "");
   DPRINTF("fs_stdout_file_staging=%d\n", bOutputFileStaging);

   DPRINTF("fs_stderr_host=%s\n", fs_stderr_host);
   DPRINTF("fs_stderr_path=%s\n", fs_stderr_path);
   DPRINTF("fs_stderr_tmp_path=%s/%s\n", tmpdir, fs_stderr_file ? fs_stderr_file : "");
   DPRINTF("fs_stderr_file_staging=%d\n", bErrorFileStaging);

   fprintf(fp, "fs_stdin_host=%s\n", fs_stdin_host);
   fprintf(fp, "fs_stdin_path=%s\n", fs_stdin_path);
   fprintf(fp, "fs_stdin_tmp_path=%s/%s\n", tmpdir, fs_stdin_file ? fs_stdin_file : "");
   fprintf(fp, "fs_stdin_file_staging=%d\n", bInputFileStaging);

   fprintf(fp, "fs_stdout_host=%s\n", fs_stdout_host);
   fprintf(fp, "fs_stdout_path=%s\n", fs_stdout_path);
   fprintf(fp, "fs_stdout_tmp_path=%s/%s\n", tmpdir, fs_stdout_file ? fs_stdout_file : "");
   fprintf(fp, "fs_stdout_file_staging=%d\n", bOutputFileStaging);

   fprintf(fp, "fs_stderr_host=%s\n", fs_stderr_host);
   fprintf(fp, "fs_stderr_path=%s\n", fs_stderr_path);
   fprintf(fp, "fs_stderr_tmp_path=%s/%s\n", tmpdir, fs_stderr_file ? fs_stderr_file : "");
   fprintf(fp, "fs_stderr_file_staging=%d\n", bErrorFileStaging);

   fprintf(fp, "stdout_path=%s\n", stdout_path);
   fprintf(fp, "stderr_path=%s\n", stderr_path);
   fprintf(fp, "stdin_path=%s\n", stdin_path);
   fprintf(fp, "merge_stderr=%d\n", (int) lGetBool(jep, JB_merge_stderr));

   fprintf(fp, "tmpdir=%s\n", tmpdir);

   DPRINTF("stdout_path=%s\n", stdout_path);
   DPRINTF("stderr_path=%s\n", stderr_path);
   DPRINTF("stdin_path=%s\n", stdin_path);
   DPRINTF("merge_stderr=%d\n", (int) lGetBool(jep, JB_merge_stderr));

   {
      u_long32 jb_now = lGetUlong(jep, JB_type);
      int handle_as_binary = (JOB_TYPE_IS_BINARY(jb_now) ? 1 : 0);
      int no_shell = (JOB_TYPE_IS_NO_SHELL(jb_now) ? 1 : 0);

      fprintf(fp, "handle_as_binary=%d\n", handle_as_binary);
      fprintf(fp, "no_shell=%d\n", no_shell);
   }

   if (lGetUlong(jep, JB_checkpoint_attr) &&
       (ep = lGetObject(jep, JB_checkpoint_object))) {
      fprintf(fp, "ckpt_job=1\n");
      fprintf(fp, "ckpt_restarted=%d\n", petep != nullptr ? 0 : (int) lGetUlong(jatep, JAT_job_restarted));
      fprintf(fp, "ckpt_pid=%d\n", (int) lGetUlong(jatep, JAT_pvm_ckpt_pid));
      fprintf(fp, "ckpt_osjobid=%s\n", lGetString(jatep, JAT_osjobid) ? lGetString(jatep, JAT_osjobid) : "0");
      fprintf(fp, "ckpt_interface=%s\n", lGetString(ep, CK_interface));
      fprintf(fp, "ckpt_command=%s\n", lGetString(ep, CK_ckpt_command));
      fprintf(fp, "ckpt_migr_command=%s\n", lGetString(ep, CK_migr_command));
      fprintf(fp, "ckpt_rest_command=%s\n", lGetString(ep, CK_rest_command));
      fprintf(fp, "ckpt_clean_command=%s\n", lGetString(ep, CK_clean_command));
      fprintf(fp, "ckpt_dir=%s\n", lGetString(ep, CK_ckpt_dir));
      fprintf(fp, "ckpt_signal=%s\n", lGetString(ep, CK_signal));

      if (!(lGetUlong(jep, JB_checkpoint_attr) & CHECKPOINT_AT_MINIMUM_INTERVAL)) {
         interval = 0;
      } else {
         parse_ulong_val(nullptr, &interval, TYPE_TIM, lGetString(master_q, QU_min_cpu_interval), nullptr, 0);
         interval = MAX(interval, lGetUlong(jep, JB_checkpoint_interval));
      }
      fprintf(fp, "ckpt_interval=%d\n", (int) interval);
   } else {
      fprintf(fp, "ckpt_job=0\n");
   }

   /*
    * Shorthand for this code sequence:
    *   - obtain resource A from master queue and write out to shepherd config file
    *   - check if resource is job consumable and indicate to shepherd
    */
#define WRITE_COMPLEX_AND_CONSUMABLE_ATTR(A, T) \
   fprintf(fp, #A"=%s\n", sge_exec_job_get_limit(&dstr_limit, QU_##A, #A, T, master_q, jatep, petep, qualified_hostname)); \
   job_is_requesting_consumable(jep, #A) ? fprintf(fp, #A"_is_consumable_job=1\n") : fprintf(fp, #A"_is_consumable_job=0\n")
#define WRITE_COMPLEX_ATTR(A, T) \
   fprintf(fp, #A"=%s\n", sge_exec_job_get_limit(&dstr_limit, QU_##A, #A, T, master_q, jatep, petep, qualified_hostname))
                                 \
   {
      DSTRING_STATIC(dstr_limit, 512);
      WRITE_COMPLEX_AND_CONSUMABLE_ATTR(h_vmem, TYPE_MEM);
      WRITE_COMPLEX_AND_CONSUMABLE_ATTR(s_vmem, TYPE_MEM);

      WRITE_COMPLEX_AND_CONSUMABLE_ATTR(h_cpu, TYPE_TIM);
      WRITE_COMPLEX_AND_CONSUMABLE_ATTR(s_cpu, TYPE_TIM);

      WRITE_COMPLEX_AND_CONSUMABLE_ATTR(h_stack, TYPE_MEM);
      WRITE_COMPLEX_AND_CONSUMABLE_ATTR(s_stack, TYPE_MEM);

      WRITE_COMPLEX_AND_CONSUMABLE_ATTR(h_data, TYPE_MEM);
      WRITE_COMPLEX_AND_CONSUMABLE_ATTR(s_data, TYPE_MEM);

      // @todo why not use WRITE_COMPLEX_AND_CONSUMABLE_ATTR? Can't they be made consumable?
      WRITE_COMPLEX_ATTR(h_core, TYPE_MEM);
      WRITE_COMPLEX_ATTR(s_core, TYPE_MEM);

      WRITE_COMPLEX_ATTR(h_rss, TYPE_MEM);
      WRITE_COMPLEX_ATTR(s_rss, TYPE_MEM);

      WRITE_COMPLEX_ATTR(h_fsize, TYPE_MEM);
      WRITE_COMPLEX_ATTR(s_fsize, TYPE_MEM);
   }

   {
      char *s;

      mconf_get_s_descriptors(&s);
      fprintf(fp, "s_descriptors=%s\n", s);
      sge_free(&s);

      mconf_get_h_descriptors(&s);
      fprintf(fp, "h_descriptors=%s\n", s);
      sge_free(&s);

      mconf_get_s_maxproc(&s);
      fprintf(fp, "s_maxproc=%s\n", s);
      sge_free(&s);

      mconf_get_h_maxproc(&s);
      fprintf(fp, "h_maxproc=%s\n", s);
      sge_free(&s);

      mconf_get_s_memorylocked(&s);
      fprintf(fp, "s_memorylocked=%s\n", s);
      sge_free(&s);

      mconf_get_h_memorylocked(&s);
      fprintf(fp, "h_memorylocked=%s\n", s);
      sge_free(&s);

      mconf_get_s_locks(&s);
      fprintf(fp, "s_locks=%s\n", s);
      sge_free(&s);

      mconf_get_h_locks(&s);
      fprintf(fp, "h_locks=%s\n", s);
      sge_free(&s);
   }

   fprintf(fp, "priority=%s\n", lGetString(master_q, QU_priority));
   fprintf(fp, "shell_path=%s\n", shell_path);
   fprintf(fp, "script_file=%s\n", str_script_file);
   fprintf(fp, "job_owner=%s\n", lGetString(jep, JB_owner));
   fprintf(fp, "min_gid=" sge_u32 "\n", mconf_get_min_gid());
   fprintf(fp, "min_uid=" sge_u32 "\n", mconf_get_min_uid());

   /* do path substitutions also for cwd */
   {
      char cwd_buf[SGE_PATH_MAX];
      dstring dstr_cwd;

      sge_dstring_init(&dstr_cwd, cwd_buf, sizeof(cwd_buf));

      if ((cp = expand_path(&dstr_cwd, cwd, job_id, job_is_array(jep) ? ja_task_id : 0,
                            lGetString(jep, JB_job_name),
                            lGetString(jep, JB_owner),
                            qualified_hostname))) {
         cwd = sge_dstring_copy_string(&cwd_out, cp);
      }
      fprintf(fp, "cwd=%s\n", cwd);
   }

   /* do not start prolog/epilog in case of pe tasks */
   if (petep == nullptr) {
      char *prolog = mconf_get_prolog();
      char *epilog = mconf_get_epilog();

      fprintf(fp, "prolog=%s\n",
              ((cp = lGetString(master_q, QU_prolog)) && strcasecmp(cp, "none")) ?
              cp : prolog);
      fprintf(fp, "epilog=%s\n",
              ((cp = lGetString(master_q, QU_epilog)) && strcasecmp(cp, "none")) ?
              cp : epilog);
      sge_free(&prolog);
      sge_free(&epilog);
   } else {
      fprintf(fp, "prolog=%s\n", "none");
      fprintf(fp, "epilog=%s\n", "none");
   }

   fprintf(fp, "starter_method=%s\n", (cp = lGetString(master_q, QU_starter_method)) ? cp : "none");
   fprintf(fp, "suspend_method=%s\n", (cp = lGetString(master_q, QU_suspend_method)) ? cp : "none");
   fprintf(fp, "resume_method=%s\n", (cp = lGetString(master_q, QU_resume_method)) ? cp : "none");
   fprintf(fp, "terminate_method=%s\n", (cp = lGetString(master_q, QU_terminate_method)) ? cp : "none");

   fprintf(fp, "script_timeout=" sge_u32 "\n", mconf_get_script_timeout());

   fprintf(fp, "pe=%s\n", lGetString(jatep, JAT_granted_pe) ? lGetString(jatep, JAT_granted_pe) : "none");
   fprintf(fp, "pe_slots=%d\n", pe_slots);
   fprintf(fp, "host_slots=%d\n", host_slots);

   /* write pe related data */
   if (lGetString(jatep, JAT_granted_pe)) {
      lListElem *pep = nullptr;
      /* no pe start/stop for petasks */
      if (petep == nullptr) {
         pep = lGetObject(jatep, JAT_pe_object);
      }
      fprintf(fp, "pe_hostfile=%s/%s/%s\n", execd_spool_dir, active_dir_buffer, PE_HOSTFILE);
      fprintf(fp, "pe_start=%s\n", pep != nullptr && lGetString(pep, PE_start_proc_args) ?
                                   lGetString(pep, PE_start_proc_args) : "none");
      fprintf(fp, "pe_stop=%s\n", pep != nullptr && lGetString(pep, PE_stop_proc_args) ?
                                  lGetString(pep, PE_stop_proc_args) : "none");

      /* build path for stdout of pe scripts */
      sge_get_path(qualified_hostname,
                   lGetList(jep, JB_stdout_path_list), cwd,
                   lGetString(jep, JB_owner),
                   lGetString(jep, JB_job_name),
                   job_id,
                   job_is_array(jep) ? ja_task_id : 0,
                   SGE_PAR_STDOUT, pe_stdout_path, SGE_PATH_MAX);
      fprintf(fp, "pe_stdout_path=%s\n", pe_stdout_path);

      /* build path for stderr of pe scripts */
      sge_get_path(qualified_hostname,
                   lGetList(jep, JB_stderr_path_list), cwd,
                   lGetString(jep, JB_owner),
                   lGetString(jep, JB_job_name),
                   job_id,
                   job_is_array(jep) ? ja_task_id : 0,
                   SGE_PAR_STDERR, pe_stderr_path, SGE_PATH_MAX);
      fprintf(fp, "pe_stderr_path=%s\n", pe_stderr_path);
   }

   shell_start_mode = mconf_get_shell_start_mode();
   fprintf(fp, "shell_start_mode=%s\n",
           job_get_shell_start_mode(jep, master_q, shell_start_mode));
   sge_free(&shell_start_mode);

   /* we need the basename for loginshell test */
   shell = strrchr(shell_path, '/');
   if (shell == nullptr) {
      shell = shell_path;
   } else {
      shell++;
   }
   fprintf(fp, "use_login_shell=%d\n", ck_login_sh(shell) ? 1 : 0);

   // systemd specific options:
   //    - enable_systemd
   //    - slice and scope
   //    - devices_allow
#ifdef OCS_WITH_SYSTEMD
   {
      bool enable_systemd = mconf_get_enable_systemd() && ocs::uti::Systemd::is_systemd_available();
      fprintf(fp, "enable_systemd=%d\n", enable_systemd ? 1 : 0);

      if (enable_systemd) {
         // slice and scope
         std::string systemd_slice;
         std::string systemd_scope;
         DSTRING_STATIC(error_dstr, MAX_STRING_SIZE);
         if (ocs::Job::job_get_systemd_slice_and_scope(jep, jatep, petep, systemd_slice, systemd_scope, &error_dstr)) {
            fprintf(fp, "systemd_slice=%s\n", systemd_slice.c_str());
            fprintf(fp, "systemd_scope=%s\n", systemd_scope.c_str());
         }

         // device isolation
         // for testing purposes until we have device isolation via RSMAPs
         env = lGetElemStr(lGetList(jep, JB_env_list), VA_variable, "SGE_DEBUG_DEVICES_ALLOW");
         if (env != nullptr) {
            const char *devices_allow = lGetString(env, VA_value);
            fprintf(fp, "devices_allow=%s\n", devices_allow != nullptr ? devices_allow : "");
         } else {
            fprintf(fp, "devices_allow=\n");
         }
         if (petep == nullptr) {
            lSetString(jatep, JAT_systemd_scope, systemd_scope.c_str());
         } else {
            lSetString(petep, PET_systemd_scope, systemd_scope.c_str());
         }

         // in case of tightly integrated parallel jobs, we need to store the systemd slice.
         // this is one place (where the master task is started),
         // the other one is when the slave container is started.
         if (petep == nullptr) {
            ocs::execd::execd_store_tight_pe_slice(jep, jatep, systemd_slice.c_str());
         }
      }
   }
#endif

   /* the following values are needed by the reaper */
   if (mailrec_unparse(lGetList(jep, JB_mail_list), mail_str, sizeof(mail_str))) {
      ERROR(MSG_MAIL_MAILLISTTOOLONG_U, job_id);
   }
   fprintf(fp, "mail_list=%s\n", mail_str);
   fprintf(fp, "mail_options=" sge_u32 "\n", lGetUlong(jep, JB_mail_options));
   fprintf(fp, "forbid_reschedule=%d\n", mconf_get_forbid_reschedule() ? 1 : 0);
   fprintf(fp, "forbid_apperror=%d\n", mconf_get_forbid_apperror() ? 1 : 0);
   fprintf(fp, "queue=%s\n", lGetString(master_q, QU_qname));
   fprintf(fp, "host=%s\n", lGetHost(master_q, QU_qhostname));
   {
      dstring range_string = DSTRING_INIT;
      range_list_print_to_string(processor_set, &range_string, true, false, false);
      fprintf(fp, "processors=%s\n", sge_dstring_get_string(&range_string));
      sge_dstring_free(&range_string);


      if (sge_dstring_get_string(&core_binding_strategy_string) == nullptr) {
         processor_binding_strategy = "nullptr";
      } else {
         processor_binding_strategy = sge_dstring_get_string(&core_binding_strategy_string);
      }
      /* write binding strategy */
      fprintf(fp, "binding=%s\n", processor_binding_strategy);

   }
   if (petep != nullptr) {
      fprintf(fp, "job_name=%s\n", lGetString(petep, PET_name));
   } else {
      fprintf(fp, "job_name=%s\n", lGetString(jep, JB_job_name));
   }
   fprintf(fp, "job_id=" sge_u32 "\n", job_id);
   fprintf(fp, "ja_task_id=" sge_u32 "\n", job_is_array(jep) ? ja_task_id : 0);
   if (petep != nullptr) {
      fprintf(fp, "pe_task_id=%s\n", pe_task_id);
   }
   fprintf(fp, "account=%s\n", (lGetString(jep, JB_account) ? lGetString(jep, JB_account) : DEFAULT_ACCOUNT));
   if (petep != nullptr) {
      fprintf(fp, "submission_time=" sge_u64 "\n", lGetUlong64(petep, PET_submission_time));
   } else {
      fprintf(fp, "submission_time=" sge_u64 "\n", lGetUlong64(jep, JB_submission_time));
   }

   {
      u_long32 notify = 0;
      if (lGetBool(jep, JB_notify)) {
         parse_ulong_val(nullptr, &notify, TYPE_TIM, lGetString(master_q, QU_notify), nullptr, 0);
      }
      fprintf(fp, "notify=" sge_u32 "\n", notify);
   }

   /*
   ** interactive (qsh) jobs have no exec file
   */
   if (!lGetString(jep, JB_script_file)) {
      u_long32 jb_now;
      if (petep != nullptr) {
         jb_now = JOB_TYPE_QRSH;
      } else {
         jb_now = lGetUlong(jep, JB_type);
      }

      if (!(JOB_TYPE_IS_QLOGIN(jb_now) ||
            JOB_TYPE_IS_QRSH(jb_now) ||
            JOB_TYPE_IS_QRLOGIN(jb_now))) {
         char *xterm = mconf_get_xterm();
         DPRINTF("interactive job\n");
         /*
         ** get xterm configuration value
         */

         if (xterm) {
            fprintf(fp, "exec_file=%s\n", xterm);
            DPRINTF("exec_file=%s\n", xterm);
         } else {
            snprintf(err_str, err_length, SFNMAX, MSG_EXECD_NOXTERM);
            FCLOSE(fp);
            lFreeList(&environmentList);
            sge_dstring_free(&core_binding_strategy_string);
            DRETURN(-2);
            /*
            ** this causes a general failure
            */
         }
         sge_free(&xterm);
      }
   }

   if ((cp = lGetString(jep, JB_project)) && *cp != '\0')
      fprintf(fp, "acct_project=%s\n", cp);
   else
      fprintf(fp, "acct_project=none\n");


   {
      const lList *args;
      const lListElem *se;

      int nargs = 1;

      /* for pe tasks we have no args - the daemon (rshd etc.) to start
       * comes from the cluster configuration 
       */
      if (petep != nullptr) {
         args = nullptr;
      } else {
         args = lGetList(jep, JB_job_args);
      }

      fprintf(fp, "njob_args=" sge_u32 "\n", lGetNumberOfElem(args));

      for_each_ep(se, args) {
         const char *arg = lGetString(se, ST_name);
         if (arg != nullptr) {
            fprintf(fp, "job_arg%d=%s\n", nargs++, arg);
         } else {
            fprintf(fp, "job_arg%d=\n", nargs++);
         }
      }
   }

   fprintf(fp, "queue_tmpdir=%s\n", lGetString(master_q, QU_tmpdir));
   /*
   ** interactive jobs need a display to send output back to (only qsh - JG)
   */
   if (!lGetString(jep, JB_script_file)) {         /* interactive job  */
      u_long32 jb_now;
      if (petep != nullptr) {
         jb_now = JOB_TYPE_QRSH;
      } else {
         jb_now = lGetUlong(jep, JB_type);    /* detect qsh case  */
      }

      /* check DISPLAY variable for qsh jobs */
      if (JOB_TYPE_IS_QSH(jb_now)) {
         lList *answer_list = nullptr;

         if (job_check_qsh_display(jep, &answer_list, false) == STATUS_OK) {
            env = lGetElemStr(lGetList(jep, JB_env_list), VA_variable, "DISPLAY");
            fprintf(fp, "display=%s\n", lGetString(env, VA_value));
         } else {
            /* JG: TODO: the whole function should not use err_str to return
             *           error descriptions, but an answer list.
             */
            const char *error_string = lGetString(lFirst(answer_list), AN_text);
            if (error_string != nullptr) {
               snprintf(err_str, err_length, SFNMAX, error_string);
            }
            lFreeList(&answer_list);
            lFreeList(&environmentList);
            FCLOSE(fp);
            sge_dstring_free(&core_binding_strategy_string);
            DRETURN(-1);
         }
      }
   }

   /* if "pag_cmd" is not set, do not use AFS setup for this host */
   pag_cmd = mconf_get_pag_cmd();
   if (feature_is_enabled(FEATURE_AFS_SECURITY) && pag_cmd &&
       strlen(pag_cmd) && strcasecmp(pag_cmd, "none")) {
      fprintf(fp, "use_afs=1\n");

      shepherd_name = SGE_COSHEPHERD;
      snprintf(coshepherd_path, sizeof(coshepherd_path), "%s/%s/%s", binary_path, sge_get_arch(), shepherd_name);
      fprintf(fp, "coshepherd=%s\n", coshepherd_path);
      set_token_cmd = mconf_get_set_token_cmd();
      fprintf(fp, "set_token_cmd=%s\n", set_token_cmd ? set_token_cmd : "none");
      sge_free(&set_token_cmd);
      fprintf(fp, "token_extend_time=%d\n", (int) mconf_get_token_extend_time());
   } else {
      fprintf(fp, "use_afs=0\n");
   }
   sge_free(&pag_cmd);

   fprintf(fp, "admin_user=%s\n", admin_user);

   /* notify method */
   fprintf(fp, "notify_kill_type=%d\n", mconf_get_notify_kill_type());
   notify_kill = mconf_get_notify_kill();
   fprintf(fp, "notify_kill=%s\n", notify_kill ? notify_kill : "default");
   sge_free(&notify_kill);
   fprintf(fp, "notify_susp_type=%d\n", mconf_get_notify_susp_type());
   notify_susp = mconf_get_notify_susp();
   fprintf(fp, "notify_susp=%s\n", notify_susp ? notify_susp : "default");
   sge_free(&notify_susp);
   if (mconf_get_use_qsub_gid()) {
      fprintf(fp, "qsub_gid=" sge_u32 "\n", lGetUlong(jep, JB_gid));
   } else {
      fprintf(fp, "qsub_gid=%s\n", "no");
   }

   /* config for interactive jobs */
   {
      u_long32 jb_now;
      int pty_option;
      if (petep != nullptr) {
         jb_now = JOB_TYPE_QRSH;
      } else {
         jb_now = lGetUlong(jep, JB_type);
      }

      if (petep != nullptr) {
         fprintf(fp, "pe_task_id=%s\n", pe_task_id);
      }

      pty_option = (int) lGetUlong(jep, JB_pty);
      if (pty_option < 0 || pty_option > 2) {
         pty_option = 2;   /* 2 = use default */
      }
      fprintf(fp, "pty=%d\n", pty_option);

      if (JOB_TYPE_IS_QLOGIN(jb_now) || JOB_TYPE_IS_QRSH(jb_now)
          || JOB_TYPE_IS_QRLOGIN(jb_now)) {
         const lListElem *elem;
         char daemon[SGE_PATH_MAX];

         fprintf(fp, "master_host=%s\n", masterhost);
         fprintf(fp, "commd_port=-1\n");  /* commd_port not used for GE > 6.0 */

         if ((elem = lGetElemStr(environmentList, VA_variable, "QRSH_PORT")) != nullptr) {
            fprintf(fp, "qrsh_control_port=%s\n", lGetString(elem, VA_value));
         }
        
         snprintf(daemon, sizeof(daemon), "%s/utilbin/%s/", sge_root, arch);
        
         if(JOB_TYPE_IS_QLOGIN(jb_now)) {
            char* qlogin_daemon = mconf_get_qlogin_daemon();
            fprintf(fp, "qlogin_daemon=%s\n", qlogin_daemon);
            sge_free(&qlogin_daemon);
         } else {
            // @todo CS-1262 we do no longer deliver a rshd or rlogind
            //       no longer need the write_osjob_id config value, remove it from here and from shepherd
            if (JOB_TYPE_IS_QRSH(jb_now)) {
               char *rsh_daemon = mconf_get_rsh_daemon();
               strcat(daemon, "rshd");
               if (strcasecmp(rsh_daemon, "none") == 0) {
                  strcat(daemon, " -l");
                  fprintf(fp, "rsh_daemon=%s\n", daemon);
                  write_osjob_id = 0; /* will be done by our rshd */
               } else {
                  fprintf(fp, "rsh_daemon=%s\n", rsh_daemon);
                  if (strncmp(daemon, rsh_daemon, strlen(daemon)) == 0) {
                     write_osjob_id = 0; /* will be done by our rshd */
                  }
               }
               sge_free(&rsh_daemon);

               fprintf(fp, "qrsh_tmpdir=%s\n", tmpdir);

               if (petep != nullptr) {
                  fprintf(fp, "qrsh_pid_file=%s/pid.%s\n", tmpdir, pe_task_id);
               } else {
                  fprintf(fp, "qrsh_pid_file=%s/pid\n", tmpdir);
               }
            } else {
               if (JOB_TYPE_IS_QRLOGIN(jb_now)) {
                  char *rlogin_daemon = mconf_get_rlogin_daemon();
                  strcat(daemon, "rlogind");
                  if (strcasecmp(rlogin_daemon, "none") == 0) {
                     strcat(daemon, " -l");
                     fprintf(fp, "rlogin_daemon=%s\n", daemon);
                     write_osjob_id = 0; /* will be done by our rlogind */
                  } else {
                     fprintf(fp, "rlogin_daemon=%s\n", rlogin_daemon);
                     if (strncmp(daemon, rlogin_daemon, strlen(daemon)) == 0) {
                        write_osjob_id = 0; /* will be done by our rlogind */
                     }
                  }
                  sge_free(&rlogin_daemon);
               }
            }
         }
      }
   }

   /* shall shepherd write osjob_id, or is it done by (our) rshd */
   fprintf(fp, "write_osjob_id=%d\n", write_osjob_id);

   /* should the job inherit the execd's environment */
   fprintf(fp, "inherit_env=%d\n", (int) mconf_get_inherit_env());

   /* should the addgrp-id be used to kill processes */
   fprintf(fp, "enable_addgrp_kill=%d\n", (int) mconf_get_enable_addgrp_kill());

   if (strcasecmp(bootstrap_get_security_mode(), "csp") == 0) {
      csp_mode = true;
   }
   fprintf(fp, "csp=%d\n", (int) csp_mode);

   /* with new interactive job support, shepherd needs ignore_fqdn and default_domain */
   fprintf(fp, "ignore_fqdn=%d\n", bootstrap_get_ignore_fqdn());
   fprintf(fp, "default_domain=%s\n", bootstrap_get_default_domain());

   lFreeList(&environmentList);
   FCLOSE(fp);

   sge_dstring_free(&core_binding_strategy_string);

   /********************** finished writing config ************************/

   /* test whether we can access scriptfile */
   /*
   ** tightly integrated (qrsh) and interactive jobs dont need to access script file
   */
   if (petep == nullptr) {
      u_long32 jb_now = lGetUlong(jep, JB_type);
      JOB_TYPE_CLEAR_IMMEDIATE(jb_now);            /* batch jobs can also be immediate */
      if (jb_now == 0) {                          /* it is a batch job */
         if (SGE_STAT(str_script_file, &buf)) {
            snprintf(err_str, err_length, MSG_EXECD_UNABLETOFINDSCRIPTFILE_SS,
                     str_script_file, strerror(errno));
            DRETURN(-2);
         }
      }
   }

   shepherd_name = SGE_SHEPHERD;
   snprintf(shepherd_path, sizeof(shepherd_path), "%s/%s/%s", binary_path, arch, shepherd_name);

   if (SGE_STAT(shepherd_path, &buf)) {
      /* second chance: without architecture */
      snprintf(shepherd_path, sizeof(shepherd_path), "%s/%s", binary_path, shepherd_name);
      if (SGE_STAT(shepherd_path, &buf)) {
         snprintf(err_str, err_length, MSG_EXECD_NOSHEPHERD_SSS, arch, shepherd_path, strerror(errno));
         DRETURN(-2);
      }
   }

   pag_cmd = mconf_get_pag_cmd();
   shepherd_cmd = mconf_get_shepherd_cmd();
   if (shepherd_cmd && strlen(shepherd_cmd) &&
       strcasecmp(shepherd_cmd, "none")) {
      if (SGE_STAT(shepherd_cmd, &buf)) {
         snprintf(err_str, err_length, MSG_EXECD_NOSHEPHERDWRAP_SS, shepherd_cmd, strerror(errno));
         sge_free(&pag_cmd);
         sge_free(&shepherd_cmd);
         DRETURN(-2);
      }
   }
   else if (mconf_get_do_credentials() && feature_is_enabled(FEATURE_DCE_SECURITY)) {
      snprintf(dce_wrapper_cmd, sizeof(dce_wrapper_cmd), "/%s/utilbin/%s/starter_cred", sge_root, arch);
      if (SGE_STAT(dce_wrapper_cmd, &buf)) {
         snprintf(err_str, err_length, MSG_DCE_NOSHEPHERDWRAP_SS, dce_wrapper_cmd, strerror(errno));
         sge_free(&pag_cmd);
         sge_free(&shepherd_cmd);
         DRETURN(-2);
      }
   } else if (feature_is_enabled(FEATURE_AFS_SECURITY) && pag_cmd &&
              strlen(pag_cmd) && strcasecmp(pag_cmd, "none")) {
      int fd, len;
      const char *cp;

      if (SGE_STAT(coshepherd_path, &buf)) {
         shepherd_name = SGE_COSHEPHERD;
         snprintf(coshepherd_path, sizeof(coshepherd_path), "%s/%s", binary_path, shepherd_name);
         if (SGE_STAT(coshepherd_path, &buf)) {
            snprintf(err_str, err_length, MSG_EXECD_NOCOSHEPHERD_SSS, arch, coshepherd_path, strerror(errno));
            sge_free(&pag_cmd);
            sge_free(&shepherd_cmd);
            DRETURN(-2);
         }
      }
      set_token_cmd = mconf_get_set_token_cmd();
      if (!set_token_cmd ||
          !strlen(set_token_cmd) || !mconf_get_token_extend_time()) {
         snprintf(err_str, err_length, SFNMAX, MSG_EXECD_AFSCONFINCOMPLETE);
         sge_free(&pag_cmd);
         sge_free(&shepherd_cmd);
         DRETURN(-2);
      }
      sge_free(&set_token_cmd);

      /* JG: TODO (254) use function sge_get_active_job.... */
      str_fname = sge_dstring_sprintf(&dstr_fname, "%s/%s", active_dir_buffer, TOKEN_FILE);
      if ((fd = SGE_OPEN3(str_fname, O_RDWR | O_CREAT | O_TRUNC, 0600)) == -1) {
         snprintf(err_str, err_length, MSG_EXECD_NOCREATETOKENFILE_S, strerror(errno));
         sge_free(&pag_cmd);
         sge_free(&shepherd_cmd);
         DRETURN(-2);
      }

      cp = lGetString(jep, JB_tgt);
      if (!cp || !(len = strlen(cp))) {
         snprintf(err_str, err_length, SFNMAX, MSG_EXECD_TOKENZERO);
         sge_free(&pag_cmd);
         sge_free(&shepherd_cmd);
         DRETURN(-3); /* problem of this user */
      }
      if (write(fd, cp, len) != len) {
         snprintf(err_str, err_length, MSG_EXECD_NOWRITETOKEN_S, strerror(errno));
         sge_free(&pag_cmd);
         sge_free(&shepherd_cmd);
         DRETURN(-2);
      }
      close(fd);
   }
   sge_free(&pag_cmd);
   sge_free(&shepherd_cmd);

   /* send mail to users if requested */
   if (petep == nullptr) {
      if (VALID(MAIL_AT_BEGINNING, lGetUlong(jep, JB_mail_options))) {
         dstring subject = DSTRING_INIT;
         dstring body = DSTRING_INIT;
         dstring ds = DSTRING_INIT;

         sge_ctime64(lGetUlong64(jatep, JAT_start_time), &ds);

         if (job_is_array(jep)) {
            sge_dstring_sprintf(&subject, MSG_MAIL_STARTSUBJECT_UUS, job_id,
                                ja_task_id, lGetString(jep, JB_job_name));
            sge_dstring_sprintf(&body, MSG_MAIL_STARTBODY_UUSSSSS,
                                job_id, ja_task_id,
                                lGetString(jep, JB_job_name),
                                lGetString(jep, JB_owner),
                                lGetString(master_q, QU_qname),
                                lGetHost(master_q, QU_qhostname), sge_dstring_get_string(&ds));
         } else {
            sge_dstring_sprintf(&subject, MSG_MAIL_STARTSUBJECT_US, job_id,
                                lGetString(jep, JB_job_name));
            sge_dstring_sprintf(&body, MSG_MAIL_STARTBODY_USSSSS,
                                job_id,
                                lGetString(jep, JB_job_name),
                                lGetString(jep, JB_owner),
                                lGetString(master_q, QU_qname),
                                lGetHost(master_q, QU_qhostname), sge_dstring_get_string(&ds));
         }

         cull_mail(EXECD, lGetList(jep, JB_mail_list), sge_dstring_get_string(&subject),
                   sge_dstring_get_string(&body), MSG_MAIL_TYPE_START);
         sge_dstring_free(&ds);
         sge_dstring_free(&subject);
         sge_dstring_free(&body);
      }
   }

   /* Change to jobs directory. Father changes back to cwd. We do this to
      ensure chdir() works before forking. */
   if (chdir(active_dir_buffer)) {
      snprintf(err_str, err_length, MSG_FILE_CHDIR_SS, active_dir_buffer, strerror(errno));
      DRETURN(-2);
   }
   {

      /* 
       * Add the signals to the set of blocked signals. Save previous signal 
       * mask. We do this before the fork() to avoid any race conditions with
       * signals being immediately sent to the shepherd after the fork.
       * After the fork we  need to restore the oldsignal mask in the execd
       */
      sigemptyset(&sigset);
      sigaddset(&sigset, SIGTTOU);
      sigaddset(&sigset, SIGTTIN);
      sigaddset(&sigset, SIGUSR1);
      sigaddset(&sigset, SIGUSR2);
      sigaddset(&sigset, SIGCONT);
      sigaddset(&sigset, SIGWINCH);
      sigaddset(&sigset, SIGTSTP);
      sigprocmask(SIG_BLOCK, &sigset, &sigset_oset);
   }

   /* now fork and exec the shepherd */
   if (getenv("SGE_FAILURE_BEFORE_FORK")) {
      i = -1;
   } else {
#if defined(SOLARIS)
      i = sge_smf_contract_fork(err_str, err_length);
      if (i == -4) {
          /* Could not load libcontract or libscf */
          sge_exit(1);
      } else if (i < -1) {
          i = -2; /* Disable queue */
      }
#else
      i = fork();
#endif
   }

   if (i != 0) { /* parent or -1 */
      sigprocmask(SIG_SETMASK, &sigset_oset, nullptr);

      if (petep == nullptr) {
         /* nothing to be done for petasks: We do not signal individual petasks, but always the whole jatask */
         lSetUlong64(jep, JB_hard_wallclock_gmt, 0); /* in case we are restarting! */
         lSetUlong(jatep, JAT_pending_signal, 0);
         lSetUlong64(jatep, JAT_pending_signal_delivery_time, 0);
      }

      if (chdir(execd_spool_dir))       /* go back */
         /* if this happens (don't know how) we have a real problem */
         ERROR(MSG_FILE_CHDIR_SS, execd_spool_dir, strerror(errno));
      if (i == -1) {
         if (getenv("SGE_FAILURE_BEFORE_FORK")) {
            snprintf(err_str, err_length, "FAILURE_BEFORE_FORK");
         } else {
            snprintf(err_str, err_length, MSG_EXECD_NOFORK_S, strerror(errno));
         }
      }

      DRETURN(i);
   }

   // If we are running under systemd control,
   // we need to move the shepherd out of the execd scope.
   // We create a new scope for the shepherd, e.g. "ocs-shepherd.scope".
   // The shepherd process may *not* run within the scope of running a job - it might have limits which would lead
   // to sge_shepherd being killed.
   // Moving the shepherd pid to a new scope must be done here (in the child process), otherwise the shepherd
   // might already have forked and have started job processes within the execd scope.
   // The shepherd scope is automatically deleted again when the last shepherd process exits.
   // Therefore, we need to create a new scope when the first shepherd process is started,
   // and just attach the following shepherd processes to this scope.
#if defined (OCS_WITH_SYSTEMD)
   starting_shepherd_ok = ocs::execd::execd_move_shepherd_to_scope();
#endif

   if (starting_shepherd_ok) {  /* close all fd's except 0,1,2 */
      int keep_open[3];

      keep_open[0] = 0;
      keep_open[1] = 1;
      keep_open[2] = 2;
      sge_close_all_fds(keep_open, 3);
   }

   /*
    * set KRB5CCNAME so shepherd assumes user's identify for
    * access to DFS or AFS file systems
    */
   if (starting_shepherd_ok) {
      if ((feature_is_enabled(FEATURE_DCE_SECURITY) ||
           feature_is_enabled(FEATURE_KERBEROS_SECURITY)) &&
          lGetString(jep, JB_cred)) {

         char ccname[1024];
         snprintf(ccname, sizeof(ccname), "KRB5CCNAME=FILE:/tmp/krb5cc_%s_" sge_u32, "sge", job_id);
         putenv(ccname);
      }
   }

   if (starting_shepherd_ok) {
      DPRINTF("**********************CHILD*********************\n");
      shepherd_name = SGE_SHEPHERD;
      snprintf(ps_name, sizeof(ps_name), "%s-" sge_u32, shepherd_name, job_id);

      pag_cmd = mconf_get_pag_cmd();
      shepherd_cmd = mconf_get_shepherd_cmd();
      if (shepherd_cmd && strlen(shepherd_cmd) &&
          strcasecmp(shepherd_cmd, "none")) {
         DPRINTF("CHILD - About to exec shepherd wrapper job ->%s< under queue -<%s<\n",
                 lGetString(jep, JB_job_name),
                 lGetString(master_q, QU_full_name));
         execlp(shepherd_cmd, ps_name, nullptr);
      } else if (mconf_get_do_credentials() && feature_is_enabled(FEATURE_DCE_SECURITY)) {
         DPRINTF("CHILD - About to exec DCE shepherd wrapper job ->%s< under queue -<%s<\n",
                 lGetString(jep, JB_job_name),
                 lGetString(master_q, QU_full_name));
         execlp(dce_wrapper_cmd, ps_name, nullptr);
      } else if (!feature_is_enabled(FEATURE_AFS_SECURITY) || !pag_cmd ||
                 !strlen(pag_cmd) || !strcasecmp(pag_cmd, "none")) {
         DPRINTF("CHILD - About to exec ->%s< under queue -<%s<\n",
                 lGetString(jep, JB_job_name),
                 lGetString(master_q, QU_full_name));

         if (ISTRACE)
            execlp(shepherd_path, ps_name, nullptr);
         else
            execlp(shepherd_path, ps_name, "-bg", nullptr);
      } else {
         char commandline[2048];

         DPRINTF("CHILD - About to exec PAG command job ->%s< under queue -<%s<\n",
                 lGetString(jep, JB_job_name), lGetString(master_q, QU_full_name));
         if (ISTRACE) {
            snprintf(commandline, sizeof(commandline), "exec %s", shepherd_path);
         } else {
            snprintf(commandline, sizeof(commandline), "exec %s -bg", shepherd_path);
         }

         execlp(pag_cmd, pag_cmd, "-c", commandline, nullptr);
      }
      sge_free(&pag_cmd);
      sge_free(&shepherd_cmd);
   }

   /*---------------------------------------------------*/
   /* exec() failed - do what shepherd does if it fails */
   // @todo can we set the host in error state (all queues) when starting_shepherd_ok is false?
   //       Otherwise the job will get scheduled to this host over and over again.
   fp = fopen("error", "w");
   if (fp) {
      fprintf(fp, "failed to exec shepherd for job" sge_u32"\n", job_id);
      FCLOSE(fp);
   }

   fp = fopen("exit_status", "w");
   if (fp) {
      fprintf(fp, "1\n");
      FCLOSE(fp);
   }

FCLOSE_ERROR:
   CRITICAL(SFNMAX, MSG_EXECD_NOSTARTSHEPHERD);

   exit(1);

   /* just to please insure */
   return -1;
}

/*****************************************************
 check whether a shell should be called as login shell
 *****************************************************/
static int ck_login_sh(char *shell) {
   char *cp;
   char *login_shells;
   int ret;

   DENTER(TOP_LAYER);

   login_shells = mconf_get_login_shells();

   if (login_shells == nullptr) {
      DRETURN(0);
   }

   cp = login_shells;

   while (*cp) {

      /* skip delimiters */
      while (*cp && (*cp == ',' || *cp == ' ' || *cp == '\t')) {
         cp++;
      }

      ret = strncmp(cp, shell, strlen(shell));
      DPRINTF("strncmp(\"%s\", \"%s\", %d) = %d\n", cp, shell, strlen(shell), ret);
      if (!ret) {
         sge_free(&login_shells);
         DRETURN(1);
      }

      /* skip name of shell, proceed until next delimiter */
      while (*cp && *cp != ',' && *cp != ' ' && *cp != '\t') {
         cp++;
      }
   }

   sge_free(&login_shells);

   DRETURN(0);
}


static int
get_nhosts(const lList *gdil_orig) {
   int nhosts = 0;
   const lListElem *ep;
   lList *cache = lCreateList("", STU_Type);
   const char *hostname;

   DENTER(TOP_LAYER);
   for_each_ep(ep, gdil_orig) {
      hostname = lGetHost(ep, JG_qhostname);
      if (lGetElemStr(cache, STU_name, hostname) == nullptr) {
         nhosts++;
         lAddElemStr(&cache, STU_name, hostname, STU_Type);
      }
   }
   lFreeList(&cache);

   DRETURN(nhosts);
}
