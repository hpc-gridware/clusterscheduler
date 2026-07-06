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

For clusters using *classic* spooling, the global configuration, the per-host local configurations
(`local_conf`) and the scheduler configuration are now spooled in the qmaster spool directory together with
all other objects, instead of in `$SGE_ROOT/$SGE_CELL/common`. Consequently the `spooling_params` entry in
the `bootstrap` file is now a single qmaster spool directory path; the obsolete two-argument form
`<common_dir>;<spool_dir>` is rejected and qmaster will not start with it.

The upgrade procedure handles this automatically: it rewrites `spooling_params` to the single spool directory
and the configuration is re-spooled there. No manual action is required. If you maintain the `bootstrap` file
by other means, update `spooling_params` to the qmaster spool directory path before starting the upgraded
qmaster.

[//]: # (Each file has to end with two empty lines)

