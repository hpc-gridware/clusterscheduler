# Upgrade Notes

## GDI Request Limits

If your cluster uses `gdi_request_limits` with rules targeting *qstat*, *qhost*, *qrstat*, or *qquota* by specific
object type, those rules must be updated after upgrading to version 9.2. See the
[Compatibility Notes](07_compatibility_notes.md#gdi-request-limits-for-status-query-commands) for details and
replacement examples.

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

[//]: # (Each file has to end with two empty lines)

