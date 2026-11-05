# Upgrade Notes

## Limits of Parallel Jobs

The resource limits of a queue, e.g. *h_cpu*, *h_vmem* or *h_rss*, are limits per slot. For a
parallel job, xxqs_name_sxx_execd(8) and xxqs_name_sxx_shepherd(8) computed the limit of a task in
different ways ([CS-3017](https://hpc-gridware.atlassian.net/browse/CS-3017)):

- The shepherd multiplied *s_cpu*, *h_cpu*, *s_vmem*, *h_vmem*, *s_data*, *h_data*, *s_stack* and
  *h_stack* by the number of slots of the job on the host, for every task. With
  *master_forks_slaves* or *daemon_forks_slaves* the execution daemon had already multiplied them,
  so these tasks got the limit multiplied twice. *s_rss* and *h_rss* were not multiplied, so on a
  host with systemd *MemoryMax* and *MemoryHigh* came from a different value than the
  setrlimit(2) limits.
- The execution daemon compared the usage of all tasks of the job on a host together against the
  limit multiplied by the number of slots.

Now the execution daemon computes the limit of each task once, and the shepherd applies it as it is,
see xxqs_name_sxx_queue_conf(5). The master task of a loosely integrated parallel job, the master
task with *master_forks_slaves* and every slave task with *daemon_forks_slaves* get the limit
multiplied by the number of slots of the job on the host. Every other task gets the limit of a
single slot. A limit the job requests as a consumable of type JOB is not multiplied.

After the upgrade, these tasks get a lower limit than before:

- a task of a tightly integrated parallel job without *master_forks_slaves* or
  *daemon_forks_slaves*, for the limits the shepherd used to multiply;
- the master task with *master_forks_slaves* and every slave task with *daemon_forks_slaves*,
  which no longer get these limits multiplied twice;
- the master task with *daemon_forks_slaves* but without *master_forks_slaves*, which got the
  limit of all slots on the master host before, also *s_rss* and *h_rss*.

A job whose tasks used more than their new limit is killed.

## Jobs Reaching their Memory Limit under systemd

On a host which runs its jobs under systemd control, the lower of *h_rss* and *h_vmem* is the
*MemoryMax* of the job's scope. When a job reaches it, the memory cgroup OOM killer kills a process
of the job. Job scopes are delegated units, and for those systemd defaults *OOMPolicy* to
*continue*: the job kept running and ended with its own exit status, a *DefaultOOMPolicy* set in
the systemd configuration does not apply.

On hosts with cgroup v2 and systemd 252 or newer, xxqs_name_sxx_shepherd(8) now sets
*OOMPolicy=kill* on the job's scope ([CS-3020](https://hpc-gridware.atlassian.net/browse/CS-3020)).
The OOM killer kills all processes of the job at once, the job ends with exit status 137 and
failed 100, and the abort mail is sent if the job requested it. Jobs which used to survive the
OOM kill of one of their processes now end as failed. Older systems are not affected.

The accounting record of such a job shows *deleted_by* `oom-killer@<host>`
([CS-3022](https://hpc-gridware.atlassian.net/browse/CS-3022)), so that a workflow manager can
tell an OOM kill from other failures. When the qmaster deletes a parallel job because one of its
tasks failed, the records of the other tasks now show who ended that task, e.g. `oom-killer@<host>`
or `execd@<host>`. A task of a parallel job which the execution daemon or the OOM killer ended
itself shows that cause in its record, also when the job was deleted for another reason.

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

## The Host List of a Cluster Queue Is Stored as a Host Group

A cluster queue no longer stores a host list of its own. Every queue owns a host group named after it with a
doubled `@` as prefix -- `@@all.q` for *all.q* -- and that group's member list is the queue's host list. See the
[Compatibility Notes](07_compatibility_notes.md#the-host-list-of-a-cluster-queue-is-a-host-group) for what this
changes at the user interface; in short, only the output of `qconf -shgrpl`.

The regular upgrade procedure (`inst_sge -upd`) handles the migration automatically. The host lists are dumped
with the cluster queues of the old cluster and re-added to the upgraded one with `qconf -Aq`, which creates
each queue together with its host group and writes the host list into it. No manual action is required. As
always, the upgrade procedure has to be run -- replacing only the binaries is not a supported way to install a
new minor or major version. A queue that reaches the new qmaster without its host group, from a spool directory written before this change, is given an empty one at startup, which is the same thing the missing host list said.

**Saved configurations do not contain queue host groups.** When a configuration is saved from a cluster
running this version, the queue host groups are left out of the dump: they are restored before the cluster
queues exist, and each one is recreated from its queue's *hostlist* a few steps later anyway. A configuration
saved by an earlier version restores unchanged, and every queue reaches the same hosts as before.

**If your cluster has a cluster queue whose name is longer than 252 characters**, rename it before the
upgrade. Queue names are now limited so that the name of the host group derived from them still fits the spool
file name limit (see *Cluster Queue Names Are Limited to 252 Characters* in the Compatibility Notes). There is no
advance check for this in the upgrade procedure: such a queue is refused while the saved configuration is being
loaded, with a message naming the cluster queue. Names of that length are unusual, so this affects practically
no cluster.

## The Default Queue Can Follow the Execution Host List

A new installation lets the default queue *all.q* reference the host group `@exec_hosts`, which the qmaster
maintains from the execution host list, and no longer creates the host group `@allhosts` unless asked to. See
the [Compatibility Notes](07_compatibility_notes.md#the-host-group-allhosts-is-optional) for the details.

**The upgrade procedure does not change this in an existing cluster.** `@allhosts` and the host list of *all.q*
are carried over as they are, and the installation of an additional execution host keeps asking whether to add
the host to `@allhosts`, as long as that group exists.

To switch an upgraded cluster over, let *all.q* reference `@exec_hosts`:

```
qconf -mattr queue hostlist @exec_hosts all.q
```

From then on every execution host has an *all.q* queue instance, including hosts that were deliberately left
out of `@allhosts`. `@exec_hosts` is maintained by the qmaster and contains every execution host; a single
host cannot be removed from it. If such hosts exist, keep the previous host list of *all.q*. Once no queue,
resource quota set or other host group references `@allhosts` any more, it can be deleted with
`qconf -dhgrp @allhosts`. Without `@allhosts` the installation of an execution host no longer asks about a
queue; it only sets the slots of the host's *all.q* queue instance.

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

## Systemd Unit Files: File Descriptor Limit

A systemd service does not inherit the settings of `/etc/security/limits.conf`. A unit file without a
`LimitNOFILE=` setting therefore runs with systemd's default soft limit of 1024 file descriptors.

That limit matters for `sge_qmaster`: it derives the maximum number of dynamic event clients - used by
`qsub -sync y` and by DRMAA sessions - from its soft file descriptor limit, and with 1024 descriptors the
maximum is silently capped at 979, below the default of 1000. See `MAX_DYN_EC` in xxqs_name_sxx_conf(5).

The unit file template of the qmaster service now sets:

```text
LimitNOFILE=65536
```

The template of the execd service carries the same setting commented out. It is not enabled by default
because the limit is inherited by the jobs: `sge_shepherd` sets a job's file descriptor limit only when the
execd parameters `S_DESCRIPTORS` / `H_DESCRIPTORS` are configured, so enabling it would raise the limit for
every job on the host.

*An upgrade does not touch the unit files of an existing installation.* They were generated at installation
time and are left alone, so an upgraded cluster keeps the old limit until the unit files are updated. There
are two ways to do that.

As user `root`, edit the unit file `/etc/systemd/system/ocs<qmaster_port>-qmaster.service`, add the
`LimitNOFILE` setting to the `[Service]` section, and reload and restart the service:

```bash
systemctl daemon-reload
systemctl restart ocs<qmaster_port>-qmaster
```

Alternatively let the installer recreate the unit files from the current templates:

```bash
cd $SGE_ROOT
./inst_sge -upd-rc
```

This removes the existing unit files and installs new ones on the qmaster, all shadow hosts and all
execution hosts. Note that it *stops the daemons* whose unit file it replaces and does not start them again -
the new unit is installed and enabled, but the cluster has to be started afterwards, for example with
`inst_sge -start-all`. Run it during a maintenance window, and note that it needs `root` and reaches the
other hosts over ssh; use `-noremote` to work on the local host only.

## Resource Quota Sets: *-mrqs* Does Not Delete Any More

Nothing has to be done before or during the upgrade — the saved configuration is loaded with
`-Arqs`, which never deleted anything. What changes is the interactive command afterwards:
`qconf -mrqs` applies what the edited text holds and no longer deletes the rule sets left out of
it, so a procedure that removed a rule set by deleting its block has to use `qconf -drqs <name>`.
See the [Compatibility Notes](07_compatibility_notes.md#qconf--mrqs-no-longer-deletes-the-rule-sets-left-out-of-the-text).

## Reader Threads: *auto* for New Installations

The stored procedures of qstat, qhost, qrstat and qquota evaluate requests in the server context inside the reader
threads of `sge_qmaster`, and such requests take most of their CPU time there. With the 4 reader threads earlier
installations configured, a master host with many CPUs answers few of those requests in parallel while its other
CPUs stay idle.

The `reader_threads` entry of the bootstrap file now also accepts *auto*: `sge_qmaster` then starts half the CPUs it
may use as reader threads, at least 4 and at most 32, honouring its CPU affinity and a cgroup v2 CPU quota. A new
installation writes *auto*. See xxqs_name_sxx_bootstrap(5).

*An upgrade does not change the entry.* An upgraded cluster keeps its number. To switch to *auto*, change the line
in `$SGE_ROOT/$SGE_CELL/common/bootstrap` to

```text
reader_threads          auto
```

and restart `sge_qmaster`. With *loglevel* set to *log_info* in the global configuration, the messages file then
shows the number it started; with the default *log_warning* it does not. Each reader thread working on a request
holds its own copy of the data, so plan for a higher peak memory of the qmaster.

[//]: # (Each file has to end with two empty lines)


