# Upgrade Notes

## TLS: Private Keys Move Below the Cell

The private keys of the daemons used to be stored in `/var/lib/ocs/<qmaster_port>/private`. The path now also
contains the cell: `/var/lib/ocs/<qmaster_port>/<cell>/private`. The port alone did not identify an
installation, so two clusters using the same port - a side-by-side upgrade after the switch over, or a
reinstallation into another cell - shared the directory, and the one that did not write the key failed to start
with *key values mismatch*
([CS-2487](https://hpc-gridware.atlassian.net/browse/CS-2487)).

Nothing has to be done for the upgrade: the daemons notice that the key of their certificate is not in the new
place, and create certificate and key anew on the first start. The old files stay behind and are no longer read.
They can be removed once every daemon on the host has been started with the new version:

    rm -f /var/lib/ocs/<qmaster_port>/*.pem /var/lib/ocs/<qmaster_port>/private/*.pem

Removing them is not done automatically - xxQS_NAMExx does not delete key material it may not have created
itself.

## GDI Request Limits

If your cluster uses `gdi_request_limits` with rules targeting *qstat*, *qhost*, *qrstat*, or *qquota* by specific
object type, those rules must be updated after upgrading to version 9.2. See the
[Compatibility Notes](07_compatibility_notes.md#gdi-request-limits-for-status-query-commands) for details and
replacement examples.

The object types *MANAGER* and *OPERATOR* have also been removed from `gdi_request_limits`. The upgrade
procedure removes any rule that uses them automatically (managers and operators are access lists now; limit
them via the *USER_SET* object instead). See the
[Compatibility Notes](07_compatibility_notes.md#object-types-manager-and-operator-removed).

The same applies to the object types *AHOST* and *SHOST*, removed in 9.2 because admin and submit hosts are
host groups now. Any rule using them is removed by the upgrade procedure as well; limit these operations via
the *HGROUP* object instead. See the
[Compatibility Notes](07_compatibility_notes.md#object-types-ahost-and-shost-removed).

## Zombie Jobs Removed, Replaced by Finished-Job Retention

Version 9.2 removes the pre-existing *zombie jobs* mechanism. The `qstat -s z` option is gone. The retention
feature that replaces it (see the *Retained Finished Jobs* section in
[Major Enhancements](03_major_enhancements.md#retained-finished-jobs)) is **off by default** — both
`finished_jobs_keep_time` and `finished_jobs_max` default to `0`.

To restore visibility of recently completed jobs after the upgrade, set either or both tunables via
`qconf -mconf`, for example:

    finished_jobs_keep_time  01:00:00
    finished_jobs_max        1000

and query with `qstat -s f`. See sge_conf(5) for the two additional `qmaster_params` sub-keys
(`FINISHED_JOBS_SWEEP_INTERVAL`, `FINISHED_JOBS_SWEEP_BATCH`) that control sweep cadence and per-tick prune cap,
plus sizing notes for memory, spool I/O, and startup impact.

Callers that previously scraped `qstat -s z` output should switch to `qstat -s f` (retained finished jobs) or
`qacct(1)` for history beyond the retention window.

## Classic Spooling: Configuration Moved into the Spool Directory

For clusters using *classic* spooling, the global and per-host configurations are now spooled in a `configs`
directory under the qmaster spool directory (and the scheduler configuration as a file there) together with
all other objects, instead of in `$SGE_ROOT/$SGE_CELL/common`. Consequently the `spooling_params` entry in
the `bootstrap` file is now a single qmaster spool directory path; the obsolete two-argument form
`<common_dir>;<spool_dir>` is rejected and qmaster will not start with it.

The upgrade procedure handles this automatically: it rewrites `spooling_params` to the single spool directory
and the configuration is re-spooled there. No manual action is required. If you maintain the `bootstrap` file
by other means, update `spooling_params` to the qmaster spool directory path before starting the upgraded
qmaster.

## Managers and Operators Are Stored as Access Lists

Managers and operators are no longer stored in the `managers` and `operators` files of the qmaster spool
directory. They are now the members of two reserved access lists (usersets) named `manager` and `operator`,
spooled with all other access lists. See the
[Compatibility Notes](07_compatibility_notes.md#managers-and-operators-are-reserved-access-lists) for what
this changes at the user interface.

The regular upgrade procedure (`inst_sge -upd`) handles the migration automatically: the existing managers
and operators are dumped from the old cluster and re-added to the upgraded one, where they are stored in the
reserved access lists. No manual action is required, and the old `managers`/`operators` files are simply no
longer used. As always, the upgrade procedure has to be run — replacing only the binaries is not a supported
way to install a new version.

**If your cluster uses an access list named `manager` or `operator`**, it must be renamed before the
upgrade, because those two names are now reserved. The upgrade detects this and aborts with an explanatory
message while the old cluster is still untouched, so that no half-migrated cluster can result.

Such an access list cannot be carried over automatically: everything that references it — the
`user_lists`/`xuser_lists` of queues, hosts, parallel environments and the cluster configuration, the
`acl`/`xacl` of projects, and resource quota sets — would silently resolve to the reserved manager or
operator list after the upgrade, and with that to different access rights. Rename the access list in the old
cluster, adapt the objects that reference it, and start the upgrade again.

## Admin and Submit Hosts Are Stored as Host Groups

Administrative hosts and submit hosts are no longer stored in the `admin_hosts` and `submit_hosts` entries of
the qmaster spool directory. They are now the members of two reserved host groups named `@admin_hosts` and
`@submit_hosts`, spooled with all other host groups. A third reserved group, `@exec_hosts`, mirrors the
execution host list and is maintained by the system. See the
[Compatibility Notes](07_compatibility_notes.md#admin-and-submit-hosts-are-reserved-host-groups) for what this
changes at the user interface — in short, very little: the command line interface and its messages are
unchanged.

The regular upgrade procedure (`inst_sge -upd`) handles the migration automatically: the existing admin and
submit hosts are dumped from the old cluster and re-added to the upgraded one with `qconf -ah`/`-as`, where
they are stored in the reserved host groups. No manual action is required, and the old `admin_hosts` and
`submit_hosts` spool entries are simply no longer read. As always, the upgrade procedure has to be run —
replacing only the binaries is not a supported way to install a new version.

**If your cluster uses a host group named `@admin_hosts`, `@submit_hosts` or `@exec_hosts`**, it must be
renamed before the upgrade, because those three names are now reserved. Host groups existed long before these
names were reserved, so nothing prevented such a name previously. The upgrade detects this and aborts with an
explanatory message while the old cluster is still untouched, so that no half-migrated cluster can result.

Such a host group cannot be carried over automatically: everything that references it — the *hostlist* of
cluster queues and of other host groups, and the scopes of resource quota sets — would silently resolve to the
reserved group after the upgrade, and with that to a different set of hosts and different access rights.
Rename the host group in the old cluster, adapt the objects that reference it, and start the upgrade again.

Compare `qconf -shgrp_resolved` before and after the rename to confirm the intended host set.

## Wildcard Characters in Object Names

Beginning with version 9.2 the name of a configuration object may no longer contain any of the characters
`*`, `?`, `[`, `]`, `&`, `|`, `!`, `(` and `)`. See the
[Compatibility Notes](07_compatibility_notes.md#wildcard-characters-are-no-longer-allowed-in-object-names)
for the reason and for what changes in the way such configurations resolve.

Most clusters are not affected — these characters were unusual in object names, and five of the nine were
rejected in earlier versions already. Nothing has to be done in that case.

**If your cluster does have such an object**, it must be renamed before the upgrade. The upgrade procedure
checks the saved configuration and, if it finds one, lists every offending object with its type and name and
aborts before loading anything, so that no half-migrated cluster can result:

    [CRITICAL] The saved configuration contains object names with wildcard
    expression characters (saved from version: GCS 9.1.0):

       host group "@gpustar*"
       parallel environment "mpi&openmpi"

Rename the objects in the old cluster and adapt everything that references them — the host lists of other host
groups, resource quota scopes, and the queue or parallel environment requests in job scripts and default
request files. Then save the configuration again and repeat the upgrade.

Note that renaming changes which hosts a reference resolves to, if the old name was being matched as a pattern
anywhere. Compare `qconf -shgrp_resolved` before and after to confirm the intended host set.

[//]: # (Each file has to end with two empty lines)

