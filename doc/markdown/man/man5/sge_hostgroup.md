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

# QUEUE HOST GROUPS

Every cluster queue owns one host group which carries its host list. The group is named after the queue with a
doubled "@" as prefix: the host list of the cluster queue *all.q* is the member list of the host group
*@@all.q*. The *hostlist* line of a queue configuration (see xxqs_name_sxx_queue_conf(5)) and the member list
of that group are one and the same list, reachable through two sets of commands.

The prefix "@@" is reserved for these groups. It cannot be used for a host group of your own, because a host
group name is validated from its second character onwards and "@" is not allowed there (see *hostgroup_name*
in xxqs_name_sxx_types(1)).

## Lifetime

A queue host group is created together with its cluster queue and removed together with it. It can therefore
neither be added nor deleted on its own. Creating one with `-ahgrp` or `-Ahgrp` is rejected:

    "@@all.q" is maintained by the qmaster and is created with its cluster queue

and deleting one with `-dhgrp` or `-Dhgrp` likewise:

    "@@all.q" is maintained by the qmaster and is removed with its cluster queue

## Contents

The member list, in contrast, is modified like that of any other host group -- either through the group
itself with the `-mhgrp`, `-Mhgrp` and `-?attr` options of qconf(1), or through the *hostlist* line of the
cluster queue with `-mq` and `-Mq`. Both write the same object, and a member list may contain everything any
other host group member list may contain, subgroup references included.

## References

A queue host group belongs to its cluster queue and may not be named anywhere else -- neither in the member
list of another host group nor in the *hostlist* of another cluster queue. Such a reference is rejected:

    "@@all.q" belongs to a cluster queue and cannot be referenced

## Listing

qconf(1) `-shgrpl` lists queue host groups along with all others, so the listing holds one entry per cluster
queue in addition to the host groups defined by the administrator. The `-shgrp`, `-shgrp_tree` and
`-shgrp_resolved` options display them like any other group.

The name of a cluster queue is limited so that the name derived from it still fits the spool file name limit;
see *queue_name* in xxqs_name_sxx_types(1).

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
