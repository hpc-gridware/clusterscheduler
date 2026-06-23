# Major Enhancements

## v9.1.3

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

## v9.1.2

### Updates to dbwriter

In this release, the required Java version and many other dependencies have been updated. The DBWriter now supports Java 11 or higher, ensuring compatibility with modern environments and reducing the risk of compatibility issues.

When you have an existing DBWriter installation, please ensure that at least Java 11 is installed on the DBWriter host.
Edit the file `$SGE_ROOT/$SGE_CELL/common/sgedbwriter` and modify the line setting `JAVA_HOME` to point to the Java 11 (or higher) installation directory.   
If you are doing a fresh installation, specify the Java 11 (or higher) installation directory when asked during the installation process.

### Port Range for qrsh and qlogin

When a user submits an interactive job via qrsh(1) or qlogin(1), the client binds a TCP listen socket and passes the port number to the shepherd, which connects back to it once the job starts. By default the operating system selects any free ephemeral port for this purpose. In environments with strict firewall rules between the submit host and the execution hosts — where only a specific port range is permitted through the firewall — this uncontrolled port selection prevents interactive jobs from working at all.

A new global configuration parameter porti\_range addresses this by restricting the TCP ports that the qrsh(1) and qlogin(1) client may bind to. Administrators configure the same port range in both the scheduler and the firewall, ensuring that the connect-back port is always within the allowed range.

The parameter accepts a comma-separated list of range expressions of the form n-m or n-m:s, where s is an optional step. For example:

```
port_range  50100-50200
port_range  50100-50200:2,51000-51100
```

The client tries each port in the configured ranges sequentially and binds to the first available one. If no port is free the client exits with an error. When the parameter is NONE or not configured, the operating system assigns a free ephemeral port automatically, preserving the previous behaviour.

porti\_range applies to both the builtin IJS mode and the legacy ssh/rsh mode and can be set or changed with qconf -mconf without restarting any daemon.

(Available in Open and Gridware Cluster Scheduler.)

## v9.1.0

### Advanced Binding Framework for Topology-Aware Job Placement

Open Cluster Scheduler and Gridware Cluster Scheduler 9.1.0 introduce a completely redesigned **binding framework** that elevates CPU, cache, and memory-domain placement to a **first-class scheduling concept**. Binding is now **scheduler-driven**, rather than being handled implicitly by execution daemons, enabling deterministic and topology-aware job placement.

The scheduler treats **binding units**—including threads, cores, sockets, dies, caches (L2/L3), and NUMA domains—as **consumable resources** that are detected, sorted, reserved, and assigned during scheduling. This enables precise control over how and where jobs/tasks execute within a host and ensures compatibility with advanced scheduling features such as **reservations** and **advance reservations**.

Key capabilities include:

- New `-b...` option family replacing legacy `-binding` syntax:
  - `-bamount`, `-bunit`, `-btype`
  - `-bfilter`, `-bsort`
  - `-bstart`, `-bstop`
  - `-binstance` (hwloc, PE/MPI, or custom binding implementation)
- Full scheduler integration — binding requests are resolved before dispatch
- Reservation of binding units to prevent oversubscription
- Extended topology awareness (cache hierarchy, dies, NUMA domains)
- Hybrid CPU support (performance vs efficiency cores)
- Highly configurable binding strategies for performance‑critical workloads.

*(Full enterprise functionality is available in Gridware Cluster Scheduler; Limited placement support is available in Open Cluster Scheduler)*

### Enhanced NVIDIA GPU Support with qgpu

With the release of patch 9.0.2, we made the first step towards better NVIDIA GPU support by introducing GPU resource requests and scheduling based on NVIDIA GPU properties. This has been further improved in this release.

The `qgpu` command has been added to simplify workload management for GPU resources. The `qgpu` command allows administrators to manage GPU resources more efficiently. It is available for Linux _amd64_ and Linux _arm64_. `qgpu` is a multi-purpose command which can act as a `load sensor` reporting the characteristics and metrics of NVIDIA GPU devices. For that it depends on NVIDIA DCGM to be installed on the GPU nodes. It also works as a `prolog` and `epilog` for jobs to setup NVIDIA runtime and environment variables. Further it sets up per job GPU accounting so that the GPU usage and power consumption is automatically reported in the accounting being visible in the standard `qacct -j` output. It supports all NVIDIA GPUs which are supported by NVIDIA's DCGM. For more information about `qgpu` please refer to the `Admin Guide`.

(Available in Gridware Cluster Scheduler only)

### Further Improved License Management (FlexNet license-manager integration)

This release refines **automated FlexNet license management** for Gridware Cluster Scheduler (GCS), enabling tighter integration between **FlexNet license servers** and **job scheduling decisions**.

**Key Features include :**

- **Automated License Discovery and Configuration**
  The new `license-manager` command-line tool automatically queries FlexNet license servers and synchronizes license availability with GCS. It discovers available licenses, creates corresponding GCS complexes with the `lm_` prefix, (configurable) and maintains accurate license counts without manual configuration.
- **License Monitoring**
  Operating as a GCS load sensor, the license-manager reports current license availability, allowing the scheduler to make informed job placement decisions based on actual license capacity. This prevents job failures due to license exhaustion and optimizes license utilization across the cluster.
- **External License Tracking**
  The tool provides integration for configuring licenses consumed by non-GCS processes, ensuring accurate accounting of total license availability and preventing over-subscription when external applications share the same license pool.

By eliminating manual license complex configuration and aligning scheduling decisions with real-time license capacity, this enhancement reduces configuration overhead and scheduling failures, improves cluster throughput, and maximizes return on investment for costly commercial software licenses in HPC environments. The discovered and tracked licenses are available in `qtelemetry` metrics for monitoring and observability.

(Available in Gridware Cluster Scheduler only)

### Prometheus and Grafana Metrics Exporter (qtelemetry now in beta)

> **Important:**
  **qtelemetry** left development state and is now available as beta. The interface, available metrics, and configuration options might still change in future releases. Feedback from early adopters is highly encouraged to guide further development.

This release brings **qtelemetry** into beta state, a new **metrics exporter** for Gridware Cluster Scheduler (GCS). qtelemetry enables administrators and developers to collect, export, and visualize cluster metrics for **monitoring, observability, and performance analysis**.

qtelemetry is designed for easy integration with modern monitoring stacks and provides immediate visibility into cluster health and workload behavior.

Key features include:

- Native integration with Prometheus and Grafana
  - Metrics are exposed in a format suitable for direct ingestion by Prometheus.
  - A pre-configured Grafana dashboard is available for rapid visualization.
- Comprehensive cluster metrics export, including:
  - Host metrics: CPU load, memory usage, GPU availability, and additional host-level statistics.
  - Job metrics: Queued, running, errored jobs, job waiting times, and other job lifecycle metrics. 
  - qmaster statistics: CPU and memory usage of sge_qmaster, as well as spooling filesystem information.
- Optional per-job metrics
  - Enables detailed, per-job observability.
  - Recommended only for very small workloads, as metric cardinality can grow significantly.
- Grafana dashboard support
  - Built-in compatibility with a pre-configured Grafana dashboard for Gridware Cluster Scheduler: https://grafana.com/grafana/dashboards/23208-gridware-cluster-scheduler-org/
  
(Available in Gridware Cluster Scheduler only)

### Protection Against Denial-of-Service (DoS) Attacks

This release introduces **request-rate limiting** to protect the qmaster daemon against Denial-of-Service (DoS) scenarios. Administrators can define fine-grained limits on the number of GDI requests accepted per second using the `gdi_request_limits` parameter.

Features include:

- Per-source, per-user, per-host, and per-object request filtering
- Support for wildcards, user lists, and host groups
- Clear error reporting when limits are exceeded

This mechanism ensures scheduler stability while allowing normal high-volume operations such as large-scale `qstat` usage.

*(Available in Gridware Cluster Scheduler only)*

### TLS Encryption of Component Communication (Experimental)

> **Important:** This feature is currently marked as **experimental** and is **disabled by default**. The functionality has passed internal testing but has not yet undergone extensive real-world validation. As soon as we got enough feedback from users, we will promote it to a stable feature with one of the next patch releases.

xxQS_NAMExx now supports **TLS encryption** for all internal component communication. This feature is **experimental** and **disabled by default**.

Highlights:

- Automatic certificate generation and renewal
- Configurable certificate lifetime and renewal intervals
- Enablement via installation workflow or bootstrap configuration

For configuration and deployment details, refer to:

- *Installation Guide*, chapters **“Planning the Installation”** and **“Installation of the Master Service”**
- *Administration Guide*, chapter **“TLS Encryption of Communication”**

(Available in Gridware Cluster Scheduler)

### Munge Authentication Support

Support for **Munge authentication** has been added, providing a lightweight and
secure authentication mechanism suitable for HPC environments. Switching to
munge authentication is highly recommended when using containers inside the
cluster or enabling user namespace.

Authentication can be enabled during installation or via bootstrap configuration changes.

*(Available in Open Cluster Scheduler and Gridware Cluster Scheduler)*

For configuration and setup details, refer to:

- [Munge](https://dun.github.io/munge/
- *Installation Guide*, chapters **“Planning the Installation”** and **“Installing Munge”**

(Available in Open Cluster Scheduler and Gridware Cluster Scheduler.)

### Systemd Integration

xxQS_NAMExx is now fully integrated with **systemd**, the system and service manager used by most modern Linux distributions.  This integration allows xxQS_NAMExx to be managed as a native systemd service, providing improved control over startup, shutdown, and daemon lifecycle management.

In addition, xxQS_NAMExx jobs can optionally be executed under systemd control. This enables fine-grained resource management, including **core binding**, **device isolation**, and **cgroup-based accounting**.  By default, jobs are run under systemd control (when available). This behavior can be changed by setting the `USE_SYSTEMD` parameter in `execd_params`.

Job resource usage statistics can also be collected via systemd.  Because systemd usage reporting is less detailed than xxQS_NAMExx’s built-in data collector, it is possible to revert to the native collector or use a **hybrid collection mode** in which xxQS_NAMExx supplements data that systemd does not provide.  The mode of usage collection is configurable via the `USAGE_COLLECTION` parameter in `execd_params`.

For configuration and operational details, refer to the *Installation Guide*, *Administration Guide*, and relevant man pages.

(Available in Open Cluster Scheduler and Gridware Cluster Scheduler.)

### Faulty Job Loadsensor

This release of Gridware Cluster Scheduler comes with a load sensor for faulty job handling.

All job spool files (such as the sge_shepherd trace file, error file, usage file, ...) of failed
jobs are copied to a configurable location. Optionally, the job spool files are then removed
from the execution host.

To enable the faulty job handling

* edit the load sensor and do your configuration settings
* activate the load sensor
* activate the `execd_params KEEP_ACTIVE=ERROR`

  ```bash
  $ vi $SGE_ROOT/util/resources/loadsensors/faulty_jobs.sh
  ...
  # Target directory
  FAULTY_JOBS_DIR="$SGE_ROOT/$SGE_CELL/faulty_jobs"
  # Permissions of the target directory
  FAULTY_JOBS_PERM="700"
  # Shall we delete failed jobs' active job directory? Set to "true" or "false".
  FAULTY_JOBS_DELETE="true"
  # Log file - default: /dev/null, set to a file name for debugging,
  # e.g, /tmp/faulty_jobs.log
  LOGFILE="/dev/null"
  ...
  ```

  ```bash
  $ qconf -mconf
  ...
  load_sensor $sge_root/util/resources/loadsensors/faulty_jobs.sh
  ...
  execd_params KEEP_ACTIVE=ERROR
  ...
  ```

(Available in Gridware Cluster Scheduler only)

### Decrease Resource Requests of Running Jobs

This release adds the means to decrease the amount of resources used by running jobs.

A typical example is freeing a license resource from a long-running job so that other jobs can consume it.

The new `qalter -when now` switch introduced with this release allows decreasing resource requests; in the example of the license, allows freeing of the license once it is no longer necessary.

See man page `qalter.1` for details.

(Available in Gridware Cluster Scheduler only)

### Wait for Certain Job States

Additionally to the existing `qsub -sync y` option, new options have been introduced that allow to wait for certain job states.

`qsub -sync r` will wait for the job to be in the running state. In case of array jobs the command will wait for the start of the first task of the job.

(Available in Open Cluster Scheduler and Gridware Cluster Scheduler)

### Departments, Users and Jobs — Department View *(Experimental)*

> **Important:** This feature is experimental and not covered by standard support. It will be removed in v9.2.0 and replaced by a fully supported PIM/RBAC-based multi-tenancy framework.

The **Department View** (`-sdv`), first introduced in Gridware Cluster Scheduler v9.0.2, has been further enhanced in this release.

Cluster administrators can now configure whether users access the **Classic View**, where all hosts, queues, and jobs are visible to everyone, or the **Department View**, where users only see cluster objects associated with their assigned department(s). Even if administrators do not enforce Department View globally, users can enable it individually to simplify the interface and reduce the number of visible objects in large clusters.

Key Features:

- **User–Department Mapping**  
  Each user is assigned to one or more departments. Users without an explicit assignment are automatically placed in the *default department*. Users belonging to multiple departments must specify the target department when submitting a job using `qsub -dept`, ensuring that each job is correctly charged and tracked.
- **Department-based Access Control**  
  Departments (or ACLs) can be assigned to cluster objects such as the global configuration, hosts, queues, projects, or resource quota sets. This restricts both access rights and visibility of those objects when Department View is enabled.  (See *user_lists* in `sge_conf(5)`, `sge_host_conf(5)`, `sge_queue_conf(5)`, `sge_pe(5)`, and `sge_resource_quota(5)`.)
- **Command-line Activation**  
  Users can activate Department View via command-line tools (`qhost`, `qstat`, `qselect`, …) using the `-sdv` switch.
- **Administrative Enforcement**  
  Managers can define default configurations (`sge_qstat`, `sge_select`, …) to enforce Department View for specific users, automatically hiding all cluster objects outside the assigned department(s).
- **Manager Visibility**  
  Cluster managers always retain full visibility of all objects, regardless of Department View settings.

(Available in Gridware Cluster Scheduler.)

[//]: # (Each file has to end with two empty lines)

