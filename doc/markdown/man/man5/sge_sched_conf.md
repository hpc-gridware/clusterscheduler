---
title: sge_sched_conf
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_sched_conf - xxQS_NAMExx default scheduler configuration file

# DESCRIPTION

*sched_conf* defines the configuration file format for xxQS_NAMExx's scheduler. In order to modify the 
configuration use qconf(1) `-msconf` command. A default configuration is provided together with the
xxQS_NAMExx distribution package.

Note, xxQS_NAMExx allows backslashes (\\) be used to escape newline (\\newline) characters. The backslash and the 
newline are replaced with a space (" ") character before any interpretation.

# FORMAT

The following parameters are recognized by the xxQS_NAMExx scheduler if present in *sched_conf*:

## algorithm

Note: Deprecated, may be removed in future release. Allows for the selection of alternative scheduling algorithms.

Currently *default* is the only allowed setting.

## load_formula

A simple algebraic expression used to derive a single weighted load value from all or part of the load parameters 
reported by xxqs_name_sxx_execd(8) for each host and from all or part of the consumable resources (see 
xxqs_name_sxx_complex(5)) being maintained for each host. The load formula expression syntax is that of a 
summation weighted load values, that is:

    {w1|load_val1[*w1]}[{+|-}{w2|load_val2[*w2]}[{+|-}...]]

Note, no blanks are allowed in the load formula. The load values and consumable resources (load_val1, ...) are 
specified by the name defined in the complex (see xxqs_name_sxx_complex(5)).  
Note: Administrator defined load values (see the *load_sensor* parameter in xxqs_name_sxx_conf(5) for details)
and consumable resources available for all hosts (see xxqs_name_sxx_complex(5)) may be used as
well as xxQS_NAMExx default load parameters.  
The weighting factors (w1, ...) are positive integers. After the expression is evaluated for each host the results 
are assigned to the hosts and are used to sort the hosts corresponding to the weighted load.
The sorted host list is used to sort queues subsequently. The default load formula is "np_load_avg".

## job_load_adjustments

The load, which is imposed by the xxQS_NAMExx jobs running on a system varies in time, and often, e.g. for the 
CPU load, requires some amount of time to be reported in the appropriate quantity by the operating system. 
Consequently, if a job was started very recently, the reported load may not provide a sufficient representation of 
the load which is already imposed on that host by the job. The reported load will adapt to the real load over time, 
but the period of time, in which the reported load is too low, may already lead to an oversubscription of that host.
xxQS_NAMExx allows the administrator to specify *job_load_adjustments* which are used in the xxQS_NAMExx scheduler 
to compensate for this problem.  
The *job_load_adjustments* are specified as a comma separated list of arbitrary load parameters or consumable 
resources and (separated by an equal sign) an associated load correction value. Whenever a job is dispatched to a 
host by the scheduler, the load parameter and consumable value set of that host is increased by the values provided 
in the *job_load_adjustments* list. These correction values are decayed linearly over time until after 
*load_adjustment_decay_time* from the start the corrections reach the value 0. If the *job_load_adjustments*
list is assigned the special denominator NONE, no load corrections are performed.  
The adjusted load and consumable values are used to compute the combined and weighted load of the hosts with the 
*load_formula* (see above) and to compare the load and consumable values against the load threshold lists defined 
in the queue configurations (see xxqs_name_sxx_queue_conf(5)). If the *load_formula* consists simply of the default 
CPU load average parameter *np_load_avg*, and if the jobs are very compute intensive, one might want to set the 
*job_load_adjustments* list to *np_load_avg=1.00*, which means that every new job dispatched to a host
will require 100 % CPU time, and thus the machine's load is instantly increased by 1.00.

## load_adjustment_decay_time

The load corrections in the *job_load_adjustments* list above are decayed linearly over time from the point of the 
job start, where the corresponding load or consumable parameter is raised by the full correction value, until after 
a time period of *load_adjustment_decay_time*, where the correction becomes 0. Proper values for 
*load_adjustment_decay_time* greatly depend upon the load or consumable parameters used and the specific 
operating system(s). Therefore, they can only be determined on-site and experimentally. For the default 
*np_load_avg* load parameter a *load_adjustment_decay_time* of 7 minutes has proven to yield reasonable results.

## maxujobs

The maximum number of jobs any user may have running in a xxQS_NAMExx cluster at the same time. If set to 0 (default) 
the users may run an arbitrary number of jobs.

## schedule_interval

At the time the scheduler thread initially registers at the event master thread in xxqs_name_sxx_qmaster(8) process 
*schedule_interval* is used to set the time interval in which the event master thread sends scheduling event updates 
to the scheduler thread. A scheduling event is a status change that has occurred within xxqs_name_sxx_qmaster(8)
which may trigger or affect scheduler decisions (e.g. a job has finished and thus the allocated resources are 
available again). In the xxQS_NAMExx default scheduler the arrival of a scheduling event report triggers a scheduler 
run. The scheduler waits for event reports otherwise.  
*schedule_interval* is a time value (see xxqs_name_sxx_queue_conf(5) for a definition of the syntax of time values).

## queue_sort_method

This parameter determines in which order several criteria are taken into account to product a sorted queue list. 
Currently, two settings are valid: *seqno* and *load*. However in both cases, xxQS_NAMExx
attempts to maximize the number of soft requests (see qsub(1) `-s` option) being fulfilled by the queues for a 
particular as the primary criterion.  
Then, if the *queue_sort_method* parameter is set to *seqno*, xxQS_NAMExx will use the *seq_no* parameter as 
configured in the current queue configurations (see xxqs_name_sxx_queue_conf(5)) as the next criterion to sort 
the queue list. The *load_formula* (see above) has only a meaning if two queues have equal sequence numbers. If
*queue_sort_method* is set to *load* the load according the*load_formula* is the criterion after maximizing a 
job's soft requests and the sequence number is only used if two hosts have the same load. The sequence number 
sorting is most useful if you want to define a fixed order in which queues are to be filled (e.g. the cheapest 
resource first).

The default for this parameter is *load*.

## halftime

When executing under a share based policy, the scheduler "ages" (i.e. decreases) usage to implement a sliding 
window for achieving the share entitlements as defined by the share tree. The *halftime* defines the
time interval in which accumulated usage will have been decayed to half its original value. Valid values are 
specified in hours or according to the time format as specified in xxqs_name_sxx_queue_conf(5). If the value is set to 0, 
the usage is not decayed.

## usage_weight_list

xxQS_NAMExx accounts for the consumption of the resources CPU-time, memory and IO to determine the usage which is 
imposed on a system by a job. A single usage value is computed from these three input parameters by multiplying 
the individual values by weights and adding them up. The weights are defined in the *usage_weight_list*. 
The format of the list is

    cpu=wcpu,mem=wmem,io=wio

where wcpu, wmem and wio are the configurable weights. The weights are real number. The sum of all tree weights 
should be 1.

## compensation_factor

Determines how fast xxQS_NAMExx should compensate for past usage below of above the share entitlement defined 
in the share tree. Recommended values are between 2 and 10, where 10 means faster compensation.

## weight_user

The relative importance of the user shares in the functional policy. Values are of type real.

## weight_project

The relative importance of the project shares in the functional policy. Values are of type real.

## weight_department

The relative importance of the department shares in the functional policy. Values are of type real.

## weight_job

The relative importance of the job shares in the functional policy. Values are of type real.

## weight_tickets_functional

The maximum number of functional tickets available for distribution by xxQS_NAMExx. Determines the relative 
importance of the functional policy. See under xxqs_name_sxx_priority(5) for an overview on job priorities.

## weight_tickets_share

The maximum number of share based tickets available for distribution by xxQS_NAMExx. Determines the relative 
importance of the share tree policy. See under xxqs_name_sxx_priority(5) for an overview on job priorities.

## weight_deadline

The weight applied on the remaining time until a jobs latest start time. Determines the relative importance of 
the deadline. See under xxqs_name_sxx_priority(5) for an overview on job priorities.

## weight_waiting_time

The weight applied on the jobs waiting time since submission. Determines the relative importance of the waiting time. 
See under xxqs_name_sxx_priority(5) for an overview on job priorities.

## weight_urgency

The weight applied on jobs normalized urgency when determining priority finally used. Determines the relative 
importance of urgency. See under xxqs_name_sxx_priority(5) for an overview on job priorities.

## weight_priority

The weight applied on jobs normalized POSIX priority when determining priority finally used. Determines the 
relative importance of POSIX priority. See under xxqs_name_sxx_priority(5) for an overview on job priorities.

## weight_ticket

The weight applied on normalized ticket amount when determining priority finally used. Determines the relative 
importance of the ticket policies. See under xxqs_name_sxx_priority(5) for an overview on job priorities.

## flush_finish_sec

The parameters are provided for tuning the system's scheduling behavior. By default, a scheduler run is triggered 
in the scheduler interval. When this parameter is set to 1 or larger, the scheduler will be triggered x seconds 
after a job has finished. Setting this parameter to 0 disables the flush after a job has finished.

## flush_submit_sec

The parameters are provided for tuning the system's scheduling behavior. By default, a scheduler run is triggered 
in the scheduler interval. When this parameter is set to 1 or larger, the scheduler will be triggered x seconds 
after a job was submitted to the system. Setting this parameter to 0 disables the flush after a job was submitted.

## schedd_job_info

The default scheduler can keep track why jobs could not be scheduled during the last scheduler run. This parameter 
enables or disables the observation. The value *true* enables the monitoring *false* turns it off.

It is also possible to activate the observation only for certain jobs. This will be done if the parameter is 
set to *job_list* followed by a comma separated list of job ids.

The user can obtain the collected information with the command qstat -j.

## params

This is foreseen for passing additional parameters to the xxQS_NAMExx scheduler. 

Changing *params* will take immediate effect. The default for *params* is none.

The following values are recognized:

### DURATION_OFFSET  

If set, overrides the default of value 60 seconds. This parameter is used by the xxQS_NAMExx scheduler when 
planning resource utilization as the delta between net job runtimes and total time until resources become
available again. Net job runtime as specified with `-l h_rt=...` or `-l s_rt=...` or *default_duration* always 
differs from total job runtime due to delays before and after actual job start and finish. Among the
delays before job start is the time until the end of a *schedule_interval*, the time it takes to deliver a job to
*sge_execd*(8) and the delays caused by *prolog* in xxqs_name_sxx_queue_conf(5), *start_proc_args* in xxqs_name_sxx_pe(5) and 
*starter_method* in xxqs_name_sxx_queue_conf(5)

(*notify*, *terminate_method* or *checkpointing*), procedures run after actual job finish, such as 
*stop_proc_args* in xxqs_name_sxx_pe(5) or *epilog* in xxqs_name_sxx_queue_conf(5) , and the delay until a new *schedule_interval*.  
If the offset is too low, resource reservations (see *max_reservation*) can be delayed repeatedly due to an overly
optimistic job circulation time.

### PROFILE 

If set equal to 1, the scheduler logs profiling information summarizing each scheduling run.

### MONITOR 

If set equal to 1, the scheduler records information for each scheduling run allowing to reproduce job 
resources utilization in the file *\<xxqs_name_sxx_root>/\<cell>/common/schedule*.

### PE_RANGE_ALG 

This parameter sets the algorithm for the pe range computation. The default is automatic, which means that the 
scheduler will select the best one, and it should not be necessary to change it to a different setting in normal 
operation. If a custom setting is needed, the following values are available:  

* auto : the scheduler selects the best algorithm  
* least : starts the resource matching with the lowest slot amount first  
* bin : starts the resource matching in the middle of the pe slot range  
* highest : starts the resource matching with the highest slot amount first

## reprioritize_interval

Interval (HH:MM:SS) to reprioritize jobs on the execution hosts based on the current ticket amount for the running 
jobs. If the interval is set to 00:00:00 the reprioritization is turned off. The default value is 00:00:00. The 
reprioritization tickets are calculated by the scheduler and update events for running jobs are only sent after 
the scheduler calculated new values. How often the schedule should calculate the tickets is defined by the 
reprioritize_interval. Because the scheduler is only triggered in a specific interval (scheduler_interval) this means
the reprioritize_interval has only a meaning if set greater than the scheduler_interval. For example, if the 
scheduler_interval is 2 minutes and reprioritize_interval is set to 10 seconds, this means the jobs get
re-prioritized every 2 minutes.

## report_pjob_tickets

This parameter allows to tune the system's scheduling run time. It is used to enable / disable the reporting of 
pending job tickets to the qmaster. It does not influence the tickets calculation. The sort order of jobs in qstat 
is only based on the submit time, when the reporting is turned off. The reporting should be turned off in a system 
with a very large amount of jobs by setting this parameter to "false".

## halflife_decay_list

The halflife_decay_list allows to configure different decay rates for the "finished_jobs usage types, which is used 
in the pending job ticket calculation to account for jobs which have just ended. This allows the user the pending 
jobs algorithm to count finished jobs against a user or project for a configurable decayed time period. This 
feature is turned off by default, and the halftime is used instead. The halflife_decay_list also allows one to 
configure different decay rates for each usage type being tracked (cpu, io, and mem). The list is specified in 
the following format:

    <USAGE_TYPE>=<TIME>[:<USAGE_TYPE>=<TIME>[:<USAGE_TYPE>=<TIME>]]
  
\<USAGE_TYPE> can be one of the following: cpu, io, or mem. \<TIME> can be -1, 0 or a timespan specified in minutes. 
If \<TIME> is -1, only the usage of currently running jobs is used. 0 means that the usage is not decayed.

## policy_hierarchy

This parameter sets up a dependency chain of ticket based policies. Each ticket based policy in the dependency chain 
is influenced by the previous policies and influences the following policies. A typical scenario is to assign 
precedence for the override policy over the share-based policy. The override policy determines in such a case how
share-based tickets are assigned among jobs of the same user or project. Note that all policies contribute to the 
ticket amount assigned to a particular job regardless of the policy hierarchy definition. Yet the tickets calculated 
in each of the policies can be different depending on "*POLICY_HIERARCHY*".

The "*POLICY_HIERARCHY*" parameter can be a up to 3 letter combination of the first letters of the 3 ticket 
based policies S(hare-based), F(unctional) and O(verride). So a value "OFS" means that the override policy takes 
precedence over the functional policy, which finally influences the share-based policy. Less than 3 letters mean 
that some of the policies do not influence other policies and also are not influenced by other policies. So a value 
of "FS" means that the functional policy influences the share-based policy and that there is no interference with
the other policies.

The special value "NONE" switches off policy hierarchies.

## share_override_tickets

If set to "true" or "1", override tickets of any override object instance are shared equally among all running jobs 
associated with the object. The pending jobs will get as many override tickets, as they would have, when they were 
running. If set to "false" or "0", each job gets the full value of the override tickets associated with the object.
The default value is "true".

## share_functional_shares

If set to "true" or "1", functional shares of any functional object instance are shared among all the jobs associated 
with the object. If set to "false" or "0", each job associated with a functional object, gets the full functional 
shares of that object. The default value is "true".

## max_functional_jobs_to_schedule

The maximum number of pending jobs to schedule in the functional policy. The default value is 200.

## max_pending_tasks_per_job

The maximum number of subtasks per pending array job to schedule. This parameter exists in order to reduce scheduling 
overhead. The default value is 50.

## max_reservation

The maximum number of reservations scheduled within a schedule interval. When a runnable job can not be started due 
to a shortage of resources a reservation can be scheduled instead. A reservation can cover consumable resources 
with the global host, any execution host and any queue. For parallel jobs reservations are done also for slots 
resource as specified in xxqs_name_sxx_pe(5). As job runtime the maximum of the time specified with `-l h_rt=...` or 
`-l s_rt=...` is assumed. For jobs that have neither of them the default_duration is assumed. Reservations prevent 
jobs of lower priority as specified in xxqs_name_sxx_priority(5) from utilizing the reserved resource quota during the time 
of reservation. Jobs of lower priority are allowed to utilize those reserved resources only if their prospective job 
end is before the start of the reservation (backfilling). Reservation is done only for non-immediate jobs (-now no)
that request reservation (-R y). If max_reservation is set to "0" no job reservation is done.

Note, that reservation scheduling can be performance consuming and hence reservation scheduling is switched off 
by default. Since reservation scheduling performance consumption is known to grow with the number of pending jobs, 
the use of -R y option is recommended only for those jobs actually queuing for bottleneck resources. Together with the
max_reservation parameter this technique can be used to narrow down performance impacts.

## default_duration

When job reservation is enabled through max_reservation xxqs_name_sxx_sched_conf(5) parameter the default duration is assumed 
as runtime for jobs that have neither `-l h_rt=...` nor `-l s_rt=...` specified. In contrast to a
*h_rt*/*s_rt* time limit the default_duration is not enforced.

# FILES

For a complete list of files used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

## <xxqs_name_sxx_root>/<cell>/common/sched_configuration
scheduler thread configuration

# SEE ALSO

xxqs_name_sxx_intro(1), qalter(1), qconf(1), qstat(1), qsub(1), xxqs_name_sxx_complex(5), xxqs_name_sxx_queue_conf(5), 
xxqs_name_sxx_execd(8), xxqs_name_sxx_qmaster(8), 

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
