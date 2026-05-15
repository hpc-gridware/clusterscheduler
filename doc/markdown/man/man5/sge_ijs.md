---
title: sge_ijs
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

sge_ijs - xxQS_NAMExx Interactive Job Support (IJS)

# DESCRIPTION

**Interactive Job Support (IJS)** is the xxQS_NAMExx channel that connects a user's local terminal to a
process running on a compute node — the mechanism behind `qrsh(1)` and `qlogin(1)`. It carries:

- `qlogin` — interactive login shell, semantically similar to `ssh(1)` / `telnet(1)` but routed through the
  cluster.
- `qrsh` (without command) — interactive shell over the IJS PTY, in the same vein as `ssh user@host` or
  `rsh(1)`.
- `qrsh <command> [args ...]` — non-interactive remote command with stdin / stdout / stderr piped through
  IJS. With `-noshell` the command runs directly without shell parsing.
- `qrsh -reconnect <job_id>` — take over an existing interactive job whose original client disconnected
  (see *Reconnect / grace period* below).

`qsh(1)` is *not* an IJS user: it submits a job that runs an `xterm` on the exec host, and the xterm talks
to the user's `$DISPLAY` directly via plain X11. The rest of this page concerns the `qrsh` / `qlogin` IJS
data channel.

Unlike a batch job submitted with `qsub(1)`, an interactive job needs:

- bidirectional **stdin / stdout / stderr** between the local terminal and the remote job;
- correct **TTY semantics** (raw mode, line discipline, window size, ISIG signals);
- **signal forwarding** for Ctrl+C / Ctrl+\\ / Ctrl+Z;
- on demand: **X11 forwarding**, **PTY allocation**, **reconnect** after a dropped network.

All of this has to happen across whatever firewall, NAT, and identity boundary sits between the submit
host and the exec host. The rest of this page is how xxQS_NAMExx does that under the builtin transport.

## Two transport modes

xxQS_NAMExx offers two transports for IJS — chosen at the cluster level (or per host) via three pairs of
`qconf -mconf` keys. Each pair governs one use case:

| Use case                                         | Server-side daemon | Client-side command |
| ------------------------------------------------ | ------------------ | ------------------- |
| `qlogin` — interactive login session             | `qlogin_daemon`    | `qlogin_command`    |
| `qrsh` without a command — interactive shell     | `rlogin_daemon`    | `rlogin_command`    |
| `qrsh` with a command — remote command execution | `rsh_daemon`       | `rsh_command`       |

Each key is set to either `builtin` (default) or to an external executable path. `builtin` means qmaster,
execd, shepherd, and the qrsh / qlogin client use the cluster's own commlib transport (TCP, optionally
TLS) — no external command is forked on either side; the client and the shepherd talk directly. An
external value points at a transport-providing program (typically `/usr/bin/ssh` on the client side and
`/usr/sbin/sshd -i` on the daemon side).

**builtin**

- Transport: commlib over TCP (the same channel used by qmaster control traffic).
- Identity: UNIX UID / GID + MUNGE.
- Per-host setup: qmaster MUNGE keys; `port_range` open inbound on submit host.
- Reconnect supported (`qrsh -reconnect`, `qlogin -reconnect`).
- IJS escape parser (`~.`, `~^Z`, `~?`); escape character configurable via `ijs_escape_char`.
- X11 forwarding: cookie injection on `qrsh -X` / `qlogin -X`, native proxy display on exec host.
- Keepalive / dead-connection detection: configurable via `ijs_keepalive_interval` and
  `ijs_keepalive_count`.
- Network direction: qrsh client listens, shepherd connects back (commlib over the open socket).
- Encryption: inherits the cluster security mode (plain TCP or TLS).

**external (SSH / rsh wrapper)**

- Transport: `/usr/bin/ssh -X user@host ...` (or `rsh` / `rlogin`).
- Identity: SSH key / PAM (whatever the wrapper is configured for).
- Per-host setup: SSH keys / host keys deployed everywhere; possibly `~/.rhosts`.
- Reconnect: no.
- IJS escape parser: no (`~.` only via SSH).
- X11 forwarding: delegated to `ssh -X`.
- Keepalive: SSH's `ServerAliveInterval` / `ServerAliveCountMax`.
- Network direction: qrsh client listens, daemon on exec connects back (SSH protocol over the open
  socket — `sshd -i` exec'd on that fd).
- Encryption: SSH cipher suite.

## Choosing between them

Prefer **builtin** when:

- consistent identity and access control via MUNGE rather than maintaining SSH keys across N nodes is
  desired,
- session **reconnect** (laptop close, VPN drop) is required,
- the IJS-side X11 cookie injection is needed (the wrapper relies on `ssh -X` configuration on every
  node),
- the interactive flow's diagnostics should appear through the same logging / trace surface as the rest
  of the daemons.

Prefer the **external wrapper** when:

- MUNGE is not deployable across the cluster,
- an SSH-style cipher suite or pre-existing audited SSH crypto on the data channel is required.

The two are mutually exclusive per host but can be mixed cluster-wide via per-host configuration; see
*Per-host overrides* below for the constraints.

## Security posture

- **Cluster security mode** (`tls` in the bootstrap file, or plain) selects the commlib transport. Both
  the client ↔ qmaster control plane **and** the client ↔ shepherd IJS data channel pick the same
  transport — plain TCP or TLS. There is no separate IJS-only encryption knob: turn TLS on once, it
  covers everything.
- **Identity** is end-to-end **MUNGE** + UNIX UID / GID. The reconnect handshake uses a one-time
  MUNGE-backed token issued by qmaster, so the new client never needs out-of-band credentials.

From here on this page covers the **builtin** transport. Configuring an external wrapper (typically
`/usr/bin/ssh` plus `/usr/sbin/sshd -i`, with the `SetEnv` directive needed to propagate `QRSH_PORT`) is
out of scope; see `sge_conf(5)` and the `qrsh(1)` / `qlogin(1)` man pages.

## Enabling builtin mode

`builtin` is the default value for all six transport keys, so a fresh cluster needs no configuration to
use IJS. The instructions below apply when switching back from an external SSH / rsh setup, or when
enabling builtin on selected hosts only.

### Prerequisites

- **MUNGE** installed and running on the qmaster host, every execd host, and every submit host where
  users will run `qrsh` / `qlogin`. Identity propagation goes through MUNGE; the reconnect handshake
  additionally relies on a MUNGE-backed token issued by qmaster.
- The cluster commlib mode (plain TCP or TLS) is set in the bootstrap file and applies to both the
  qmaster control plane and the IJS data channel. Switching IJS to builtin does **not** by itself enable
  TLS — that is a separate setting.
- To restrict the qrsh client's listener port, configure `port_range` (see *Network posture: port_range
  & firewall direction* below).

### Per-host overrides

The six keys live in the global cluster configuration (`qconf -mconf`), and can be overridden per host
(`qconf -mconf <hostname>`). Per-host values take precedence over the global config on that host. Remove
an override with `qconf -dconf <hostname>` to fall back to the global value.

The transport choice is resolved by two independent code paths that must agree per job:

- The **qrsh / qlogin / qrsh-no-cmd client** reads `rsh_command` / `qlogin_command` / `rlogin_command`
  from the *submit host's* local config (plus global fallback) once, before scheduling. It uses that
  choice for every job it submits, regardless of which exec host the scheduler picks. There is no second
  lookup after placement — the client does **not** consult the target exec host's per-host config.
- The **shepherd on the exec host** reads the corresponding `*_daemon` key from the *exec host's* local
  config (plus global fallback). That choice governs how it accepts the IJS connection.

If the two disagree (e.g. client uses `builtin` commlib, shepherd on the chosen exec host forks
`sshd -i`), the IJS handshake never completes and the interactive session hangs.

### Partitioning a cluster between transports

To split a cluster — for instance, keeping most nodes on builtin while running SSH on one node that
lacks MUNGE — both sides must be aligned in the same subset, and scheduling must keep jobs in their
lane:

| Subset        | Submit-host config (`rsh_command` etc.)                                       | Exec-host config (`rsh_daemon` etc.)            |
| ------------- | ----------------------------------------------------------------------------- | ----------------------------------------------- |
| builtin nodes | `builtin` (typically inherited from global)                                   | `builtin`                                       |
| ssh nodes     | `/usr/bin/ssh` / wrapper (per-host on each submit host that targets this set) | `/usr/sbin/sshd -i` / wrapper (per-host)        |

Use queues, user lists, or `-l hostname=` discipline to ensure each submit host only sends interactive
jobs to the matching subset. A `qrsh -l hostname=<other-subset>` issued from a submit host whose
`rsh_command` does not match the target's `rsh_daemon` will hang.

### When changes take effect

Configuration changes take effect immediately, but only for **newly started** interactive jobs. Running
sessions keep using whatever transport they were started with — they will not switch under the user's
feet, and they will not fail.

## Basic I/O (stdin / stdout / stderr forwarding)

The client and the shepherd open a single commlib connection and exchange the standard streams as three
distinct kinds of messages: stdin from client to job, stdout and stderr from job to client. Keeping
stderr on its own channel means a client-side `2>err.log` redirection still works — the streams do not
merge in transit.

### How the command is invoked

For batch-style `qrsh <command>` calls, two flags control how the remote command is executed on the
compute node:

- **Default**: the argv is shell-quoted and handed to `bash -c` on the exec host. PATH lookup, `$VAR`
  expansion, `~` expansion, and the rest of the shell still work. Each argv element is single-quote
  quoted before the join, so spaces, quotes, and shell metacharacters in any one argument survive the
  round trip — `qrsh /bin/sh -c "echo a; echo b"` does the right thing.
- **`-noshell`**: argv is `execvp(3)`'d directly with no shell. Use this when exact-argv semantics are
  required, or to avoid any shell at all (for instance, when submitting a binary that misinterprets a
  shell wrapper).
- **`-b y`** (binary submission flag): goes through the same shell path as the default; the quoting
  above keeps the original argv intact. Combine with `-noshell` to skip the shell entirely.

### Configuration parameters

None. Basic I/O is always on whenever the IJS data channel is open.

## PTY allocation & terminal handling

Whether the remote job gets a pseudo-terminal — and whether the client switches its local terminal into
raw mode — depends entirely on the invocation, not on cluster configuration.

### Default behavior

| Invocation                       | PTY on exec host? |
| -------------------------------- | ----------------- |
| `qlogin`                         | yes               |
| `qrsh` (no command)              | yes               |
| `qrsh <command>`                 | no                |
| `qrsh -b y <command>`            | no                |
| any of the above with `-pty y`   | yes (forced)      |
| any of the above with `-pty n`   | no (forced)       |

When a PTY is allocated, the shepherd opens a master / slave pair on the exec host, becomes the session
leader, and runs the job with the slave as its controlling terminal. Output flows through the PTY master
and the shepherd over the commlib connection to the client's stdout; stdin goes the other way.

### Client-side raw mode

When the client's stdout is a TTY (`isatty(STDOUT_FILENO) == 1`), qrsh puts the local terminal into
**raw mode** for the duration of the session. This disables the local line discipline — bytes typed at
the keyboard are sent through unchanged, including signal characters (Ctrl+C, Ctrl+\\, Ctrl+Z). The
remote PTY then re-interprets them at the other end (see *Signal forwarding* below). The terminal is
restored on session exit and on `~.` disconnect.

If the client's stdout is not a TTY (typical for shell pipelines and CI wrappers that redirect output),
the client skips raw mode entirely. Bytes flow byte-for-byte, but the *local* line discipline still
applies — so e.g. Ctrl+C delivers SIGINT to the qrsh client itself rather than reaching the remote job.

### Configuration parameters

None. PTY allocation is per-invocation only.

## Window size & SIGWINCH propagation

When a PTY session starts, the client reads its own terminal dimensions and ships them to the shepherd,
which applies them to the exec-side PTY with `ioctl(TIOCSWINSZ)`. After that, every SIGWINCH the client
receives (xterm resize, tmux pane resize, etc.) is converted to a control message and propagated the
same way. The remote shell sees its own SIGWINCH and re-flows accordingly — `less(1)`, `top(1)`,
`vim(1)`, and any full-screen tool react as expected.

This only happens when a PTY is allocated. `qrsh <command>` without `-pty y` has no remote PTY to
resize.

### Zero-dimension fallback

Some clients report a 0×0 winsize at session start — typical for CI runners, headless wrappers, and
terminals that have not drawn their first frame yet. Setting a PTY to 0×0 produces unusable output
(every line wraps at column 0). qrsh detects this case and substitutes a sentinel size of 60 rows × 80
columns until a real SIGWINCH arrives, after which the actual dimensions take over.

### Configuration parameters

None.

## Signal forwarding

POSIX line discipline maps control characters to signals via the terminal's `ISIG` flag:

| Key     | Termios char | Byte   | Signal  |
| ------- | ------------ | ------ | ------- |
| Ctrl+C  | `VINTR`      | `0x03` | SIGINT  |
| Ctrl+\\ | `VQUIT`      | `0x1c` | SIGQUIT |
| Ctrl+Z  | `VSUSP`      | `0x1a` | SIGTSTP |

Whether the mapping happens on the client side or on the exec-side PTY determines where the signal
actually lands, and that depends on whether the session has a PTY and a TTY-attached client.

### PTY session with a TTY client

In a typical interactive session (`qlogin`, `qrsh` without command, `qrsh -pty y`) the client's local
terminal is in raw mode. Signal characters are *not* interpreted locally — the raw bytes (`0x03`,
`0x1c`, `0x1a`) flow through stdin to the shepherd and into the exec-side PTY, whose ISIG layer
delivers SIGINT / SIGQUIT / SIGTSTP to the remote job's foreground process group. The signal reaches
the job, not the qrsh client.

Ctrl+Z has an extra step: after forwarding `0x1a`, the qrsh client also raises SIGTSTP on itself. The
effect is that pressing Ctrl+Z pauses the remote job *and* puts the qrsh client into the background —
the same way a local shell would background a foreground process. `fg` in the parent shell resumes
both ends. To suspend only the client without pausing the remote job, use the IJS escape `~^Z` (see
*IJS escape sequences* below).

### Without a PTY, or with a non-TTY client stdout

For `qrsh <command>` (no `-pty y`) or when the client's stdout is redirected, the client never enters
raw mode. The *local* line discipline intercepts Ctrl+C / Ctrl+\\ / Ctrl+Z and delivers the signal to
the qrsh client itself. qrsh's handler exits cleanly, the commlib connection drops, and the remote job
dies through that disconnect. The job has no opportunity to catch or handle the signal — only the PTY
path gives you that.

### Client-side SIGHUP / SIGTERM

SIGHUP or SIGTERM delivered to the qrsh client (closing the terminal emulator, `pkill -HUP qrsh`, etc.)
is treated as a clean exit signal: workers shut down, the terminal is restored, the connection drops.
Whether the remote job dies immediately or is held in a grace window depends on the reconnect timeout
(see *Reconnect / grace period* below).

### Configuration parameters

None for the byte-forwarding path. The IJS escape character is separately configurable; see *IJS escape
sequences* below.

## IJS escape sequences (disconnect, suspend, help)

When the qrsh client is in raw mode, an in-band escape parser watches the stdin byte stream for short
command sequences. At a **line start** (the previous forwarded byte was `\n` or `\r`, or the session has
just begun), the configured escape character followed by an action character triggers a local operation
— those bytes are not forwarded to the remote shell.

| Sequence | Action                                                                                            |
| -------- | ------------------------------------------------------------------------------------------------- |
| `~.`     | **Disconnect** — close the session. The remote job's fate depends on the grace timer.             |
| `~^Z`    | **Local suspend** — stop only the qrsh client; the remote job keeps running. `fg` resumes it.     |
| `~?`     | Print escape-sequence help to the local terminal.                                                 |
| `~~`     | Emit a literal `~`.                                                                               |

The line-start requirement matters: typing the escape character mid-line forwards it as a normal byte.
An unrecognised pair (e.g. `~x` where `x` is neither `.`, `^Z`, `?`, nor the escape character itself)
forwards both bytes verbatim — so existing scripts that emit stray `~` bytes are not corrupted.

### Configuring the escape character

Set via `qmaster_params` (cluster-wide):

- `ijs_escape_char=<char>` — single character. Defaults to `~`.
- `ijs_escape_char=none` — disables the escape parser entirely.

A non-`~` value is sometimes necessary when the IJS session is reached through an SSH wrapper (test
harnesses, certain jump-host setups). The wrapping SSH intercepts its own `~` escapes before the bytes
reach qrsh, so the qrsh parser never sees them. Setting `ijs_escape_char=^` (or any other unused
character) lets the sequence pass through SSH untouched.

### Local vs remote suspend

Two suspend mechanisms coexist; pick deliberately:

- **Remote suspend** — plain `Ctrl+Z` at the shell. The byte travels through the PTY, the remote ISIG
  layer delivers SIGTSTP to the remote foreground process group, **the remote job pauses**. The qrsh
  client also self-suspends on this path; `bg` / `fg` at the remote shell are the normal idioms.
- **Local suspend** — the IJS escape + `^Z` (e.g. `~^Z`). The parser restores the local terminal and
  raises SIGTSTP on the qrsh client; **the remote job keeps running**. `fg` in the parent shell
  resumes the client.

The qrsh option `-suspend_remote yes` adds an explicit SIGSTOP-via-control-message on top of the
PTY-delivered SIGTSTP when Ctrl+Z is pressed. Useful for jobs that spawn children *outside* the
foreground process group: those children would not be reached by the PTY's ISIG layer, but the explicit
shepherd-driven SIGSTOP reaches the whole job.

The everyday use case for local suspend: background a long-running interactive job and detach the
client without killing the job. Combined with reconnect, it gives near-tmux-like detach / reattach
behavior.

### Configuration parameters

- `ijs_escape_char` — single character or `none`. Default `~`. (`qmaster_params`)

See `sge_conf(5)` for the canonical parameter entry under `qmaster_params`.

## Keepalive / dead-connection detection

A working TCP connection between qrsh client and shepherd can sit silent indefinitely without either
end noticing the other has gone — a router NAT rule timing out, a laptop sleeping with no warning, a
process killed `-9` on the far side. OS-level TCP keepalive defaults are far too coarse for an
interactive session (Linux default: 2 h before the first probe). IJS has its own application-layer
keepalive that runs on a much shorter scale.

### How it works

When enabled, the qrsh client sends a keepalive probe to the shepherd after `ijs_keepalive_interval`
seconds of idle time on the data channel. The shepherd replies with an ACK; the client counts
consecutive missed ACKs and gives up after `ijs_keepalive_count` of them, prints a message to the
user's terminal, and tears down the local end. The shepherd then sees the connection close and follows
the unexpected-disconnect path — which either kills the job or enters the reconnect grace window
depending on `ijs_reconnect_timeout` (see *Reconnect / grace period* below).

Probes are emitted only when the channel is otherwise idle. A session actively transferring data (build
output, `tail -f`, etc.) does not need them and will not generate them.

### Configuration parameters

Both in `qmaster_params` (cluster-wide):

- `ijs_keepalive_interval=<seconds>` — idle seconds before the next probe. Default `60`. Set to `0`
  to disable keepalive entirely.
- `ijs_keepalive_count=<n>` — number of consecutive missed ACKs before the client declares the
  connection dead. Default `3`.

See `sge_conf(5)` for the canonical entries under `qmaster_params`.

With defaults, the client gives up after roughly `interval × (count + 1)` ≈ 4 minutes of unanswered
probes. Tune the interval down for snappier disconnect detection at the cost of more idle traffic, or
up on a flaky link where transient ACK drops are common.

### Interaction with reconnect

Keepalive protects the **client** from a stuck connection. When the client tears down on missed ACKs,
the shepherd applies the reconnect grace policy the same way as for any other disconnect — so a
keepalive-driven teardown still benefits from `qrsh -reconnect` when `ijs_reconnect_timeout > 0`.

## Reconnect / grace period

By default, when a qrsh client disconnects unexpectedly the job dies promptly. With the reconnect
feature enabled, the shepherd instead pauses the job and waits for a fresh client to take over — for
users with flaky VPNs, laptops that sleep, or shells closed accidentally.

### How it works

When `ijs_reconnect_timeout > 0` and the shepherd notices its IJS connection has been torn down without
a clean shutdown handshake:

1. It SIGSTOPs the job so it does not keep producing output with nowhere to go.
2. It begins polling for a small per-job file `reconnect.info` in the job spool directory. The file is
   written by execd when qmaster instructs it to, and contains the new client's host, port, and a
   one-time random token (a 32-byte nonce generated by qmaster from `/dev/urandom`).
3. When the file appears, the shepherd opens a fresh commlib connection out to the new client's
   listener, sends a reconnect request carrying the token, and waits up to ~5 s for the client to
   accept it.
4. On accept, the shepherd swaps in the new connection, applies the new client's terminal window size
   to the job's PTY, SIGCONTs the job, and resumes normal forwarding.
5. If `ijs_reconnect_timeout` expires with no successful handshake, the shepherd SIGCONTs the job to
   unfreeze it and then SIGKILLs it — the user-visible outcome is the same as if reconnect had been
   disabled.

### Taking over a session

From a fresh shell on any **submit host** (i.e. any host listed by `qconf -ss`):

```
qrsh -reconnect <job_id>
```

(or `qlogin -reconnect <job_id>` for a qlogin-style session)

The new client must run under the same UNIX user as the job — qmaster validates ownership before
issuing the reconnect token, and foreign-user attempts are rejected cleanly. Only one reconnect is
honored at a time; a second concurrent attempt is dropped without disrupting the existing session.
After the grace timer expires the job is gone — a subsequent `-reconnect` fails cleanly.

It does **not** have to be the *same* submit host the job was originally launched from — any submit
host with network reachability from the exec host is fine.

### Configuration parameters

`qmaster_params` (cluster-wide):

- `ijs_reconnect_timeout=<seconds>` — grace window in seconds. Default `0` (disabled).

See `sge_conf(5)` for the canonical parameter entry under `qmaster_params`.

### Operational guidance

- **Disabled** (`0`) gives pre-9.2 IJS behaviour — any disconnect kills the job immediately. Most
  resource-conservative setting; pick it if no one needs reconnect.
- **60-300 s** recovers transient connection blips without leaving abandoned slots tied up.
- **Hours** suit laptop / VPN use cases but should be paired with a job runtime limit (`h_rt`) to
  bound the worst case.
- Cost when no disconnect happens is essentially zero — no extra traffic, no extra state.
- The shepherd captures `ijs_reconnect_timeout` at job start. Changing the param while a session is
  already in grace does NOT shorten or extend that session's grace window — the running shepherd
  continues with the value it read when the job first started. New jobs submitted after the change
  pick up the new value. Implication: tightening the timeout to limit a slot-hoarding session
  already in grace requires `qdel`'ing that session — the param change alone will not help.

### Limitations

- Reconnect requires the `job_id` out-of-band — there is no discovery flow listing "your jobs awaiting
  reconnect". The pragmatic convention is to log the job_id locally on startup or look it up with
  `qstat(1)` from a separate session.
- The new client must be reachable from the exec host on a `port_range` port. A NAT that prevents the
  exec host from connecting back to the new client breaks the handshake step.

## X11 forwarding

For GUI tools that need to display on the user's workstation while running on the compute node, IJS
provides an X11 forwarding layer roughly analogous to `ssh -X`, but built into the qrsh / qlogin
client rather than delegated to an external SSH wrapper.

### How it works

`-X` (capital X) on `qrsh` or `qlogin` enables forwarding (`qsub(1)` rejects it; for `qsh(1)` the flag
uses a different, non-IJS path):

```
  user X server                qrsh -X (client)            shepherd                 job
        |                            |                         |                     |
        |                            |  xauth list $DISPLAY    |                     |
        |                            |  (read cookie)          |                     |
        |                            |---- cookie message ---->|                     |
        |                            |                         |                     |
        |                            |          create proxy socket /tmp/.X11-unix/X<N>
        |                            |          cookie -> ~/.Xauthority              |
        |                            |          DISPLAY=:N.0 in job environment      |
        |                            |                         |---- start job ----->|
        |                            |                         |                     |
        |                            |                         |<-- connect (proxy) -|
        |                            |<------ relay X11 ------ |                     |
        |<---- forward to user X --- |                         |                     |
        |---- reply ---------------->|---- relay ------------->| ---- deliver ------->|
```

1. The client reads the `MIT-MAGIC-COOKIE-1` entry for the local `$DISPLAY` via `xauth list`. If no
   cookie is available, the client logs a warning and runs the job without X11 forwarding.
2. The cookie is sent to the shepherd at session startup, **before the job is unblocked**, so the
   proxy display is in place when the job first reads `$DISPLAY`.
3. The shepherd creates a Unix-domain proxy display at `/tmp/.X11-unix/X<N>` (`N >= 10` to avoid
   colliding with real local displays), writes the cookie to the user's `~/.Xauthority`, and injects
   `DISPLAY=:<N>.0` into the job's environment.
4. X11 clients started by the job connect to the proxy socket; the shepherd relays the X11 protocol
   over the IJS data channel to the user's local X server.
5. The proxy display, its socket, and the Xauthority entry are removed when the session ends.

No per-host SSH key setup, no manual cookie propagation, no `xhost +`.

### When `-X` quietly does nothing

- The user's `.bashrc` / login profile unsets or overwrites `DISPLAY` before launching the GUI — the
  proxy is still there, but the binary points at the wrong display.
- The local `$DISPLAY` uses an auth protocol other than `MIT-MAGIC-COOKIE-1` — the cookie lookup
  returns nothing, forwarding is disabled.
- `$DISPLAY` is empty (headless / CI host) — same outcome.

In all three cases the client emits a warning on stderr and the job runs without X11.

### X11 does not survive `qrsh -reconnect`

When a `qrsh -X` session is taken over via `qrsh -reconnect`, the proxy display the original client
set up dies with the original client process. The shell's `DISPLAY` env var still points at the dead
`:N.0` proxy after reconnect, and the reconnect handshake does **not** allocate a new proxy on the
new client's host nor update `DISPLAY` in the shell's environment. X clients started after the
reconnect fail with `unable to open display`.

Workarounds for the user:

- Re-export `DISPLAY` from inside the reconnected session to point at a working X server reachable
  from the exec host (rarely practical — the exec host typically has no direct route to the user's
  workstation X server, which is exactly why `-X` exists).
- Restart the entire session — submit a fresh `qrsh -X` rather than reconnecting.

### Configuration parameters

None. X11 forwarding is per-invocation only — there is no qmaster_param toggle and no cluster-wide
policy.

## Network posture: `port_range` & firewall direction

### Connect-back model

The qrsh client binds a TCP port on its submit host and listens; the shepherd (or, in external mode,
the daemon process started by the shepherd) opens a connection out **from** the exec host **to** that
port. The direction is the same for both transports.

```
  qrsh client          qmaster                   execd                   shepherd
        |                  |                        |                        |
        | bind TCP port    |                        |                        |
        | (port_range)     |                        |                        |
        |                  |                        |                        |
        |- job (+ port) -->|                        |                        |
        |                  |--- dispatch ---------->|                        |
        |                  |                        |--- spawn ------------->|
        |                  |                        |                        |
        |<-------------- connect to submit:port ---------------------------- |
        |                                                                    |
        |========== stdin / stdout / stderr over this socket ================|
```

This matches the typical HPC firewall posture: compute → submit is allowed (jobs deliver results, mail,
accounting events back to the submit side), submit → compute is often blocked. IJS specifically
exploits that asymmetry: the client never opens a connection toward a compute node.

The practical implication for the cluster operator: **the qrsh client's listener port must be reachable
inbound on the submit host**. That is the only firewall rule IJS adds.

### Restricting the port range

By default the qrsh client asks the OS for an ephemeral port (in the unprivileged range — typically
`32768-60999` on Linux). For sites that need predictable firewall rules, set `port_range` in
`qmaster_params`:

```
port_range  50100-50500
```

The grammar is a comma-separated list of ranges, each `n-m` or `n-m:s` (with `s` as a step):

```
port_range  50100-50200:2,51000-51100      # every other port from 50100-50200, plus 51000-51100
port_range  50100                          # a single port (only one concurrent interactive session at a time)
```

The client tries ports sequentially within the listed ranges and binds to the first free one. If no
port is available the client exits with an error — so the range needs to be at least as large as the
expected number of concurrent interactive sessions per submit host. Changes to `port_range` take effect
immediately, both for new jobs and without disrupting running sessions.

When `port_range` is unset or set to `NONE`, ephemeral ports are used.

### NAT and multi-homed submit hosts

The shepherd connects to whatever hostname qmaster has on file for the submit host — i.e. the name
reported by the submit host at job-submission time. On a multi-homed submit host where qmaster knows it
by one interface but the compute side can only reach it through another, the connect-back fails: there
is no separate "advertise this address" knob for IJS. Workarounds:

- Configure the host alias file so qmaster's name for the submit host resolves to an address reachable
  from every exec host.
- Ensure submit hosts are reachable via a routable hostname / address from the compute network.

NAT between the submit network and the compute network breaks IJS for the same reason — there is no
application-layer mechanism to traverse a NAT boundary from the compute side. Mitigations (running the
submit machine on the compute network, a VPN, etc.) are deployment choices and out of scope here.

### Configuration parameters

- `port_range` — comma-separated list of port ranges (`n-m` or `n-m:s`) or `NONE`. Default unset (OS
  ephemeral). (`qmaster_params`)

See `sge_conf(5)` for the canonical parameter entry under `qmaster_params`.

## Authentication & identity

IJS does not invent its own authentication. It piggybacks on the cluster's existing identity stack —
MUNGE for cross-host message authentication, the OS's `passwd` / `group` databases for UID and
supplementary-group lookup, and whatever the site has wired behind those via NSS (LDAP / NIS / AD /
local files).

### What MUNGE does for IJS

MUNGE is the trust anchor between any two daemons and between any client and qmaster. It signs and
verifies a credential carrying the caller's UID / GID; without a valid credential the receiver refuses
the request. For IJS specifically:

1. **At session start** — the qrsh / qlogin client's GDI request to qmaster is MUNGE-authenticated.
   qmaster confirms which UNIX user is submitting and stamps that identity into the job record.
2. **For reconnect** — the qrsh client's `-reconnect` GDI request is authenticated the same way as any
   other request, so qmaster knows which UNIX user is asking. qmaster then validates ownership (the
   requester must be the same user that owns the job) before issuing a reconnect token. The token
   itself is just a 32-byte random nonce; MUNGE is not involved in the handshake between the new
   client and the shepherd.

MUNGE authenticates the **caller's UID** only — the supplementary group list comes from an OS lookup on
the receiving side. MUNGE credentials have a short validity window (default 5 minutes); a cluster with
significant clock skew across hosts will see auth failures even when keys are correct. Run NTP /
chrony.

### When MUNGE is disabled

xxQS_NAMExx can be deployed without MUNGE. In that case, commlib authentication falls back to weaker
host-based checks (or TLS-cert checks if TLS is enabled), and the link between an incoming request and
a specific UNIX UID is no longer cryptographically tied to the OS — it is whatever the caller declares.
The reconnect token mechanism described above works identically either way: the token is independent
of MUNGE.

For strong cross-host authentication, run **both** MUNGE and TLS: MUNGE ties each request to the
caller's verified UID / GID, and TLS provides mutual-cert host authentication and encrypts the
transport. The two layers cover different threats and do not substitute for each other.

### UID, GID, supplementary groups

The shepherd resolves the job owner's primary GID and supplementary groups from the local `passwd` /
`group` databases on the exec host (via `getpwnam(3)` / `getgrouplist(3)`) at job-start. Two
consequences:

- **UID consistency across hosts is mandatory.** If the submit host knows the user as UID 1001 and the
  exec host knows them as UID 2001, MUNGE happily accepts the submit-side credential — but every
  file-system access on the exec side will treat the job as a different user. Centralised user
  databases (LDAP / NIS / AD) normally solve this; local-only `/etc/passwd` setups need explicit
  coordination.
- **Supplementary groups are resolved per host.** A user belonging to `mpi-team` on the submit host
  but not on the exec host will run on the exec host without that group's privileges. Mostly
  invisible, occasionally surprising when checking permissions on a shared filesystem.

### LDAP / NIS / Active Directory

Transparent. The shepherd does standard NSS lookups; whatever `nsswitch.conf(5)` configures (`passwd:`
/ `group:` lines) takes effect. No IJS-specific configuration on top.

### Configuration parameters

None at the cluster level for IJS authentication. The cluster security mode (plain commlib vs TLS) is
configured in the bootstrap file and applies to all commlib traffic; MUNGE configuration (key
distribution, daemon setup) is an OS / site concern outside the xxQS_NAMExx config surface.

## Debugging & troubleshooting

When an IJS session misbehaves — fails to start, fails to forward something, fails to reconnect —
evidence is scattered across the client process, the qmaster log, the execd log, and the per-job
shepherd trace.

### Where to look

| Source                | Path / invocation                                                  | What it tells you                                                                            |
| --------------------- |--------------------------------------------------------------------| -------------------------------------------------------------------------------------------- |
| qrsh / qlogin client  | `qrsh -verbose` (add a second `-verbose` for more)                 | scheduling progress, submission errors, X11 warnings                                         |
| qmaster               | `$SGE_ROOT/$SGE_CELL/spool/qmaster/messages`                       | every GDI request reaching qmaster, every reconnect handshake decision, signal-trigger events |
| execd                 | `$SGE_ROOT/$SGE_CELL/spool/<host>/messages` (or `execd_spool_dir`) | job-start, signal delivery, configuration-propagation events                                 |
| shepherd (per job)    | `$EXECD_SPOOL/<host>/active_jobs/<job_id>.1/trace`                 | every commlib message received, PTY setup, signal forwarding, reconnect attempts             |

The shepherd trace is normally deleted by execd at job end. Preserve it past the job by setting
`KEEP_ACTIVE=true` in `execd_params`.

### Preserving shepherd traces

```
qconf -mattr conf execd_params KEEP_ACTIVE=true global
qconf -sconf | grep execd_params
```

Confirm every token is comma-separated. Revert `KEEP_ACTIVE=true` after debugging — keeping it on
permanently fills `active_jobs/` with stale per-job directories.

### Common failure modes

- **`port_range` exhaustion** — qrsh exits at submit time complaining it cannot bind a port.
  *Symptom*: failed `qrsh` invocations correlated with high interactive-job concurrency. *Fix*: widen
  the range.
- **PTY allocation failure** — out of pseudo-terminals on the exec host (`/proc/sys/kernel/pty/nr`
  near `/proc/sys/kernel/pty/max`), or `/dev/ptmx` permissions wrong. *Symptom*: job dies at start
  with "could not allocate PTY". *Fix*: raise `kernel.pty.max`; audit `/dev/ptmx` (should be
  `crw-rw-rw-`).
- **X11 cookie missing or stale** — `qrsh -X` runs the job without forwarding. When `DISPLAY` is set
  but the cookie is stale, a warning is emitted on stderr. *Diagnostic*: run `xauth list $DISPLAY`
  on the submit host before invoking qrsh.
- **Shell rc files overriding `DISPLAY`** — `qrsh -X` sets up forwarding correctly, but the user's
  `.bashrc` on the exec host unsets or overrides `DISPLAY` before any GUI is launched. *Symptom*:
  forwarding "works" but GUIs do not appear or appear on the wrong screen. *Fix*: audit the user's
  remote dotfiles.

## Known limitations

- **`qrsh -reconnect` requires the job id out-of-band.** There is no `qstat` filter for "my jobs in
  grace" and no `qrsh -reconnect-any` discovery flow. Users either log the job id locally at submit
  time or look it up via `qstat -u $USER` from a separate session.
- **The IJS escape parser is line-start-only.** Triggering `~.` (or whatever `ijs_escape_char`
  resolves to) requires the previous *forwarded* byte to have been `\n` or `\r`. In practice this
  matches shell-user reflexes — escapes are nearly always typed at a fresh prompt — and it prevents
  random `~` bytes in transferred file content from being misinterpreted. But it does mean inline
  tooling that injects bytes mid-line cannot trigger the parser even if the escape sequence is
  present.
- **No multi-attach.** Each IJS session has exactly one attached client. Unlike `tmux(1)` /
  `screen(1)`, a second `qrsh -reconnect` while another client is still attached is rejected;
  reconnect is a take-over, not a join.
- **No scrollback preserved across reconnect.** The new client starts with a fresh terminal; output
  emitted between disconnect and the shepherd's SIGSTOP is lost.
- **Suspend reaches the job leader, not the cgroup.** The reconnect grace period's SIGSTOP targets
  the job's leader PID. Children that left the leader's process group (daemonised helpers, MPI side
  processes) continue running across the grace window and may keep emitting output for a defunct
  client. The `-suspend_remote yes` Ctrl+Z helper addresses the *user-driven* version of the same
  problem, but does not apply to the grace path.
- **X11 forwarding does not survive `qrsh -reconnect`.** The X11 proxy display the original client
  created dies with the original client process. The shell's `DISPLAY` env var still points at the
  dead `:N.0` proxy after reconnect, and the reconnect handshake neither allocates a new proxy on
  the new client's host nor updates `DISPLAY` in the shell's environment. To recover an X11-capable
  session, the user must restart with a fresh `qrsh -X` rather than reconnecting.

# SEE ALSO

sge_intro(1), qrsh(1), qlogin(1), qsh(1), qsub(1), qconf(1), sge_conf(5), sge_bootstrap(5)

# COPYRIGHT

See sge_intro(1) for a full statement of rights and permissions.
