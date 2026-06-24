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

[//]: # (Each file has to end with two empty lines)

