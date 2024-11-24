---
title: qhost
section: 1
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

`qhost` - show the status of xxQS_NAMExx hosts, queues, jobs

# SYNTAX

`qhost` \[`-F` \[*resource_name*, ...\]\] \[`-help`\] \[`-h` *host_list*\] \[`-j`\] \[`-l` *resource*=*val*, ...\] \[`-ncb`\] \[`-sdv`\] \[`-u` *user*, ...\] \[`-xml`\].

# DESCRIPTION

`qhost` shows the current status of the available xxQS_NAMExx hosts, queues and the jobs associated with the queues. Selection options allow you to get or filter information about specific hosts, queues, jobs or users. If multiple selections are done a host is only displayed if all selection criteria for a host are met. Without any options `qhost` will display a list of all hosts without queue or job information.

The administrator and the user may define files (see xxqs_name_sxx_qhost(5)), which can contain any of the options described below. A cluster-wide xxqs_name_sxx_qhost file may be placed under $xxQS_NAME_Sxx_ROOT/$xxQS_NAME_Sxx_CELL/common/xxqs_name_sxx_qhost. The user private file is searched at the location $HOME/.sge_qhost. The home directory request file has the highest precedence over the cluster global file. Command 
line can be used to override the flags contained in the files.

# OPTIONS

## -F \[*resource_name*, ...\]  
`qhost` will present a detailed listing of the current resource availability per host with respect to all resources (if the option argument is omitted) or with respect to those resources contained in the *resource_name* list. Please refer to the description of the *Full Format* in section *OUTPUT FORMATS* below for further detail.

## -help  
Prints a listing of all options.

## -h *host_list*  
Prints a list of all hosts contained in *host_list*.

## -j  
Prints all jobs running on the queues hosted by the shown hosts. This switch calls `-q` implicitly. If combined with the `-sdv` option the output will be restricted to those hosts and queues where the user has access rights and to jobs that are owned by the user or by a user that is in the same department as the user that executes `qhost`.

## -l *resource*\[=*value*\],...  
Defines the resources to be granted by the hosts which should be included in the host list output. Matching is performed on hosts based on non-mutable resource availability information only. That means load values are always ignored except the so-called static load values (i.e. *arch*, *num_proc*, *mem_total*, *swap_total* and *virtual_total*) ones. Also, consumable utilization is ignored. If there are multiple `-l` resource requests they will be concatenated by a logical *AND*: a host needs to match all resources to be displayed.

## -ncb  
(Deprecated)

This command line switch can be used in order to get 6.2u5 compatible output with other `qhost` command line switches. In that case the output of the corresponding command will suppress information concerning the execution host topology. Note that this option will be removed in the next major version.

## -q  
Show information about the queues instances hosted by the displayed hosts. In combination with the `-sdv` option the output will be restricted to those hosts and queues where the user has access rights.

## -sdv
This switch is available in Gridware Cluster Scheduler only.

This option is only effective if the executing user has no administrative rights.

If the `-sdv` option is used, the output of the command will be restricted to a department specific view. This means that following parts of the output will be suppressed:

* host information where the user has no access rights
* queue instance information where the user has no access rights
* queue instance information if the corresponding host is not accessible by the user
* jobs that are not owned by the user or by a user that is in the same department as the user that executes `qhost`
* job information if the host or queue instance is not accessible by the user 

Please be aware that changing access permissions for a user that has active jobs in a queue or on a host where access is removed will not affect the running jobs. The jobs will continue to run until they finish, but this will lead to a situation where the job owner cannot see the jobs anymore in case the department view is enforced.

The department specific view can be enforced by adding the `-sdv` switch to the $HOME/.sge_qhost file. Administrators can enforce this behavior by adding this switch to the default sge_qhost file.

## -u *user*, ...  
Display information only on those jobs and queues being associated with the users from the given user list.

## -xml  
This option can be used with all other options and changes the output to XML. The used schemas are referenced in the XML output. The output is printed to stdout.

If the `-xml` parameter is combined with `-ncb` then the XML output will contain 6.2u5 compatible output. In combination with the `-sdv` option the output will be restricted to those hosts and queues where the user has access rights and to jobs that are owned by the user or by a user that is in the same department as the user that executes `qhost`.

# OUTPUT FORMATS

Depending on the presence or absence of the `-q` or `-F` and `-j` option three output formats need to be differentiated. The `-sdv` option will restrict the output to a department specific view where non-administrative users will only see information about hosts, queues and jobs where they have access rights or where the jobs are owned by users in the same department.

## Default Format (without `-q`, `-F` and `-j`)

For each host one line is printed. The output consists of consisting of:

* the hostname 
* the architecture. 
* the number of total cores.
* the number of sockets.
* the number of cores.
* the number of threads.
* the load. 
* the total memory. 
* the used memory. 
* the total swap space. 
* the Used swap space.

If the `-q` option is supplied, each host status line also contains extra lines for every queue hosted by the host consisting of,

* the queue name. 
* the queue type - one of B(atch), I(nteractive), C(heckpointing), P(arallel) or combinations thereof, 
* the number of reserved, used and available job slots, 
* the state of the queue - one of u(nknown) if the corresponding xxqs_name_sxx_execd(8) cannot be contacted, 
  a(larm), A(larm), C(alendar suspended), s(uspended), S(ubordinate), d(isabled), D(isabled), E(rror)
  or combinations thereof.

If the state is a(alarm) at least one of the load thresholds defined in the *load_thresholds* list of the queue configuration (see xxqs_name_sxx_queue_conf(5)) is currently exceeded, which prevents from scheduling further jobs to that queue.

As opposed to this, the state A(larm) indicates that at least one of the suspend thresholds of the queue (see xxqs_name_sxx_queue_conf(5)) is currently exceeded. This will result in jobs running in that queue being successively suspended until no threshold is violated.

The states s(uspended) and d(isabled) can be assigned to queues and released via the qmod(1) command. Suspending a queue will cause all jobs executing in that queue to be suspended.

The states D(isabled) and C(alendar suspended) indicate that the queue has been disabled or suspended automatically via the calendar facility of xxQS_NAMExx (see xxqs_name_sxx_calendar_conf(5)), while the S(ubordinate) state indicates, that the queue has been suspended via subordination to another queue (see xxqs_name_sxx_queue_conf(5) for details). When suspending a queue (regardless of the cause) all jobs executing in that queue are suspended too.

If an E(rror) state is displayed for a queue, xxqs_name_sxx_execd(8) on that host was unable to locate the xxqs_name_sxx_shepherd(8) executable on that host in order to start a job. Please check the error logfile of that xxqs_name_sxx_execd(8) for leads on how to resolve the problem. Please enable the queue afterward via the `-c` option of the qmod(1) command manually.

If the `-F` option was used, resource availability information is printed following the host status line. For each resource (as selected in an option argument to `-F` or for all resources if the option argument was omitted) a single line is displayed with the following format:

* a one letter specifier indicating whether the current resource availability value was dominated by either  
    * **g** - a cluster global,  
    * **h** - a host total
* a second one letter specifier indicating the source for the current resource availability value, being one of  
    * **l** - a load value reported for the resource,  
    * **L** - a load value for the resource after administrator defined load scaling has been applied,  
    * **c** - availability derived from the consumable resources facility (see xxqs_name_sxx_complexes(5)),  
    * **f** - a fixed availability definition derived from a non-consumable complex attribute or a fixed resource limit. 
* after a colon the name of the resource on which information is displayed.
* after an equal sign the current resource availability value.

The displayed availability values and the sources from which they derive are always the minimum values of all possible combinations. Hence, for example, a line of the form *qf:h_vmem=4G* indicates that a queue currently has a maximum availability in virtual memory of 4 Gigabyte, where this value is a fixed value (e.g. a resource limit in the queue configuration) and it is queue dominated, i.e. the host in total may have more virtual memory available than this, but the queue doesn't allow for more. Contrarily a line "hl:h_vmem=4G" would also indicate an upper bound of 4 Gigabyte virtual memory availability, but the limit would be derived from a load value currently reported for the host. So while the queue might allow for jobs with higher virtual memory requirements, the host on which this particular queue resides currently only has 4 Gigabyte available.

After the queue status line (in case of `-j`) a single line is printed for each job running currently in this queue. Each job status line contains

* the job ID,
* the job name,
* the job owner name,
* the status of the job - one of t(ransfering), r(unning), R(estarted), s(uspended), S(uspended) or T(hreshold) 
  (see the *Reduced Format* section in qstat(1) for detailed information),
* the start date and time and the function of the job (*MASTER* or *SLAVE* - only meaningful in case of a parallel job) 
  and the priority of the jobs.

# ENVIRONMENTAL VARIABLES

For a complete list of common environment variables used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# FILES

For a complete list of common environment variables used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# SEE ALSO

xxqs_name_sxx_intro(1), qalter(1), qconf(1), qhold(1), qmod(1), qstat(1), qsub(1), xxqs_name_sxx_qhost(5), xxqs_name_sxx_queue_conf(5), xxqs_name_sxx_execd(8), xxqs_name_sxx_qmaster(8), xxqs_name_sxx_shepherd(8).

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
