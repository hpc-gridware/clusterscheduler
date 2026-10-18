# Major Enhancements

## v9.1.7

### Further Resource Map Improvements

Work on resource maps continued after 9.1.6. Where that release made it possible to say that the instances granted for a request have to *agree* with one another, this one makes it possible to say *which* instances a job wants, and to pick them by what the administrator recorded about them rather than by name. The scheduler now also decides which instances a job is granted while it matches a host, not afterwards while it books them, so a reservation reflects the instances a job will actually receive.

For the complete grammar of a resource map, its characteristics and its requests, see the **xxqs_name_sxx_rsmap(5)** man page.

#### Choosing the Instances by Identifier

A request may name the instances it wants with the *id=* parameter. The value is an expression over the identifiers of the map: shell wildcards, combined with `!` for not, `&` for and and `|` for or, and grouped with brackets.

```bash
$ qsub -l 'gpu=1[id=gpu0]' job.sh
$ qsub -l 'gpu=1[id=gpu1*]' job.sh
$ qsub -l 'gpu=2[id=(gpu1*|gpu2*)&!*b]' job.sh
```

The first asks for exactly one instance, the second for any share of card `gpu1`, and the third for the `a` shares of two particular cards. Where the identifiers of a map say which device a share belongs to, this is how a job asks for a share of a particular device. More instances may match than the job asked for, and the scheduler chooses among those that do. A job whose expression matches nothing anywhere waits; an expression which does not parse is refused at submission time.

Quote the request — a shell would otherwise read the brackets as a file name pattern.

#### Matching Against the Characteristics of an Instance

A parameter whose name is not one of the reserved words is read as a complex, and the request is matched against the characteristic of that name on the instance. A job can therefore ask for a GPU by what it is rather than by what it is called:

```bash
$ qsub -l 'gpu=1[gpu_memory=40G]' job.sh
```

The comparison uses the relation operator of that complex, in the same direction as an ordinary resource request, and no operator is written in the request. A *gpu_memory* complex defined `<=` makes the request above mean "at least 40G"; a *sharing* complex defined `>=`, holding the number of jobs a card is shared by, makes `[sharing=2]` mean "a card shared by no more than two jobs". How the complex is defined is what decides how a request for it reads. An instance which does not carry the characteristic never matches, there being nothing to compare against.

The parameters of a request combine, so a job can constrain the device and the properties of its shares at once:

```bash
$ qsub -l 'gpu=4[same=id,gpu_memory=80G]' job.sh
```

Four shares of one card, and only of a card with at least 80G on board.

#### One Group of Instances Across All Hosts of a Job

The *scope=* parameter says how far a *same=* constraint reaches. It takes `host`, which is what a request leaving it out gets and what *same=* has meant so far, or `job`:

```bash
$ qsub -pe mpi 2 -l 'gpu=1[same=id,scope=job]' job.sh
```

Every instance the job is granted, on every host it runs on, then belongs to one group. The group is chosen once for the whole job over the hosts it could run on, and every host is held to it afterwards. Because identifiers belong to the host they are configured on, this is useful where a map is named consistently across the cluster — the job asks for the group of that name everywhere, and a host which does not have it cannot run the job.

(Available in Open and Gridware Cluster Scheduler; characteristics and request parameters in Gridware Cluster Scheduler only.)

### AMD GPU Support with qgpu

qgpu 1.2.0 supports AMD Instinct GPUs: the load sensor, the installer, device selection in the prolog
and epilog, per-job GPU accounting and a quick health check. The vendor is detected per host by the
load sensor and per grant by the prolog, so one queue configuration and one set of wrapper scripts
serve NVIDIA and AMD hosts in the same cluster.


An AMD host reports `amd_gpu_count`, `amd_gpu_backend` and a set of `amd_gpu_<N>_*` load values,
its devices are managed in an `AMD_GPUS` resource map, and a job is given `ROCR_VISIBLE_DEVICES`
for the devices it was granted, plus `AMD_VISIBLE_DEVICES` for container runtimes where the GPUs
can be identified by UUID. qgpu reads the interfaces of the amdgpu kernel driver directly (KFD
topology, sysfs and hwmon), so it needs no ROCm installation and no vendor library. Per-job
accounting reads the driver's hardware counters at job start and end and appears as `amd_*` values
in `qacct -j`, on MI300 and MI350 class GPUs. The health check runs quick checks of the kernel
interfaces in the prolog. Where a GPU is partitioned, each partition is a GPU in its own right, with
its own index, render node and resource map identifier; accounting does not cover partitioned GPUs.
For details see the Admin Guide.

*(Available in Gridware Cluster Scheduler only)*

### TLS Encryption Available in Open Cluster Scheduler

TLS encryption of the communication between the components, introduced in 9.1.0, is no longer limited to Gridware Cluster Scheduler. `inst_sge -tls` and the `security_mode tls` setting in the bootstrap file are now part of Open Cluster Scheduler as well, so an open source cluster can be installed with encrypted component communication, certificate generation and automatic renewal.

Encryption remains opt-in and requires the cluster to be set up with `security_mode` set to `tls`, which means restarting all components. OpenSSL 3 must be installed on every host of the cluster; it is not shipped with xxQS_NAMExx.

For configuration and deployment details see the *Installation Guide*, chapter **"Enabling TLS Encryption"**, and the *Administration Guide*, chapter **"TLS Encryption of Communication"**.

(Available in Open and Gridware Cluster Scheduler.)

### slurm-shim Delivered with the Product

The slurm-shim lets applications and users that expect Slurm command line tools work against an xxQS_NAMExx cluster. It is now delivered as a package alongside the other product packages, as `ocs-<version>-slurm-shim.tar.gz` and `gcs-<version>-slurm-shim.tar.gz`, instead of having to be fetched separately.

The package is self-contained: the binaries for the supported architectures, their checksums, a README with the installation instructions, and its own third-party license information including the SBOM. It unpacks into `$SGE_ROOT/slurm-shim`.

(Available in Open and Gridware Cluster Scheduler.)

### Execution Hosts Joining the Cluster on Their Own

An execution daemon on a host which is not configured in the cluster is normally refused: a host has to be an administrative host and an execution host before its daemon may register. That ordering is awkward where hosts come and go on their own — nodes started from a machine image, cloud instances, a pool that grows and shrinks — because something has to configure the cluster before each new host can report for duty.

Two new *qmaster_params* entries let the cluster accept such hosts by rule. *AUTO_ADD_HOSTS* takes a list of rules naming the hosts that may add themselves, and optionally the host group each is added to:

```
qmaster_params   AUTO_ADD_HOSTS=10.42.0.0/16:@cloud;gpu-*.example.com:@gpu
```

A matcher containing a slash is an IPv4 network in CIDR notation; every other matcher is an fnmatch(3) pattern matched against the name of the host and against its address. The first matching rule decides. Where a rule names a host group, the host is added to it and the group is created if it does not exist, so queue instances for the new host appear wherever a cluster queue uses that group — a prepared cluster queue is then all the configuration a new host needs. The slots of the new host are set to the number of processors it reports, as the execution host installation would do, and a value an administrator sets afterwards is not overwritten.

Only the execution daemon of the host itself may add the host, and the host is added as an execution host and nothing else: it does not become an administrative or a submit host, and every other request from it is refused until an administrator grants more.

*AUTO_REMOVE_HOSTS_AFTER* takes care of the other direction. A host which added itself this way and has then been unknown to the master daemon for that long is taken out of its host groups and deleted from the execution host list, which removes its queue instances with it:

```
qmaster_params   AUTO_ADD_HOSTS=10.42.0.0/16:@cloud,AUTO_REMOVE_HOSTS_AFTER=0:30:00
```

This is what keeps a reclaimed cloud instance from appearing in `qhost` and `qstat` forever. A host an administrator added is never removed, however long it has been unreachable, and a host which still has jobs running is kept until they are gone. Unset or `0`, the default, switches the removal off.

See the **xxqs_name_sxx_conf(5)** man page for both parameters.

(Available in Open and Gridware Cluster Scheduler.)

## v9.1.6

### Resource Map Requests, Reservations and Shared Devices

9.1.5 gave the instances of a resource map **characteristics** — typed metadata describing what sets one instance apart from its siblings. 9.1.6 builds on that with a large number of improvements and fixes around resource maps, of which the most visible is that a job can now state a requirement about the *relationship* between the instances it is granted, rather than only how many it wants.

The motivating case is a shared device. An identifier may be listed more than once to model N-way sharing of one physical resource, so a host with two GPUs usable by four jobs each is configured as

```bash
$ qconf -mattr exechost complex_values 'gpu=8(0 0 0 0 1 1 1 1)' node01
```

The scheduler sees eight instances, so eight single-instance jobs can run, but only two identifiers are ever handed out. A multithreaded job that needs a whole card has to be granted four shares of *one* identifier — four shares spread over two cards are of no use to it.

For the complete grammar see the new **xxqs_name_sxx_rsmap(5)** man page.

#### Parameters in a Resource Map Request

A request may carry a list of parameters in brackets after the amount:

```bash
$ qsub -l 'gpu=4[same=id]' job.sh
```

The parameters are separated by commas and combine, so an instance has to satisfy all of them. Quote the request — a shell would otherwise read the brackets as a file name pattern. The names `id`, `same`, `scope`, `distinct` and `bind` are reserved for the scheduler; any other name is read as a complex.

The JSV libraries handle requests carrying such a parameter list, so a job verification script sees and may modify them. A request with a parameter list is a Gridware Cluster Scheduler feature; an Open Cluster Scheduler refuses one when the job is submitted rather than accepting it and granting whichever instances happen to be free.

#### All Instances from One Device

The *same=id* parameter requires every instance granted for the request to be a share of one identifier:

```bash
$ qsub -l 'gpu=4[same=id]' job.sh
```

On the host above, this is how a job asks for a whole card instead of shares of two. What the constraint costs depends on the *consumable* setting of the complex: for *consumable YES* the amount is taken per slot, so a group of four instances serves four slots of `-l 'gpu=1[same=id]'` whatever the queue would otherwise allow; for *JOB* and *HOST* the amount is taken once, so a group either serves the request or the host cannot run the job.

Such a request takes part in resource reservation like any other. A job submitted with `-R y` is given a reserved start time at which one group is actually free, not merely one at which the amount is free across several groups, so a job waiting for a whole card is not passed over indefinitely by jobs asking for single shares.

#### Agreement on a Characteristic

*same=* also takes the name of a characteristic instead of `id`, and the instances granted then have to carry the same value for it:

```bash
$ qsub -l 'gpu=2[same=numa_node]' job.sh
```

Two instances whose `numa_node` characteristic agrees. Several identifiers may carry the same value, so unlike *same=id* the instances may come from different devices as long as they agree — which is what a job wants when it cares about the placement of its devices rather than about getting one device whole. An instance which does not carry the named characteristic cannot take part, there being nothing for it to agree with.

#### Resource Maps in Advance Reservations

An advance reservation now holds particular instances rather than only a count, and xxqs_name_sxx_qrstat(1) reports which ones it holds. A job running inside a reservation is granted from that set, and a job running outside one is not granted an instance a reservation holds for the time it runs. A job whose request cannot be met from the reserved instances is refused when it is submitted.

#### Implicit Instance Identifiers

The identifier list of a resource map may be left out, and the instances are then named `0` to *amount*-1:

```bash
$ qconf -mattr exechost complex_values 'gpu=4' node01
```

This is equivalent to `gpu=4(0-3)` and is stored and displayed in that longer form. How many identifiers may be created this way is limited by *MAX_RSMAP_IDS* in *qmaster_params*, 512 by default; a larger amount has to be written as an explicit list. Setting *MAX_RSMAP_IDS* to `0` rejects the short form altogether.

#### A Manual Page for Resource Maps

Resource maps were described across the complex and host configuration man pages. The new **xxqs_name_sxx_rsmap(5)** man page brings the whole subject together: defining the complex, configuring the instances on a host, characteristics, every form of request, what a job sees in its environment, device isolation, and the limitations.

(Available in Open and Gridware Cluster Scheduler; characteristics and request parameters in Gridware Cluster Scheduler only.)

## v9.1.5

### RSMAP Characteristics and Device Isolation

A resource map (RSMAP) names the individual instances of a host resource — the GPUs in a node, its network interfaces, a pool of license seats — so that the scheduler can both count them and record which one each job received. Until now an instance was nothing more than a name. Anything an administrator knew about the hardware behind that name had to be encoded into the name itself and decoded again by a prolog script.

Instances can now carry **characteristics**: typed metadata attached to one specific instance. A characteristic describes what sets that instance apart from its siblings — the device file of a GPU, the memory on the card, its affinity to a CPU socket, the bandwidth of an interface. Characteristics are written in square brackets after the instance name and separated by commas:

```
gpu=2(gpu0[device=/dev/nvidia0,gpu_memory=80G] \
gpu1[device=/dev/nvidia1,gpu_memory=40G])
```

Every characteristic name must be defined as a complex before it can be used, and the type of that complex determines how the value is parsed: `gpu_memory=80G` is read as a *MEMORY* value, a bandwidth as *INT*, a device path as *RESTRING*. The characteristics of an instance follow that instance through the scheduler, so a job is told which GPU it was granted along with everything the administrator recorded about it.

The first consumer of this mechanism is **device isolation**. A characteristic named `devices` lists the device files a job may use, together with the access mode for each. When the job starts, `sge_execd` passes that list to `sge_shepherd`, which hands it to systemd as the `DeviceAllow` property of the job's scope and sets `DevicePolicy` to `closed`. The job then sees only the devices belonging to the GPU it was actually granted; the remaining GPUs on the host are invisible to it.

Configuring a two-GPU node:

```bash
$ qconf -mattr exechost complex_values \
  'gpu=2(gpu0[devices=/dev/nvidia0:rw;/dev/nvidiactl:r] \
         gpu1[devices=/dev/nvidia1:rw;/dev/nvidiactl:r])' \
  node01
```

Within the `devices` value, a semicolon separates one device from the next and a colon separates a device from its access mode (`r`, `w` or `rw`). A device given without a mode defaults to read-only.

Two jobs each requesting `-l gpu=1` are then granted different instances, and each is confined to the devices of the GPU it received:

```bash
$ qsub -l gpu=1 $SGE_ROOT/examples/jobs/sleeper.sh
Your job 21 ("Sleeper") has been submitted

$ systemctl show ocs8012.21.scope | grep ^Device
DevicePolicy=closed
DeviceAllow=/dev/nvidia0 rw
DeviceAllow=/dev/nvidiactl r
```

Long definitions may be split across lines with a trailing backslash; whitespace inside a characteristics block is ignored.

For the complete grammar, including how an instance may be listed more than once to model a shared device, see the **xxqs_name_sxx_complex(5)** and **xxqs_name_sxx_host_conf(5)** man pages. Device isolation is described in the *Administration Guide* chapter **"Systemd Integration"**.

(Available in Gridware Cluster Scheduler only.)

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

