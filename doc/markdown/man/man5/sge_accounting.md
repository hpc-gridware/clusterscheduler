---
title: sge_accounting
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_accounting - xxQS_NAMExx accounting file format

# DESCRIPTION

An accounting record is written to the xxQS_NAMExx accounting file for each job having finished. The accounting 
file is processed by qacct(1) to derive accounting statistics.

Two different file formats are supported: One-line JSON format (JSONL) and colon-separated format (deprecated).
The default format is JSONL.
See [FORMAT](#FORMAT) below for details.

Accounting records can contain the following items of information:

## *account*

An account string as specified by the `qsub` or `qalter` `-A` option.

## *arid*

Advance reservation identifier. If the job used resources of an advance reservation, then this field contains a
positive integer identifier; otherwise the value is 0.

## *ar_submission_time*
The time when the advance reservation was submitted (GMT unix time stamp, JSONL: in microseconds,
0 if the job was not running in an advance reservation).

## *category*

A string specifying the job category.

## *cpu*

The cpu time usage in seconds.

## *department*

The department which was assigned to the job.

## *end_time*

End time (GMT unix time stamp, JSONL: in microseconds).

## *exit_status*

Exit status of the job script (or xxQS_NAMExx specific status in case of certain error conditions). The exit status
is determined by following the normal shell conventions. If the command terminates normally, the value of the command
is its exit status. However, in the case that the command exits abnormally, a value of 0200 (octal), 128 (decimal) is
added to the value of the command to make up the exit status.

For example, if a job dies through signal 9 (SIGKILL) then the exit status becomes 128 + 9 = 137.

## *failed*

Indicates the problem which occurred in case a job could not be started on the execution host (e.g., because the
owner of the job did not have a valid account on that machine). If xxQS_NAMExx tries to start a job multiple times,
this may lead to multiple entries in the accounting file corresponding to the same job ID.

## *granted_pe*

The parallel environment which was selected for that job.

## *group*

The effective group id of the job owner when executing the job.

## *hostname*

Name of the execution host.

## *io*

The amount of data transferred in input/output operations.

## *ioops*

The number of input/output operations.

## *iow*

The io wait time in seconds.

## *job_name*

Job name.

## *job_number*

Job identifier - job number.

## maxpss

The maximum pss (proportional set size) in bytes.
Available only on Linux when `execd_params ENABLE_MEM_DETAILS=TRUE`.

## *maxrss*

The maximum rss (resident set size) in bytes.

(JSONL only)

## *maxvmem*

The maximum vmem size in bytes.

## *mem*

The integral memory usage in Gbytes cpu seconds.

## *owner*

Owner of the xxQS_NAMExx job.

## *pe_taskid*

If this identifier is set the task was part of a tightly integrated parallel job and was passed to xxQS_NAMExx via the
`qrsh -inherit` interface.

## *priority*

Priority value assigned to the job corresponding to the *priority* parameter in the queue configuration
(see xxqs_name_sxx_queue_conf(5)).

## *project*

The project which was assigned to the job.

## *qname*

Name of the cluster queue in which the job has run.

## *ru_wallclock*

Difference between *end_time* and *start_time* (see above).

The remainder of the accounting entries follow the contents of the standard UNIX rusage structure as described in
getrusage(2). Depending on the operating system where the job was executed, some fields may be 0. The following
entries are provided:

    ru_utime
    ru_stime
    ru_maxrss
    ru_ixrss
    ru_ismrss
    ru_idrss
    ru_isrss
    ru_minflt
    ru_majflt
    ru_nswap
    ru_inblock
    ru_oublock
    ru_msgsnd
    ru_msgrcv
    ru_nsignals
    ru_nvcsw
    ru_nivcsw

## *slots*

The number of slots which were dispatched to the job by the scheduler.

## *start_time*

Start time (GMT unix time stamp, JSONL: in microseconds).

## *submission_time*

Submission time (GMT unix time stamp, JSONL: in microseconds).

## *task_number*

Array job task index number.

## *wallclock*

The wallclock time usage in seconds.

(JSONL only)


# FORMAT

Two possible formats are supported:
* A new one-line JSON format (JSONL) which allows for easy extensibility and more flexible processing of the accounting data.
* The old colon separated format known from SGE (deprecated).

## One Line JSON Format (JSONL)

Beginning with xxQS_NAMExx 9.0.0 the accounting file by default is written in a one-line
JSON format (JSONL).

The file path is `$SGE_ROOT/$SGE_CELL/common/accounting.jsonl`.

Values are contained in the following structure and order:

* job_number
* task_number
* start_time
* end_time
* owner
* group
* account
* qname
* hostname
* department
* slots
* arid (optional, when the job was running in an advance reservation)
* job_name
* priority
* submission_time
* submit_cmd_line
* ar_submission_time (optional, when the job was running in an advance reservation)
* pe_task_id (optional, when the accounting record is for a task of a tightly integrated parallel job)
* category
* failed
* exit_status
* usage - array containing all rusage values
   * rusage
   * ru_wallclock
   * ru_utime
   * ru_stime
   * ru_maxrss
   * ru_ixrss
   * ru_ismrss
   * ru_idrss
   * ru_isrss
   * ru_minflt
   * ru_majflt
   * ru_nswap
   * ru_inblock
   * ru_oublock
   * ru_msgsnd
   * ru_msgrcv
   * ru_nsignals
   * ru_nvcsw
   * ru_nivcsw
* eusage - array containing all usage values gathered by sge_execd
   * wallclock
   * cpu
   * mem
   * io
   * ioops
   * iow
   * maxvmem
   * maxrss
   * maxpss - optional
* optionaly further usage containers (arrays) containing custom usage, see xxqs_name_sxx_conf(5), `reporting_params`, `usage_patterns`.

## Colon Separated Format

For compatibility reasons, the accounting file can be written in the colon-separated format. This format is still 
supported but is deprecated and will be removed in a future release.

To enable the colon separated format add `old_accounting=true` to the `reporting_params` in the global 
configuration.

The file path is `$SGE_ROOT/$SGE_CELL/common/accounting`.

The accounting file is a text file.  
Each line in the file represents an accounting record entry.  
Accounting record entries are separated by colon (':') signs.  
Empty lines and lines which contain one character or less are ignored.  

The entries denote in their order of appearance:

* qname
* hostname
* group
* owner
* job_name
* job_number
* account
* priority
* submission_time
* start_time
* end_time
* failed
* exit_status
* ru_wallclock
* ru_utime
* ru_stime
* ru_maxrss
* ru_ixrss
* ru_ismrss
* ru_idrss
* ru_isrss
* ru_minflt
* ru_majflt
* ru_nswap
* ru_inblock
* ru_oublock
* ru_msgsnd
* ru_msgrcv
* ru_nsignals
* ru_nvcsw
* ru_nivcsw
* project
* department
* granted_pe
* slots
* task_number
* cpu
* mem
* io
* category
* iow
* pe_task_id (an empty string if the accounting record is not for a task of a tightly integrated parallel job)
* maxvmem
* arid (0 if the job was not running in an advance reservation)
* ar_submission_time (0 if the job was not running in an advance reservation)


# SEE ALSO

xxqs_name_sxx_intro(1), qacct(1), qalter(1), qsub(1), getrusage(2), xxqs_name_sxx_conf(5), xxqs_name_sxx_queue_conf(5), xxqs_name_sxx_reporting(5).

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
