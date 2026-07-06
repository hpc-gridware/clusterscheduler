# Compatibility Notes

## GDI Request Limits for Status Query Commands

With the introduction of stored procedures in version 9.2, the GDI request behavior of the status query commands
*qstat*, *qhost*, *qrstat*, and *qquota* has changed fundamentally.

In version 9.1.x and earlier, these commands issued multiple individual GDI GET requests per invocation — for
example, a single `qstat -f` would trigger up to 15 separate GET requests for different object types (jobs, queues,
execution hosts, complexes, etc.).

Starting with version 9.2, each of these commands sends a single GET request targeting the object type `PROC`, which
triggers a stored procedure on the qmaster to collect all required data at once. As a result, each command invocation
corresponds to exactly one GDI request.

**Impact on existing `gdi_request_limits` configurations:**

- Rules that match these commands by specific object type — such as `qstat:get:job:*:*=N` or
  `qstat:get:cqueue:*:*=N` — will no longer fire in version 9.2, because these commands no longer issue
  object-specific GET requests. Such rules should be replaced.
- Generic wildcard rules such as `*:get:*:*:*=N` will now match the new `GET:PROC` request from these commands
  and may apply an unintended limit.

Replace existing rules for these commands with a `PROC`-targeted rule and adjust the limit value accordingly.
Since there is no longer a multiplier (one command = one request), the limit can be set directly to the desired
number of invocations per second:

| Version 9.1.x        | Version 9.2             |
|----------------------|-------------------------|
| `qstat:get:*:*:*=N`  | `qstat:get:proc:*:*=N`  |
| `qhost:get:*:*:*=N`  | `qhost:get:proc:*:*=N`  |
| `qrstat:get:*:*:*=N` | `qrstat:get:proc:*:*=N` |
| `qquota:get:*:*:*=N` | `qquota:get:proc:*:*=N` |

## *qquota* Plain Output: Memory and Time Limits Shown With Units

In the plain (non-XML, non-JSON) output of *qquota*, resource-quota limit and usage values are now displayed in the
human-readable form that matches the resource's complex attribute type, instead of the raw canonical number:

- **MEMORY** attributes are shown with a unit, e.g. `4.000G` instead of `4294967296`.
- **TIME** attributes are shown as `HH:MM:SS` (or `D:HH:MM:SS`), e.g. `01:00:00` instead of `3600`.
- All other types (INT, DOUBLE, BOOL, …) continue to be shown as plain integers.

Previously the plain output printed the raw canonical value (bytes for memory, seconds for time) and additionally
truncated it to the column width, so a 4 GiB limit could appear as the misleading value `42949672`.

**Impact:** scripts that parse the plain *qquota* output and expect the raw numeric value must be adapted. For
machine-readable, unformatted numeric values use `qquota -xml` or `qquota -json` instead — their output is
**unchanged** and continues to report the canonical numeric value.

## Spool Files Created With Owner-Only Permissions (0600)

Flatfile spool files written by the qmaster (under `$SGE_ROOT/<cell>/spool/qmaster/...`) are now created with mode
**0600** — read/write for the owning administrative user only. Previously they were created with mode 0666, so the
final on-disk permissions depended entirely on the daemon's umask and could leave the files group- or
world-readable (and, under a permissive umask, even writable).

These files hold the authoritative cluster configuration and job metadata. Restricting them to the owner closes an
information-disclosure and tampering exposure for any local user able to traverse the spool directory.

**Impact:** any external tooling or process that read or modified qmaster spool files *directly* — as a user other
than the qmaster/shadowd administrative account — will no longer be able to access them. Direct access to spool
files has never been a supported interface. To read, back up, version, or modify configuration objects
programmatically, use the file- and directory-based *qconf* interface (`-S<obj>` to export, `-A<obj>`/`-M<obj>` to
apply) described in the *File-Based Bulk Configuration and Export with qconf* section of the **Major Enhancements** —
it operates through the qmaster and removes any need to touch spool files directly.

## Classic Spooling: Configuration Stored in the Spool Directory

With *classic* spooling, the global configuration, the per-host local configurations (`local_conf`) and the
scheduler configuration are now stored in the qmaster spool directory (`$SGE_ROOT/<cell>/spool/qmaster/...`,
owner-only) together with all other spooled objects. Previously they were kept as flat files under
`$SGE_ROOT/<cell>/common` (`configuration`, `local_conf/<host>`, `sched_configuration`). The `common`
directory continues to hold the non-spooled files (`bootstrap`, `act_qmaster`, `settings.sh`, host aliases,
etc.).

As a consequence, the classic `spooling_params` entry in the `bootstrap` file is now a single qmaster spool
directory path instead of the two-argument `<common_dir>;<spool_dir>` form, which is no longer accepted
(see [Upgrade Notes](06_upgrade_notes.md)).

**Impact:** any external tooling that read configuration by path from
`$SGE_ROOT/<cell>/common/configuration` or `$SGE_ROOT/<cell>/common/local_conf/<host>` must be adapted.
Reading configuration files directly has never been a supported interface — execution daemons obtain their
configuration from the qmaster over GDI, not from disk. Use *qconf* (`-sconf`/`-Mconf` and the host-specific
configuration commands) to read and modify configuration programmatically.

[//]: # (Each file has to end with two empty lines)

