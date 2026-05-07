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

[//]: # (Each file has to end with two empty lines)

