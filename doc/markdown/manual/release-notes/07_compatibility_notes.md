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

[//]: # (Each file has to end with two empty lines)

