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

A host group entry is used to merge host names to groups. Each host group entry file defines one group. Inside 
a group definition file you can also reference to groups. These groups are called subgroups. A subgroup is 
referenced by the sign "@" as first character of the name.

A list of currently configured host group entries can be displayed via the qconf(1) `-shgrpl` option. 
The contents of each enlisted host group entry can be shown via the `-shgrp` switch. The output follows
the xxqs_name_sxx_hostgroup format description. New host group entries can be created and existing can be 
modified via the `-ahgrp`, `-mhgrp`, `-dhgrp` and `-?attr` options to qconf(1).

Note, xxQS_NAMExx allows backslashes (\\) be used to escape newline (\\newline) characters. The backslash and 
the newline are replaced with a space (" ") character before any interpretation.

# FORMAT

A host group entry contains following parameters:

## group_name

The group_name defines the host group name. Host group names have to begin with an '@' character as explained 
for *hostgroup_name* in xxqs_name_sxx_types(5).

## hostlist

The name of all hosts and host groups (see *host_identifier* in xxqs_name_sxx_types(1)) which are member of the 
group. As list separators white-spaces are supported only. Default value for this parameter is NONE.

Note, if the first character of the *host_identifier* is an "@" sign the name is used to reference a 
xxqs_name_sxx_hostgroup(5) which is taken as subgroup of this group.

# RESERVED HOST GROUPS

Three host group names are reserved by xxQS_NAMExx and are created automatically when
xxqs_name_sxx_qmaster(8) starts. **None of the three can be deleted**, and a cluster that already owns a
user-defined host group under one of these names cannot be upgraded until it is renamed (see the upgrade
notes).

## @admin_hosts

The administrative host list -- a host must be a member to run administrative xxQS_NAMExx commands. It is
modified through qconf(1) `-ah`/`-dh` as well as through the ordinary host group options, and displayed by
`-sh`. The host running xxqs_name_sxx_qmaster(8) cannot be removed from it.

## @submit_hosts

The list of hosts allowed to submit jobs. Modified through qconf(1) `-as`/`-ds` as well as the ordinary host
group options, and displayed by `-ss`.

## @exec_hosts

The set of configured execution hosts, excluding the *global* and *template* pseudo-hosts. This group is
**maintained by the system** and is therefore **read-only for every user, including managers**: it is
recomputed from the execution host list whenever a host is added or removed and rebuilt at every qmaster
startup, so any attempt to modify it via `-mhgrp`, `-Mhgrp` or the `-?attr` options is rejected. Reference it
from a cluster queue's *hostlist* to have the queue follow the execution host list automatically.

This asymmetry is deliberate: *@admin_hosts* and *@submit_hosts* are yours to edit, *@exec_hosts* is derived.

# EXAMPLE

This is a typical host group entry:

    group_name @bigMachines
    hostlist @solaris64 @solaris32 fangorn balrog

The entry will define a new host group called *@bigMachines*. In this host group are the host *fangorn*, *balrog* 
and all members of the host groups *@solaris64* and *@solaris32*.

# SEE ALSO

xxqs_name_sxx\_\_types(1), qconf(1)

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
