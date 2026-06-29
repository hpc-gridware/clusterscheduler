# Major Enhancements

## v9.2.0

### Postgres Spooling Backend

Gridware Cluster Scheduler 9.1.3 introduces **PostgreSQL** as a third xxqs_name_sxx_qmaster spooling backend, joining the existing *classic* (filesystem) and *berkeleydb* options. The new backend stores the cluster's spool state in a remote PostgreSQL database, removing the BerkeleyDB environment lock and the NFS quirks that surround it.

Key characteristics:

- **ACID-compliant shared store.** All spool writes go through a PostgreSQL transaction. Two tables — `config` and `jobs` — hold the byte-identical CULL pack format used by the BerkeleyDB backend.
- **High availability.** Multiple shadow masters share the same PostgreSQL instance. The qmaster spool directory still lives on a shared filesystem because sge_shadowd polls a heartbeat file there, but the bulk of the state (configuration, jobs, queues, sharetree, …) is reached over TCP — no BerkeleyDB environment directory on NFS.
- **Single-role provisioning.** A PostgreSQL superuser creates one role and one database owned by that role; `spoolinit init` (run automatically by `inst_sge -m`) creates the schema. No cross-role GRANT setup is required.
- **Credential handling via libpq `.pgpass`.** The `bootstrap` file is world-readable, so embedding `password=` in `spooling_params` is rejected. Operators point `passfile=` at a 0600 `.pgpass` owned by the qmaster user; the installer can write this file as part of `inst_sge -m`.
- **Backup and restore.** `inst_sge -bup` and `-rst` add postgres arms that drive `pg_dump`/`psql` (with `--single-transaction` so restores are all-or-nothing) and copy the `.pgpass` alongside the dump for a complete round-trip.
- **Upgrade.** `inst_sge -upd` carries an existing postgres spool forward by truncating the spool tables (preserving the `__schema_version` row) so the upgraded qmaster starts from a clean slate against the same database. The same upgrade procedure also lets operators **switch to postgres from any other spooling method** (classic or berkeleydb) without a separate migration step — choose `postgres` at the spooling-method prompt and supply the connection parameters; the upgrade discards the old on-disk spool and populates a fresh PostgreSQL schema.
- **Configurable via libpq conninfo.** Standard libpq keywords (host, port, dbname, user, sslmode, sslcert/sslkey/sslrootcert, keepalives_*, connect_timeout) plus the xxQS_NAMExx extension `retry_max_seconds=` are accepted in `spooling_params`. See `sge_bootstrap(5)` for the keyword reference.

For provisioning steps, conninfo syntax, troubleshooting, and the HA story, see the *Installation Guide* chapter **"PostgreSQL Spooling"** and the *Administration Guide* chapter **"High Availability and Reliability"**.

(Available in Gridware Cluster Scheduler only.)


### Stored Procedures for Status Query Commands

Version 9.2 introduces stored procedures for status query commands. Instead of having command line clients issue
multiple individual GDI requests to collect and assemble data, a stored procedure encapsulates the entire data
collection, filtering, and output rendering on the server side. The client sends a single `GET_PROCEDURE` request;
the qmaster executes the procedure and returns the fully formatted result.

**Benefits**

- **Reduced network overhead**: A single round-trip replaces the multiple GDI requests previously required per
  command invocation. For example, `qstat -f` previously triggered up to 15 individual object fetches; it now
  issues a single request.
- **Server-side rendering**: Output formatting (plain text, XML, JSON) is performed inside the qmaster, eliminating
  the need for clients to fetch raw object data and format it locally.
- **Consistent output**: Because formatting is centralized, all clients — command line, DRMAA, REST API — receive
  identical, well-defined output for the same query.
- **Better scalability**: Fewer GDI requests per command invocation reduces contention on the qmaster under high
  query load.

**Architecture**

Stored procedures follow a model-view-controller pattern:

- *Parameter*: The client serializes its command-line arguments and options into a parameter bundle and transmits
  them as part of the `GET_PROCEDURE` GDI request.
- *Model*: The server-side model runs inside the qmaster process and reads data directly from the in-memory master
  object lists, without additional GDI round-trips.
- *View*: A format-specific view (plain text, XML, or JSON) renders the collected data into the final output.
- *Controller*: Orchestrates model and view, applies filtering, and returns the rendered response to the client.

**Access Control**

Stored procedures respect the same role-based access control as regular GDI GET requests. The qmaster verifies
the authenticated identity of the requesting user and host before executing a procedure.

**Available Procedures**

The following status query commands have been converted to use stored procedures:

- *qstat* — job and queue status (variants: default format, cluster queue format, job format, qselect)
- *qhost* — host and resource information
- *qrstat* — advance reservation status
- *qquota* — resource quota information

(Available in Open and Gridware Cluster Scheduler.)

### X11 Forwarding for Interactive Jobs

The builtin Interactive Job Support (IJS) mode now supports X11 forwarding for qrsh(1) and qlogin(1).
When a user adds the `-X` flag, graphical applications launched inside the interactive session display on
the user's local X server, regardless of which execution host the job runs on.

**How it works**

1. The qrsh(1) or qlogin(1) client reads the MIT-MAGIC-COOKIE-1 for the local display from the user's
   Xauthority database and transmits it to the shepherd as part of the session setup.
2. The shepherd creates a Unix-domain proxy display at `/tmp/.X11-unix/XN` on the execution host
   (N >= 10, first available), writes a matching Xauthority entry for the proxy display into the job
   owner's `~/.Xauthority`, and injects `DISPLAY=:N.0` into the job environment before the job starts.
3. When an X client inside the job opens a connection to the proxy display, the shepherd relays the X11
   protocol stream back through the commlib connection to the qrsh(1) or qlogin(1) client, which forwards
   it to the real X server.

The proxy display socket and the Xauthority entry are cleaned up automatically when the job ends.

**Usage**

    qrsh -X -b y xterm
    qlogin -X

If `DISPLAY` is not set in the submitting environment, qrsh(1) and qlogin(1) skip X11 setup and proceed
normally without forwarding, so `-X` is safe to use even on headless submit hosts.

X11 forwarding requires builtin IJS mode (`rsh_command builtin` / `qlogin_command builtin` in the global
configuration). It is not available with the legacy ssh/rsh transport.

(Available in Open and Gridware Cluster Scheduler.)

### Port Range for qrsh and qlogin

When a user submits an interactive job via qrsh(1) or qlogin(1), the client binds a TCP listen socket and
passes the port number to the shepherd, which connects back to it once the job starts. By default the operating
system selects any free ephemeral port for this purpose. In environments with strict firewall rules between the
submit host and the execution hosts — where only a specific port range is permitted through the firewall — this
uncontrolled port selection prevents interactive jobs from working at all.

A new global configuration parameter `port_range` addresses this by restricting the TCP ports that the qrsh(1)
and qlogin(1) client may bind to. Administrators configure the same port range in both the scheduler and the
firewall, ensuring that the connect-back port is always within the allowed range.

The parameter accepts a comma-separated list of range expressions of the form `n-m` or `n-m:s`, where `s` is an
optional step. For example:

    port_range  50100-50200
    port_range  50100-50200:2,51000-51100

The client tries each port in the configured ranges sequentially and binds to the first available one. If no
port is free the client exits with an error. When the parameter is `NONE` or not configured, the operating
system assigns a free ephemeral port automatically, preserving the previous behaviour.

`port_range` applies to both the builtin IJS mode and the legacy ssh/rsh mode and can be set or changed with
`qconf -mconf` without restarting any daemon.

(Available in Open and Gridware Cluster Scheduler.)

### Reconnect to Running Interactive Sessions (qrsh -reconnect)

Interactive jobs (`qrsh`, `qlogin`) historically died the moment the client connection dropped — a laptop
suspend, VPN hiccup, or accidental terminal close meant the whole session was gone, including any
long-running work inside it. The new reconnect feature lets the user pick the session back up from any submit
host as long as the job is still alive.

Two pieces work together:

* **Grace period at the shepherd.** A new global configuration parameter `ijs_reconnect_timeout` in
  `qmaster_params` declares how long (in seconds) the shepherd holds an interactive job alive after an
  unexpected client disconnect. During this window the shepherd `SIGSTOP`s the job and polls for a reconnect.
  When the timeout expires without a reconnect, the job is killed as before. The default is `0` (disabled —
  pre-9.2.0 behaviour preserved).

* **`-reconnect <job_id>` client mode** (qrsh and qlogin). A new client mode that asks qmaster to broker a
  reconnect to a running job that the caller owns. qmaster validates ownership, generates a single-use token,
  relays the new client's listen address and the token to the execd that runs the job, and returns the token
  to the client. The waiting shepherd reads the relayed info, opens a fresh commlib connection back to the
  new client, presents the token, and on a match `SIGCONT`s the job and resumes the PTY bridge. The original
  client is fully replaced by the new one — keystrokes and output now flow to the new terminal.

Example:

    # Administrator: allow 5 minutes for users to recover from disconnects
    qconf -mconf global   # qmaster_params: ijs_reconnect_timeout=300

    # User on laptop:
    $ qrsh                # job 1234 starts, user is working in shell ...
    # ... VPN drops, terminal disappears ...

    # User reconnects to office network and runs from any submit host:
    $ qrsh -reconnect 1234
    # ... same shell, same working directory, same processes still running ...

The handshake uses a one-time token brokered by qmaster, so the reconnect request must come from the job
owner; another user with the job id cannot hijack the session. commlib TLS (already used for IJS in 9.1.0)
protects the wire. The original client's listen port and the reconnect client's listen port both use the
configured `port_range`, so firewall rules established for the initial session continue to apply.

The grace period uses systemd cgroup freeze when the systemd integration is active — the job is paused
without losing any in-memory state, and resumes seamlessly on reconnect.

(Available in Open and Gridware Cluster Scheduler.)

### File-Based Bulk Configuration and Export with qconf

`qconf` gains a complete file- and directory-based interface for the cluster's configuration objects.
Operations that previously worked on a single object through an interactive editor can now be driven from
files and whole directories in a single command, making cluster configuration scriptable, idempotent, and
well suited to backup, migration, and external configuration management.

**Bulk add, modify and delete**

The file-based add (`-A<obj>`), modify (`-M<obj>`) and delete (`-D<obj>`) operations now accept either a single
file or a directory of files. A directory is processed as one batch — every object in it is applied in a single
request, and a one-line summary reports how many objects were processed and how many failed. The name-list
delete (`-d<obj> a,b,c`) removes several objects in one call. These operations are available for all
configuration object types: calendars, complex entries, checkpointing environments, host and global
configurations, execution hosts, host groups, parallel environments, projects, cluster queues, resource quota
sets, usersets and users, plus the share tree and scheduler configuration.

**Add and modify are interchangeable (upsert)**

`-a`/`-A` and `-m`/`-M` no longer fail depending on whether an object already exists. Applying a file creates the
object if it is missing and modifies it if it is present, so re-applying the same file is idempotent and safe to
repeat — the behaviour external configuration-management tools rely on. Likewise, deleting an object that no
longer exists is reported and skipped rather than treated as an error.

**Configuration export (`-S<obj>`)**

A new save operation, `-S<obj>`, is the inverse of the file-based add: it writes objects to re-importable files.
`qconf -Sq queues/` exports every cluster queue to its own file, named after the queue, and `qconf -Aq queues/`
reads them all back. Exported files use exactly the format the importer reads, so a complete cluster
configuration can be exported, edited or placed under version control, and re-applied unchanged. A single object
can be exported by name (for example `qconf -Sprj myproject`).

**Safety for batch operations**

- `-dry` previews exactly what a directory-based add or delete would do, without contacting the qmaster.
- `-strict` validates a whole directory first and applies nothing if any file is malformed.
- `-f` is required to overwrite an existing export file and to skip the confirmation prompt of a directory-wide
  delete.
- Object names that are not valid file names, or that would collide on a case-insensitive file system, are
  rejected during export rather than silently altered.

**Structured JSON format (`-fmt json`)**

The show (`-s<obj>`), file-based add/modify (`-A`/`-M`) and export (`-S`) operations accept `-fmt json`, which
emits and reads a structured JSON representation with natively typed values (numbers, booleans, nested arrays)
validated against published JSON schemas. This makes qconf output and input directly consumable by external
tooling such as `jq`, scripts and configuration-management frameworks, instead of the traditional flat-file
format. The default remains the classic ASCII format.

**Usage**

    # Back up every queue, edit, and re-apply
    qconf -Sq queues/
    qconf -Aq queues/

    # Apply a whole directory of host configurations in one batch
    qconf -Mconf hosts/

    # Preview a bulk delete without performing it
    qconf -dry -Dq queues/

    # Export and re-import in structured JSON
    qconf -fmt json -Sp pes/
    qconf -fmt json -Ap pes/

Together these changes let administrators treat cluster configuration as files that can be exported, diffed,
stored in version control, and re-applied — the foundation for managing a cluster's configuration with standard
configuration-management workflows.

(Available in Open and Gridware Cluster Scheduler.)

[//]: # (Each file has to end with two empty lines)

