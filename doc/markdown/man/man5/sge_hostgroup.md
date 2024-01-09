---
title: sge_hostgroup
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_hostgroup - host group entry file format

# DESCRIPTION

A host group entry is used to merge host names to groups. Each host
group entry file defines one group. Inside a group definition file you
can also reference to groups. These groups are called subgroups. A
subgroup is referenced by the sign "@" as first character of the name.

A list of currently configured host group entries can be displayed via
the *qconf*(1) **-shgrpl** option. The contents of each enlisted host
group entry can be shown via the **-shgrp** switch. The output follows
the *hostgroup* format description. New host group entries can be
created and existing can be modified via the **-ahgrp**, **-mhgrp**,
**-dhgrp** and **-?attr** options to *qconf*(1).

Note, xxQS_NAMExx allows backslashes (\\) be used to escape newline
(\\newline) characters. The backslash and the newline are replaced with
a space (" ") character before any interpretation.

# FORMAT

A host group entry contains following parameters:

## **group_name**

The group_name defines the host group name. Host group names have to
begin with an '@' character as explained for *hostgroup_name* in
*sge_types*(5).

## **hostlist**

The name of all hosts and host groups (see *host_identifier* in
*sge_types*(1)) which are member of the group. As list separators
white-spaces are supported only. Default value for this parameter is
NONE.

Note, if the first character of the *host_identifier* is an "@" sign the
name is used to reference a *hostgroup*(5) which is taken as sub group
of this group.

# EXAMPLE

This is a typical host group entry:

group_name @bigMachines

hostlist @solaris64 @solaris32 fangorn balrog

The entry will define a new host group called **@bigMachines**. In this
host group are the host **fangorn**, **balrog** and all members of the
host groups **@solaris64** and **@solaris32**.

# SEE ALSO

*xxqs_name_sxx\_\_types*(1), *qconf*(1)

# COPYRIGHT

See *xxqs_name_sxx_intro*(1) for a full statement of rights and
permissions.
