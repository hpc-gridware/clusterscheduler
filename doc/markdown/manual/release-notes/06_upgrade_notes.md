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

[//]: # (Each file has to end with two empty lines)

