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

An entry that begins with the reserved prefix *host:* is a **matcher** and is described below.

# MATCHERS

A member of a host group is one of three things, and which one is decided by how it is written:

    @name       a reference to another host group
    host:gpu*   a matcher -- describes a set of hosts
    gpu001      a host name -- names exactly one host

An entry beginning with an "@" is a group reference; that rule comes first. An entry beginning
with one of the reserved prefixes is a matcher. Everything else is a host name.

A **matcher describes** a set of hosts instead of naming one. Every host whose name matches the
pattern is a member, whether or not the cluster has ever heard of that host:

    $ qconf -shgrp @gpu_nodes
    group_name @gpu_nodes
    hostlist host-0002.lab.hpc-gridware.com host:gpu*

Here *host-0002* is a member because it is named, and every host called *gpu...* is a member
because it is described. The second kind needs no administrative act per host, which is what
matchers exist for: a node that has just booted can be a member before anybody has entered it.

Put a matcher in quotes on the command line. It contains a '\*', and an unquoted one is replaced
by file names from the working directory before xxQS_NAMExx ever sees it.

The prefixes **ip:** and **ip6:** are reserved as well -- in every field of the system that takes
a host name, not only here -- but are not supported in this version:

    $ qconf -aattr hostgroup hostlist 'ip:10.0.0.0/8' @gpu_nodes
    address matchers ("ip:") are not yet supported in this version: "ip:10.0.0.0/8"

They are reserved now so that address ranges can be added later without inventing a second
notation. A host that is genuinely called *ip:something* is therefore not expressible, which is
the price of a prefix that can be recognised without ambiguity.

## What happens to a matcher when it is written

A matcher is normalised when it is stored, exactly as a host name is resolved when it is stored,
and for the same reason: so that two spellings of the same thing become one. The domain handling
in force -- *ignore_fqdn* and *default_domain*, see xxqs_name_sxx_bootstrap(5) -- is applied to
the pattern, and every change is reported:

    $ qconf -aattr hostgroup hostlist 'host:gpu*.lab.hpc-gridware.com' @gpu_nodes
    NOTE: "host:gpu*.lab.hpc-gridware.com" was truncated to "host:gpu*" (ignore_fqdn is set).
    No modification because "host:gpu*" already exists in "hostlist" of "hostgroup"

That example shows both halves: the pattern was truncated, and the result was already a member,
so it was collapsed rather than added twice. Duplicates are collapsed for host names in the same
way. A note does not make the command fail -- the change was carried out.

Writing the stored form back changes nothing and says nothing. The round trip is idempotent, not
character for character equal, which has always been true of host names too.

## What is refused

A matcher whose pattern is empty describes nothing:

    $ qconf -aattr hostgroup hostlist 'host:' @gpu_nodes
    "host:" has an empty pattern; a matcher has to describe something.

A pattern that matches **every** host is far-reaching but legitimate, and it is carried out with
a warning:

    $ qconf -aattr hostgroup hostlist 'host:*' @login_nodes
    WARNING: "host:*" admits every host. @login_nodes thereby confers a standing grant on every
    host that can reach the qmaster.

What is refused is a catch-all **nobody asked for** -- one that arises only because the
normalisation truncated the pattern:

    $ qconf -aattr hostgroup hostlist 'host:*.lab.hpc-gridware.com' @login_nodes
    "host:*.lab.hpc-gridware.com" is truncated to "host:*" when ignore_fqdn is set and then admits
    every host. Write the catch-all itself if that is what you want.

**The detection of a catch-all has a limit, and it is worth knowing.** It recognises a pattern
that is *literally* a catch-all. It does not evaluate what a pattern happens to cover in a
particular installation. `host:[a-z0-9-]*` matches every name that can occur in the DNS and is
not detected; neither is `host:*.example.com` in an installation where every host lives in
*example.com*. A matcher is a grant -- read it before you write it.

## The resolved membership is not the membership

`qconf -shgrp` shows the definition. `qconf -shgrp_resolved` shows which of the hosts the system
*knows about* are members -- a derivation, and for a group carrying matchers the two are
different things:

    $ qconf -shgrp_resolved @gpu_nodes
    host-0002.lab.hpc-gridware.com
    matchers: host:gpu*

The extra line is there because the list above it must never be read on its own as "the
membership". It says who is a member among the hosts that are configured; it cannot say who
*would* be admitted, because no operation anywhere returns the names matching a pattern.

For a group whose population is disjoint from the configured hosts, the resolved membership is
legitimately **empty** while the group admits on every request:

    $ qconf -shgrp_resolved @login_nodes
    matchers: host:login*

That is not a misconfiguration and not an error. `qconf -shgrp_tree` marks a matcher as such and
shows it as a leaf -- it references nothing that could be expanded:

    $ qconf -shgrp_tree @gpu_nodes
    @gpu_nodes
       host-0002.lab.hpc-gridware.com
       host:gpu* (matcher)

To ask about one host rather than the set, use `qconf -shgrp_why` (see xxqs_name_sxx_conf(1)).

# RESERVED HOST GROUPS

Three host group names are reserved by xxQS_NAMExx and are created automatically when
xxqs_name_sxx_qmaster(8) starts. **None of the three can be deleted**, and a cluster that already owns a
user-defined host group under one of these names cannot be upgraded until it is renamed (see the upgrade
notes).

## @admin_hosts

The administrative host list -- a host must be a member to run administrative xxQS_NAMExx commands. It is
modified through qconf(1) `-ah`/`-dh` as well as through the ordinary host group options, and displayed by
`-sh`. The host running xxqs_name_sxx_qmaster(8) cannot be removed from it.

A matcher is allowed here, and it changes what this list *is*. Until now the member list **was** the
record of who holds administrative rights: it enumerated them, and every entry was put there by
somebody. A matcher turns it into a rule, and the qmaster says so when one is introduced:

    $ qconf -ah 'host:mgmt*'
    WARNING: @admin_hosts now contains a matcher. Every host whose name matches "mgmt*" is thereby
    admitted as an administrative host without any further administrative act: host:mgmt*

    host:mgmt* added to administrative host list

Every host whose name fits is admitted from that moment on, including hosts that do not exist yet.
Whoever can make a host answer to a matching name can make it an administrative host. That is the
point of the feature and the reason it is worth a warning: use a pattern that only your own naming
scheme can satisfy, and remember that the name comes from the name service, not from xxQS_NAMExx.

The warning follows the **reach**, not the group. A matcher written into a group that
*@admin_hosts* references grants exactly what one written into *@admin_hosts* grants, so it is
announced in the same way and names the reserved group it reaches:

    $ qconf -aattr hostgroup hostlist 'host:mgmt*' @management
    WARNING: @management now contains the matcher host:mgmt* and is reached by @admin_hosts;
    every host whose name matches "mgmt*" thereby becomes an administrative host without any
    further administrative act.

## @submit_hosts

The list of hosts allowed to submit jobs. Modified through qconf(1) `-as`/`-ds` as well as the ordinary host
group options, and displayed by `-ss`. A matcher is allowed here too, and carries the same warning; the
grant it confers is narrower than that of *@admin_hosts*, not different in kind.

## @exec_hosts

The set of configured execution hosts, excluding the *global* and *template* pseudo-hosts. This group is
**maintained by the system** and is therefore **read-only for every user, including managers**: it is
recomputed from the execution host list whenever a host is added or removed and rebuilt at every qmaster
startup, so any attempt to modify it via `-mhgrp`, `-Mhgrp` or the `-?attr` options is rejected. Reference it
from a cluster queue's *hostlist* to have the queue follow the execution host list automatically.

Unless the installation was asked to create the host group *@allhosts*, the default queue *all.q* references
*@exec_hosts*, so every execution host has an *all.q* queue instance.

This asymmetry is deliberate: *@admin_hosts* and *@submit_hosts* are yours to edit, *@exec_hosts* is derived.
It is also why *@exec_hosts* takes no matcher: the group *is* the set of configured execution hosts, so a
rule describing that set would be describing itself.

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

An entry using all three member classes:

    group_name @gpu_nodes
    hostlist @legacy_gpu gpu001 host:gpu*

*@legacy_gpu* contributes its own members, *gpu001* is named, and every host whose name begins
with *gpu* is described -- including hosts that are not configured anywhere, and including hosts
that do not exist yet.

# SEE ALSO

xxqs_name_sxx\_\_types(1), qconf(1), xxqs_name_sxx_bootstrap(5), xxqs_name_sxx_resource_quota(5)

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
