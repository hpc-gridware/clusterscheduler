# Major Enhancements

## v9.1.0prealpha1

### Advanced Binding Framework

OCS and GCS 9.1.0 introduce a completely redesigned **binding framework**
that manages CPU, cache, and memory-domain placement as a first-class
scheduling concept. Binding is no longer handled implicitly by the
execution daemon; it is now a **scheduler-driven resource model**,
enabling predictable, topology-aware job placement — including full
compatibility with **reservations**, **advance reservations**, and other
complex scheduling policies.

The new implementation treats **binding units** (threads, cores, sockets,
L2/L3 caches, dies, and NUMA domains) as **consumable resources** that
are detected, sorted, and assigned during scheduling. This design allows
precise control over *where* and *how* jobs run within each host.

- **New `-b...` option family** replaces the legacy `-binding` syntax:

    * `-bamount` — number of binding units per slot or host
    * `-bunit` — topology level (thread, core, socket, die, L2, L3, NUMA)
    * `-btype` — apply binding per slot or host
    * `-bfilter` — pre-filter eligible units
    * `-bsort` — define fill-up order of free vs. used resources
    * `-bstart` / `-bstop` — restrict binding to a defined topology range
    * `-binstance` — define the component that implements binding
      (`hwloc`, PE/MPI, or custom)

- **Full scheduler integration** — binding requests are resolved before
  job dispatch; jobs start only when the requested topology region is
  guaranteed.
- **Reservation of binding units** — prevents oversubscription and
  ensures exclusive access to requested hardware regions.
- **Topology awareness** — automatic detection of sockets, CPUs, and
  threads (OCS) plus cache hierarchy, die, and NUMA layout (GCS) enables
  intelligent placement decisions.
- **Hybrid-CPU support** — distinction between performance and efficiency
  units on heterogeneous architectures.
- **Enhanced control** — users can define detailed binding strategies to
  maximize performance for multi-core and hybrid workloads.

For comprehensive details on the new binding framework, including
configuration and usage examples, refer to:

- `qsub(1)` and `sge_binding(5)` manual pages
- The blog series on [hpc-gridware.com](https://hpc-gridware.com) covering the new binding framework
 
(Available in Open Cluster Scheduler and Gridware Cluster Scheduler)

### Departments, Users and Jobs — Department View *(Beta)*

*(This feature is still in beta and may change or be removed in future
releases.)*

The **Department View** (`-sdv`), first introduced in Gridware Cluster
Scheduler v9.0.2, has been further enhanced in this release.

Cluster administrators can now configure whether users access the
**Classic View**, where all hosts, queues, and jobs are visible to
everyone, or the **Department View**, where users only see cluster
objects associated with their assigned department(s).  
Even if administrators do not enforce Department View globally, users can
enable it individually to simplify the interface and reduce the number of
visible objects in large clusters.

Key Features:

- **User–Department Mapping**  
  Each user is assigned to one or more departments. Users without an
  explicit assignment are automatically placed in the *default
  department*.  
  Users belonging to multiple departments must specify the target
  department when submitting a job using `qsub -dept`, ensuring that each
  job is correctly charged and tracked.

- **Department-based Access Control**  
  Departments (or ACLs) can be assigned to cluster objects such as the
  global configuration, hosts, queues, projects, or resource quota sets.
  This restricts both access rights and visibility of those objects when
  Department View is enabled.  
  (See *user_lists* in `sge_conf(5)`, `sge_host_conf(5)`,
  `sge_queue_conf(5)`, `sge_pe(5)`, and `sge_resource_quota(5)`.)

- **Command-line Activation**  
  Users can activate Department View via command-line tools
  (`qhost`, `qstat`, `qselect`, …) using the `-sdv` switch.

- **Administrative Enforcement**  
  Managers can define default configurations (`sge_qstat`,
  `sge_select`, …) to enforce Department View for specific users,
  automatically hiding all cluster objects outside the assigned
  department(s).

- **Manager Visibility**  
  Cluster managers always retain full visibility of all objects,
  regardless of Department View settings.

(Available in Open Cluster Scheduler and Gridware Cluster Scheduler.)

### Prevent Denial of Service Attacks

The **Gridware Cluster Scheduler** now includes protection against
Denial of Service (DoS) attacks by limiting the number of requests
accepted by the `xxqs_name_sxx_qmaster(8)` daemon per second.  
This limit is controlled via the `gdi_request_limits` parameter, which is
configured in the global cluster configuration.

Parameter Syntax:

```
gdi_request_limits := limit_rule [ ',' limit_rule ]* | 'NONE' .
limit_rule := source ':' type ':' object ':' user ':' host '=' limit .
```

Each *limit_rule* defines a filtering pattern and a corresponding limit.
The fields `source`, `type`, `object`, `user`, and `host` act as filter
criteria. Wildcards (`*`) are supported, as well as **user lists** for
`user` and **host groups** for `host`.  
The `limit` specifies the maximum number of requests accepted per second.

Detailed syntax information is available in `xxqs_name_sxx_conf(5)`.

Example:

```
gdi_request_limits=*:add:job:john:*=500,
                   *:add:job:eng-users:@eng-hosts=100,
                   *:add:job:*:*=50,
                   qstat:get:*:*:*=60000
```

Explanation:
- **Rule 1:** Allows user `john` to submit up to **500 jobs per second**.
- **Rule 2:** Allows all users in the `eng-users` list to submit **100 jobs
  per second** on hosts in the `@eng-hosts` group.
- **Rule 3:** Allows all other users to submit **50 jobs per second**.
- **Rule 4:** Allows **60,000 `qstat` requests per second**.

If a user exceeds the configured limit, the corresponding command-line tool displays an error message identifying the violated rule.

Practical Consideration:

Be aware that a single command, such as `qstat`, can generate multiple
GDI requests depending on its options. For instance,  
`qstat -f` may query up to **15 different objects** (jobs, queues,
execution hosts, etc.) in one call.

Therefore, limits should be chosen to permit normal user activity.  
For example, a limit of **60,000 `get` requests** allows approximately:

- 5,000 `qstat -f` commands per second, or
- 60,000 `qstat -j` commands per second.
- 
(Available in Gridware Cluster Scheduler)

### TLS Encryption of Component Communication

xxQS_NAMExx now supports **TLS encryption** for all communication between
its components. TLS can be enabled during installation or later by
modifying the **bootstrap file** and restarting all xxQS_NAMExx services.

The required certificates are **generated and renewed automatically**.
Certificate lifetime and renewal intervals are configurable in the
bootstrap file.

This feature is currently marked as **experimental** and is **disabled by
default**.

For configuration and deployment details, refer to:

- *Installation Guide*, chapters **“Planning the Installation”** and **“Installation of the Master Service”**
- *Administration Guide*, chapter **“TLS Encryption of Communication”**

(Available in Gridware Cluster Scheduler)

### Munge Authentication

xxQS_NAMExx now supports **Munge authentication**.  
[Munge](https://dun.github.io/munge/) is a lightweight authentication
service that provides a secure and efficient mechanism for validating
users and services within a cluster environment.

Munge can be used to authenticate users across all xxQS_NAMExx components.
Authentication can be enabled during installation or later by modifying
the **bootstrap file** and restarting all xxQS_NAMExx services.

For configuration and setup details, refer to:

- *Installation Guide*, chapters **“Planning the Installation”** and **“Installing Munge”**

(Available in Open Cluster Scheduler and Gridware Cluster Scheduler.)

### Systemd Integration

xxQS_NAMExx is now fully integrated with **systemd**, the system and
service manager used by most modern Linux distributions.  
This integration allows xxQS_NAMExx to be managed as a native systemd
service, providing improved control over startup, shutdown, and daemon
lifecycle management.

In addition, xxQS_NAMExx jobs can optionally be executed under systemd
control. This enables fine-grained resource management, including **core
binding**, **device isolation**, and **cgroup-based accounting**.  
By default, jobs are run under systemd control (when available). This
behavior can be changed by setting the `USE_SYSTEMD` parameter in
`execd_params`.

Job resource usage statistics can also be collected via systemd.  
Because systemd usage reporting is less detailed than xxQS_NAMExx’s
built-in data collector, it is possible to revert to the native collector
or use a **hybrid collection mode** in which xxQS_NAMExx supplements data
that systemd does not provide.  
The mode of usage collection is configurable via the
`USAGE_COLLECTION` parameter in `execd_params`.

For configuration and operational details, refer to the  
*Installation Guide*, *Administration Guide*, and relevant man pages.

(Available in Open Cluster Scheduler and Gridware Cluster Scheduler.)

### qsub -sync r

Additionally to the existing `qsub -sync y` option, new options have been introduced that allow to wait for certain job states.

`qsub -sync r` will wait for the job to be in the running state. In case of array jobs the command will wait for the start of the first task of the job.

(Available in Open Cluster Scheduler and Gridware Cluster Scheduler)

[//]: # (Each file has to end with two empty lines)

