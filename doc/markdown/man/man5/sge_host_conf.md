---
title: sge_host_conf
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_host_conf - xxQS_NAMExx execution host configuration file format

# DESCRIPTION

xxqs_name_sxx_host_conf reflects the format of the template file for the execution host configuration. Via the 
`-ae` and `-me` options of the qconf(1) command, you can add execution hosts and modify the configuration of any 
execution host in the cluster. Default execution host entries are added automatically as soon as a 
xxqs_name_sxx_execd(8) registers to xxqs_name_sxx_qmaster(8) for the very first time from a certain host. The 
qconf(1) `-sel` switch can be used to display a list of execution host being currently configured in your 
xxQS_NAMExx system. Via the `-se` option you can print the execution host configuration of a specified host.

The special hostname *global* can be used to define cluster global characteristics.

Note, xxQS_NAMExx allows backslashes (\\) be used to escape newline (\\newline) characters. The backslash and 
the newline are replaced with a space (" ") character before any interpretation.

# FORMAT

The format of a xxqs_name_sxx_host_conf file is defined as follows:

## hostname

The execution hosts name as defined for *host_name* in xxqs_name_sxx_types(1).

## load_scaling

A comma separated list of scaling values to be applied to each or part of the load values being reported by the 
xxqs_name_sxx_execd(8) on the host and being defined in the cluster global "host" complex 
(see xxqs_name_sxx_complex(5)). The load scaling factors are intended to level hardware or operating system 
specific differences between execution hosts.

The syntax of a load factor specification is as follows: First the name of the load value (as defined in the 
"host" complex) is given and, separated by an equal sign, the load scaling value is provided. No blanks are 
allowed in between the load_scaling value string.

The parameter *load_scaling* is not meaningful for the definition of the "global" host.

## complex_values

*complex_values* defines quotas for resource attributes managed via this host. Each complex attribute is followed 
by an "=" sign and the value specification compliant with the complex attribute type (see xxqs_name_sxx_complex(5)). 
Quota specifications are separated by commas.

The quotas are related to the resource consumption of all jobs on a host in the case of consumable resources 
(see xxqs_name_sxx_complex(5) for details on consumable resources) or they are interpreted on a per job slot basis 
in the case of non-consumable resources. Consumable resource attributes are commonly used to manage free memory, 
free disk space or available floating software licenses while non-consumable attributes usually define distinctive 
characteristics like type of hardware installed.

For consumable resource attributes an available resource amount is determined by subtracting the current resource 
consumption of all running jobs on the host from the quota in the *complex_values* list. Jobs can only be dispatched 
to a host if no resource requests exceed any corresponding resource availability obtained by this scheme. The quota
definition in the *complex_values* list is automatically replaced by the current load value reported for this 
attribute, if load is monitored for this resource and if the reported load value is more stringent than the quota. 
This effectively avoids over subscription of resources.

Note: Load values replacing the quota specifications may have become more stringent because they have been scaled 
(see *load_scaling* above) and/or load adjusted (see xxqs_name_sxx_sched_conf(5)). The `-F` option of qstat(1) 
provide detailed information on the actual availability of consumable resources and on the origin of the values 
taken into account currently.

Note also: The resource consumption of running jobs (used for the availability calculation) as well as the 
resource requests of the jobs waiting to be dispatched either may be derived from explicit user requests during 
job submission (see the `-l` option to qsub(1)) or from a "default" value configured for an attribute by the 
administrator (see xxqs_name_sxx_complex(5)). The `-r` option to qstat(1) can be used for retrieving full detail 
on the actual resource requests of all jobs in the system.

For non-consumable resources xxQS_NAMExx simply compares the job's attribute requests with the corresponding 
specification in *complex_values* taking the relation operator of the complex attribute definition into account 
(see xxqs_name_sxx_complex(5)). If the result of the comparison is "true", the host is suitable for the job with 
respect to the particular attribute. For parallel jobs each job slot to be occupied by a parallel task is meant 
to provide the same resource attribute value.

Note: Only numeric complex attributes can be defined as consumable resources and hence non-numeric attributes 
are always handled on a per job slot basis.

The default value for this parameter is NONE, i.e. no administrator defined resource attribute quotas are 
associated with the host.

## load_values

This entry cannot be configured but is only displayed in case of a qconf(1) `-se` command. All load values are 
displayed as reported by the xxqs_name_sxx_execd(8) on the host. The load values are enlisted in a comma 
separated list. Each load value start with its name, followed by an equal sign and the reported value.

## processors

Note: Deprecated, may be removed in future release. This entry cannot be configured but is only displayed in 
case of a qconf(1) `-se` command. Its value is the number of processors which has been detected by 
xxqs_name_sxx_execd(8) on the corresponding host.

## usage_scaling

The format is equivalent to *load_scaling* (see above), the only valid attributes to be scaled however are 
*cpu* for CPU time consumption, *mem* for Memory consumption aggregated over the life-time of jobs and
*io* for data transferred via any I/O devices. The default NONE means "no scaling", i.e. all scaling factors are 1.

## user_lists

The *user_lists* parameter contains a comma separated list of so-called user access lists as described in 
xxqs_name_sxx_access_list(5). Each user contained in at least one of the enlisted access lists has access to the
host. If the *user_lists* parameter is set to NONE (the default) any user has access being not explicitly excluded 
via the *xuser_lists* parameter described below. If a user is contained both in an access list enlisted in 
*xuser_lists* and *user_lists* the user is denied access to the host.

## xuser_lists

The *xuser_lists* parameter contains a comma separated list of so-called user access lists as described in 
xxqs_name_sxx_access_list(5). Each user contained in at least one of the enlisted access lists is not allowed to
access the host. If the *xuser_lists* parameter is set to NONE (the default) any user has access. If a user is 
contained both in an access list enlisted in *xuser_lists* and *user_lists* the user is denied access to the host.

## projects

The *projects* parameter contains a comma separated list of projects that have access to the host. Any projects 
not in this list are denied access to the host. If set to NONE (the default), any project has access that is not 
specifically excluded via the *xprojects* parameter described below. If a project is in both the *projects* and
*xprojects* parameters, the project is denied access to the host.

## xprojects

The *xprojects* parameter contains a comma separated list of projects that are denied access to the host. If set 
to NONE (the default), no projects are denied access other than those denied access based on the *projects* 
parameter described above. If a project is in both the *projects* and *xprojects* parameters, the project is 
denied access to the host.

## report_variables

The *report_variables* parameter contains a comma separated list of variables that shall be written to the 
reporting file. The variables listed here will be written to the reporting file when a load report arrives from 
an execution host.

Default settings can be done in the global host. Host specific settings for report_variables will override 
settings from the global host.

# SEE ALSO

xxqs_name_sxx_intro(1), xxqs_name_sxx_types(1), qconf(1), uptime(1), xxqs_name_sxx_access_list(5), 
xxqs_name_sxx_complex(5), xxqs_name_sxx_execd(8), xxqs_name_sxx_qmaster(8).

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
