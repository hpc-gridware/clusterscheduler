---
title: sge_access_list
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_access_list - xxQS_NAMExx access list file format

# DESCRIPTION

Access lists are used in xxQS_NAMExx to define access permissions of users to

* the cluster (see (x)user_lists in xxqs_name_sxx_conf(5))
* hosts (see (x)user_lists in xxqs_name_sxx_host_conf(5))
* queues (see (x)user_lists in xxqs_name_sxx_queue_conf(5))
* parallel environments (see (x)user_lists in xxqs_name_sxx_pe(5))
* cluster resources limited by resource quotas (see xxqs_name_sxx_resource_quote(5))

A list of currently configured access lists can be displayed via the `qconf -sul` option. The contents of each
enlisted access list can be shown via the `-su` switch. The output follows the xxqs_name_sxx_access_list(5) format
description. New access lists can be created and existing can be modified via the `-au` and `-du` options
to `qconf`.

Departments are a special form of access list that additionally allow assignment of 
functional shares and override tickets.

Note, xxQS_NAMExx allows backslashes (\\) be used to escape newline (\\newline) characters. The backslash and the 
newline are replaced with a space (" ") character before any interpretation.

Following predefined access lists gain access to specific functionalities of the xxQS_NAMExx system:

## arusers

Allows to submit, control and delete advance reservations (see qsub(1)).

## deadlineusers

Allows to specify a deadline for jobs (see qsub(1) -dl).

## defaultdepartment

The *defaultdepartment* is the default department for users that are not assigned to any other department.
In difference to other departments, users or groups of users must not explicitly be referenced in the *entries* field of the *defaultdepartment* so that a user can be part of the *defaultdepartment*.

# FORMAT

The following list of access list parameters specifies the access list content:

## *name*

The name of the access list as defined for *userset_name* in xxqs_name_sxx_types(1).

## *type*

The type of the access list, currently one of *ACL,* or *DEPT* or a combination of both in a comma separated list. 
Depending on this parameter the access list can be used as access list only or as a department.

## *oticket*

The amount of override tickets currently assigned to the department.

## *fshare*

The current functional share of the department.

## *entries*

The *entries* parameter contains the comma separated list of those UNIX users (see *user_name* in 
xxqs_name_sxx_types(1)) or those UNIX groups that are assigned to the access list or the department. 

By default, only a user's primary UNIX group is considered to gain/reject access. Supplementary groups are only 
considered if the parameter `ENABLE_SUP_GRP_EVAL=1` is defined as *qmaster_param* in the configuration. 

A group is differentiated from a username by prefixing the group name with a '@' sign. For UNIX groups that have no 
symbolic name it is allowed to specify the '@' character followed by the UNIX group ID.

Pure access lists allow enlisting any user or group in any access list.

When using departments, each user or group enlisted *should* only be enlisted in one department, in order to ensure a 
unique assignment of jobs to departments. It is allowed to enlist a user or group in multiple departments, but
this will automatically lead to a random assignment of jobs to departments if the submitting user of a job does not
specify a department explicitly using the `-dept` switch during job submission.

If a user does not belong to any department, the *defaultdepartment* is assigned, if existing.

Displaying commands like qstat(1) and qhost(1) allow to provide a department specific view by using the `-sdv` switch.
Those commands will then show hosts/queues/job information in the context of the department and access lists the user 
belongs to. 

# SEE ALSO

xxqs_name_sxx_intro(1), xxqs_name_sxx_types(1), qconf(1), qsub(1), xxqs_name_sxx_pe(5), xxqs_name_sxx_queue_conf(5).

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
