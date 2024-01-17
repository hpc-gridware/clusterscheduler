---
title: qdel
section: 1
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

`qdel` - delete xxQS_NAMExx jobs from queues

# SYNTAX

`qdel` \[`-f`\] \[`-help`\] \[`-u` *wc_user_list*\] \[*wc_job_range_list*\] \[`-t` *task_id_range*\]

# DESCRIPTION

`qdel` provides a means for a user/operator/manager to delete one or more jobs. A manager/operator can delete jobs 
belonging to any user, while a regular user can only delete his or her own jobs. If a manager wants to delete another 
user's job, the manager can specify the job id. If the manager is using a job name or pattern, he or she must also
specify the user's name via `-u` *wc_user_list*. A `qdel` *wc_job_name* will delete only the jobs of the calling user 
by default. Find additional information concerning *wc_user_list* and *wc_job_list* in sge_types(1).

# OPTIONS

## -f  
Force deletion of job(s). The job(s) are deleted from the list of jobs registered at xxqs_name_sxx_qmaster(8) 
even if the xxqs_name_sxx_execd(8) controlling the job(s) does not respond to the delete forwarded by 
xxqs_name_sxx_qmaster(8).

Users which are neither xxQS_NAMExx managers nor operators can only use the `-f` option (for their own jobs) if 
the cluster configuration entry *qmaster_params* contains the flag *ENABLE_FORCED_QDEL* (see
xxqs_name_sxx_conf(5)). However, behavior for administrative and non-administrative users differs. Jobs are deleted 
from the xxQS_NAMExx database immediately in case of administrators. Otherwise, a regular deletion is attempted 
first and a forced cancellation is only executed if the regular deletion was unsuccessful.

Additionally regular `qdel` requests can result in a forced deletion of a job if *ENABLE_FORCED_QDEL_IF_UNKNOWN* is 
set in the *qmaster_params* (see xxqs_name_sxx_conf(5))

## -help  
Prints a listing of all options.

## -t  
Deletes specified tasks of array job. It means tasks created by `qsub -t` command. For example after creating 
array job by command `qsub -t 1-100 $SGE_ROOT/examples/sleeper.sh` it is possible to delete tasks 5-10 from
job array by command `qdel job_id -t 5-10`. All other tasks (1-4 and 11-100) will be executed.

## -u *wc_user_list* 
Deletes only those jobs which were submitted by users specified in the list of usernames. For managers, it is 
possible to use `qdel -u "\*"` to delete all jobs of all users. If a manager wants to delete a specific job of 
a user, he has to specify the user and the job. If no job is specified all jobs from that user are deleted.

## *wc_job_range_list*
A list of jobs, which should be deleted. Find details in 

# ENVIRONMENTAL VARIABLES

For a complete list of common environment variables used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# FILES

For a complete list of common environment variables used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# SEE ALSO

xxqs_name_sxx_intro(1), qdel(1), qstat(1), qsub(1), xxqs_name_sxx_qmaster(8), xxqs_name_sxx_execd(8).

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
