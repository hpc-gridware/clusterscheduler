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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <cerrno>

#include "uti/sge_log.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_spool.h"
#include "uti/sge_time.h"
#include "uti/sge_unistd.h"

#include "cull/cull_file.h"
#include "cull/cull_list.h"

#include "sgeobj/sge_str.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_suser.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_object.h"

#include "spool/sge_dirent.h"

#include "basis_types.h"
#include "msg_common.h"
#include "msg_spoollib_classic.h"
#include "read_write_job.h"
#include "sge_job_qmaster.h"
#include "uti/sge.h"

static lList *ja_task_list_create_from_file(u_long32 job_id, 
                                            u_long32 ja_task_id,
                                            sge_spool_flags_t flags);

static lListElem *ja_task_create_from_file(u_long32 job_id,
                                           u_long32 ja_task_id,
                                           const char *pe_task_id,
                                           sge_spool_flags_t flags);

static int ja_task_write_to_disk(lListElem *ja_task, u_long32 job_id,
                                 const char *pe_task_id,
                                 sge_spool_flags_t flags); 

static int job_write_ja_task_part(lListElem *job, u_long32 ja_task_id,
                                  const char *pe_task_id,
                                  sge_spool_flags_t flags);

static int job_write_as_single_file(const lListElem *job, u_long32 ja_task_id,
                                   sge_spool_flags_t flags);

static lListElem *job_create_from_file(u_long32 job_id, u_long32 task_id,
                                       sge_spool_flags_t flags);    

static int job_has_to_spool_one_file(const lListElem *job,
                                     const lList *pe_list,
                                     sge_spool_flags_t flags);

static lListElem *pe_task_create_from_file(u_long32 job_id,
                                           u_long32 ja_task_id,
                                           const char *pe_task_id,
                                           sge_spool_flags_t flags);

static int job_remove_script_file(u_long32 job_id);

/* Here we cache the path of the last task spool dir that has been created.
   In case a task spool dir is removed the cache is no longer a proof of the
   existence of the task spool dir and is reinitialized */
static char old_task_spool_dir[SGE_PATH_MAX] = "";

static lListElem *job_create_from_file(u_long32 job_id, u_long32 ja_task_id,
                                       sge_spool_flags_t flags)
{
   lListElem *job = nullptr;
   char spool_path[SGE_PATH_MAX] = "";

   DENTER(TOP_LAYER);

   sge_get_file_path(spool_path, sizeof(spool_path), JOB_SPOOL_DIR, FORMAT_DEFAULT, flags, job_id, ja_task_id, nullptr);

   if (sge_is_directory(spool_path)) {
      char spool_path_common[SGE_PATH_MAX];
      lList *ja_tasks = nullptr;

      sge_get_file_path(spool_path_common, sizeof(spool_path_common), JOB_SPOOL_FILE, FORMAT_DEFAULT,
                        flags, job_id, ja_task_id, nullptr);
      job = lReadElemFromDisk(nullptr, spool_path_common, JB_Type, "job");
      if (job) {
         ja_tasks = ja_task_list_create_from_file(job_id, ja_task_id, flags); 
         if (ja_tasks) {
            lList *ja_task_list = lGetListRW(job, JB_ja_tasks);
            if (ja_task_list) {
               lAddList(ja_task_list, &ja_tasks);
            } else {
               lSetList(job, JB_ja_tasks, ja_tasks);
            }
            ja_tasks = nullptr;
            lPSortList(ja_tasks, "%I+", JAT_task_number); 
         } else {
            /*
             * This is no error! It only means that there is no enrolled
             * task in the spool area (all tasks are unenrolled)
             */
         }
      }
   } else {
      job = lReadElemFromDisk(nullptr, spool_path, JB_Type, "job");
   }
   DRETURN(job);
}

static lList *ja_task_list_create_from_file(u_long32 job_id, 
                                            u_long32 ja_task_id,
                                            sge_spool_flags_t flags)
{
   lList *dir_entries = nullptr;
   lList *ja_task_entries = nullptr;
   lList *pe_task_entries = nullptr;
   lList *ja_tasks = nullptr;
   lList *pe_tasks = nullptr;
   const lListElem *dir_entry;
   char spool_dir_job[SGE_PATH_MAX];
   DENTER(TOP_LAYER);

   ja_tasks = lCreateList("ja_tasks", JAT_Type); 
   if (!ja_tasks) {
      DTRACE;
      goto error;
   }
   sge_get_file_path(spool_dir_job, sizeof(spool_dir_job), JOB_SPOOL_DIR, FORMAT_DEFAULT, flags, job_id, ja_task_id, nullptr);
   dir_entries = sge_get_dirents(spool_dir_job);
   for_each_ep(dir_entry, dir_entries) {
      const char *entry;
 
      entry = lGetString(dir_entry, ST_name);
      if (strcmp(entry, ".") && strcmp(entry, "..") && 
          strcmp(entry, "common")) {
         char spool_dir_tasks[SGE_PATH_MAX];
         const lListElem *ja_task_entry;

         snprintf(spool_dir_tasks, sizeof(spool_dir_tasks), SFN "/" SFN, spool_dir_job, entry);
         ja_task_entries = sge_get_dirents(spool_dir_tasks);
         for_each_ep(ja_task_entry, ja_task_entries) {
            const char *ja_task_string;

            ja_task_string = lGetString(ja_task_entry, ST_name);
            if (strcmp(ja_task_string, ".") && strcmp(ja_task_string, "..")) {
               char spool_dir_pe_tasks[SGE_PATH_MAX];
               const lListElem *pe_task_entry;
               u_long32 ja_task_id;
               lListElem *ja_task;

               ja_task_id = atol(ja_task_string);
               if (ja_task_id == 0) {
                  DTRACE;
                  goto error;
               }
               snprintf(spool_dir_pe_tasks, sizeof(spool_dir_pe_tasks), SFN "/" SFN, spool_dir_tasks,
                       ja_task_string);

               if (sge_is_directory(spool_dir_pe_tasks)) {
                  char spool_path_ja_task[SGE_PATH_MAX];

                  sge_get_file_path(spool_path_ja_task, sizeof(spool_path_ja_task), TASK_SPOOL_FILE,
                                    FORMAT_DEFAULT, flags, job_id, ja_task_id, nullptr);
                  ja_task = lReadElemFromDisk(nullptr, spool_path_ja_task, JAT_Type, "ja_task");
                  pe_tasks = nullptr;
                  pe_task_entries = sge_get_dirents(spool_dir_pe_tasks);
                  for_each_ep(pe_task_entry, pe_task_entries) {
                     const char *pe_task_string;

                     pe_task_string = lGetString(pe_task_entry, ST_name);
                     if (strcmp(pe_task_string, ".") && 
                         strcmp(pe_task_string, "..") &&
                         strcmp(pe_task_string, "common")) {
                        lListElem *pe_task;
                        
                        pe_task = pe_task_create_from_file(job_id, ja_task_id, pe_task_string, flags);
                        if (pe_task) {
                           if (!pe_tasks) {
                              pe_tasks = lCreateList("pe_tasks", PET_Type); 
                           }
                           lAppendElem(pe_tasks, pe_task);
                        } else {
                           DTRACE;
                           goto error;
                        }
                     }
                  }
                  lFreeList(&pe_task_entries);
                  lSetList(ja_task, JAT_task_list, pe_tasks);
               } else {
                  ja_task = ja_task_create_from_file(job_id, ja_task_id, nullptr, flags);
               }
               if (ja_task) {
                  lAppendElem(ja_tasks, ja_task);
               } else {
                  DTRACE;
                  goto error;
               }
            }
         } 
         lFreeList(&ja_task_entries);
      }
   }
   lFreeList(&dir_entries);

   if (!lGetNumberOfElem(ja_tasks)) {
      DTRACE;
      goto error; 
   } 
   DRETURN(ja_tasks);
error:
   lFreeList(&ja_tasks);
   lFreeList(&dir_entries);
   lFreeList(&ja_task_entries);  
   lFreeList(&pe_task_entries);
   DRETURN(nullptr);
}

static lListElem *ja_task_create_from_file(u_long32 job_id, 
                                           u_long32 ja_task_id, 
                                           const char *pe_task_id,
                                           sge_spool_flags_t flags) 
{
   lListElem *ja_task;
   char spool_path_ja_task[SGE_PATH_MAX];

   sge_get_file_path(spool_path_ja_task, sizeof(spool_path_ja_task), TASK_SPOOL_DIR_AS_FILE,
                     FORMAT_DEFAULT, flags, job_id, ja_task_id, nullptr);
   ja_task = lReadElemFromDisk(nullptr, spool_path_ja_task, JAT_Type, "ja_task");
   return ja_task;
}

static lListElem *pe_task_create_from_file(u_long32 job_id,
                                           u_long32 ja_task_id,
                                           const char *pe_task_id,
                                           sge_spool_flags_t flags)
{
   lListElem *pe_task;
   char spool_path_pe_task[SGE_PATH_MAX];

   sge_get_file_path(spool_path_pe_task, sizeof(spool_path_pe_task), PE_TASK_SPOOL_FILE,
                     FORMAT_DEFAULT, flags, job_id, ja_task_id, pe_task_id);
   pe_task = lReadElemFromDisk(nullptr, spool_path_pe_task, PET_Type, "pe_task");
   return pe_task;
   
}

/****** spool/classic/job_write_spool_file() **********************************
*  NAME
*     job_write_spool_file() -- makes a job/task persistent 
*
*  SYNOPSIS
*     int job_write_spool_file(lListElem *job, u_long32 ja_taskid, 
*                              sge_spool_flags_t flags) 
*
*  FUNCTION
*     This function writes a job or a task of an array job into the spool 
*     area. It may be used within the qmaster or execd code.
*   
*     The result from this function looks like this within the spool area
*     of the master for the job 10001, the array job 10002.1-3, 
*     the tightly integrated job 20011 (two pe_tasks).
*     
*   
*     $SGE_ROOT/default/spool/qmaster/jobs
*     +---00
*         +---0001
*         |   +---0001                     (JB_Type file)
*         |   +---0002 
*         |       +---common               (JB_Type without JB_ja_tasks)
*         |       +---1-4096
*         |           +---1                (JAT_Type file) 
*         |           +---2                (JAT_Type file)
*         |           +---3                (JAT_Type file)
*         +---0002
*             +---0011
*                 +---common               (JB_Type without JB_ja_tasks)
*                 +---1-4096
*                     +---1
*                         +--- common      (JAT_Type file witout JAT_task_list)
*                         +--- 1.speedy    (PET_Type file)
*                         +--- 2.speedy    (PET_Type file)
*                         +--- past_usage  (PET_Type file)
*
*  INPUTS
*     lListElem *job          - full job (JB_Type) 
*     u_long32 ja_taskid      - 0 or a allowed array job task id 
*     const char *pe_task_id  - pe task id
*     sge_spool_flags_t flags - where/how should we spool the object 
*        SPOOL_HANDLE_AS_ZOMBIE   -> has to be used for zombie jobs 
*        SPOOL_WITHIN_EXECD       -> has to be used within the execd 
*        SPOOL_DEFAULT            -> if no other flags are needed
*
*  RESULT
*     int - 0 on success otherwise != 0 
******************************************************************************/
int job_write_spool_file(lListElem *job, u_long32 ja_taskid,
                         const char *pe_task_id,
                         sge_spool_flags_t flags) 
{
   int ret = 0;
   int report_long_delays = flags & SPOOL_WITHIN_EXECD;
   u_long32 start = 0;
   
   DENTER(TOP_LAYER);

   if (report_long_delays) {
      start = sge_get_gmt();
   }

   if (job_has_to_spool_one_file(job, *object_type_get_master_list(SGE_TYPE_PE), flags)) {
      ret = job_write_as_single_file(job, ja_taskid, flags); 
   } else {
      if (!(flags & SPOOL_ONLY_JATASK) && !(flags & SPOOL_ONLY_PETASK)) {
         ret = job_write_common_part(job, ja_taskid, flags);
      }
      if (!ret && !(flags & SPOOL_IGNORE_TASK_INSTANCES)) {
         ret = job_write_ja_task_part(job, ja_taskid, pe_task_id, flags); 
      }
   }

   if (report_long_delays) {
      u_long32 time = sge_get_gmt() - start;
      if (time > 30) {
         /* administrators need to be aware of suspicious spooling delays */
         WARNING(MSG_CONFIG_JOBSPOOLINGLONGDELAY_UUI, sge_u32c(lGetUlong(job, JB_job_number)), sge_u32c(ja_taskid), (int)time);
      }
   }

   DRETURN(ret);
}

static int job_has_to_spool_one_file(const lListElem *job, 
                                     const lList *pe_list,
                                     sge_spool_flags_t flags) 
{
   DENTER(TOP_LAYER);

   if ((flags & SPOOL_HANDLE_AS_ZOMBIE) || (flags & SPOOL_WITHIN_EXECD)) {
      DRETURN(1);
   } 
   
   if (job_might_be_tight_parallel(job, pe_list) || (job_get_submit_ja_tasks(job) > sge_get_ja_tasks_per_file())) {
      DRETURN(0);
   }

   DRETURN(1);
}

static int job_write_as_single_file(const lListElem *job, u_long32 ja_task_id,
                                   sge_spool_flags_t flags) 
{
   int ret = 0;
   u_long32 job_id;
   char job_dir_third[SGE_PATH_MAX] = "";
   char spool_file[SGE_PATH_MAX] = "";
   char tmp_spool_file[SGE_PATH_MAX] = "";

   DENTER(TOP_LAYER);
   job_id = lGetUlong(job, JB_job_number);

   sge_get_file_path(job_dir_third, sizeof(job_dir_third), JOB_SPOOL_DIR, FORMAT_THIRD_PART, flags, job_id, ja_task_id, nullptr);
   sge_mkdir(job_dir_third, 0755, false, false);
   sge_get_file_path(spool_file, sizeof(spool_file), JOB_SPOOL_DIR_AS_FILE, FORMAT_DEFAULT, flags, job_id, ja_task_id, nullptr);
   sge_get_file_path(tmp_spool_file, sizeof(tmp_spool_file), JOB_SPOOL_DIR_AS_FILE, FORMAT_DOT_FILENAME, flags, job_id, ja_task_id, nullptr);
   ret = lWriteElemToDisk(job, tmp_spool_file, nullptr, "job");
   if (!ret && (rename(tmp_spool_file, spool_file) == -1)) {
      ret = 1;
   }
   DRETURN(ret);
}

static int job_write_ja_task_part(lListElem *job, u_long32 ja_task_id,
                                  const char *pe_task_id,
                                  sge_spool_flags_t flags)
{
   lListElem *ja_task, *next_ja_task;
   u_long32 job_id;
   int ret = 0;
   DENTER(TOP_LAYER); 

   job_id = lGetUlong(job, JB_job_number);
   if (ja_task_id != 0) {
      next_ja_task = lGetElemUlongRW(lGetList(job, JB_ja_tasks), JAT_task_number, ja_task_id);
   } else {
      next_ja_task = lFirstRW(lGetList(job, JB_ja_tasks));
   }
   while ((ja_task = next_ja_task)) {
      if (ja_task_id != 0) {
         next_ja_task = nullptr;
      } else {
         next_ja_task = lNextRW(ja_task);
      }

      if ((flags & SPOOL_WITHIN_EXECD) ||
         job_is_enrolled(job, lGetUlong(ja_task, JAT_task_number))) {
         if (job_might_be_tight_parallel(job, *object_type_get_master_list(SGE_TYPE_PE))) {
            flags = (sge_spool_flags_t)((int)flags | SPOOL_HANDLE_PARALLEL_TASKS);
         }

         ret = ja_task_write_to_disk(ja_task, job_id, pe_task_id, flags);
         if (ret) {
            DTRACE;
            break;
         }
      }
   }
   DRETURN(ret);
}

int job_write_common_part(lListElem *job, u_long32 ja_task_id,
                                 sge_spool_flags_t flags) 
{
   int ret = 0;
   u_long32 job_id;
   char spool_dir[SGE_PATH_MAX];
   char spoolpath_common[SGE_PATH_MAX], tmp_spoolpath_common[SGE_PATH_MAX];
   lList *ja_tasks;

   DENTER(TOP_LAYER);

   job_id = lGetUlong(job, JB_job_number);
   sge_get_file_path(spool_dir, sizeof(spool_dir), JOB_SPOOL_DIR, FORMAT_DEFAULT, flags, job_id, ja_task_id, nullptr);
   sge_mkdir(spool_dir, 0755, false, false);
   sge_get_file_path(spoolpath_common, sizeof(spoolpath_common), JOB_SPOOL_FILE, FORMAT_DEFAULT, flags, job_id, ja_task_id, nullptr);
   sge_get_file_path(tmp_spoolpath_common, sizeof(tmp_spoolpath_common), JOB_SPOOL_FILE, FORMAT_DOT_FILENAME, flags, job_id, ja_task_id, nullptr);

   ja_tasks = nullptr;
   lXchgList(job, JB_ja_tasks, &ja_tasks);
   ret = lWriteElemToDisk(job, tmp_spoolpath_common, nullptr, "job");
   lXchgList(job, JB_ja_tasks, &ja_tasks);

   if (!ret && (rename(tmp_spoolpath_common, spoolpath_common) == -1)) {
      DTRACE;
      ret = 1;
   }

   DRETURN(ret);
}



static int ja_task_write_to_disk(lListElem *ja_task, u_long32 job_id,
                                 const char *pe_task_id,
                                 sge_spool_flags_t flags)
{
   int handle_pe_tasks = flags & SPOOL_HANDLE_PARALLEL_TASKS;
   int ret = 0;
   DENTER(TOP_LAYER);

   /* this is a tightly integrated parallel job */
   if (handle_pe_tasks) {
      char task_spool_dir[SGE_PATH_MAX];
      char task_spool_file[SGE_PATH_MAX];
      char tmp_task_spool_file[SGE_PATH_MAX];
      lListElem *pe_task = nullptr;
      lListElem *next_pe_task = nullptr;
      u_long32 ja_task_id = lGetUlong(ja_task, JAT_task_number);
      const lList *pe_task_list = lGetList(ja_task, JAT_task_list);

      sge_get_file_path(task_spool_dir, sizeof(task_spool_dir), TASK_SPOOL_DIR, FORMAT_DEFAULT, flags, job_id, ja_task_id, nullptr);
      sge_get_file_path(task_spool_file, sizeof(task_spool_file), TASK_SPOOL_FILE, FORMAT_DEFAULT, flags, job_id, ja_task_id, nullptr);
      sge_get_file_path(tmp_task_spool_file, sizeof(tmp_task_spool_file), TASK_SPOOL_FILE, FORMAT_DOT_FILENAME, flags, job_id, ja_task_id, nullptr);

      /* create task spool directory if necessary */
      if ((flags & SPOOL_WITHIN_EXECD) || 
          strcmp(old_task_spool_dir, task_spool_dir)) {
         strcpy(old_task_spool_dir, task_spool_dir);
         sge_mkdir(task_spool_dir, 0755, false, false);
      }

      /* spool ja_task */
      if (!(flags & SPOOL_ONLY_PETASK)) {
         lList *tmp_task_list = nullptr;

         /* we do *not* want to spool the pe_tasks together with the ja_task */
         lXchgList(ja_task, JAT_task_list, &tmp_task_list);
         /* spool ja_task to temporary file */
         ret = lWriteElemToDisk(ja_task, tmp_task_spool_file, nullptr, "ja_task");
         lXchgList(ja_task, JAT_task_list, &tmp_task_list);
         /* make spooling permanent = COMMIT */
         if (!ret && (rename(tmp_task_spool_file, task_spool_file) == -1)) {
            DTRACE;
            goto error;
         }
      }

      /* spool pe_task(s) */
      if (!(flags & SPOOL_ONLY_JATASK)) {
         if (pe_task_id != 0) {
            next_pe_task = lGetElemStrRW(pe_task_list, PET_id, pe_task_id);
         } else {
            next_pe_task = lFirstRW(pe_task_list);
         }
         while ((pe_task = next_pe_task)) {
            char pe_task_spool_file[SGE_PATH_MAX];
            char tmp_pe_task_spool_file[SGE_PATH_MAX];
            const char* pe_task_id_string = lGetString(pe_task, PET_id);

            if (pe_task_id != 0) {
               next_pe_task = nullptr;
            } else {
               next_pe_task = lNextRW(pe_task);
            }

            sge_get_file_path(pe_task_spool_file, sizeof(pe_task_spool_file), PE_TASK_SPOOL_FILE, FORMAT_DEFAULT,
                              flags, job_id, ja_task_id, pe_task_id_string);
            sge_get_file_path(tmp_pe_task_spool_file, sizeof(tmp_pe_task_spool_file), PE_TASK_SPOOL_FILE,
                              FORMAT_DOT_FILENAME, flags, job_id, ja_task_id, pe_task_id_string);

            /* spool pe_task to temporary file */
            ret = lWriteElemToDisk(pe_task, tmp_pe_task_spool_file, nullptr, "pe_task");
            /* make spooling permanent = COMMIT */
            if (!ret && (rename(tmp_pe_task_spool_file, pe_task_spool_file) == -1)) {
               DTRACE;
               goto error;
            }
      
            DTRACE;
         }
      }
   } else {
      /* this is a non tightly parallel array task */
      char task_spool_dir[SGE_PATH_MAX];
      char task_spool_file[SGE_PATH_MAX];
      char tmp_task_spool_file[SGE_PATH_MAX];

      sge_get_file_path(task_spool_dir, sizeof(task_spool_dir), TASKS_SPOOL_DIR, FORMAT_DEFAULT, flags,
                        job_id, lGetUlong(ja_task, JAT_task_number), nullptr);
      sge_get_file_path(task_spool_file, sizeof(task_spool_file), TASK_SPOOL_DIR_AS_FILE,
                        FORMAT_DEFAULT, flags, job_id, lGetUlong(ja_task, JAT_task_number), nullptr);
      sge_get_file_path(tmp_task_spool_file, sizeof(tmp_task_spool_file), TASK_SPOOL_DIR_AS_FILE,
                        FORMAT_DOT_FILENAME, flags, job_id, lGetUlong(ja_task, JAT_task_number), nullptr);

      /* create task spool directory if necessary */
      if ((flags & SPOOL_WITHIN_EXECD) || strcmp(old_task_spool_dir, task_spool_dir)) {
         strcpy(old_task_spool_dir, task_spool_dir);
         sge_mkdir(task_spool_dir, 0755, false, false);
      }

      /* spool ja_task to temporary file */
      ret = lWriteElemToDisk(ja_task, tmp_task_spool_file, nullptr, "ja_task");
      /* make spooling permanent = COMMIT */
      if (!ret && (rename(tmp_task_spool_file, task_spool_file) == -1)) {
         DTRACE;
         goto error;
      }    
   }

error:
   DRETURN(ret);
}

int job_remove_spool_file(u_long32 jobid, u_long32 ja_taskid, 
                          const char *pe_task_id,
                          sge_spool_flags_t flags)
{
   char spool_dir[SGE_PATH_MAX] = "";
   char spool_dir_second[SGE_PATH_MAX] = "";
   char spool_dir_third[SGE_PATH_MAX] = "";
   char spoolpath_common[SGE_PATH_MAX] = "";
   int within_execd = flags & SPOOL_WITHIN_EXECD;
   int handle_as_zombie = flags & SPOOL_HANDLE_AS_ZOMBIE;
   int one_file;
   const lList *master_list = handle_as_zombie ? 
                        *object_type_get_master_list(SGE_TYPE_ZOMBIE) : 
                        *object_type_get_master_list(SGE_TYPE_JOB);
   lListElem *job = lGetElemUlongRW(master_list, JB_job_number, jobid);
   int try_to_remove_sub_dirs = 0;
   dstring error_msg;
   char error_msg_buffer[SGE_PATH_MAX];

   DENTER(TOP_LAYER);

   sge_dstring_init(&error_msg, error_msg_buffer, sizeof(error_msg_buffer));

   one_file = job_has_to_spool_one_file(job, *object_type_get_master_list(SGE_TYPE_PE), flags);
   if (ja_taskid != 0 && pe_task_id != nullptr && !one_file) {
       char pe_task_spool_file[SGE_PATH_MAX];

       sge_get_file_path(pe_task_spool_file, sizeof(pe_task_spool_file), PE_TASK_SPOOL_FILE,
                         FORMAT_DEFAULT, flags, jobid, ja_taskid, pe_task_id);
      
       DPRINTF(("try to remove " SFN "\n", pe_task_spool_file));
       if (sge_is_file(pe_task_spool_file) && !sge_unlink(nullptr, pe_task_spool_file)) {
          ERROR(MSG_JOB_CANNOT_REMOVE_SS, MSG_JOB_PE_TASK_SPOOL_FILE, pe_task_spool_file);
      }
   }

   if (ja_taskid != 0 && pe_task_id == nullptr && !one_file) {
      char task_spool_dir[SGE_PATH_MAX];
      char task_spool_file[SGE_PATH_MAX];
      int remove_task_spool_file = 0;

      sge_get_file_path(task_spool_dir, sizeof(task_spool_dir), TASKS_SPOOL_DIR, FORMAT_DEFAULT, flags, jobid, ja_taskid, nullptr);
      sge_get_file_path(task_spool_file, sizeof(task_spool_file), TASK_SPOOL_DIR_AS_FILE, FORMAT_DEFAULT, flags, jobid, ja_taskid, nullptr);

      if (within_execd) {
         remove_task_spool_file = 1;
      } else {
         remove_task_spool_file = job_is_enrolled(job, ja_taskid);
      }
      DPRINTF(("remove_task_spool_file = %d\n", remove_task_spool_file));;

      if (remove_task_spool_file) {
         DPRINTF(("removing " SFN "\n", task_spool_file));
         if (sge_is_directory(task_spool_file)) {
            if (sge_rmdir(task_spool_file, &error_msg)) {
               ERROR(MSG_JOB_CANNOT_REMOVE_SS, MSG_JOB_TASK_SPOOL_FILE, error_msg_buffer);
            } 
         } else {
            if (!sge_unlink(nullptr, task_spool_file)) {
               ERROR(MSG_JOB_CANNOT_REMOVE_SS, MSG_JOB_TASK_SPOOL_FILE, task_spool_file);
            }
         }

         /*
          * Following sge_rmdir call may fail. We can ignore this error.
          * This is only an indicator that another task is running which has 
          * been spooled in the directory.
          */  
         DPRINTF(("try to remove " SFN "\n", task_spool_dir));
         if (sge_rmdir(task_spool_dir, &error_msg)) {
            ERROR(MSG_JOB_CANNOT_REMOVE_SS, MSG_JOB_TASK_SPOOL_FILE, error_msg_buffer);
         } 

         /* 
          * a task spool directory has been removed: reinit 
          * old_task_spool_dir to ensure mkdir() is performed 
          */
         old_task_spool_dir[0] = '\0';
      }
   }

   sge_get_file_path(spool_dir, sizeof(spool_dir), JOB_SPOOL_DIR, FORMAT_DEFAULT, flags, jobid, ja_taskid, nullptr);
   sge_get_file_path(spool_dir_third, sizeof(spool_dir_third), JOB_SPOOL_DIR, FORMAT_THIRD_PART, flags, jobid, ja_taskid, nullptr);
   sge_get_file_path(spool_dir_second, sizeof(spool_dir_second), JOB_SPOOL_DIR, FORMAT_SECOND_PART, flags, jobid, ja_taskid, nullptr);
   sge_get_file_path(spoolpath_common, sizeof(spoolpath_common), JOB_SPOOL_FILE, FORMAT_DEFAULT, flags, jobid, ja_taskid, nullptr);
   try_to_remove_sub_dirs = 0;
   if (!one_file) {
      if (ja_taskid == 0) { 
         DPRINTF(("removing " SFN "\n", spoolpath_common));
         if (!sge_unlink(nullptr, spoolpath_common)) {
            ERROR(MSG_JOB_CANNOT_REMOVE_SS, MSG_JOB_JOB_SPOOL_FILE, spoolpath_common);
         }
         DPRINTF(("removing " SFN "\n", spool_dir));
         if (sge_rmdir(spool_dir, nullptr)) {
            ERROR(MSG_JOB_CANNOT_REMOVE_SS, MSG_JOB_JOB_SPOOL_DIRECTORY, spool_dir);
         }
         try_to_remove_sub_dirs = 1;
      }
   } else {
      DPRINTF(("removing " SFN "\n", spool_dir));
      if (!sge_unlink(nullptr, spool_dir)) {
         ERROR(MSG_JOB_CANNOT_REMOVE_SS, MSG_JOB_JOB_SPOOL_FILE, spool_dir);
      }
      try_to_remove_sub_dirs = 1;
   }
   /*
    * Following sge_rmdir calls may fail. We can ignore these errors.
    * This is only an indicator that another job is running which has been
    * spooled in the same directory.
    */
   if (try_to_remove_sub_dirs) {
      DPRINTF(("try to remove " SFN "\n", spool_dir_third));

      if (!sge_rmdir(spool_dir_third, nullptr)) {
         DPRINTF(("try to remove " SFN "\n", spool_dir_second));
         sge_rmdir(spool_dir_second, nullptr);
      }
   }

   DRETURN(0);
}

static int job_remove_script_file(u_long32 job_id)
{
   char script_file[SGE_PATH_MAX] = "";
   int ret = 0;
   DENTER(TOP_LAYER);

   PROF_START_MEASUREMENT(SGE_PROF_JOBSCRIPT);
   sge_get_file_path(script_file, sizeof(script_file), JOB_SCRIPT_FILE, FORMAT_DEFAULT, SPOOL_DEFAULT, job_id, 0, nullptr);
   if (unlink(script_file)) {
      if (errno!=ENOENT) {
         ERROR(MSG_CONFIG_FAILEDREMOVINGSCRIPT_SS, strerror(errno), script_file);
         DTRACE;
         ret = 1;
      }
   } else {
      INFO(MSG_CONFIG_REMOVEDSCRIPTOFBADJOBFILEX_S, script_file);
   }
   PROF_STOP_MEASUREMENT(SGE_PROF_JOBSCRIPT);
   DRETURN(ret);
}

int job_list_read_from_disk(lList **job_list, const char *list_name, int check,
                            sge_spool_flags_t flags, int (*init_function)(lListElem*)) 
{
   char first_dir[SGE_PATH_MAX] = ""; 
   lList *first_direnties; 
   lListElem *first_direntry;
   DSTRING_STATIC(dstr_path, SGE_PATH_MAX);
   const char *str_path;
   int handle_as_zombie = (flags & SPOOL_HANDLE_AS_ZOMBIE) > 0;
   lList *master_suser_list = *object_type_get_master_list_rw(SGE_TYPE_SUSER);

   DENTER(TOP_LAYER); 
   sge_get_file_path(first_dir, sizeof(first_dir), JOBS_SPOOL_DIR, FORMAT_FIRST_PART, flags, 0, 0, nullptr);
   first_direnties = sge_get_dirents(first_dir);

   if (first_direnties && !sge_silent_get()) {
      printf(MSG_CONFIG_READINGIN_S, list_name);
      printf("\n");
   }

   sge_status_set_type(STATUS_DOTS);
   for (; (first_direntry = lFirstRW(first_direnties)); lRemoveElem(first_direnties, &first_direntry)) {
      char second_dir[SGE_PATH_MAX] = "";
      lList *second_direnties;
      lListElem *second_direntry;
      const char *first_entry_string;


      first_entry_string = lGetString(first_direntry, ST_name);
      str_path = sge_dstring_sprintf(&dstr_path, "%s/%s", first_dir, first_entry_string);
      if (!sge_is_directory(str_path)) {
         ERROR(MSG_CONFIG_NODIRECTORY_S, str_path);
         break;
      }
   
      snprintf(second_dir, sizeof(second_dir), SFN "/" SFN, first_dir, first_entry_string);
      second_direnties = sge_get_dirents(second_dir);
      for (; (second_direntry = lFirstRW(second_direnties)); lRemoveElem(second_direnties, &second_direntry)) {
         char third_dir[SGE_PATH_MAX] = "";
         lList *third_direnties;
         lListElem *third_direntry;
         const char *second_entry_string;

         second_entry_string = lGetString(second_direntry, ST_name);
         str_path = sge_dstring_sprintf(&dstr_path, "%s/%s/%s", first_dir, first_entry_string,
                 second_entry_string);
         if (!sge_is_directory(str_path)) {
            ERROR(MSG_CONFIG_NODIRECTORY_S, str_path);
            break;
         } 

         snprintf(third_dir, sizeof(third_dir), SFN "/" SFN, second_dir, second_entry_string);
         third_direnties = sge_get_dirents(third_dir);
         for (; (third_direntry = lFirstRW(third_direnties)); lRemoveElem(third_direnties, &third_direntry)) {
            lListElem *job;
            const lListElem *ja_task;
            char *lasts = nullptr;
            char job_dir[SGE_PATH_MAX] = "";
            char fourth_dir[SGE_PATH_MAX] = "";
            char job_id_string[SGE_PATH_MAX] = "";
            char *ja_task_id_string;
            u_long32 job_id, ja_task_id;
            int all_finished;

            sge_status_next_turn();
            snprintf(fourth_dir, sizeof(fourth_dir), SFN "/" SFN, third_dir,
                    lGetString(third_direntry, ST_name));
            snprintf(job_id_string, sizeof(job_id_string), SFN SFN SFN, 
                    lGetString(first_direntry, ST_name),
                    lGetString(second_direntry, ST_name),
                    lGetString(third_direntry, ST_name)); 
            job_id = (u_long32) strtol(job_id_string, nullptr, 10);
            strtok_r(job_id_string, ".", &lasts);
            ja_task_id_string = strtok_r(nullptr, ".", &lasts);
            if (ja_task_id_string) {
               ja_task_id = (u_long32) strtol(ja_task_id_string, nullptr, 10);
            } else {
               ja_task_id = 0;
            }
            sge_get_file_path(job_dir, sizeof(job_dir), JOB_SPOOL_DIR, FORMAT_DEFAULT, flags, job_id, ja_task_id, nullptr);

            /* check directory name */
            if (strcmp(fourth_dir, job_dir)) {
               fprintf(stderr, "%s %s\n", fourth_dir, job_dir);
               DPRINTF(("Invalid directory " SFN "\n", fourth_dir));
               continue;
            }

            /* read job */
            job = job_create_from_file(job_id, ja_task_id, flags);
            if (!job) {
               job_remove_script_file(job_id);
               continue;
            }

            /* check for scriptfile before adding job */
            all_finished = 1;
            for_each_ep(ja_task, lGetList(job, JB_ja_tasks)) {
               if (lGetUlong(ja_task, JAT_status) != JFINISHED) {
                  all_finished = 0;
                  break;
               }
            }
            if (check && !all_finished && lGetString(job, JB_script_file)) {
               char script_file[SGE_PATH_MAX];
               SGE_STRUCT_STAT stat_buffer;

               sge_get_file_path(script_file, sizeof(script_file), JOB_SCRIPT_FILE, FORMAT_DEFAULT, flags, job_id, 0, nullptr);
               if (SGE_STAT(script_file, &stat_buffer)) {
                  ERROR(MSG_CONFIG_CANTFINDSCRIPTFILE_U, sge_u32c(lGetUlong(job, JB_job_number)));
                  job_list_add_job(object_type_get_master_list_rw(SGE_TYPE_JOB), "job list", job, 0);
                  job_remove_spool_file(job_id, 0, nullptr, SPOOL_DEFAULT);
                  lRemoveElem(*object_type_get_master_list_rw(SGE_TYPE_JOB), &job);
                  continue;
               }
            }  
 
            /* check if filename has same name which is stored job id */
            if (lGetUlong(job, JB_job_number) != job_id) {
               ERROR(MSG_CONFIG_JOBFILEXHASWRONGFILENAMEDELETING_U, sge_u32c(job_id));
               job_remove_spool_file(job_id, 0, nullptr, flags);
               /* 
                * script is not deleted here, 
                * since it may belong to a valid job 
                */
            } 

            if (init_function) {
               init_function(job);
            }

            lSetList(job, JB_jid_successor_list, nullptr);
            job_list_add_job(job_list, list_name, job, 0);
            
            if (!handle_as_zombie) {
               job_list_register_new_job(*object_type_get_master_list(SGE_TYPE_JOB), mconf_get_max_jobs(), 1);
               suser_register_new_job(job, mconf_get_max_u_jobs(), 1, master_suser_list);
            }
         }
         lFreeList(&third_direnties);
      }
      lFreeList(&second_direnties);
   } 
   lFreeList(&first_direnties);

   if (*job_list) {
      sge_status_end_turn();
   }      

   DRETURN(0);
}
