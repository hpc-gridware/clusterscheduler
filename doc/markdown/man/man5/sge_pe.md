---
title: sge_pe
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_pe - xxQS_NAMExx parallel environment configuration file format

# DESCRIPTION

Parallel environments are parallel programming and runtime environments allowing for the execution of shared memory 
or distributed memory parallelized applications. Parallel environments usually require some kind of setup to be 
operational before starting parallel applications. Examples for common parallel environments are shared memory 
parallel operating systems and the distributed memory environments Parallel Virtual Machine (PVM) or 
Message Passing Interface (MPI).

xxqs_name_sxx_pe allows for the definition of interfaces to arbitrary parallel environments. Once a parallel 
environment is defined or modified with the `-ap` or `-mp` options to qconf(1) and linked with one or more queues 
via *pe_list* in xxqs_name_sxx_queue_conf(5) the environment can be requested for a job via the `-pe` switch to
qsub(1) together with a request of a range for the number of parallel processes to be allocated by the job. 
Additional `-l` options may be used to specify the job requirement to further detail.

Note, xxQS_NAMExx allows backslashes (\\) be used to escape newline (\\newline) characters. The backslash and the 
newline are replaced with a space (" ") character before any interpretation.

# FORMAT

The format of a xxqs_name_sxx_pe file is defined as follows:

## pe_name

The name of the parallel environment as defined for *pe_name* in xxqs_name_sxx_types(1). To be used in the 
qsub(1) `-pe` switch.

## slots

The number of parallel processes being allowed to run in total under the parallel environment concurrently. 
Type is number, valid values are 0 to 9999999.

## user_lists

A comma separated list of user access list names (see xxqs_name_sxx_access_list(5)). Each user contained in at 
least one of the enlisted access lists has access to the parallel environment. If the *user_lists* parameter is 
set to NONE (the default) any user has access being not explicitly excluded via the *xuser_lists* parameter
described below. If a user is contained both in an access list enlisted in *xuser_lists* and *user_lists* the 
user is denied access to the parallel environment.

## xuser_lists

The *xuser_lists* parameter contains a comma separated list of so called user access lists as described in 
xxqs_name_sxx_access_list(5). Each user contained in at least one of the enlisted access lists is not allowed to
access the parallel environment. If the *xuser_lists* parameter is set to NONE (the default) any user has access. 
If a user is contained both in an access list enlisted in *xuser_lists* and *user_lists* the user is denied access 
to the parallel environment.

## start_proc_args

The invocation command line of a start-up procedure for the parallel environment. The start-up procedure is invoked by
xxqs_name_sxx_shepherd(8) prior to executing the job script. Its purpose is to setup the parallel environment 
correspondingly to its needs. An optional prefix "user@" specifies the user under which this procedure is to be 
started. The standard output of the start-up procedure is redirected to the file \<REQUEST>.po\<JID> in the job's
working directory (see qsub(1)), with \<REQUEST> being the name of the job as displayed by qstat(1) and \<JID> 
being the job's identification number. Likewise, the standard error output is redirected to \<REQUEST>.pe\<JID>.  
The following special variables being expanded at runtime can be used (besides any other strings which have to 
be interpreted by the start and stop procedures) to constitute a command line:

* $sge_root
  The product root directory.

* $sge_cell
  The name of the cell directory.

* $pe_hostfile  
  The pathname of a file containing a detailed description of the layout of the parallel environment to be setup by 
  the start-up procedure. Each line of the file refers to a host on which parallel processes are to be run. The first 
  entry of each line denotes the hostname, the second entry the number of parallel processes to be run on the host, 
  the third entry the name of the queue, and the fourth entry a processor range to be used in case of a multiprocessor 
  machine. The first line in the PE hostfile always refers to the master task host.
  
  Example PE hostfile contents:
  
  ```text
  execution-3.us-central1-a.c.internal 32 all.q@execution-3.us-central1-a.c.internal UNDEFINED
  execution-1.us-central1-a.c.internal 32 all.q@execution-1.us-central1-a.c.internal UNDEFINED
  execution-0.us-central1-a.c.internal 32 all.q@execution-0.us-central1-a.c.internal UNDEFINED
  execution-2.us-central1-a.c.internal 32 all.q@execution-2.us-central1-a.c.internal UNDEFINED
  ```
  
  Or with 1 slot per host:
  
  ```text
  execution-3.us-central1-a.c.internal 1 all.q@execution-3.us-central1-a.c.internal UNDEFINED
  execution-1.us-central1-a.c.internal 1 all.q@execution-1.us-central1-a.c.internal UNDEFINED
  execution-2.us-central1-a.c.internal 1 all.q@execution-2.us-central1-a.c.internal UNDEFINED
  execution-0.us-central1-a.c.internal 1 all.q@execution-0.us-central1-a.c.internal UNDEFINED
  ```

* $host  
  The name of the host on which the start-up or stop procedures are started.

* $job_owner  
  The user name of the job owner.

* $job_id  
  xxQS_NAMExx's unique job identification number.

* $job_name  
  The name of the job.

* $pe  
  The name of the parallel environment in use.

* $pe_slots  
  Number of slots granted for the job.

* $processors  
  The *processors* string as contained in the queue configuration (see xxqs_name_sxx_queue_conf(5)) of the master 
  queue (the queue in which the start-up and stop procedures are started).

* $queue  
  The cluster queue of the master queue instance.

## stop_proc_args

The invocation command line of a shutdown procedure for the parallel environment. The shutdown procedure is invoked 
by xxqs_name_sxx_shepherd(8) after the job script has finished. Its purpose is to stop the parallel environment and 
to remove it from all participating systems. An optional prefix "user@" specifies the user under which this procedure 
is to be started. The standard output of the stop procedure is also redirected to the file \<REQUEST>.po\<JID> in the
job's working directory (see qsub(1)), with \<REQUEST> being the name of the job as displayed by qstat(1) and 
\<JID> being the job's identification number. Likewise, the standard error output is redirected to \<REQUEST>.pe\<JID>  
The same special variables as for *start_proc_args* can be used to constitute a command line.

## allocation_rule

The allocation rule is interpreted by the scheduler thread and helps the scheduler to decide how to distribute 
parallel processes among the available machines. If, for instance, a parallel environment is built for shared memory 
applications only, all parallel processes have to be assigned to a single machine, no matter how much suitable 
machines are available. If, however, the parallel environment follows the distributed memory paradigm, an even 
distribution of processes among machines may be favorable.The current version of the scheduler only understands the 
following allocation rules:

* \<int>:  
  An integer number fixing the number of processes per host. If the number is 1, all processes have to reside on 
  different hosts. If the special denominator **$pe_slots** is used, the full range of processes as specified with 
  the qsub(1) `-pe` switch has to be allocated on a single host (no matter which value belonging to the range is 
  finally chosen for the job to be allocated).

* $fill_up:  
  Starting from the best suitable host/queue, all available slots are allocated. Further hosts and queues are 
  "filled up" as long as a job still requires slots for parallel tasks.

* $round_robin:  
  From all suitable hosts a single slot is allocated until all tasks requested by the parallel job are dispatched. 
  If more tasks are requested than suitable hosts are found, allocation starts again from the first host. The 
  allocation scheme walks through suitable hosts in a best-suitable-first order.

## control_slaves

This parameter can be set to TRUE or FALSE (the default). It indicates whether xxQS_NAMExx is the creator of the 
slave tasks of a parallel application via xxqs_name_sxx_execd(8) and xxqs_name_sxx_shepherd(8) and thus has full 
control over all processes in a parallel application, which enables capabilities such as resource limitation and 
correct accounting. However, to gain control over the slave tasks of a parallel application, a sophisticated PE
interface is required, which works closely together with xxQS_NAMExx facilities. Such PE interfaces are available 
through your local xxQS_NAMExx support office.

Please set the control_slaves parameter to false for all other PE interfaces.

## job_is_first_task

The *job_is_first_task* parameter can be set to TRUE or FALSE. A value of TRUE indicates that the xxQS_NAMExx job 
script already contains one of the tasks of the parallel application (the number of slots reserved for the job is 
the number of slots requested with the -pe switch), while a value of FALSE indicates that the job script (and its 
child processes) is not part of the parallel program (the number of slots reserved for the job is the number of 
slots requested with the -pe switch + 1).

If wallclock accounting is used (execd_params ACCT_RESERVED_USAGE and/or SHARETREE_RESERVED_USAGE set to TRUE) and 
*control_slaves* is set to FALSE, the *job_is_first_task* parameter influences the accounting for the job: A value 
of TRUE means that accounting for cpu and requested memory gets multiplied by the number of slots requested with 
the `-pe` switch, if *job_is_first_task* is set to FALSE, the accounting information gets multiplied by number of 
slots + 1.

## urgency_slots

For pending jobs with a slot range PE request the number of slots is not determined. This setting specifies the 
method to be used by xxQS_NAMExx to assess the number of slots such jobs might finally get.

The assumed slot allocation has a meaning when determining the resource-request-based priority contribution for 
numeric resources as described in xxqs_name_sxx_priority(5) and is displayed when qstat(1) is run without `-g t` 
option.

The following methods are supported:

* \<int>:  
  The specified integer number is directly used as prospective slot amount.

* min:  
  The slot range minimum is used as prospective slot amount. If no lower bound is specified with the range 1 is 
  assumed.

* max:  
  The slot range maximum is used as prospective slot amount. If no upper bound is specified with the range 
  the absolute maximum possible due to the PE's *slots* setting is assumed.

* avg:  
  The average of all numbers occurring within the job's PE range request is assumed.

## accounting_summary

This parameter is only checked if *control_slaves* (see above) is set to TRUE and thus xxQS_NAMExx is the creator 
of the slave tasks of a parallel application via xxqs_name_sxx_execd(8) and xxqs_name_sxx_shepherd(8). In this case, 
accounting information is available for every single slave task started by xxQS_NAMExx.

The *accounting_summary* parameter can be set to TRUE or FALSE. A value of TRUE indicates that only a single 
accounting record is written to the xxqs_name_sxx_accounting(5) file, containing the accounting summary of the
whole job including all slave tasks, while a value of FALSE indicates an
individual xxqs_name_sxx_accounting(5) record is written for every slave task, as well as for the master task.  
Note: When running tightly integrated jobs with *SHARETREE_RESERVED_USAGE* set, and with having *accounting_summary*
enabled in the parallel environment, reserved usage will only be reported by the master task of the parallel job. 
No per parallel task usage records will be sent from execd to qmaster, which can significantly reduce load on 
qmaster when running large tightly integrated parallel jobs.

## ign_sreq_on_mhost

This parameter can be used to define the scheduling behavior for parallel jobs which are submitted with the `-scope` 
submit option, see qsub(1).

By default, *ign_sreq_on_mhost* is set to FALSE. This means that slave tasks which are running besides the master 
task on the master host must fulfill the resource requirements specified in the slave scope request. For consumable 
slave requests enough capacity needs to be available and the slave tasks consume the requested amount from consumable 
resources.

There are situations where the master task requires multiple slots and its resource requirements are well known and 
either no slave tasks are started on the master host or they are started as part of the master task, as sub processes 
or as threads.
In this case *ign_sreq_on_mhost* can be set to TRUE. This means that on the master host only global and master 
requests need to be fulfilled, slave requests are ignored.
Slave tasks on the master host will not consume any consumable resources except for slots.

In order for queue limits to be correctly applied to the master task *ign_sreq_on_mhost* set to TRUE
should be combined with *master_forks_slaves* set to TRUE.

## master_forks_slaves

This parameter is only checked if *control_slaves* (see above) is set to TRUE and thus xxQS_NAMExx is the creator 
of the slave tasks of a parallel application via xxqs_name_sxx_execd(8) and xxqs_name_sxx_shepherd(8).

Slave tasks of tightly integrated parallel jobs are usually started by calling
`qrsh -inherit <slave_host> <command>` on the master host.

Usually applications either start every slave task individually with a separate `qrsh -inherit` call  
or the master task starts slave tasks on the master host per fork/exec or as threads of the master process.

If slave tasks on the master host are started individually then keep the setting of *master_forks_slaves* as 
FALSE (default). If slave tasks on the master host are started via fork/exec or as threads
then set *master_forks_slaves* to TRUE.

The setting of *master_forks_slaves* influences the behavior of the xxqs_name_sxx_execd(8):
If *master_forks_slaves* is set to TRUE, no slave tasks can be started with `qrsh -inherit` on the master host  
and limits set for the master task (the job script) will be multiplied by the number of slots allocated for the 
job on the master host.

## daemon_forks_slaves

This parameter is only checked if *control_slaves* (see above) is set to TRUE and thus xxQS_NAMExx is the creator 
of the slave tasks of a parallel application via xxqs_name_sxx_execd(8) and xxqs_name_sxx_shepherd(8).

Slave tasks of tightly integrated parallel jobs are usually started by calling
`qrsh -inherit <slave_host> <command>` on the master host.

Depending on the application, either slave tasks are started individually on the slave hosts with a separate 
`qrsh -inherit` call per task. Or a single process is started per slave host which then forks/execs the slave tasks 
or starts them as threads.

Default setting of *daemon_forks_slaves* is FALSE. Use this setting if slave tasks are started
individually on the slave hosts. If a single process is started per slave host which then forks/execs the 
slave tasks or starts them as threads then set *daemon_forks_slaves* to TRUE.

The setting of *daemon_forks_slaves* influences the behavior of the xxqs_name_sxx_execd(8): If *daemon_forks_slaves* 
is set to TRUE, only a single task (the daemon) can be started with `qrsh -inherit` and limits set for the one task 
(the daemon) will be multiplied by the number of slots allocated for the job on the slave host.

# RESTRICTIONS

Note, that the functionality of the start-up, shutdown and signaling procedures remains the full responsibility 
of the administrator configuring the parallel environment. xxQS_NAMExx will just invoke these procedures and 
evaluate their exit status. If the procedures do not perform their tasks properly or if the parallel environment 
or the parallel application behave unexpectedly, xxQS_NAMExx has no means to detect this.

# SEE ALSO

xxqs_name_sxx_intro(1), xxqs_name_sxx__types(1), qconf(1), qdel(1), qmod(1), qsub*(1), xxqs_name_sxx_access_list(5),
xxqs_name_sxx_qmaster*(8), xxqs_name_sxx_shepherd(8).

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
