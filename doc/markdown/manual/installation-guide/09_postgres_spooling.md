# PostgreSQL Spooling

xxQS_NAMExx supports PostgreSQL as a spooling backend in addition to
classic flatfile spooling and Berkeley DB spooling. PostgreSQL
spooling stores the cluster's configuration and job state in a
remote, shared, ACID-compliant relational database, making it the
recommended choice for high-availability deployments and for sites
that require durable, transactional spool storage.

This chapter walks an administrator through:

- when to choose PostgreSQL spooling over the other backends
- preparing the PostgreSQL server
- creating the database, roles, and grants
- installing xxQS_NAMExx against PostgreSQL (manual and auto-install)
- the `.pgpass` password convention
- the `spooling_params` token reference
- backup and restore for a PostgreSQL-spooled cluster
- troubleshooting common failure modes

## Overview

### When to use PostgreSQL spooling

Choose `postgres` over `classic` or `berkeleydb` when:

- **High availability** matters: multiple `sge_shadowd` hosts share
  a single, durable spool store. The shadowd-failover mechanism
  itself is unchanged across backends; the spool store becomes the
  shared substrate that lets the new master take over without data
  loss.
- **NFS for spooling is not viable**: Berkeley DB on NFSv4 is not a
  supported configuration; classic on a shared filesystem has no
  transactions and no ACID guarantees.
- **Compliance or audit** requires the cluster's persistent state
  to live in a managed RDBMS with its own backup, replication, and
  access-control story.
- An operator is migrating from a Grid Engine deployment that
  already used PostgreSQL-based spooling.

Classic and Berkeley DB remain fully supported for single-master,
non-HA, or simple test deployments.

### What PostgreSQL spooling does not do

PostgreSQL spooling does NOT change qmaster's HA architecture
itself: `sge_shadowd` still detects qmaster outages via heartbeat
file, still negotiates the next master, and still re-starts qmaster
on a shadow host. PostgreSQL spooling only changes where the
persistent state lives.

PostgreSQL spooling does NOT provide multi-master qmaster
behavior — at any time exactly one qmaster instance is writing to
the database; shadow masters are warm standbys.

## PostgreSQL server prerequisites

### Supported versions

xxQS_NAMExx PostgreSQL spooling targets **PostgreSQL 12 or newer**.
The implementation depends on `INSERT ... ON CONFLICT` (UPSERT,
PG 9.5+), `CREATE TABLE IF NOT EXISTS` (PG 9.1+), and stable
`libpq` behavior; PG 12 is the lower bound chosen for ongoing
upstream support.

### Network reachability

The PostgreSQL server must be reachable on its listening port (TCP
5432 by default) from:

- every qmaster host (primary master and all shadow masters)
- every host that runs `inst_sge -bup` or `inst_sge -rst` against
  this cluster

The connection is via TCP only — Unix-socket / peer authentication
is not used by the qmaster runtime path because it must work across
hosts uniformly.

### Connection sizing

qmaster opens one PostgreSQL connection per worker thread. The
recommended `max_connections` setting on the PostgreSQL server is:

```
max_connections >= (qmaster_worker_threads * number_of_qmaster_hosts) + headroom
```

where `qmaster_worker_threads` comes from the bootstrap file's
`worker_threads` value and `headroom` covers shadowd, `spoolinit`,
`spooledit`, and operator `psql` sessions. A starting point of 64
covers small clusters; tune from there based on monitoring.

### Idle session timeouts

Operators commonly enable idle-session reaping on a shared
PostgreSQL server. If you set either of these, choose values that
exceed your cluster's longest legitimate idle interval:

- `idle_session_timeout` (PG 14+) — terminates idle connections
- `idle_in_transaction_session_timeout` — terminates connections
  idle inside a transaction

qmaster's spool framework reconnects transparently when libpq
reports `CONNECTION_BAD` outside a transaction. A reconnect that
fires while a spool transaction is in progress surfaces as a
`MSG_POSTGRES_CONNDIEDMIDTXN` error and aborts the in-flight spool
transaction by design — long idle timeouts inside transactions
trade availability for latency in this dimension.

### File-system layout on the PostgreSQL server

The PostgreSQL server's data directory and write-ahead log location
are unrelated to xxQS_NAMExx's `$SGE_ROOT` and follow PostgreSQL's
own conventions. xxQS_NAMExx does not require any shared filesystem
between the qmaster host and the PostgreSQL host.

## Provisioning PostgreSQL for xxQS_NAMExx

PostgreSQL spooling involves three layers, each created by a
different actor at a different time:

- **Layer 1: PostgreSQL database** (for example `gcs_spool`).
  Created by a DB administrator via `psql` and `CREATE DATABASE`
  before `inst_sge -m` runs.
- **Layer 2: PostgreSQL role** that qmaster connects as (for
  example `gcs`). Created by a DB administrator via `psql` and
  `CREATE ROLE` before `inst_sge -m` runs.
- **Layer 3: Spool tables** `config` and `jobs`. Created by
  `spoolinit init`, which `inst_sge -m` dispatches automatically
  as its final step.

You, the DB administrator, provision layers 1 and 2 via `psql`.
The installer handles layer 3 automatically.

### Step 1: Create the spool database

As a PostgreSQL superuser:

```sql
CREATE DATABASE gcs_spool
    ENCODING 'UTF8'
    LC_COLLATE 'C'
    LC_CTYPE 'C'
    TEMPLATE template0;
```

`LC_COLLATE 'C'` is recommended (not strictly required): the
spool-table primary key is declared with `COLLATE "C"` at table
creation time, but matching the database default avoids surprise
when running ad-hoc `psql` queries against the cluster.

### Step 2: Create the qmaster role

```sql
CREATE ROLE gcs WITH LOGIN PASSWORD '<choose-a-strong-password>';
```

This is the role qmaster connects as for its long-lived runtime
DML traffic. It is *not* the role `spoolinit` uses for the initial
DDL — see *Two-role privilege model* below.

### Step 3: Grant runtime privileges

The qmaster runtime role needs:

- `CONNECT` on the spool database
- `USAGE` on the schema where the spool tables live (typically
  `public`)
- `SELECT`, `INSERT`, `UPDATE`, `DELETE` on the `config` and
  `jobs` tables

The tables don't exist yet (`spoolinit` will create them in step 4
below), so the cleanest path is to authorize the role to receive
the DML grant automatically when `spoolinit` creates the tables.
Two equivalent approaches:

**Option A — atomic grant in `spoolinit init` (recommended).** Set
`qmaster_role=<role_name>` in `spooling_params`; `spoolinit init`
will issue the `GRANT ... ON config, jobs TO <role>` inside the
same transaction as the `CREATE TABLE` statements, so the spool is
immediately usable by the qmaster role once `spoolinit` succeeds.

**Option B — operator-explicit default privileges.** Run this
once BEFORE `spoolinit init`:

```sql
\c gcs_spool
GRANT CONNECT ON DATABASE gcs_spool TO gcs;
GRANT USAGE ON SCHEMA public TO gcs;
ALTER DEFAULT PRIVILEGES IN SCHEMA public
    GRANT SELECT, INSERT, UPDATE, DELETE ON TABLES TO gcs;
```

Critical ordering: `ALTER DEFAULT PRIVILEGES` is **forward-only**
in PostgreSQL — it affects tables created **after** the ALTER.
Running ALTER after `spoolinit init` does *not* retroactively
grant on the existing `config` / `jobs` tables. If you hit a
permission error from qmaster because you ALTERed too late, the
recovery is an explicit:

```sql
GRANT SELECT, INSERT, UPDATE, DELETE ON config, jobs TO gcs;
```

### Two-role privilege model: spoolinit vs qmaster

If your site enforces least-privilege role separation, use two
distinct PostgreSQL roles:

- **`spoolinit` role** (operator-driven, used only at install
  time): `CONNECT` on the database, `USAGE` on the schema,
  `CREATE` on the schema, plus DML on the tables it creates. The
  operator supplies this credential at install time only; it is
  not stored in the bootstrap.

- **qmaster runtime role** (long-lived): `CONNECT`, `USAGE` on
  the schema, `SELECT`/`INSERT`/`UPDATE`/`DELETE` on `config` and
  `jobs`. **No DDL privileges at all.** This is the role that
  lives in qmaster's `spooling_params` for the cluster's operating
  lifetime.

Single-role deployments (where `spoolinit` and qmaster use the
same credential) are supported and simpler; the framework does
not branch on this choice. qmaster simply never attempts DDL.

### PG 15+ note: explicit schema grants

PostgreSQL 15 removed the implicit `CREATE` privilege on the
`public` schema. Operators copying example SQL from older
references will hit `permission denied for schema public` at
`spoolinit` time unless they explicitly `GRANT USAGE ON SCHEMA
public` and (for the `spoolinit` role) `GRANT CREATE ON SCHEMA
public`. The examples above already include `GRANT USAGE`.

## Installing xxQS_NAMExx against PostgreSQL

### Manual install with inst_sge -m

When the qmaster installer reaches the spooling-method step, select
`postgres`:

```
Please choose a spooling method (berkeleydb|classic|postgres) [classic] >> postgres
```

The installer then asks for the PostgreSQL connection parameters:

```
PostgreSQL spooling: please enter the connection parameters.
The PostgreSQL database and the role that qmaster will connect as
must already exist (created via 'psql' by a DB administrator with
CREATE DATABASE / CREATE ROLE privileges before running this
installer). This installer will then run 'spoolinit init' to
create the 'config' and 'jobs' tables inside that database.

PostgreSQL host >> pgdb.example.com
PostgreSQL port [5432] >>
Database name >> gcs_spool
Database user (qmaster runtime role) >> gcs
SSL mode (e.g. disable, require, verify-full;
  leave empty to let libpq pick its default) [] >>
```

Then the installer offers to set up a `.pgpass` file:

```
The installer can write a libpq .pgpass file holding the password
so qmaster can authenticate without a cleartext credential in any
world-readable file. Decline if your pg_hba.conf uses peer / trust
/ ident / GSSAPI / certificate authentication.
Set up a .pgpass file with a password? (y/n) [n] >> y
.pgpass path [$SGE_ROOT/$SGE_CELL/common/.pgpass] >>
Database password (will not be echoed) >>
```

The password is **not echoed**. The installer writes the
`.pgpass` file with mode `0600` and changes ownership to the
cluster admin user so libpq's strict-permission check succeeds
when qmaster reads it.

After all prompts, the installer runs `spoolinit init` against
the database, creating the `config` and `jobs` tables (and
issuing the cross-role `GRANT` when `qmaster_role=...` is in
`spooling_params`).

### Auto-install with inst_sge -auto

For unattended installs, the auto-installer template carries the
PostgreSQL connection parameters as `SPOOLING_PG_*` variables.
Use `SPOOLING_METHOD="postgres"` and fill in the connection
variables:

```sh
SPOOLING_METHOD="postgres"

SPOOLING_PG_HOST="pgdb.example.com"
SPOOLING_PG_PORT="5432"
SPOOLING_PG_DBNAME="gcs_spool"
SPOOLING_PG_USER="gcs"

# Optional: sslmode (disable | allow | prefer | require | verify-ca | verify-full)
SPOOLING_PG_SSLMODE=""

# Optional: path to write the .pgpass file. Empty defaults to
# $SGE_ROOT/$SGE_CELL/common/.pgpass.
SPOOLING_PG_PASSFILE=""

# Optional: password to write into the .pgpass file. Leave empty
# when pg_hba.conf uses peer / trust / ident / GSSAPI / certificate
# auth. Do NOT check this into source control.
SPOOLING_PG_PASSWORD="<password>"
```

When `SPOOLING_PG_PASSWORD` is non-empty, the installer writes a
`.pgpass` file at `SPOOLING_PG_PASSFILE` (or the default path),
sets it to mode `0600`, and `chown`s it to the cluster admin
user. After the write, the installer clears
`SPOOLING_PG_PASSWORD` from its own environment so the cleartext
does not survive into subprocess environments.

Missing or invalid auto-install variables abort the install with
a diagnostic naming the missing field — there is no silent fallback
to wrong credentials.

## The .pgpass password convention

xxQS_NAMExx follows libpq's standard `.pgpass` mechanism for
PostgreSQL password handling:

- The qmaster's `spooling_params` carries a `passfile=<path>`
  token; libpq reads the password from that file at connect time.
- The file must be:
  - **mode `0600`** (libpq enforces this; less restrictive
    permissions cause libpq to silently ignore the file)
  - **owned by the user running qmaster** (typically the cluster
    admin user)
  - in the libpq `.pgpass` format:
    `hostname:port:database:username:password`
- A `password=<cleartext>` token in `spooling_params` is **not
  supported** — the bootstrap file is world-readable and must not
  carry secrets. The qmaster startup actively rejects bootstraps
  containing `password=` with a directing error.

### File location

The installer's default path is
`$SGE_ROOT/$SGE_CELL/common/.pgpass`. This sits inside the cell
directory, which is conventionally world-listable but whose
contained `.pgpass` is protected by the `0600` file mode.
Operators who prefer additional path obscurity can place the
file elsewhere and adjust `passfile=` in `spooling_params`.

### File contents

The installer writes a single entry with wildcard host / port /
database fields and a specific username:

```
*:*:*:gcs:<password>
```

This shape matches any host / port / dbname combination as long
as the connecting username is `gcs`. The username field is the
load-bearing discriminator; the wildcards make the entry survive
FQDN-vs-shortname mismatches between the qmaster's connect-time
host string and the file entry.

If you maintain `.pgpass` manually (multiple PostgreSQL backends
on the same host), constrain the host / port / dbname fields
accordingly; libpq matches the first matching entry top-to-bottom.

### Lifecycle

When the PostgreSQL password rotates, update the `.pgpass` file
on the qmaster host and every shadow master host. qmaster
re-reads the file on each connection attempt, so no qmaster
restart is required; the next reconnect after the rotation picks
up the new password.

## spooling_params token reference

`spooling_params` in the bootstrap file is a libpq conninfo
string with xxQS_NAMExx-specific extensions. The full token set:

### Standard libpq tokens

- `host=<hostname>` — PostgreSQL server hostname (required)
- `port=<port>` — TCP port (default 5432)
- `dbname=<name>` — spool database name (required)
- `user=<role>` — qmaster runtime role (required)
- `sslmode=<mode>` — `disable`, `allow`, `prefer`, `require`,
  `verify-ca`, `verify-full`; libpq's default is `prefer`.
- `sslrootcert=<path>`, `sslcert=<path>`, `sslkey=<path>` —
  TLS certificate paths for cert-based authentication
- `passfile=<path>` — path to a libpq `.pgpass` file (see the
  `.pgpass` convention section above)
- `keepalives=1`, `keepalives_idle=<sec>`,
  `keepalives_interval=<sec>`, `keepalives_count=<n>` — TCP
  keepalive controls; recommended on long-running qmaster
  connections to defeat middlebox idle-drops

### xxQS_NAMExx-specific extensions

These tokens are parsed and consumed by xxQS_NAMExx before the
remaining conninfo string is passed to libpq:

- `retry_max_seconds=<N>` — per-thread retry envelope for
  transient connection failures. Range 5..300; default 30. The
  `5` minimum prevents accidentally disabling the
  broken-connection reconnect path; the `300` ceiling keeps the
  envelope below the shadowd failover interval.
- `qmaster_role=<name>` — names the PostgreSQL role that the
  atomic `GRANT` in `spoolinit init` targets. Consumed by
  `spoolinit` only; ignored at qmaster runtime. Omit for
  single-role deployments.

### Not supported

- `password=<cleartext>` — forbidden by policy; the bootstrap
  is world-readable. Use `passfile=` instead.
- `postgresql://...` URI form — not supported in v1; use the
  key=value form.

### Example

```
spooling_method      postgres
spooling_lib         libspoolp
spooling_params      host=pgdb.example.com port=5432 dbname=gcs_spool user=gcs sslmode=require passfile=/opt/gcs/default/common/.pgpass keepalives=1 keepalives_idle=60
```

## Backup and restore

`inst_sge -bup` / `-rst` support PostgreSQL spooling alongside
classic and Berkeley DB. The PostgreSQL backup arm requires
`pg_dump` and `psql` to be on `PATH`; missing tools cause the
backup to abort with a directing error rather than producing a
silent empty backup.

### What is backed up

- The common configuration files (bootstrap, accounting,
  settings.sh, etc.) — same set as Berkeley DB backups
- The `.pgpass` file when it lives under
  `$SGE_ROOT/$SGE_CELL/common/.pgpass` (paths outside the cell
  directory are operator-managed; the backup arm prints a note
  and skips them)
- The PostgreSQL `config` table, dumped via:
  ```
  pg_dump --no-password --clean --if-exists --table=config <conninfo>
  ```
  The `jobs` table is **intentionally excluded** — backups
  capture the cluster's configuration, not its job state.

### What is not backed up

- The `jobs` table content (per above; matches BDB's
  jobs-not-included behavior)
- PostgreSQL server-side configuration (`postgresql.conf`,
  `pg_hba.conf`, etc.) — those are the responsibility of the
  PostgreSQL administrator, not xxQS_NAMExx

### Restore semantics

`inst_sge -rst` against a PostgreSQL-spooled backup:

1. Recreates `$SGE_ROOT/$SGE_CELL` and `common/` if missing.
2. Recreates the qmaster spool directory (`qmaster_spool_dir`
   from the restored bootstrap) — qmaster chdirs into this
   directory at startup regardless of spooling method, so it
   must exist on the filesystem even when spool data lives in
   the database.
3. Copies the common-dir files and the `.pgpass` (re-`chmod 0600`
   + re-`chown` to the cluster admin user — `cp` does not
   preserve mode, and the tar extract may have run as a
   different user).
4. Loads the SQL dump via:
   ```
   psql --no-password -v ON_ERROR_STOP=1 -f <dumpfile> <conninfo>
   ```
   The dumpfile starts with `DROP TABLE IF EXISTS config`
   (from `pg_dump --clean --if-exists`) so the restore is
   idempotent against a fresh database or one that already
   carries a `config` table.

### Credential handling during backup/restore

`PGPASSFILE` is set per-invocation on the `pg_dump` / `psql`
command line (not exported to the operator's shell or cron
environment) so the credential reference does not survive
outside the single subprocess.

## Troubleshooting

### Connection failed at qmaster startup

When qmaster cannot open a PostgreSQL connection at startup, the
spool framework reports:

```
postgres connect failed for "<spooling_params>": <libpq error>
```

Check, in order:

1. The PostgreSQL server is running and reachable on the
   configured host:port (`psql -h <host> -p <port>` from the
   qmaster host).
2. `pg_hba.conf` permits the qmaster role from the qmaster
   host's IP address.
3. The `.pgpass` file exists at the path in `passfile=`, is
   `0600`, and is owned by the user running qmaster.
4. The `.pgpass` file contains an entry whose user field
   matches the `user=` token in `spooling_params`.

### No password supplied at connection time

When libpq reports:

```
fe_sendauth: no password supplied
```

it found `passfile=` in `spooling_params` but could not extract
a usable password from the file. Most common causes:

- `.pgpass` permissions are not exactly `0600` (libpq silently
  ignores the file). `chmod 0600 <path>` and retry.
- `.pgpass` is owned by a user other than the one running
  qmaster. `chown <admin-user> <path>` and retry.
- The `.pgpass` entry's user field does not match the
  connecting user. Adjust the entry to use `*:*:*:<role>:<pw>`
  or a more specific match.

### Spool transaction aborted by connection loss

When the spool framework reports:

```
spool transaction aborted by connection loss
```

a PostgreSQL connection died while a qmaster spool transaction
was in flight (for example `idle_in_transaction_session_timeout`
on the server side, or a network blip). qmaster aborted the
transaction and returned `STATUS_EDISK` to the client; the
job-state change that triggered the spool is retried on the
next event.

Tune the server-side timeout upward, or set `keepalives=1` plus
`keepalives_idle=<N>` in `spooling_params` so libpq sends TCP
keepalives faster than the server's idle reaper.

### Spool tables are missing

When qmaster reports:

```
postgres spool table "config" is missing; run spoolinit to provision the schema
```

qmaster connected to the database but the `config` (or `jobs`)
table does not exist. The expected path: `spoolinit init` ran
during the install and created the tables. If you see this
message at qmaster startup, run:

```
$SGE_ROOT/utilbin/<arch>/spoolinit postgres libspoolp \
    "<spooling_params from your bootstrap>" init
```

### Insufficient privileges during spoolinit

When `spoolinit init` reports:

```
postgres DDL requires elevated privileges (PG error 42501)
```

it was invoked with a role that lacks `CREATE` on the target
schema. Use a role with DDL privileges for `spoolinit` (per the
two-role privilege model above); qmaster's runtime role is
intentionally DML-only.

### Passfile permissions too open

When qmaster reports:

```
postgres passfile "<path>" has group/other-readable permissions; chmod 0600
```

the `.pgpass` file's mode is not `0600` (or stricter). qmaster's
strict-permission check rejected the file before passing it to
libpq. Run `chmod 0600 <path>` and restart qmaster.

### Passfile ownership mismatch

When qmaster reports:

```
postgres passfile "<path>" is owned by uid X but qmaster runs as uid Y
```

the `.pgpass` file is not owned by the user qmaster runs as.
This often happens when `.pgpass` was copied between hosts via
`scp` (which preserves ownership numerically, not by name) or
was created by `root` during a manual install step. Run
`chown <qmaster-user> <path>` and restart qmaster.

### Schema version mismatch

When qmaster reports:

```
postgres spool __schema_version mismatch: expected "X", found "Y"
```

the spool schema in the database was created by a different
version of xxQS_NAMExx than the one currently running. Either
upgrade qmaster to a compatible version, or, for a fresh install
against an existing populated database, back up the data, drop
the tables, and re-run `spoolinit init` against the new version's
schema.

## See also

- The chapter **Backup and Restore** for the cross-backend
  backup/restore command shapes.
- The man page **sge_bootstrap.5** for the full bootstrap file
  format (every backend uses the same bootstrap shape; only
  `spooling_method`, `spooling_lib`, and `spooling_params`
  vary).
- The chapter **High Availability and Reliability** in the
  Administrator Guide for the broader HA story that PostgreSQL
  spooling enables.
