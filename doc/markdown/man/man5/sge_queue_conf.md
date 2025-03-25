---
title: sge_queue_conf
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_queue_conf - xxQS_NAMExx queue configuration file format

# DESCRIPTION

This manual page describes the format of the template file for the cluster queue configuration. Via the `-aq` 
and `-mq` options of the qconf(1) command, you can add cluster queues and modify the configuration of any queue 
in the cluster. Any of these change operations can be rejected, as a result of a failed integrity verification.

The queue configuration parameters take as values strings, integer decimal numbers or boolean, time and memory 
specifiers (see *time_specifier* and *memory_specifier* in xxqs_name_sxx_types(5)) as well as comma separated lists.

Note, xxQS_NAMExx allows backslashes (\\) be used to escape newline (\\newline) characters. The backslash and the 
newline are replaced with a space (" ") character before any interpretation.

# FORMAT

The following list of parameters specifies the queue configuration file content:

## qname

The name of the cluster queue as defined for *queue_name* in xxqs_name_sxx_types(1). As template default 
"template" is used.

## hostlist

A list of host identifiers as defined for *host_identifier* in xxqs_name_sxx_types(1). For each host xxQS_NAMExx 
maintains a queue instance for running jobs on that particular host. Large amounts of hosts can easily be managed 
by using host groups rather than by single host names. As list separators white-spaces and "," can be used. 
(template default: NONE).

If more than one host is specified it can be desirable to specify divergences with the further below parameter 
settings for certain hosts. These divergences can be expressed using the enhanced queue configuration specifier 
syntax. This syntax builds upon the regular parameter specifier syntax separately for each parameter:

    "[" host_identifier = <parameters_specifier_syntax> "]"
        [ "," "[" host_identifier = <parameters_specifier_syntax> "]" ]

note, even in the enhanced queue configuration specifier syntax an entry without brackets denoting the default 
setting is required and used for all queue instances where no divergences are specified. Tuples with a host group 
*host_identifier* override the default setting. Tuples with a host name host_identifier override both the default 
and the host group setting.

Note that also with the enhanced queue configuration specifier syntax a default setting is always needed for 
each configuration attribute; otherwise the queue configuration gets rejected. Ambiguous queue configurations 
with more than one attribute setting for a particular host are rejected. Configurations containing override 
values for hosts not enlisted under 'hostname' are accepted but are indicated by `-sds` of qconf(1). The cluster 
queue should contain an unambiguous specification for each configuration attribute of each queue instance
specified under hostname in the queue configuration. Ambiguous configurations with more than one attribute setting 
resulting from overlapping host groups are indicated by `-explain c` of qstat(1) and cause the queue instance with 
ambiguous configurations to enter the c(onfiguration ambiguous) state.

## seq_no

In conjunction with the hosts load situation at a time this parameter specifies this queue's position in the 
scheduling order within the suitable queues for a job to be dispatched under consideration of the *queue_sort_method* 
(see xxqs_name_sxx_sched_conf(5)).

Regardless of the *queue_sort_method* setting, qstat(1) reports queue information in the order defined by the 
value of the *seq_no*. Set this parameter to a monotonically increasing sequence. (type number; template default: 0).

## load_thresholds

*load_thresholds* is a list of load thresholds. Already if one of the thresholds is exceeded no further jobs will 
be scheduled to the queues and qmaster will signal an overload condition for this node. Arbitrary load values being 
defined in the "host" and "global" complexes (see xxqs_name_sxx_complex(5) for details) can be used.

The syntax is that of a comma separated list with each list element consisting of the *complex_name* 
(see xxqs_name_sxx_types(5)) of a load value, an equal sign and the threshold value being intended to trigger the
overload situation (e.g. load_avg=1.75,users_logged_in=5).

Note: Load values as well as consumable resources may be scaled differently for different hosts if specified in 
the corresponding execution host definitions (refer to xxqs_name_sxx_host_conf(5) for more information). Load 
thresholds are compared against the scaled load and consumable values.

## suspend_thresholds

A list of load thresholds with the same semantics as that of the *load_thresholds* parameter (see above) except 
that exceeding one of the denoted thresholds initiates suspension of one of multiple jobs in
the queue. See the *nsuspend* parameter below for details on the number of jobs which are suspended. There is 
an important relationship between the *suspend_threshold* and the *scheduler_interval*. If you have for example 
a suspend threshold on the np_load_avg, and the load exceeds the threshold, this does not have immediate effect. Jobs
continue running until the next scheduling run, where the scheduler detects the threshold has been exceeded 
and sends an order to qmaster to suspend the job. The same applies for unsuspending.

## nsuspend

The number of jobs which are suspended/enabled per time interval if at least one of the load thresholds in the 
*suspend_thresholds* list is exceeded or if no *suspend_threshold* is violated anymore respectively. *nsuspend* 
jobs are suspended in each time interval until no *suspend_thresholds* are exceeded anymore or all jobs in the
queue are suspended. Jobs are enabled in the corresponding way if the *suspend_thresholds* are no longer exceeded. 
The time interval in which the suspensions of the jobs occur is defined in *suspend_interval* below.

## suspend_interval

The time interval in which further *nsuspend* jobs are suspended if one of the *suspend_thresholds* (see above 
for both) is exceeded by the current load on the host on which the queue is located. The time interval is also 
used when enabling the jobs. The syntax is that of a *time_specifier* in xxqs_name_sxx_types(5).

## priority

The *priority* parameter specifies the nice(2) value at which jobs in this queue will be run. The type is number 
and the default is zero (which means no nice value is set explicitly). Negative values (up to -20) correspond to a 
higher scheduling priority, positive values (up to +20) correspond to a lower scheduling priority.

Note, the value of priority has no effect, if xxQS_NAMExx adjusts priorities dynamically to implement 
ticket-based entitlement policy goals. Dynamic priority adjustment is switched off by default due to
xxqs_name_sxx_conf(5) *reprioritize* being set to false.

## min_cpu_interval

The time between two automatic checkpoints in case of transparently checkpointing jobs. The maximum of the 
time requested by the user via qsub(1) and the time defined by the queue configuration is used as checkpoint 
interval. Since checkpoint files may be considerably large and thus writing them to the file system may become 
expensive, users and administrators are advised to choose sufficiently large time intervals. *min_cpu_interval* 
is of type time and the default is 5 minutes (which usually is suitable for test purposes only). The syntax is 
that of a *time_specifier* in xxqs_name_sxx_types(5).

## processors

A set of processors in case of a multiprocessor execution host can be defined to which the jobs executing in this 
queue are bound. The value type of this parameter is a range description like that of the `-pe` option of qsub(1) 
(e.g. 1-4,8,10) denoting the processor numbers for the processor group to be used. Obviously the interpretation 
of these values relies on operating system specifics and is thus performed inside xxqs_name_sxx_execd(8) running 
on the queue host. Therefore, the parsing of the parameter has to be provided by the execution daemon and the 
parameter is only passed through xxqs_name_sxx_qmaster(8) as a string.

Currently, support is only provided for multiprocessor machines running Solaris. In the case of Solaris the 
processor set must already exist, when this processors parameter is configured. So the processor set has to be 
created manually. In the case of Digital UNIX only one job per processor set is allowed to execute at the same time,
i.e. *slots* (see above) should be set to 1 for this queue.

## qtype

The type of queue. Currently *batch, interactive* or a combination in a comma separated list or *NONE*.

Jobs that need to be scheduled immediately (*qsh*, *qlogin*, *qrsh* and *qsub* with option *-now yes*) can 
ONLY be scheduled on *interactive* queues.

The formerly supported types parallel and checkpointing are not allowed anymore. A queue instance is implicitly 
of type parallel/checkpointing if there is a parallel environment or a checkpointing interface specified for this 
queue instance in *pe_list*/*ckpt_list*. Formerly possible settings e.g.

    qtype   PARALLEL

could be transferred into

    qtype   NONE
    pe_list pe_name

(type string; default: batch interactive).

## pe_list

The list of administrator-defined parallel environment (see xxqs_name_sxx_pe(5)) names to be associated with the queue. 
The default is *NONE*.

## ckpt_list

The list of administrator-defined checkpointing interface names (see *ckpt_name* in sge_types(1)) to be associated 
with the queue. The default is *NONE*.

## rerun

Defines a default behavior for jobs which are aborted by system crashes or manual "violent" (via kill(1)) shutdown 
of the complete xxQS_NAMExx system (including the xxqs_name_sxx_shepherd(8) of the jobs and their process hierarchy) 
on the queue host. As soon as xxqs_name_sxx_execd(8) is restarted and detects that a job has been aborted for such 
reasons it can be restarted if the jobs are restartable. A job may not be restartable, for example, if it updates
databases (first reads then writes to the same record of a database/file) because the abortion of the job may have 
left the database in an inconsistent state. If the owner of a job wants to overrule the default behavior for the 
jobs in the queue the `-r` option of qsub(1) can be used.

The type of this parameter is boolean, thus either TRUE or FALSE can be specified. The default is FALSE, i.e. do 
not restart jobs automatically.

## slots

The maximum number of concurrently executing jobs allowed in any queue instance defined by the queue. Type is 
number, valid values are 0 to 9999999.

## tmpdir

The *tmpdir* parameter specifies the absolute path to the base of the temporary directory filesystem. When 
xxqs_name_sxx_execd(8) launches a job, it creates a uniquely-named directory in this filesystem for the
purpose of holding scratch files during job execution. At job completion, this directory and its contents 
are removed automatically. The environment variables TMPDIR and TMP are set to the path of each jobs scratch 
directory (type string; default: /tmp).

## shell

If either *posix_compliant* or *script_from_stdin* is specified as the *shell_start_mode* parameter in 
xxqs_name_sxx_conf(5) the *shell* parameter specifies the executable path of the command interpreter (e.g.
sh(1) or csh(1)) to be used to process the job scripts executed in the queue. The definition of *shell* can 
be overruled by the job owner via the qsub(1) `-S` option.

The type of the parameter is string. The default is /bin/csh.

## shell_start_mode

This parameter defines the mechanisms which are used to actually invoke
the job scripts on the execution hosts. The following values are
recognized:

* unix_behavior  
  If a user starts a job shell script under UNIX interactively by invoking it just with the script name the 
  operating system's executable loader uses the information provided in a comment such as `#!/bin/csh` in the
  first line of the script to detect which command interpreter to start to interpret the script. This mechanism 
  is used by xxQS_NAMExx when starting jobs if *unix_behavior* is defined as *shell_start_mode*.

* posix_compliant  
  POSIX does not consider first script line comments such a `#!/bin/csh` as being significant. The POSIX standard 
  for batch queuing systems (P1003.2d) therefore requires a compliant queuing system to ignore such lines but to 
  use user specified or configured default command interpreters instead. Thus, if *shell_start_mode* is set to
  *posix_compliant* xxQS_NAMExx will either use the command interpreter indicated by the `-S` option of the 
  qsub(1) command or the *shell* parameter of the queue to be used (see above).

* script_from_stdin  
  Setting the *shell_start_mode* parameter either to *posix_compliant* or *unix_behavior* requires you to set 
  the umask in use for xxqs_name_sxx_execd(8) such that every user has read access to the active_jobs directory 
  in the spool directory of the corresponding execution daemon. In case you have *prolog* and *epilog* scripts
  configured, they also need to be readable by any user who may execute jobs.  

If this violates your site's security policies you may want to set *shell_start_mode* to *script_from_stdin*. 
This will force xxQS_NAMExx to open the job script as well as the epilogue and prologue scripts for reading into 
STDIN as root (if xxqs_name_sxx_execd(8) was started as root) before changing to the job owner's user account. 
The script is then fed into the STDIN stream of the command interpreter indicated by the `-S` option of the 
qsub(1) command or the *shell* parameter of the queue to be used (see above).  
Thus setting *shell_start_mode* to *script_from_stdin* also implies *posix_compliant* behavior. Note, however, 
that feeding scripts into the STDIN stream of a command interpreter may cause trouble if commands like rsh(1) 
are invoked inside a job script as they also process the STDIN stream of the command interpreter. These problems 
can usually be resolved by redirecting the STDIN channel of those commands to come from /dev/null (e.g. `rsh host 
date < /dev/null`). Note also, that any command-line options associated with the job are passed to the executing
shell. The shell will only forward them to the job if they are not recognized as valid shell options.

The default for *shell_start_mode* is *posix_compliant*. Note, though, that the *shell_start_mode* can only be 
used for batch jobs submitted by qsub(1) and can't be used for interactive jobs submitted by qrsh(1), qsh(1), qlogin(1).

## prolog

The executable path of a shell script that is started before execution of xxQS_NAMExx jobs with the same 
environment setting as that for the xxQS_NAMExx jobs to be started afterwards. An optional prefix "user@"
specifies the user under which this procedure is to be started. The procedures standard output and the error 
output stream are written to the same file used also for the standard output and error output of each job. 
This procedure is intended as a means for the xxQS_NAMExx administrator to automate the execution of general 
site specific tasks like the preparation of temporary file systems with the need for the same context 
information as the job. This queue configuration entry overwrites cluster global or execution host specific 
*prolog* definitions (see xxqs_name_sxx_conf(5)).

The default for *prolog* is the special value NONE, which prevents from execution of a prologue script. The 
special variables for constituting a command line are the same like in *prolog* definitions
of the cluster configuration (see xxqs_name_sxx_conf(5)).

Exit codes for the prolog attribute can be interpreted based on the following exit values:

* 0: Success  
* 99: Reschedule job  
* 100: Put job in error state  
* Anything else: Put queue in error state

## epilog

The executable path of a shell script that is started after execution of xxQS_NAMExx jobs with the same 
environment setting as that for the xxQS_NAMExx jobs that has just completed. An optional prefix "user@"
specifies the user under which this procedure is to be started. The procedures standard output and the error 
output stream are written to the same file used also for the standard output and error output of each
job. This procedure is intended as a means for the xxQS_NAMExx administrator to automate the execution of 
general site specific tasks like the cleaning up of temporary file systems with the need for the same context 
information as the job. This queue configuration entry overwrites cluster global or execution host specific *epilog*
definitions (see xxqs_name_sxx_conf(5)).

The default for *epilog* is the special value NONE, which prevents from execution of a epilogue script. The special 
variables for constituting a command line are the same like in *prolog* definitions of the cluster configuration 
(see xxqs_name_sxx_conf(5)).

Exit codes for the epilog attribute can be interpreted based on the following exit values:

* 0: Success  
* 99: Reschedule job  
* 100: Put job in error state  
* Anything else: Put queue in error state

## starter_method

The specified executable path will be used as a job starter facility responsible for starting batch jobs. The 
executable path will be executed instead of the configured shell to start the job. The job arguments will be 
passed as arguments to the job starter. The following environment variables are used to pass information to the 
job starter concerning the shell environment which was configured or requested to start the job.

* SGE_STARTER_SHELL_PATH  
  The name of the requested shell to start the job

* SGE_STARTER_SHELL_START_MODE  
  The configured *shell_start_mode*

* SGE_STARTER_USE_LOGIN_SHELL  
  Set to "true" if the shell is supposed to be used as a login shell (see *login_shells* in xxqs_name_sxx_conf(5))

The starter_method will not be invoked for qsh, qlogin or qrsh acting as rlogin.

## suspend_method

## resume_method

## terminate_method

These parameters can be used for overwriting the default method used by xxQS_NAMExx for suspension, release of 
a suspension and for termination of a job. Per default, the signals SIGSTOP, SIGCONT and SIGKILL are delivered 
to the job to perform these actions. However, for some applications this is not appropriate.

If no executable path is given, xxQS_NAMExx takes the specified parameter entries as the signal to be delivered 
instead of the default signal. A signal must be either a positive number or a signal name with "SIG" as prefix and 
the signal name as printed by `kill -l` (e.g. *SIGTERM*).

If an executable path is given (it must be an absolute path starting with a "/") then this command together 
with its arguments is started by xxQS_NAMExx to perform the appropriate action. The following special variables 
are expanded at runtime and can be used (besides any other strings which have to be interpreted by the procedures) 
to constitute a command line:

* $sge_root
  The product root directory.

* $sge_cell
  The name of the cell directory.

* $host  
  The name of the host on which the procedure is started.

* $job_owner  
  The user name of the job owner.

* $job_id  
  xxQS_NAMExx's unique job identification number.

* $job_name  
  The name of the job.

* $queue  
  The name of the queue.

* $job_pid  
  The pid of the job.

## notify

The time waited between delivery of SIGUSR1/SIGUSR2 notification signals and suspend/kill signals if job was 
submitted with the qsub(1) `-notify` option.

## owner_list

The *owner_list* enlists comma separated the login(1) user names (see *user_name* in sge_types(1)) of those users 
who are authorized to disable and suspend this queue through qmod(1) (xxQS_NAMExx operators and managers can do 
this by default). It is customary to set this field for queues on interactive workstations where the computing
resources are shared between interactive sessions and xxQS_NAMExx jobs, allowing the workstation owner to have 
priority access. (default: NONE).

## user_lists

The *user_lists* parameter contains a comma separated list of xxQS_NAMExx user access list names as described 
in xxqs_name_sxx_access_list(5).
Each user contained in at least one of the enlisted access lists has access to the queue. If the *user_lists* 
parameter is set to NONE (the default) any user has access being not explicitly excluded via the *xuser_lists* 
parameter described below. If a user is contained both in an access list enlisted in *xuser_lists* and *user_lists* 
the user is denied access to the queue.

## xuser_lists

The *xuser_lists* parameter contains a comma separated list of xxQS_NAMExx user access list names as described 
in xxqs_name_sxx_access_list(5). Each user contained in at least one of the enlisted access lists is not
allowed to access the queue. If the *xuser_lists* parameter is set to NONE (the default) any user has access. 
If a user is contained both in an access list enlisted in *xuser_lists* and *user_lists* the user is denied 
access to the queue.

## projects

The *projects* parameter contains a comma separated list of xxQS_NAMExx projects (see xxqs_name_sxx_project(5)) 
that have access to the queue. Any project not in this list are denied access to the queue. If set to NONE 
(the default), any project has access that is not specifically excluded via the *xprojects* parameter described 
below. If a project is in both the *projects* and *xprojects* parameters, the project is denied access to the queue.

## xprojects

The *xprojects* parameter contains a comma separated list of xxQS_NAMExx projects (see xxqs_name_sxx_project(5)) 
that are denied access to the queue. If set to NONE (the default), no projects are denied access other than those 
denied access based on the *projects* parameter described above. If a project is in both the *projects* and *xprojects*
parameters, the project is denied access to the queue.

## subordinate_list

There are two different types of subordination:

1. Queuewise subordination

   A list of xxQS_NAMExx queue names as defined for *queue_name* in sge_types(1). Subordinate relationships are 
   in effect only between queue instances residing at the same host. The relationship does not apply and is ignored 
   when jobs are running in queue instances on other hosts. Queue instances residing on the same host will be 
   suspended when a specified count of jobs is running in this queue instance. The list specification is the 
   same as that of the *load_thresholds* parameter above, e.g. `low_pri_q=5,small_q`. The numbers denote the job 
   slots of the queue that have to be filled in the superordinated queue to trigger the suspension of the 
   subordinated queue. If no value is assigned a suspension is triggered if all slots of the queue are filled.

   On nodes which host more than one queue, you might wish to accord better service to certain classes of jobs 
   (e.g. queues that are dedicated to parallel processing might need priority over low priority production
   queues; default: NONE).

2. Slotwise preemption

   The slotwise preemption provides a means to ensure that high priority jobs get the resources they need, while 
   at the same time low priority jobs on the same host are not unnecessarily preempted, maximizing the host 
   utilization. The slotwise preemption is designed to provide different preemption actions, but with the 
   current implementation only suspension is provided. This means there is a subordination relationship
   defined between queues similar to the queuewise subordination, but if the suspend threshold is exceeded, 
   not the whole subordinated queue is suspended, there are only single tasks running in single slots suspended.

   Like with queuewise subordination, the subordination relationships are in effect only between queue instances 
   residing at the same host. The relationship does not apply and is ignored when jobs and tasks are running in 
   queue instances on other hosts.

   The syntax is:

           slots=<threshold>(<queue_list>)

   where

           <threshold>  = a positive integer number
           <queue_list> = <queue_def>[,<queue_list>]
           <queue_def>  = <queue>[:<seq_no>][:<action>]
           <queue>      = a xxQS_NAMExx queue name as defined for

   *queue_name* in sge_types(1).

            <seq_no>    = sequence number among all subordinated queues
                          of the same depth in the tree. The higher the
                          sequence number, the lower is the priority of
                          the queue.
                          Default is 0, which is the highest priority.
            <action>    = the action to be taken if the threshold is
                          exceeded. Supported is:
                          "sr": Suspend the task with the shortest run time.
                          "lr": Suspend the task with the longest run time.
                          Default is "sr".

   Some examples of possible configurations and their functionalities:

   a) The simplest configuration

       subordinate_list slots=2(B.q)

   which means the queue "B.q" is subordinated to the current queue (let's call it "A.q"), the suspend threshold 
   for all tasks running in "A.q" and "B.q" on the current host is two, the sequence number of "B.q" is "0" and 
   the action is "suspend task with shortest run time first". This subordination relationship looks like this:

          A.q
           |
          B.q

   This could be a typical configuration for a host with a dual core CPU. This subordination configuration ensures 
   that tasks that are scheduled to "A.q" always get a CPU core for themselves, while jobs in "B.q" are not 
   preempted as long as there are no jobs running in "A.q".

   If there is no task running in "A.q", two tasks are running in "B.q" and a new task is scheduled to "A.q", the 
   sum of tasks running in "A.q" and "B.q" is three. Three is greater than two, this triggers the defined action. 
   This causes the task with the shortest run time in the subordinated queue "B.q" to be suspended. After 
   suspension, there is one task running in "A.q", on task running in "B.q" and one task suspended in "B.q".

   b) A simple tree

       subordinate_list slots=2(B.q:1, C.q:2)

   This defines a small tree that looks like this:

          A.q
         /   \
       B.q   C.q

   A use case for this configuration could be a host with a dual core CPU and queue "B.q" and "C.q" for jobs 
   with different requirements, e.g. "B.q" for interactive jobs, "C.q" for batch jobs. Again, the tasks in
   "A.q" always get a CPU core, while tasks in "B.q" and "C.q" are suspended only if the threshold of running 
   tasks is exceeded. Here the sequence number among the queues of the same depth comes into play. Tasks 
   scheduled to "B.q" can't directly trigger the suspension of tasks in "C.q", but if there is a task to be 
   suspended, first "C.q" will be searched for a suitable task.

   If there is one task running in "A.q", one in "C.q" and a new task is scheduled to "B.q", the threshold 
   of "2" in "A.q", "B.q" and "C.q" is exceeded. This triggers the suspension of one task in either "B.q" or
   "C.q". The sequence number gives "B.q" a higher priority than "C.q", therefore the task in "C.q" is suspended. 
   After suspension, there is one task running in "A.q", one task running in "B.q" and one task suspended in "C.q".

   c) More than two levels

   Configuration of A.q:

          subordinate_list slots=2(B.q)  
 
   Configuration of B.q:

          subordinate_list slots=2(C.q)

   looks like this:

          A.q
           |
          B.q
           |
          C.q

   These are three queues with high, medium and low priority. If a task is scheduled to "C.q", first the subtree 
   consisting of "B.q" and "C.q" is checked, the number of tasks running there is counted. If the threshold
   which is defined in "B.q" is exceeded, the job in "C.q" is suspended. Then the whole tree is checked, if the 
   number of tasks running in "A.q", "B.q" and "C.q" exceeds the threshold defined in "A.q" the task in "C.q"
   is suspended. This means, the effective threshold of any subtree is not higher than the threshold of the root 
   node of the tree. If in this example a task is scheduled to "A.q", immediately the number of tasks
   running in "A.q", "B.q" and "C.q" is checked against the threshold defined in "A.q".

   d) Any tree

            A.q
           /   \
         B.q   C.q
        /     /   \
       D.q    E.q  F.q
                     \
                      G.q 

  The computation of the tasks that are to be (un)suspended always starts at the queue instance that is modified, 
  i.e. a task is scheduled to, a task ends at, the configuration is modified, a manual or other automatic
  (un)suspend is issued, except when it is a leaf node, like "D.q", "E.q" and "G.q" in this example. Then the 
  computation starts at its parent queue instance (like "B.q", "C.q" or "F.q" in this example). From there
  first all running tasks in the whole subtree of this queue instance are counted. If the sum exceeds the 
  threshold configured in the subordinate_list, in this subtree a task is searched to be suspended.
  Then the algorithm proceeds to the parent of this queue instance, counts all running tasks in the whole subtree 
  below the parent and checks if the number exceeds the threshold configured at the parent's subordinate_list. 
  If so, it searches for a task to suspend in the whole subtree below the parent. And so on, until it did this 
  computation for the root node of the tree.

## complex_values

*complex_values* defines quotas for resource attributes managed via this queue. The syntax is the same as for 
*load_thresholds* (see above). The quotas are related to the resource consumption of all jobs in a queue in the 
case of consumable resources (see xxqs_name_sxx_complex(5) for details on consumable resources) or they are 
interpreted on a per queue slot (see *slots* above) basis in the case of non-consumable resources. Consumable 
resource attributes are commonly used to manage free memory, free disk space or available floating software licenses
while non-consumable attributes usually define distinctive characteristics like type of hardware installed.

For consumable resource attributes an available resource amount is determined by subtracting the current resource 
consumption of all running jobs in the queue from the quota in the *complex_values* list. Jobs can only be 
dispatched to a queue if no resource requests exceed any corresponding resource availability obtained by this 
scheme. The quota definition in the *complex_values* list is automatically replaced by the current load value 
reported for this attribute, if load is monitored for this resource and if the reported load value is more
stringent than the quota. This effectively avoids oversubscription of resources.

Note: Load values replacing the quota specifications may have become more stringent because they have been scaled 
(see xxqs_name_sxx_host_conf(5)) and/or load adjusted (see xxqs_name_sxx_sched_conf(5)). The `-F` option of qstat(1) 
provide detailed information on the actual availability of consumable resources and on the origin of the values 
taken into account currently.

Note also: The resource consumption of running jobs (used for the availability calculation) as well as the 
resource requests of the jobs waiting to be dispatched either may be derived from explicit user requests during 
job submission (see the `-l` option to qsub(1)) or from a "default" value configured for an attribute by the 
administrator (see xxqs_name_sxx_complex(5)). The `-r` option to qstat(1) can be used for retrieving full detail 
on the actual resource requests of all jobs in the system.

For non-consumable resources xxQS_NAMExx simply compares the job's attribute requests with the corresponding 
specification in *complex_values* taking the relation operator of the complex attribute definition into account 
(see xxqs_name_sxx_complex(5)). If the result of the comparison is "true", the queue is suitable for the job with 
respect to the particular attribute. For parallel jobs each queue slot to be occupied by a parallel task is meant 
to provide the same resource attribute value.

Note: Only numeric complex attributes can be defined as consumable resources and hence non-numeric attributes 
are always handled on a per queue slot basis.

The default value for this parameter is NONE, i.e. no administrator defined resource attribute quotas are 
associated with the queue.

## calendar

specifies the *calendar* to be valid for this queue or contains NONE (the default). A calendar defines the 
availability of a queue depending on time of day, week and year. Please refer to xxqs_name_sxx_calendar_conf(5) 
for details on the xxQS_NAMExx calendar facility.

Note: Jobs can request queues with a certain calendar model via a `*-l c=<cal_name>*` option to qsub(1).

## initial_state

defines an initial state for the queue either when adding the queue to the system for the first time or on 
start-up of the xxqs_name_sxx_execd(8) on the host on which the queue resides. Possible values are:

* default  
  The queue is enabled when adding the queue or is reset to the previous status when xxqs_name_sxx_execd(8) 
  comes up (this corresponds to the behavior in earlier xxQS_NAMExx releases not supporting initial_state).

* enabled  
  The queue is enabled in either case. This is equivalent to a manual and explicit `qmod -e` command (see qmod(1)).

* disabled  
  The queue is disable in either case. This is equivalent to a manual and explicit `qmod -d` command (see qmod(1)).

# RESOURCE LIMITS

The first two resource limit parameters, *s_rt* and *h_rt*, are implemented by xxQS_NAMExx. They define the 
"real time" or also called "elapsed" or "wall clock" time having passed since the start of the job. If *h_rt* is 
exceeded by a job running in the queue, it is aborted via the SIGKILL signal (see kill(1)). If *s_rt* is exceeded, 
the job is first "warned" via the SIGUSR1 signal (which can be caught by the job) and finally aborted after the 
notification time defined in the queue configuration parameter *notify* (see above) has passed. In cases when
*s_rt* is used in combination with job notification it might be necessary to configure a signal other than 
SIGUSR1 using the NOTIFY_KILL and NOTIFY_SUSP execd_params (see sge_conf(5)) so that the jobs
signal-catching mechanism can "differ" the cases and react accordingly.

The resource limit parameters *s_cpu* and *h_cpu* are implemented by xxQS_NAMExx as a job limit. They impose a 
limit on the amount of combined CPU time consumed by all the processes in the job. If *h_cpu* is exceeded by a 
job running in the queue, it is aborted via a SIGKILL signal (see kill(1)). If *s_cpu* is exceeded, the job is sent a
SIGXCPU signal which can be caught by the job. If you wish to allow a job to be "warned" so it can exit gracefully 
before it is killed then you should set the *s_cpu* limit to a lower value than *h_cpu*. For parallel processes, 
the limit is applied per slot which means that the limit is multiplied by the number of slots being used by the 
job before being applied.

The resource limit parameters *s_vmem* and *h_vmem* are implemented by xxQS_NAMExx as a job limit. They impose a 
limit on the amount of combined virtual memory consumed by all the processes in the job. If *h_vmem* is exceeded 
by a job running in the queue, it is aborted via a SIGKILL signal (see kill(1)). If *s_vmem* is exceeded, the job is
sent a SIGXCPU signal which can be caught by the job. If you wish to allow a job to be "warned" so it can exit 
gracefully before it is killed then you should set the *s_vmem* limit to a lower value than *h_vmem*. For parallel 
processes, the limit is applied per slot which means that the limit is multiplied by the number of slots being used by
the job before being applied.

The remaining parameters in the queue configuration template specify per job soft and hard resource limits as 
implemented by the setrlimit(2) system call. See this manual page on your system for more information. By default, 
each limit field is set to infinity (which means RLIM_INFINITY as described in the setrlimit(2) manual page). The
value type for the CPU-time limits *s_cpu* and *h_cpu* is time. The value type for the other limits is memory. 
Note: Not all systems support setrlimit(2).

Note also: *s_vmem* and *h_vmem* (virtual memory) are only available on systems supporting RLIMIT_VMEM (see 
setrlimit(2) on your operating system).

# SEE ALSO

xxqs_name_sxx_intro(1), sge_types(1), csh(1), qconf(1), qmon(1), qrestart(1), qstat*(1), qsub*(1), sh(1), nice(2), 
setrlimit(2), xxqs_name_sxx_access_list(5), xxqs_name_sxx_calendar_conf(5), xxqs_name_sxx_conf(5), 
xxqs_name_sxx_complex(5), xxqs_name_sxx_host_conf(5), xxqs_name_sxx_sched_conf(5), xxqs_name_sxx_execd(8), 
xxqs_name_sxx_qmaster(8), xxqs_name_sxx_shepherd(8).

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
