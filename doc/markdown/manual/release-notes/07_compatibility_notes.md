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

### Object Types *MANAGER* and *OPERATOR* Removed

The `gdi_request_limits` object types *MANAGER* and *OPERATOR* no longer exist in version 9.2. Managers and
operators are now the members of the reserved *manager* and *operator* access lists (see *Managers and
Operators Are Reserved Access Lists* below), and adding or removing them is performed on those access lists.
A rule that limited manager or operator operations should therefore target the *USER_SET* object instead.

A configuration that still contains a rule with object *MANAGER* or *OPERATOR* would be rejected by the
version 9.2 qmaster. The upgrade procedure handles this automatically: any such rule is removed from
`gdi_request_limits` during the upgrade (if no rule remains, the value is set to *NONE*). No manual action
is required; adapt your rules to *USER_SET* afterwards if you want to keep limiting these operations.

### Object Types *AHOST* and *SHOST* Removed

The `gdi_request_limits` object types *AHOST* and *SHOST* no longer exist in version 9.2. Admin and submit
hosts are now the members of the reserved *@admin_hosts* and *@submit_hosts* host groups (see *Admin and
Submit Hosts Are Reserved Host Groups* below), and adding or removing them is performed on those host groups.
A rule that limited admin or submit host operations should therefore target the *HGROUP* object instead.

A configuration that still contains a rule with object *AHOST* or *SHOST* would be rejected by the version 9.2
qmaster. The upgrade procedure handles this automatically: any such rule is removed from `gdi_request_limits`
during the upgrade (if no rule remains, the value is set to *NONE*). No manual action is required; adapt your
rules to *HGROUP* afterwards if you want to keep limiting these operations.

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

With *classic* spooling, the global and per-host configurations are now stored in a `configs` directory under
the qmaster spool directory — `$SGE_ROOT/<cell>/spool/qmaster/configs/global` and `.../configs/<host>` — and
the scheduler configuration as `.../spool/qmaster/sched_configuration`, owner-only, together with all other
spooled objects. Previously they were kept as flat files under `$SGE_ROOT/<cell>/common` (`configuration`,
`local_conf/<host>`, `sched_configuration`). The `common` directory continues to hold the non-spooled files
(`bootstrap`, `act_qmaster`, `settings.sh`, host aliases, etc.).

As a consequence, the classic `spooling_params` entry in the `bootstrap` file is now a single qmaster spool
directory path instead of the two-argument `<common_dir>;<spool_dir>` form, which is no longer accepted
(see [Upgrade Notes](06_upgrade_notes.md)).

**Impact:** any external tooling that read configuration by path from
`$SGE_ROOT/<cell>/common/configuration` or `$SGE_ROOT/<cell>/common/local_conf/<host>` must be adapted.
Reading configuration files directly has never been a supported interface — execution daemons obtain their
configuration from the qmaster over GDI, not from disk. Use *qconf* (`-sconf`/`-Mconf` and the host-specific
configuration commands) to read and modify configuration programmatically.

## Managers and Operators Are Reserved Access Lists

Managers and operators are now stored as the members of two reserved access lists (usersets) named `manager`
and `operator`, instead of in their own `managers` and `operators` files in the qmaster spool directory. The
access list is the single place where they live, which allows RBAC roles to reference managers and operators
by access list name instead of duplicating those lists.

**The manager and operator command line interface behaves as before.** `qconf -am`, `-dm`, `-sm`, `-ao`,
`-do` and `-so` continue to work and are kept for convenience; they now operate on the reserved access lists.
The listing output of `qconf -sm` and `-so` (the plain list of names) is unchanged.

**One visible change — the confirmation messages of `-am`/`-ao`/`-dm`/`-do`.** Because these commands now add
to / remove from an access list, they report the access-list wording instead of the former manager/operator
wording, for example:

| Operation | Before | Since 9.2 |
|-----------|--------|-----------|
| `qconf -am user` | `added "user" to manager list` | `added "user" to access list "manager"` |
| `qconf -am user` (existing) | `manager "user" already exists` | `"user" is already in access list "manager"` |
| `qconf -dm user` | `removed "user" from manager list` | `deleted user "user" from access list "manager"` |

Scripts that parse this confirmation text (rather than relying on the exit code) must be adapted. The
equivalent operations via the access list interface (`qconf -au`/`-du`) always used this wording.

In addition, both access lists are ordinary access lists at the interface: they are listed by `qconf -sul`,
shown by `qconf -su manager`, and can be modified with `-au`, `-du`, `-mu`, `-Mu` and `-Au`. `qconf -au user
manager` and `qconf -am user` are two ways of doing the same thing. Because these two access lists carry the
permissions of the cluster, three restrictions apply to them and to no other access list:

* they cannot be deleted (`qconf -dul`, `-Du`),
* their *type* must remain *ACL*,
* the user *root* and the admin user cannot be removed from them — the same rule `-dm`/`-do` always enforced,
  now also enforced when going through `-du`/`-mu`.

**Impact:** the `managers` and `operators` files no longer exist, and the qmaster no longer reads them. Any
tooling that read those files directly must use `qconf -sm`/`-so` (unchanged) instead. Reading spool files
directly has never been a supported interface. Clusters that use an access list named `manager` or
`operator` must rename it before upgrading; the upgrade detects the collision and aborts with an
explanatory message — see the [Upgrade Notes](06_upgrade_notes.md#managers-and-operators-are-stored-as-access-lists).

## Admin and Submit Hosts Are Reserved Host Groups

Administrative hosts and submit hosts are now stored as the members of two reserved host groups named
`@admin_hosts` and `@submit_hosts`, instead of in their own `admin_hosts` and `submit_hosts` entries in the
qmaster spool directory. The host group is the single place where they live, which allows RBAC roles to
reference admin and submit hosts by host group name instead of duplicating those lists.

**The admin and submit host command line interface behaves as before, including its messages.** `qconf -ah`,
`-dh`, `-sh`, `-as`, `-ds` and `-ss` continue to work and are kept for convenience; they now operate on the
reserved host groups. The listing output of `-sh` and `-ss` is unchanged for a cluster that does not use
nesting — same names, same sort order — and so is the `-fmt json` document, including its schema. Unlike the
manager and operator commands described above, **the confirmation and error messages of these six commands are
byte-for-byte identical to version 9.1**, including the exit codes: adding a host that is already present
still fails with `adminhost "..." already exists`, and deleting one that is not a member still fails with
`denied: administrative host "..." does not exist`. Scripts parsing this text do not need to be adapted.

Both groups are ordinary host groups at the interface: they are listed by `qconf -shgrpl`, shown by
`qconf -shgrp @admin_hosts`, and can be modified with `-mhgrp`, `-Mhgrp` and the `-?attr` options.
`qconf -aattr hostgroup hostlist host @admin_hosts` and `qconf -ah host` are two ways of doing the same
thing.

**What is genuinely new** follows from admin and submit hosts being host groups, which the flat lists could
not express:

* `-ah`/`-as` accept a host group reference, e.g. `qconf -as @lx_cluster`, making every host of that group a
  submit host. Nested references are validated by the qmaster like any other host group reference, cycles
  included.
* `-sh`/`-ss` print such a reference as a `@group` entry. This can only appear if an administrator introduced
  the nesting, so the output of a cluster upgraded from 9.1 is unchanged until they do. Use
  `qconf -shgrp_resolved @admin_hosts` for the resolved host set and `-shgrp_tree` for the structure.
* `-dh`/`-ds` remove a **direct** member only. Deleting a host that is an admin or submit host only because a
  nested group contains it is refused, naming the containing group(s), and exits non-zero:

      denied: host "lx-01" is not a direct member of "@submit_hosts" but is contained in
      @lx_cluster; use "qconf -mhgrp" to change the nesting

  In 9.1 this case could not arise, since the flat lists had no notion of nesting. Silently succeeding would
  leave the host deleted but still a submit host.

Because these two host groups carry the permissions of the cluster, restrictions apply to them and to no other
host group: they cannot be deleted, and the host running the qmaster cannot be removed from `@admin_hosts` —
the same rule `-dh` always enforced, now also enforced when going through `-mhgrp` or the `-?attr` options.

A third reserved host group, **`@exec_hosts`**, is new in 9.2. It contains the configured execution hosts
(excluding the *global* and *template* pseudo-hosts) and is maintained by the system: it is recomputed when an
execution host is added or removed and rebuilt at every qmaster startup. It is therefore **read-only for every
user, including managers** — `-mhgrp`, `-Mhgrp` and the `-?attr` options reject it:

    denied: the host group "@exec_hosts" is maintained by the system from the execution host
    list and cannot be modified

Referencing it from a cluster queue's *hostlist* makes the queue follow the execution host list
automatically. One consequence: an execution host that a queue reaches *only* through `@exec_hosts` can still
be deleted with `qconf -de` — the queue instance derived from the reserved group does not count as a reference,
because otherwise no execution host could ever be deleted once a queue named the group.

**Impact:** the `admin_hosts` and `submit_hosts` spool entries no longer exist, and the qmaster no longer reads
them. Any tooling that read them directly must use `qconf -sh`/`-ss` (unchanged) instead; reading spool files
directly has never been a supported interface. The dedicated GDI request targets for admin and submit hosts
and their event types (`ADMINHOST`/`SUBMITHOST`) have been removed as well — event clients receive host group
events for these changes now. Clusters that use a host group named `@admin_hosts`, `@submit_hosts` or
`@exec_hosts` must rename it before upgrading; the upgrade detects the collision and aborts with an
explanatory message — see the
[Upgrade Notes](06_upgrade_notes.md#admin-and-submit-hosts-are-stored-as-host-groups).

## Wildcard Characters Are No Longer Allowed in Object Names

The name an object is created with may no longer contain any of the characters that make up a wildcard
expression: `*`, `?`, `[`, `]`, `&`, `|`, `!`, `(` and `)`. Five of them — `[`, `]`, `|`, `(` and `)` — were
already rejected in earlier versions; version 9.2 adds `*`, `?`, `&` and `!`, closing a gap in the name
validation. This applies to the primary names of host groups, cluster queues, parallel environments, access
lists, users, projects, calendars, checkpointing interfaces, resource quota sets and roles.

**Wildcards on the referencing side are unaffected.** They keep working exactly as documented in
`sge_types(1)`: resource quota scopes such as `hosts {@gpu*}`, queue patterns such as
`-q '*@@gpuhosts'`, and `qsub -pe 'mpi*' 4` continue to accept patterns. The restriction applies to names
only, never to references.

The reason for the restriction is that names and the references pointing at them share one namespace. A host
group literally named `@gpustar*` cannot be addressed unambiguously — every reference to it is also a pattern
matching `@gpustar1`, `@gpustar2` and so on.

**Impact:** such a configuration also resolved inconsistently, and that is corrected in the same release. Where
a host group was referenced as a *member* of another host group, the member entry was matched as a pattern
against all host groups instead of being looked up by name. The same configuration therefore resolved to
different host sets depending on the code path: `qconf -shgrp_resolved` resolved exactly, while resource quota
matching and `-q` matching resolved by pattern, so resource quota rules could apply to hosts that were not
members of the referenced host group. Both paths now resolve members exactly.

Clusters that have an object whose name contains one of the four newly excluded characters — and that reference
it from a resource quota set, a `-q` request or the host list of another host group — see a reduced, and now
correct, scope after the upgrade. Existing objects are not rejected retroactively; only the creation of new ones
is validated. The upgrade procedure detects such names and aborts with an explanatory message before anything is
loaded — see the [Upgrade Notes](06_upgrade_notes.md#wildcard-characters-in-object-names).

[//]: # (Each file has to end with two empty lines)

