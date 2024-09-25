---
title: sge_shepherd
section: 8
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

`xxqs_name_sxx_shepherd` - xxQS_NAMExx single job controlling agent

# SYNOPSIS

`xxqs_name_sxx_shepherd`

# DESCRIPTION

`xxqs_name_sxx_shepherd` provides the parent process functionality for a single xxQS_NAMExx job. The parent 
functionality is necessary on UNIX systems to retrieve resource usage information (see getrusage(2)) after a job 
has finished. In addition, the `xxqs_name_sxx_shepherd` forwards signals to the job, such as the signals for 
suspension, enabling, termination and the xxQS_NAMExx checkpointing signal (see xxqs_name_sxx_ckpt(1) for details).

The `xxqs_name_sxx_shepherd` receives information about the job to be started from the xxqs_name_sxx_execd(8). 
During the execution of the job it actually starts up to 5 child processes. First a prolog script is
run if this feature is enabled by the *prolog* parameter in the cluster configuration. (See xxqs_name_sxx_conf(5)). 
Next a parallel environment startup procedure is run if the job is a parallel job. (See xxqs_name_sxx_pe(5) for 
more information.) After that, the job itself is run, followed by a parallel environment shutdown procedure for 
parallel jobs, and finally an epilog script if requested by the *epilog* parameter in the cluster configuration. 
The prolog and epilog scripts as well as the parallel environment startup and shutdown procedures are to be provided
by the xxQS_NAMExx administrator and are intended for site-specific actions to be taken before and after 
execution of the actual user job.

After the job has finished and the epilog script is processed, `xxqs_name_sxx_shepherd` retrieves resource usage 
statistics about the job, places them in a job specific subdirectory of the xxqs_name_sxx_execd(8) spool directory 
for reporting through xxqs_name_sxx_execd(8) and finishes.

`xxqs_name_sxx_shepherd` also places an exit status file in the spool directory. This exit status can be viewed with 
`qacct -j JobId` (see qacct(1)); it is not the exit status of `xxqs_name_sxx_shepherd` itself but of one of the methods 
executed by `xxqs_name_sxx_shepherd`.
This exit status can have several meanings, depending on in which method an error occurred (if any). The possible 
methods are: prolog, parallel start, job, parallel stop, epilog, suspend, restart, terminate, clean, migrate, and 
checkpoint.

The following exit values are returned:

* 0 - All methods: Operation was executed successfully.

* 1 - Job script, prolog and epilog: When *FORBID_RESCHEDULE* is not set in the configuration (see 
  xxqs_name_sxx_conf(5)), the job gets re-queued. Otherwise, see "Other".

* 2 - Job script, prolog and epilog: When *FORBID_APPERROR* is not set in the configuration (see 
  xxqs_name_sxx_conf(5)), the job gets re-queued. Otherwise, see "Other".

* Other  
  - Job script: This is the exit status of the job itself. No action is taken upon this exit status because the 
    meaning of this exit status is not known.  
  - Prolog, epilog and parallel start: The queue is set to error state and the job is re-queued.  
  - Parallel stop: The queue is set to error state, but the job is not re-queued. It is assumed that the job itself 
    ran successfully and only the clean up script failed.  
  - Suspend, restart, terminate, clean, and migrate: Always successful.  
  - Checkpoint: Success, except for kernel checkpointing: checkpoint was not successful, did not happen (but migration 
    will happen by xxQS_NAMExx).

# RESTRICTIONS

`xxqs_name_sxx_shepherd` should not be invoked manually, but only by xxqs_name_sxx_execd(8).

# FILES

**sgepasswd contains a list of user names and their** corresponding
encrypted passwords. If available, the password file will be used by
**sge_shepherd. To change the contents ** of this file please use the
**sgepasswd command. It is not advised to change ** that file manually.

## <execd_spool>/job_dir/<job_id>	
job specific directory

# SEE ALSO

xxqs_name_sxx_intro(1), xxqs_name_sxx_conf(5), xxqs_name_sxx_execd(8).

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
