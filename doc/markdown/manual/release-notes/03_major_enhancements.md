# Major Enhancements

## v9.2.0

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

* **`qrsh -reconnect <job_id>` client mode.** A new client mode that asks qmaster to broker a reconnect to a
  running job that the caller owns. qmaster validates ownership, generates a single-use token, relays the new
  client's listen address and the token to the execd that runs the job, and returns the token to the client.
  The waiting shepherd reads the relayed info, opens a fresh commlib connection back to the new client,
  presents the token, and on a match `SIGCONT`s the job and resumes the PTY bridge. The original client is
  fully replaced by the new one — keystrokes and output now flow to the new terminal.

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

[//]: # (Each file has to end with two empty lines)

