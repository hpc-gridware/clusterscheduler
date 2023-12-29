---
title: qmod
section: 1
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

qmod - modify a xxQS_NAMExx queue and running job

# SYNTAX

**qmod** \[ **options** \] \[ **wc_job_range_list** \| **wc_queue_list**
\]

# DESCRIPTION

*Qmod* enables users classified as *owners* (see *queue_conf* (5) for
details) of a workstation to modify the state of xxQS_NAMExx queues for
his/her machine as well as the state of his/her own jobs. A
manager/operator or root can execute *qmod* for any queue and job in a
cluster but only from administrative hosts. Find additional information
concerning *wc_queue_list* and *wc_job_list* in *sge_types* (1).

# OPTIONS

-c  
**Note:** Deprecated, may be removed in future release. Please use the
**-cj** or **-cq** switch instead.  
Clears the error state of the specified jobs(s)/queue(s).

-cj  
Clears the error state of the specified jobs(s).

-cq  
Clears the error state of the specified queue(s).

-d  
Disables the queue(s), i.e. no further jobs are dispatched to disabled
queues while jobs already executing in these queues are allowed to
finish.

-e  
Enables the queue(s).

-f  
Force the modification action for the queue despite the apparent current
state of the queue. For example if a queue appears to be suspended but
the job execution seems to be continuing the manager/operator can force
a suspend operation which will send a SIGSTOP to the jobs. In any case,
the queue or job status will be set even if the
*xxqs_name_sxx_execd* (8) controlling the queues/jobs cannot be reached.
Requires manager/operator privileges.

-help  
Prints a listing of all options.

-r  
**Note:** Deprecated, may be removed in future release. Please use the
**-rj** or **-rq** switch instead.  
If applied to queues, reschedules all jobs currently running in this
queue. If applied to running jobs, reschedules the jobs. Requires root
or manager privileges. In order for a job to be rescheduled, it or the
queue in which it is executing must have the rerun flag activated. (See
-r option in the *qsub* (1) man page and the *rerun* option in the
*queue_conf* (5) man page for more information.) Additional restrictions
apply for parallel and checkpointing jobs. (See the *reschedule_unknown*
description in the *sge_conf* (5) man page for details).

-rj  
If applied to running jobs, reschedules the jobs. Requires root or
manager privileges.

-rq  
If applied to queues, reschedules all jobs currently running in this
queue. Requires root or manager privileges.

-s  
**Note:** Deprecated, may be removed in future release. Please use the
**-sj** or **-sq** switch instead.  
If applied to queues, suspends the queues and any jobs which might be
active. If applied to running jobs, suspends the jobs.

-sj  
If applied to running jobs, suspends the jobs. If a job is both
suspended explicitly and via suspension of its queue, a following
unsuspend of the queue will not release the suspension state on the job.

-sq  
If applied to queues, suspends the queues and any jobs which might be
active.

-us  
**Note:** Deprecated, may be removed in future release. Please use the
**-usj** or **-usq** switch instead.  
If applied to queues, unsuspends the queues and any jobs which might be
active. If applied to jobs, unsuspends the jobs.

-usj  
If applied to jobs, unsuspends the jobs. If a job is both suspended
explicitly and via suspension of its queue, a following unsuspend of the
queue will not release the suspension state on the job.

-usq  
If applied to queues, unsuspends the queues and any jobs which might be
active.

# ENVIRONMENTAL VARIABLES

xxQS_NAME_Sxx_ROOT  
Specifies the location of the xxQS_NAMExx standard configuration files.

xxQS_NAME_Sxx_CELL  
If set, specifies the default xxQS_NAMExx cell. To address a xxQS_NAMExx
cell *qmod* uses (in the order of precedence):

> The name of the cell specified in the environment variable
> xxQS_NAME_Sxx_CELL, if it is set.
>
> The name of the default cell, i.e. **default**.

xxQS_NAME_Sxx_DEBUG_LEVEL  
If set, specifies that debug information should be written to stderr. In
addition the level of detail in which debug information is generated is
defined.

xxQS_NAME_Sxx_QMASTER_PORT  
If set, specifies the tcp port on which *xxqs_name_sxx_qmaster* (8) is
expected to listen for communication requests. Most installations will
use a services map entry for the service "sge_qmaster" instead to define
that port.

# FILES

    <xxqs_name_sxx_root>/<cell>/common/act_qmaster
    	xxQS_NAMExx master host file

# SEE ALSO

*xxqs_name_sxx_intro* (1), *xxqs_name_sxx_ckpt* (1), *qstat* (1),
*queue_conf* (5), *xxqs_name_sxx_execd* (8), *xxqs_name_sxx_types* (1).

# COPYRIGHT

See *xxqs_name_sxx_intro* (1) for a full statement of rights and
permissions.
