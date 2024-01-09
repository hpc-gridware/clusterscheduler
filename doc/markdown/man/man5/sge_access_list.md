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

Access lists are used in xxQS_NAMExx to define access permissions of
users to queues (see *xxqs_name_sxx_queue_conf*(5)) or parallel environments (see
*xxqs_name_sxx_pe*(5)). A list of currently configured access lists can
be displayed via the *qconf*(1) **-sul** option. The contents of each
enlisted access list can shown via the **-su** switch. The output
follows the *access_list* format description. New access lists can be
created and existing can be modified via the **-au** and **-du** options
to *qconf*(1).

Departments are a special form of access list that additionally allow
assignment of functional shares and override tickets.

Note, xxQS_NAMExx allows backslashes (\\) be used to escape newline
(\\newline) characters. The backslash and the newline are replaced with
a space (" ") character before any interpretation.

# FORMAT

The following list of *access_list* parameters specifies the
*access_list* content:

## **name**

The name of the access list as defined for *userset_name* in
*sge_types*(1).

## **type**

The type of the access list, currently one of *ACL,* or *DEPT* or a
combination of both in a comma separated list. Depending on this
parameter the access list can be used as access list only or as a
department.

## **oticket**

The amount of override tickets currently assigned to the department.

## **fshare**

The current functional share of the department.

## **entries**

The entries parameter contains the comma separated list of those UNIX
users (see *user_name* in *sge_types*(1)) or those primary UNIX groups
that are assigned to the access list or the department. Only a user's
primary UNIX group is used; secondary groups are ignored. Only symbolic
names are allowed. A group is differentiated from a user name by
prefixing the group name with a '@' sign. Pure access lists allow
enlisting any user or group in any access list.

When using departments, each user or group enlisted may only be enlisted
in one department, in order to ensure a unique assignment of jobs to
departments. To jobs whose users do not match with any of the users or
groups enlisted under entries the *defaultdepartment* is assigned, if
existing.

# SEE ALSO

*xxqs_name_sxx_intro*(1), *sge_types*(1), *qconf*(1),
*xxqs_name_sxx_pe*(5), *xxqs_name_sxx_queue_conf*(5).

# COPYRIGHT

See *xxqs_name_sxx_intro*(1) for a full statement of rights and
permissions.
