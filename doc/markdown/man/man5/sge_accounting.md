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

An accounting record is written to the xxQS_NAMExx accounting file for
each job having finished. The accounting file is processed by
*qacct*(1) to derive accounting statistics.

# FORMAT

Each job is represented by a line in the accounting file. Empty lines
and lines which contain one character or less are ignored. Accounting
record entries are separated by colon (':') signs. The entries denote in
their order of appearance:

## **qname**

Name of the cluster queue in which the job has run.

## **hostname**

Name of the execution host.

## **group**

The effective group id of the job owner when executing the job.

## **owner**

Owner of the xxQS_NAMExx job.

## **job_name**

Job name.

## **job_number**

Job identifier - job number.

## **account**

An account string as specified by the *qsub*(1) or *qalter*(1) **-A**
option.

## **priority**

Priority value assigned to the job corresponding to the **priority**
parameter in the queue configuration (see *xxqs_name_sxx_queue_conf*(5)).

## **submission_time**

Submission time (GMT unix time stamp).

## **start_time**

Start time (GMT unix time stamp).

## **end_time**

End time (GMT unix time stamp).

## **failed**

Indicates the problem which occurred in case a job could not be started
on the execution host (e.g. because the owner of the job did not have a
valid account on that machine). If xxQS_NAMExx tries to start a job
multiple times, this may lead to multiple entries in the accounting file
corresponding to the same job ID.

## **exit_status**

Exit status of the job script (or xxQS_NAMExx specific status in case of
certain error conditions). The exit status is determined by following
the normal shell conventions. If the command terminates normally the
value of the command is its exit status. However, in the case that the
command exits abnormally, a value of 0200 (octal), 128 (decimal) is
added to the value of the command to make up the exit status.

> For example: If a job dies through signal 9 (SIGKILL) then the exit
> status becomes 128 + 9 = 137.

## **ru_wallclock**

Difference between **end_time** and **start_time** (see above).

The remainder of the accounting entries follows the contents of the
standard UNIX rusage structure as described in *getrusage*(2).
Depending on the operating system where the job was executed some of the
fields may be 0. The following entries are provided:

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

## **project**

The project which was assigned to the job.

## **department**

The department which was assigned to the job.

## **granted_pe**

The parallel environment which was selected for that job.

## **slots**

The number of slots which were dispatched to the job by the scheduler.

## **task_number**

Array job task index number.

## **cpu**

The cpu time usage in seconds.

## **mem**

The integral memory usage in Gbytes cpu seconds.

## **io**

The amount of data transferred in input/output operations.

## **category**

A string specifying the job category.

## **iow**

The io wait time in seconds.

## **pe_taskid**

If this identifier is set the task was part of a parallel job and was
passed to xxQS_NAMExx via the qrsh -inherit interface.

## **maxvmem**

The maximum vmem size in bytes.

## **arid**

Advance reservation identifier. If the job used resources of an advance
reservation then this field contains a positive integer identifier
otherwise the value is "**0**" .

## **ar_submission_time**

If the job used resources of an advance reservation then this field
contains the submission time (GMT unix time stamp) of the advance
reservation, otherwise the value is "**0**" .

# SEE ALSO

*xxqs_name_sxx_intro*(1), *qacct*(1), *qalter*(1), *qsub*(1),
*getrusage*(2), *xxqs_name_sxx_queue_conf*(5).

# COPYRIGHT

See *xxqs_name_sxx_intro*(1) for a full statement of rights and
permissions.
