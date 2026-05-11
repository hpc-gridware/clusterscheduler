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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
/****************************************************************
 Structures for architecture depended data
 ****************************************************************/

#include <string>
#include <vector>

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "uti/sge_dstring.h"

struct all_drsuage {
   char *arch;
};

typedef struct all_drsuage sge_all_rusage_type;

struct necsx_drusage {
   char *arch;
   uint32_t base_prty;            /* base priority */
   uint32_t time_slice;           /* timeslice value */
   uint32_t num_procs;            /* number of processes */
   uint32_t kcore_min;            /* amount of memory usage */
   uint32_t mean_size;            /* mean memory size */
   uint32_t maxmem_size;          /* maximum memory size */
   uint32_t chars_trnsfd;         /* number of characters transfered */
   uint32_t blocks_rw;            /* total blocks read and written */
   uint32_t inst;                 /* number of instructions */
   uint32_t vector_inst;          /* number of vector instructions */
   uint32_t vector_elmt;          /* number of vector elements */
   uint32_t vec_exe;              /* execution time of vector instr */
   uint32_t flops;                /* FLOPS value */
   uint32_t concurrent_flops;     /* concurrent FLOPS value */
   uint32_t fpec;                 /* floating point data execution element
                                       count */
   uint32_t cmcc;                 /* cache miss time */
   uint32_t bccc;                 /* bank conflict time */
   uint32_t mt_open;              /* MT open counts */
   uint32_t io_blocks; /* device  I/O blocks (DSK, ADK (array disk),  XMU,
                           MASSDPS (mass data processing system disk),
                           SCD (SCSI  disk), QT (1/4" CGMT), HCT (1/2" CGMT),
                           DT (DAT), ET (8mm CGMT), MT (1/2" MT),
                           SMT (SCSI tape), IMT (other MT device connected
                           to IOX) and HMT (HIPPI MT). */
   uint32_t multi_single;         /* multitask or single task */
   uint32_t max_nproc;            /* maximum process counts */
};

typedef struct necsx_drusage sge_necsx_rusage_type;

/****************************************************************
 Structure used for reporting about the end of a job.
 ****************************************************************/
struct drusage {
   char *qname;
   char *hostname;
   char *group;   /* the user's UNIX group on the exec host */
   char *owner;    /* owner of the job we report about */
   char *project;  /* project of the job we report about */
   char *department; /* department of the job we report about */
   char *job_name; /* -N switch or script_file or "STDIN" */
   char *account;         /* accounting string see -A switch */
   uint32_t failed;           /* != 0 -> this job failed,
                                 see states in execution_states.h */
   uint32_t general_failure;  /* != 0 execd reports "can execute no job",
                                 also see above header */
   char *err_str;         /* error string if this job is canceled
                                 abnormaly */
   uint32_t priority;         /* priority of job */
   uint32_t job_number;
   uint32_t task_number;      /* job-array task number */
   const char *pe_taskid;     /* in case of tasks of a parallel job: the pe_taskid, else nullptr */
   uint64_t submission_time;
   const char *submission_command_line;
   uint64_t start_time;
   uint64_t end_time;
   uint32_t exit_status;
   uint32_t signal;
   double ru_wallclock;
   double ru_utime;      /* user time used */
   double ru_stime;      /* system time used */
   uint32_t ru_maxrss;
   uint32_t ru_ixrss;      /* integral shared text size */
   uint32_t ru_ismrss;     /* integral shared memory size*/
   uint32_t ru_idrss;      /* integral unshared data " */
   uint32_t ru_isrss;      /* integral unshared stack " */
   uint32_t ru_minflt;     /* page reclaims */
   uint32_t ru_majflt;     /* page faults */
   uint32_t ru_nswap;      /* swaps */
   uint32_t ru_inblock;    /* block input operations */
   uint32_t ru_oublock;    /* block output operations */
   uint32_t ru_msgsnd;     /* messages sent */
   uint32_t ru_msgrcv;     /* messages received */
   uint32_t ru_nsignals;   /* signals received */
   uint32_t ru_nvcsw;      /* voluntary context switches */
   uint32_t ru_nivcsw;     /* involuntary " */
   uint32_t pid;
   char *granted_pe;
   uint32_t slots;
   char *hard_resources_list;
   char *hard_queue_list;
   double wallclock;
   double cpu;
   double mem;
   double io;
   double ioops;
   double iow;
   double maxvmem;
   double maxrss;
   double maxpss;
   uint32_t ar;
   sge_all_rusage_type *arch_dep_usage;/* pointer to a structure with
                                          architecture dependend usage
                                          information */
   lList *other_usage;
   bool is_classic;  /* true if data was read from classic (colon-separated) accounting format */
};

typedef struct drusage sge_rusage_type;

/*
 * name of the usage record that will hold the time when the last intermediate
 * record has been written
 */
#define LAST_INTERMEDIATE "im_acct_time"

/*
 * time window in minutes after midnight in which intermediate usage will be
 * written - we needn't do all the checks for intermediate usage reporting
 * at any time of the day.
 */
#define INTERMEDIATE_ACCT_WINDOW 10

/*
 * minimum runtime of a job in seconds as prerequisit for the writing of
 * intermediate usage reporting - it's not worth writing an intermediate usage
 * record for jobs that have started some seconds before midnight.
 */
#define INTERMEDIATE_MIN_RUNTIME 60

bool
sge_write_rusage(dstring *buffer, rapidjson::Writer<rapidjson::StringBuffer> *writer, lListElem *jr, lListElem *job,
                 lListElem *ja_task, const char *category_str, std::vector<std::pair<std::string, std::string>> *usage_patterns, const char delimiter,
                 bool intermediate, bool is_reporting);
