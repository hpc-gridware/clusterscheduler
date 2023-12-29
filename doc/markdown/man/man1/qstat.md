---
title: qstat
section: 1
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

qstat - show the status of xxQS_NAMExx jobs and queues

# SYNTAX

**qstat** \[ **-ext** \] \[ **-f** \] \[ **-F**
\[**resource_name,...**\] \] \[ **-g {c\|d\|t}\[+\]** \] \[ **-help** \]
\[ **-j \[job_list\]** \] \[ **-l resource=val,...** \] \[ **-ne** \] \[
**-pe pe_name,...** \] \[ **-ncb** \] \[ **-pri** \] \[ **-q
wc_queue_list** \] \[ **-qs {a\|c\|d\|o\|s\|u\|A\|C\|D\|E\|S}** \] \[
**-r** \] \[ **-s {r\|p\|s\|z\|hu\|ho\|hs\|hd\|hj\|ha\|h\|a}\[+\]** \]
\[ **-t** \] \[ **-U user,...** \] \[ **-u user,...** \] \[ **-urg** \]
\[ **-xml** \]

# DESCRIPTION

*qstat* shows the current status of the available xxQS_NAMExx queues and
the jobs associated with the queues. Selection options allow you to get
information about specific jobs, queues or users. If multiple selections
are done a queue is only displayed if all selection criteria for a queue
instance are met. Without any option *qstat* will display only a list of
jobs with no queue status information.

The administrator and the user may define files (see *sge_qstat* (5)),
which can contain any of the options described below. A cluster-wide
sge_qstat file may be placed under
$xxQS_NAME_Sxx_ROOT/$xxQS_NAME_Sxx_CELL/common/sge_qstat The user
private file is searched at the location $HOME/.sge_qstat. The home
directory request file has the highest precedence over the cluster
global file. Command line can be used to override the flags contained in
the files.

# OPTIONS

-explain a\|A\|c\|E  
The character 'c' displays the reason for the c(onfiguration ambiguous)
state of a queue instance. 'a' shows the reason for the alarm state.
Suspend alarm state reasons will be displayed by 'A'. 'E' displays the
reason for a queue instance error state.

The output format for the alarm reasons is one line per reason
containing the resource value and threshold. For details about the
resource value please refer to the description of the **Full Format** in
section **OUTPUT FORMATS** below.

-ext  
Displays additional information for each job related to the job ticket
policy scheme (see OUTPUT FORMATS below).

-f  
Specifies a "full" format display of information. The **-f** option
causes summary information on all queues to be displayed along with the
queued job list.

-F \[ resource_name,... \]  
Like in the case of **-f** information is displayed on all jobs as well
as queues. In addition, *qstat* will present a detailed listing of the
current resource availability per queue with respect to all resources
(if the option argument is omitted) or with respect to those resources
contained in the resource_name list. Please refer to the description of
the **Full Format** in section **OUTPUT FORMATS** below for further
detail.

-g {c\|d\|t}\[+\]  
The **-g** option allows for controlling grouping of displayed objects.

With **-g c** a cluster queue summary is displayed. Find more
information in the section **OUTPUT FORMATS**.

With **-g d** array jobs are displayed verbosely in a one line per job
task fashion. By default, array jobs are grouped and all tasks with the
same status (for pending tasks only) are displayed in a single line. The
array job task id range field in the output (see section **OUTPUT
FORMATS**) specifies the corresponding set of tasks.

With **-g t** parallel jobs are displayed verbosely in a one line per
parallel job task fashion. By default, parallel job tasks are displayed
in a single line. Also with **-g t** option the function of each
parallel task is displayed rather than the jobs slot amount (see section
**OUTPUT FORMATS**).

-help  
Prints a listing of all options.

-j \[job_list\]  
Prints either for all pending jobs or the jobs contained in job_list
various information. The job_list can contain job_ids, job_names, or
wildcard expression *sge_types* (1).

For jobs in E(rror) state the error reason is displayed. For jobs that
could not be dispatched during in the last scheduling interval the
obstacles are shown, if 'schedd_job_info' in *sched_conf* (5) is
configured accordingly.

For running jobs available information on resource utilization is shown
about consumed cpu time in seconds, integral memory usage in Gbytes
seconds, amount of data transferred in io operations, current virtual
memory utilization in Mbytes, and maximum virtual memory utilization in
Mbytes. This information is not available if resource utilization
retrieval is not supported for the OS platform where the job is hosted.

In combination with **-cp** the output of this command will additionally
contain the information of a requested binding (see **-binding** of
*qsub* (1)) and the changes that have been applied to the topology
string (real binding) for the host where this job is running.

The topology string will contain capital letters for all those cores
that were not bound to the displayed job. Bound cores will be shown
lowercase (E.g "SCCcCSCCcC" means that core 2 on the two available
sockets where bound to this job).

Please refer to the file \<xxqs_name_sxx_root>/doc/load_parameters.asc
for detailed information on the standard set of load values.

-l resource\[=value\],...  
Defines the resources required by the jobs or granted by the queues on
which information is requested. Matching is performed on queues based on
non-mutable resource availability information only. That means load
values are always ignored except the so-called static load values (i.e.
"arch", "num_proc", "mem_total", "swap_total" and "virtual_total").
Consumable utilization is also ignored. The pending jobs are restricted
to jobs that might run in one of the above queues. In a similar fashion
also the queue-job matching bases only on non-mutable resource
availability information. If there are multiple -l resource requests
they will be concatenated by a logical AND: a queue needs to match all
resources to be displayed.

-ne  
In combination with **-f** the option suppresses the display of empty
queues. This means all queues where actually no jobs are running are not
displayed.

-ncb  
In combination with **-ncb** the output of a command will suppress
information of a requested binding and changes that have been applied to
the topology string (real binding) for the host where the job is
running. This information will disappear in combination with the
parameters **-r** and **-j**.

Please note that this command line switch is intended to provide
backward compatibility and will be removed in the next major release.

-pe pe_name,...  
Displays status information with respect to queues which are attached to
at least one of the parallel environments enlisted in the comma
separated option argument. Status information for jobs is displayed
either for those which execute in one of the selected queues or which
are pending and might get scheduled to those queues in principle.

-pri  
Displays additional information for each job related to the job
priorities in general. (see OUTPUT FORMATS below).

-q wc_queue_list  
Specifies a wildcard expression queue list to which job information is
to be displayed. Find the definition of **wc_queue_list** in
*sge_types* (1).

-qs {a\|c\|d\|o\|s\|u\|A\|C\|D\|E\|S}  
Allows for the filtering of queue instances according to state.

-r  
Prints extended information about the resource requirements of the
displayed jobs.

Please refer to the **OUTPUT FORMATS** sub-section **Expanded Format**
below for detailed information.

-s {p\|r\|s\|z\|hu\|ho\|hs\|hd\|hj\|ha\|h\|a}\[+\]  
Prints only jobs in the specified state, any combination of states is
possible. **-s prs** corresponds to the regular *qstat* output without
**-s** at all. To show recently finished jobs, use **-s z**. To display
jobs in user/operator/system/array-dependency hold, use the **-s
hu/ho/hs/hd** option. The **-s ha** option shows jobs which where
submitted with the *qsub* **-a** command. *qstat* **-s hj** displays all
jobs which are not eligible for execution unless the job has entries in
the job dependency list. *qstat* **-s h** is an abbreviation for *qstat*
**-s huhohshdhjha** and *qstat* **-s a** is an abbreviation for *qstat*
**-s psr** (see **-a**, **-hold_jid** and **-hold_jid_ad** options to
*qsub* (1)).

-t  
Prints extended information about the controlled sub-tasks of the
displayed parallel jobs. Please refer to the **OUTPUT FORMATS**
sub-section **Reduced Format** below for detailed information. Sub-tasks
of parallel jobs should not be confused with array job tasks (see **-g**
option above and **-t** option to *qsub* (1)).

-U user,...  
Displays status information with respect to queues to which the
specified users have access. Status information for jobs is displayed
either for those which execute in one of the selected queues or which
are pending and might get scheduled to those queues in principle.

-u user,...  
Display information only on those jobs and queues being associated with
the users from the given user list. Queue status information is
displayed if the **-f** or **-F** options are specified additionally and
if the user runs jobs in those queues.

The string ** $user** is a placeholder for the current username. An
asterisk "\*" can be used as username wildcard to request any users'
jobs be displayed. The default value for this switch is **-u $user**.

-urg  
Displays additional information for each job related to the job urgency
policy scheme (see OUTPUT FORMATS below).

-xml  
This option can be used with all other options and changes the output to
XML. The used schemas are referenced in the XML output. The output is
printed to stdout. For more detailed information, the schemas for the
qstat command can be found in $SGE_ROOT/util/resources/schemas/qstat.

If the **-xml** parameter is combined with **-ncb** then the XML output
does not contain tags with information about job to core binding. You
can also find schema files with the suffix "\_ncb" in the directory
$SGE_ROOT/util/resources/schemas/qstat that describe that changes.  

# OUTPUT FORMATS

Depending on the presence or absence of the **-explain**, **-f**,
**-F**, or **-qs** and **-r** and **-t** option three output formats
need to be differentiated.

The **-ext** and **-urg** options may be used to display additional
information for each job.

## **Cluster Queue Format (with -g c)**

Following the header line a section for each cluster queue is provided.
When queue instances selection are applied (-l -pe, -q, -U) the cluster
format contains only cluster queues of the corresponding queue
instances.

-   the cluster queue name.

-   an average of the normalized load average of all queue hosts. In
    order to reflect each hosts different significance the number of
    configured slots is used as a weighting factor when determining
    cluster queue load. Please note that only hosts with a np_load_value
    are considered for this value. When queue selection is applied only
    data about selected queues is considered in this formula. If the
    load value is not available at any of the hosts '-NA-' is printed
    instead of the value from the complex attribute definition.

-   the number of currently used slots.

-   the number of slots reserved in advance.

-   the number of currently available slots.

-   the total number of slots.

-   the number of slots which is in at least one of the states 'aoACDS'
    and in none of the states 'cdsuE'

-   the number of slots which are in one of these states or in any
    combination of them: 'cdsuE'

-   the **-g c** option can be used in combination with **-ext**. In
    this case, additional columns are added to the output. Each column
    contains the slot count for one of the available queue states.

## **Reduced Format (without -f, -F, and -qs)**

Following the header line a line is printed for each job consisting of

-   the job ID.

-   the priority of the job determining its position in the pending jobs
    list. The priority value is determined dynamically based on ticket
    and urgency policy set-up (see also *sge_priority* (5) ).

-   the name of the job.

-   the user name of the job owner.

-   the status of the job - one of d(eletion), E(rror), h(old),
    r(unning), R(estarted), s(uspended), S(uspended), t(ransfering),
    T(hreshold) or w(aiting).

The state d(eletion) indicates that a *qdel* (1) has been used to
initiate job deletion. The states t(ransfering) and r(unning) indicate
that a job is about to be executed or is already executing, whereas the
states s(uspended), S(uspended) and T(hreshold) show that an already
running jobs has been suspended. The s(uspended) state is caused by
suspending the job via the *qmod* (1) command, the S(uspended) state
indicates that the queue containing the job is suspended and therefore
the job is also suspended and the T(hreshold) state shows that at least
one suspend threshold of the corresponding queue was exceeded (see
*queue_conf* (5)) and that the job has been suspended as a consequence.
The state R(estarted) indicates that the job was restarted. This can be
caused by a job migration or because of one of the reasons described in
the -r section of the *qsub* (1) command.

The states w(aiting) and h(old) only appear for pending jobs. The h(old)
state indicates that a job currently is not eligible for execution due
to a hold state assigned to it via *qhold* (1), *qalter* (1) or the
*qsub* (1) **-h** option or that the job is waiting for completion of
the jobs to which job dependencies have been assigned to the job via the
**-hold_jid** or **-hold_jid-ad** options of *qsub* (1) or *qalter* (1).

The state E(rror) appears for pending jobs that couldn't be started due
to job properties. The reason for the job error is shown by the
*qstat* (1) **-j job_list** option.

-   the submission or start time and date of the job.

-   the queue the job is assigned to (for running or suspended jobs
    only).

-   the number of job slots or the function of parallel job tasks if
    **-g t** is specified.

Without **-g t** option the total number of slots occupied resp.
requested by the job is displayed. For pending parallel jobs with a PE
slot range request, the assumed future slot allocation is displayed.
With **-g t** option the function of the running jobs (MASTER or SLAVE -
the latter for parallel jobs only) is displayed.

-   the array job task id. Will be empty for non-array jobs. See the
    **-t** option to *qsub* (1) and the **-g** above for additional
    information.

If the **-t** option is supplied, each status line always contains
parallel job task information as if **-g t** were specified and each
line contains the following parallel job subtask information:

-   the parallel task ID (do not confuse parallel tasks with array job
    tasks),

-   the status of the parallel task - one of r(unning), R(estarted),
    s(uspended), S(uspended), T(hreshold), w(aiting), h(old), or
    x(exited).

-   the cpu, memory, and I/O usage,

-   the exit status of the parallel task,

-   and the failure code and message for the parallel task.

## **Full Format (with -f and -F)**

Following the header line a section for each queue separated by a
horizontal line is provided. For each queue the information printed
consists of

-   the queue name,

-   the queue type - one of B(atch), I(nteractive), C(heckpointing),
    P(arallel) or combinations thereof or N(one),

-   the number of used and available job slots,

-   the load average of the queue host,

-   the architecture of the queue host and

-   the state of the queue - one of u(nknown) if the corresponding
    *xxqs_name_sxx_execd* (8) cannot be contacted, a(larm), A(larm),
    C(alendar suspended), s(uspended), S(ubordinate), d(isabled),
    D(isabled), E(rror) or combinations thereof.

If the state is a(larm) at least on of the load thresholds defined in
the *load_thresholds* list of the queue configuration (see
*queue_conf* (5)) is currently exceeded, which prevents from scheduling
further jobs to that queue.

As opposed to this, the state A(larm) indicates that at least one of the
suspend thresholds of the queue (see *queue_conf* (5)) is currently
exceeded. This will result in jobs running in that queue being
successively suspended until no threshold is violated.

The states s(uspended) and d(isabled) can be assigned to queues and
released via the *qmod* (1) command. Suspending a queue will cause all
jobs executing in that queue to be suspended.

The states D(isabled) and C(alendar suspended) indicate that the queue
has been disabled or suspended automatically via the calendar facility
of xxQS_NAMExx (see *calendar_conf* (5)), while the S(ubordinate) state
indicates, that the queue has been suspend via subordination to another
queue (see *queue_conf* (5) for details). When suspending a queue
(regardless of the cause) all jobs executing in that queue are suspended
too.

If an E(rror) state is displayed for a queue, *xxqs_name_sxx_execd* (8)
on that host was unable to locate the *xxqs_name_sxx_shepherd* (8)
executable on that host in order to start a job. Please check the error
logfile of that *xxqs_name_sxx_execd* (8) for leads on how to resolve
the problem. Please enable the queue afterwards via the **-c** option of
the *qmod* (1) command manually.

If the c(onfiguration ambiguous) state is displayed for a queue instance
this indicates that the configuration specified for this queue instance
in *sge_conf* (5) is ambiguous. This state is cleared when the
configuration becomes unambiguous again. This state prevents further
jobs from being scheduled to that queue instance. Detailed reasons why a
queue instance entered the c(onfiguration ambiguous) state can be found
in the *sge_qmaster* (8) messages file and are shown by the qstat
-explain switch. For queue instances in this state the cluster queue's
default settings are used for the ambiguous attribute.

If an o(rphaned) state is displayed for a queue instance, it indicates
that the queue instance is no longer demanded by the current cluster
queue's configuration or the host group configuration. The queue
instance is kept because jobs which not yet finished jobs are still
associated with it, and it will vanish from qstat output when these jobs
have finished. To quicken vanishing of an orphaned queue instance
associated job(s) can be deleted using *qdel* (1). A queue instance in
(o)rphaned state can be revived by changing the cluster queue
configuration accordingly to cover that queue instance. This state
prevents from scheduling further jobs to that queue instance.

If the **-F** option was used, resource availability information is
printed following the queue status line. For each resource (as selected
in an option argument to **-F** or for all resources if the option
argument was omitted) a single line is displayed with the following
format:

-   a one letter specifier indicating whether the current resource
    availability value was dominated by either  
    \`**g**' - a cluster global,  
    \`**h**' - a host total or  
    \`**q**' - a queue related resource consumption.

-   a second one letter specifier indicating the source for the current
    resource availability value, being one of  
    \`**l**' - a load value reported for the resource,  
    \`**L**' - a load value for the resource after administrator defined
    load scaling has been applied,  
    \`**c**' - availability derived from the consumable resources
    facility (see *complexes* (5)),  
    \`**f**' - a fixed availability definition derived from a
    non-consumable complex attribute or a fixed resource limit.

-   after a colon the name of the resource on which information is
    displayed.

-   after an equal sign the current resource availability value.

The displayed availability values and the sources from which they derive
are always the minimum values of all possible combinations. Hence, for
example, a line of the form "qf:h_vmem=4G" indicates that a queue
currently has a maximum availability in virtual memory of 4 Gigabyte,
where this value is a fixed value (e.g. a resource limit in the queue
configuration) and it is queue dominated, i.e. the host in total may
have more virtual memory available than this, but the queue doesn't
allow for more. Contrarily a line "hl:h_vmem=4G" would also indicate an
upper bound of 4 Gigabyte virtual memory availability, but the limit
would be derived from a load value currently reported for the host. So
while the queue might allow for jobs with higher virtual memory
requirements, the host on which this particular queue resides currently
only has 4 Gigabyte available.

If the **-explain** option was used with the character 'a' or 'A',
information about resources is displayed, that violate load or suspend
thresholds.  
The same format as with the **-F** option is used with following
extensions:

-   the line starts with the keyword \`alarm'  

-   appended to the resource value is the type and value of the
    appropriate threshold

After the queue status line (in case of **-f**) or the resource
availability information (in case of **-F**) a single line is printed
for each job running currently in this queue. Each job status line
contains

-   the job ID,

-   the priority of the job determining its position in the pending jobs
    list. The priority value is determined dynamically based on ticket
    and urgency policy set-up (see also *sge_priority* (5) ).

-   the job name,

-   the job owner name,

-   the status of the job - one of t(ransfering), r(unning),
    R(estarted), s(uspended), S(uspended) or T(hreshold) (see the
    **Reduced Format** section for detailed information),

-   the submission or start time and date of the job.

-   the number of job slots or the function of parallel job tasks if
    **-g t** is specified.

Without **-g t** option the number of slots occupied per queue resp.
requested by the job is displayed. For pending parallel jobs with a PE
slot range request, the assumed future slot allocation is displayed.
With **-g t** option the function of the running jobs (MASTER or SLAVE -
the latter for parallel jobs only) is displayed.

If the **-t** option is supplied, each job status line also contains

-   the task ID,

-   the status of the task - one of r(unning), R(estarted), s(uspended),
    S(uspended), T(hreshold), w(aiting), h(old), or x(exited) (see the
    **Reduced Format** section for detailed information),

-   the cpu, memory, and I/O usage,

-   the exit status of the task,

-   and the failure code and message for the task.

Following the list of queue sections a *PENDING JOBS* list may be
printed in case jobs are waiting for being assigned to a queue. A status
line for each waiting job is displayed being similar to the one for the
running jobs. The differences are that the status for the jobs is
w(aiting) or h(old), that the submit time and date is shown instead of
the start time and that no function is displayed for the jobs.

In very rare cases, e.g. if *xxqs_name_sxx_qmaster* (8) starts up from
an inconsistent state in the job or queue spool files or if the **clean
queue** (**-cq**) option of *qconf* (1) is used, *qstat* cannot assign
jobs to either the running or pending jobs section of the output. In
this case as job status inconsistency (e.g. a job has a running status
but is not assigned to a queue) has been detected. Such jobs are printed
in an *ERROR JOBS* section at the very end of the output. The ERROR JOBS
section should disappear upon restart of *xxqs_name_sxx_qmaster* (8).
Please contact your xxQS_NAMExx support representative if you feel
uncertain about the cause or effects of such jobs.

## **Expanded Format (with -r)**

If the **-r** option was specified together with *qstat*, the following
information for each displayed job is printed (a single line for each of
the following job characteristics):

-   The job and master queue name.

-   The hard and soft resource requirements of the job as specified with
    the *qsub* (1) **-l** option. The per resource addend when
    determining the jobs urgency contribution value is printed (see also
    *sge_priority* (5)).

-   The requested parallel environment including the desired queue slot
    range (see **-pe** option of *qsub* (1)).

-   The requested checkpointing environment of the job (see the
    *qsub* (1) **-ckpt** option).

-   In case of running jobs, the granted parallel environment with the
    granted number of queue slots.

-   The requested job binding parameters.

## **Enhanced Output (with -ext)**

For each job the following additional items are displayed:

ntckts  
The total number of tickets in normalized fashion.

project  
The project to which the job is assigned as specified in the *qsub* (1)
**-P** option.

department  
The department, to which the user belongs (use the **-sul** and **-su**
options of *qconf* (1) to display the current department definitions).

cpu  
The current accumulated CPU usage of the job in seconds.

mem  
The current accumulated memory usage of the job in Gbytes seconds.

io  
The current accumulated IO usage of the job.

tckts  
The total number of tickets assigned to the job currently

ovrts  
The override tickets as assigned by the **-ot** option of *qalter* (1).

otckt  
The override portion of the total number of tickets assigned to the job
currently

ftckt  
The functional portion of the total number of tickets assigned to the
job currently

stckt  
The share portion of the total number of tickets assigned to the job
currently

share  
The share of the total system to which the job is entitled currently.

## **Enhanced Output (with -urg)**

For each job the following additional urgency policy related items are
displayed (see also *sge_priority* (5)):

nurg  
The jobs total urgency value in normalized fashion.

urg  
The jobs total urgency value.

rrcontr  
The urgency value contribution that reflects the urgency that is related
to the jobs overall resource requirement.

wtcontr  
The urgency value contribution that reflects the urgency related to the
jobs waiting time.

dlcontr  
The urgency value contribution that reflects the urgency related to the
jobs deadline initiation time.

deadline  
The deadline initiation time of the job as specified with the *qsub* (1)
**-dl** option.

## **Enhanced Output (with -pri)**

For each job, the following additional job priority related items are
displayed (see also *sge_priority* (5)):

nurg  
The job's total urgency value in normalized fashion.

npprior  
The job's **-p** priority in normalized fashion.

ntckts  
The job's ticket amount in normalized fashion.

ppri  
The job's **-p** priority as specified by the user.

# ENVIRONMENTAL VARIABLES

xxQS_NAME_Sxx_ROOT  
Specifies the location of the xxQS_NAMExx standard configuration files.

xxQS_NAME_Sxx_CELL  
If set, specifies the default xxQS_NAMExx cell. To address a xxQS_NAMExx
cell *qstat* uses (in the order of precedence):

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

SGE_LONG_QNAMES  
Qstat does display queue names up to 30 characters. If that is to much
or not enough, one can set a custom length with this variable. The
minimum display length is 10 characters. If one does not know the best
display length, one can set SGE_LONG_QNAMES to -1 and qstat will figure
out the best length.

# FILES

    <xxqs_name_sxx_root>/<cell>/common/act_qmaster
    	xxQS_NAMExx master host file
    <xxqs_name_sxx_root>/<cell>/common/xxqs_name_sxx_qstat
    	cluster qstat default options
    $HOME/.xxqs_name_sxx_qstat	
    	user qstat default options

# SEE ALSO

*xxqs_name_sxx_intro* (1), *qalter* (1), *qconf* (1), *qhold* (1),
*qhost* (1), *qmod* (1), *qsub* (1), *queue_conf* (5),
*xxqs_name_sxx_execd* (8), *xxqs_name_sxx_qmaster* (8),
*xxqs_name_sxx_shepherd* (8).

# COPYRIGHT

See *xxqs_name_sxx_intro* (1) for a full statement of rights and
permissions.
