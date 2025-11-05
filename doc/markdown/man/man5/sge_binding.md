---
title: sge_binding
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME
sge_binding - xxQS_NAMExx Binding Functionality

# DESCRIPTION

Starting with version 9.1 of the xxQS_NAMExx (GCS) and Open Cluster Scheduler (OCS is the open-source edition of GCS), binding decision logic has been moved from the execution nodes into the scheduler component itself. In earlier systems such as Grid Engine, Son of Grid Engine, Univa Grid Engine, and other derivatives, binding requests were treated more as hints. In xxQS_NAMExx, however, bindings are considered top-level resources which need to be fulfilled before a job is allowed to start.

This design treats modern CPU components — such as sockets, cores, threads, and memory units (e.g., NUMA nodes, caches) — as consumable resources managed directly by the scheduler. This approach provides several advantages:

- Jobs are dispatched to hosts only when the requested binding can be guaranteed, otherwise those jobs remain pending.
- Binding units will only be used by one job which avoids over subscription.
- Reservations and Advance Reservations honor binding requirements.
- The scheduler is aware of hardware components such as threads, cores, and sockets, and can distinguish between different core types within a socket (e.g., heterogeneous Intel architectures).
- Memory units, such as L2 and L3 caches or NUMA nodes, are visible to the scheduler.
- The system models the _distance_ between units, allowing tasks or threads to be placed closer together to benefit from shared caches or nearby memory nodes.
- Beyond the typical core binding found in other workload managers, bindings can also target hardware threads, dies, and sockets.
- A binding strategy can be specified which allows the scheduler to decide which units should be bound if multiple valid placements exist.

Note that binding requests are optional, but managers of a cluster can enforce an implicit binding for all jobs if a user does not specify one.

## Topology of Compute Nodes

Jobs with binding are usually scheduled to such compute nodes only where the underlying hardware topology can be detected. On Linux-based hosts and many other platforms, HWLOC is used to detect that information that is then shown as a load value named *m_topology* for each compute node.

***Example: Show topology of hosts***

```bash
qhost -F m_topology
...
     hl:m_topology=NSXYCTTYCTTYCTTYCTTYCTTYCTTYCTTYCTTYEEYEEYEEYEE
```

The output shows a topology string for a heterogeneous architecture where each letter represents the start of a certain unit within that compute node.

*S* represents a socket, *C* and *E* are two different CPU types. *C* is a power core whereas *E* is an efficiency core. The letter *T* shows hardware threads within a core. Additionally, memory units are shown. *N* represents the start of a NUMA node. *X* represents L3 caches and *Y* shows L2 caches.

Single threads (single *T* letters below a CPU) are omitted for readability.

In other words, the shown CPU topology shows a single socket (one NUMA node) with eight dual threads power cores and eight single threads efficiency cores. Each power core has access to one L2 cache, whereas two efficiency threads have to share one L2 cache. All cores within that socket share the same L3 cache.

In difference to xxQS_NAMExx the Open Cluster Scheduler does not expose memory-based information. This means that within OCS the same computer would be shown with the following topology.

***Example: Topology strings without memory node information***

```bash
qhost -F m_topology
...
     hl:m_topology=SCTTCTTCTTCTTCTTCTTCTTCTTEEEEEEEE
```

Topology strings are usually written in **uppercase**, except for components that are _not available_ for Cluster Scheduler jobs through administrative restrictions (like disabled units).

***Example: Request hosts having a specific Topology***

Topology of a compute node can be requested explicitly for a job or advance reservation.

```
qsub -l m_topology=SCTTCTTCTTCTTCTTCTTCTTCTTEEEEEEEE` ...
```

Topology strings appear in various outputs (e.g. `qstat -j`, `qrstat -ar`, `qstat -F`) to illustrate granted bindings for jobs and advance reservations. Components where a job or Advance Reservation has **exclusive access** are written in **lowercase**.

BIOS/Firmware settings can have an influence on the reported hardware topology of a compute node. Especially NUMA support and hyper threading support can be configured, which means that topology strings can change with machine reboots.

The used libraries (e.g., HWLOC) to extract topology information usually do not provide information about chiplets/dies (AMD EPYC), but that structures of a socket often correspond to the structure of caches.

## Slots and Binding

Open Cluster Scheduler and xxQS_NAMExx are built around the **slot concept**.

Think of a compute node as an airplane. The plane has a fixed number of seats, just as a node has a fixed number of slots. Each sequential job and each task of a parallel job requires one slot, just as each passenger requires a seat on the plane.

**Binding** goes a step further. While slots represent overall capacity, binding determines _where_ within the node a job runs. In the airplane analogy, this is like assigning not only a seat, but also balancing the weight distribution. Freight must be placed in specific positions to keep the plane stable; likewise, compute tasks can be pinned to particular cores, sockets, or NUMA nodes to optimize cache usage and memory locality.

In Cluster Scheduler, **slots and binding spots are loosely coupled**.

- A slot can be as small as a hardware thread or as large as an entire compute node.
- At job submission, the user specifies both:
    - the **number of slots** required (space), and
    - the **binding granularity** (threads, cores, sockets, …).

This separation allows flexible resource allocation while ensuring that jobs are placed efficiently and predictably on the available hardware.

***Examples: Requesting Binding for a job***

A parallel job requesting 64 slots, with each task bound to one compute core (power core). Note that `-bunit` may be omitted, as binding to cores is the defaults

```bash
qsub -pe mpi 64 [-bunit C] -bamount 1 ...
```


If each task is multi-threaded (e.g. 4 threads per task), it can be advantageous to request multiple threads per slot:

```bash
qsub -pe mpi 64 -bunit T -bamount 4 ...
```

## Binding Amount

The **binding amount**, specified with the `-bamount` option, defines the number of binding units required **per slot** (default) or **per assigned host** to efficiently execute a job or task on one or more machines.

- A value of `0` disables binding for a job.
- The maximum meaningful binding amount is equal to the number of available binding units on the largest compute node in the cluster. Any request exceeding this limit will prevent the job from ever being scheduled.

***Example: Binding to Cores***

Assume a parallel environment with an allocation rule of 8 (`mpi_8`):

```bash 
qsub -pe mpi_8 16 [ -bstrategy packed -btype slot -bunit C ] -bamount 2
```

This submission requests **16 slots** in total. With an allocation rule of 8, the job will be distributed across **two hosts** (16 slots ÷ 8 slots/host = 2 hosts).

On each host, **8 tasks** will run, with each task consuming one slot. With the default binding type (`slot`) and binding unit (`C`), and with a binding amount of `2`, each task is bound to **two power cores**.

Because the default binding strategy is **packed**, the scheduler ensures that the two cores for each task are placed as close together as possible. This minimizes the _distance_ between cores — for example, by favoring cores that share an L2 or L3 cache or that reside within the same socket — thereby improving memory locality and overall performance.

In this example, each task is assigned to two cores. This setup only makes sense if a task starts either two UNIX processes or a single multithreaded process with (at least) two mostly independent threads.

If the processes or threads are **highly dependent on each other**, binding to **threads** instead of cores may be more efficient. While this might cause the individual job to run longer, it can free up more resources for other jobs — improving overall cluster throughput by an estimated **5–20%** compared to core binding.

***Example: Binding to Threads***

```bash
qsub -pe mpi_8 16 [ -bstrategy packed -btype slot ] -bunit T -bamount 2`
```

Here, each task is bound to **two hardware threads** instead of two cores. This configuration can be preferable for tightly coupled processes, allowing the scheduler to make more efficient use of the underlying hardware.

## Binding Type

The **binding type**, specified with the `-btype` option, determines whether the requested binding units are allocated **per slot** (default) or **per execution host**.

- **Slot-based binding** (default): Binding units are allocated for each slot individually. This gives the scheduler more flexibility, allowing it to utilize available binding units even on nodes that are already partially occupied.

- **Host-based binding**: Binding units are allocated for the entire host at once. This is stricter, as it ensures the specified binding strategy is applied to all slots combined on a given compute node.

***Example 6: Host-based binding***

In contrast to the previous slot-based example, here host-based binding is applied. The job will be assigned a total of **16 hardware threads per host** (which is similar to the previous example).

```bash
qsub -pe mpi_8 16 [ -bstrategy packed ] -btype host -bunit T -bamount 16`
```

With **host-based binding**, the **packed** binding strategy is applied in a single sweep across all slots on the host, rather than individually per slot. This approach is often more scheduling-efficient, as it reduces fragmentation and ensures a consistent placement of resources. Efficiency is further improved when host-based binding is combined with explicit **start** and **stop** positions (described in the sections below), which allow precise control over where bindings begin and end within the host topology.

One disadvantage of host binding can be that in clusters with core fragmentation and mixed workload bigger PE jobs might starve without reservation.

### Binding Unit

The **binding unit** defines the type of hardware resource that should be allocated either per slot (default) or per host. Previous examples made use of core and thread binding. Additionally, the following unit types are supported:

- **T** or **CT** – CPU thread of a power core
- **ET** – CPU thread of an efficiency core
- **C** – Power core (default)
- **E** – Efficiency core
- **S** or **CS** – All power cores of a socket
- **ES** – All efficiency cores of a socket

If available on a compute node, the following units can also be requested with Gridware Cluster Scheduler.

- **X** or **CX** – All power units sharing the same L3 cache
- **EX** – All efficiency units sharing the same L3 cache
- **Y** or **CY** – All power units sharing the same L2 cache
- **EY** – All efficiency units sharing the same L2 cache
- **N** or **CN** – All power units of a NUMA node
- **EN** – All efficiency units of a NUMA node

The `-bunit` switch of job and advance reservation submission commands allows to specify a binding unit.

***Example 7: Chiplet/Die binding based on the outline of the cache structure***

```bash
qsub ... -btype host -bunit X -bunit 1
```

The job will bind the scheduled job/tasks to all cores that share the same L3 cache. This means that the scheduler will not only ensure that the job/tasks use those cores sharing the same NUMA node and cache but also that attached resources (GPU/IO/...) of that unit  can be used efficiently. Additionally this ensures that no other job can utilize cores of the Chiplet/Die which could have a negative impact for memory access and cache hits.

Multiple different binding units cannot be requested for a single job or AR at the current point in time.

Requests for power units or efficiency units are treated as **hard requirements**. For example, jobs requesting power units will never be bound to efficiency units, and vice versa and binding requests need to get fulfilled before a job is started.

If cache or NUMA topology information (X, Y, N) is not available for a node, jobs can still be scheduled there. In such cases:

- **N** and **X** requests are mapped to **S** (socket), and
- **Y** requests are mapped to **C** (power core) or **E** (efficiency core)

L3 cache and NUMA usually align with a socket, and L2 caches align with individual cores.

The binding that is finally applied to a job is visible in `qstat` and `qrstat` output for an object.

***Example 8: Display the binding of a job***

```bash
qstat -j <jid> or qrstat -ar <ar_id>
...
binding:               bamount=1,binstance=set,bstrategy=pack,btype=host,bunit=X
exec_binding_list 1:   host1=NSxycttycttyXYCTTYCTT...
```

The *binding* line repeats the requested binding, whereas the *exec_binding_list* shows the assigned binding per host (or the combined slot binding per host). In this example these are all cores/threads including all L2 caches below the first L3 cache of the first Socket

## Binding Filters

Binding filters allow masking of binding units so that they are excluded from binding decisions. A filter can be specified per job using the `-bfilter` option, or enforced globally by managers via a Job Submission Verifier (JSV).

Filters are expressed as **topology strings**, where masked units are written in **lowercase**. This makes it possible to exclude certain cores, threads, or sockets from being considered for binding.

**Important:** In heterogeneous clusters, the specified filter must match the topology of the potential target host. A mismatch will prevent the job from being scheduled. Binding filters are applied before optional sorting with `-bsort` which is explained further down below.

Think of binding filters like an airplane where some seats are reserved for crew or safety purposes. Even if the plane has available seats, those filtered ones are simply **not available for passengers**. Similarly, binding filters ensure certain compute resources are kept out of scheduling decisions.

***Example 9: Apply a Job Binding Filter***

The following job is submitted to a dual-socket, quad-core machine. The binding filter masks the first core of each socket, ensuring those cores are never used for binding:

```bash 
qsub -l m_topology=SCCCCSCCCC -bfilter ScCCCScCCC ...
```

Alternatively, administrators can configure a host-independent filter via the `binding_params` setting. For example, setting the `filter` parameter to `first_core` automatically masks the first core of the first socket across all nodes in the cluster. This filter applies to **all jobs**, regardless of whether they also specify a job-level filter:

```bash
qconf -sconf | grep binding_params 
... 
binding_params ... filter=first_core
```

Filters are **additive**: both, globally disabled units and job-specific disabled units will be excluded from scheduling for a job.

**Note:** Future versions of xxQS_NAMExx will support _binding_addresses_ in addition to topology strings. Binding addresses will allow filter expressions that are independently of a particular `m_topology` layout.

## Binding Strategy

**Packed Binding** is the primary (default) binding strategy in Open Cluster Scheduler (OCS) and xxQS_NAMExx (GCS). It provides more flexibility than the static binding strategies found in earlier versions of Grid Engine. 

If the product is able to select additional strategies in the future, then the binding strategy can be selected with the `-bstrategy` option.

In **OCS**, packed binding assigns **available** units sequentially from **left to right** in the node’s topology string. The scheduler starts at the first available unit and continues until either the required number of units has been assigned or the end of the topology string is reached. If all requested units are available, the binding is applied to the job. If not, the binding cannot be applied on that host.

A unit is considered available when all child units are not in use by other jobs. This means for core binding (C) all threads of selected cores must be free or for Chiplet/Die binding (X) all cores on that Chiplet/Die as well as all threads within each of those cores must be unused.

***Example 10: Packed Core Binding in OCS***

The numbers within the topology string below show the binding sequences (from left to right) on a host with two cores already in use (`NSXCCccSXCCCC`).

| Binding Sequence | Max cores used |
|------------------|----------------|
| NSX12ccSX3456    | 8              |

Here, packed binding assigns all available power cores in sequence, filling the node efficiently.

In xxQS_NAMExx, packed binding can be combined with:

- **Sorting (`-bsort`)** – reorders the topology string, e.g., by socket or core type.
- **Start (`-bstart`) and stop (`-bstop`) positions** – restricts the binding to a specific portion of the topology string.

This allows the scheduler to either **avoid or favor fragmented areas** of the node topology:

- Avoiding fragmented areas improves **cache locality** and **NUMA memory usage**.
- Favoring fragmented areas can help **schedule jobs that would otherwise be unschedulable**.

***Example 11: Packed Core Binding in xxQS_NAMExx with Sorting and Start and Stop Positions***

By combining **sorting** with **start and stop positions**, administrators can finely control resource allocation, improving either **job performance** (through cache and NUMA locality) or **scheduling success** (by finding available fragmented resources).

| Additional constraints      | Binding Sequence | Max cores used                                                |
|-----------------------------|------------------|---------------------------------------------------------------|
|                             | NSX12ccSX3456    | 8 – Fill from left to right                                   |
|                             |                  |                                                               |
| -bsort S                    | NSX56ccSX1234    | 8 – free socket utilized first                                |
|                             |                  |                                                               |
| -bsort S -bstart S -bstop s | NSXCCccSX1234    | 6 – avoids placing different jobs on the same socket          |
|                             |                  |                                                               |
| -bsort S -bstart s -bstop S | NSX12ccSXCCCC    | 4 – leaves completely unused sockets available for other jobs |

## Binding Sort

Available in Gridware Cluster Scheduler.

The **`-bsort`** option controls the order in which units are considered for binding within a host topology. By default, the **packed binding strategy** assigns units sequentially from **left to right** in the topology string. Sorting allows users to **prioritize specific hardware or memory units** when the scheduler attempts to dispatch a job.

The `-bsort` argument accepts a list of letters representing hardware and memory units. Letters corresponding to unit types **not present** on a host are ignored.

- **Capital letters** (`S`, `C`, `E`, `N`, `X`, `Y`) cause the **least-loaded units** to appear first in the topology string.
- **Lowercase letters** (`s`, `c`, `e`, `n`, `x`, `y`) bring **already-utilized units** towards the beginning, allowing jobs to fill partially used resources.

The degree of utilization is calculated based on the thread utilization within that corresponding sub-tree.

Sorting is applied **once per job**, when the scheduler examines a host for the first time. For parallel environment (PE) jobs, no re-sorting occurs after one or more tasks have been placed on a host.

Sorting is performed **per specified node**, which means child nodes are sorted within the topology tree but for node types where a specification of the sort order is absent, will not be sorted.

Sorting NUMA nodes is not strictly based on load. After the first node, the scheduler may prefer nodes with better latency, following typical NUMA “latency quadrant” patterns (e.g., N0 → N1/N2 → N3). This means that even if a node is less loaded, its position in the sort order may be influenced by memory access latency rather than utilization.

***Example 11: After the first NUMA node latency order will be preferred***

| -bsort | Sort Order                          | Quadrant order    |
|--------|-------------------------------------|-------------------|
| -      | NSxccXCC NSXCCXCC NSXcCXCC NSXCCXCC | N1 => N2/N3 => N4 |
|        |                                     |                   |
| NXC    | NSXCCXCC NSXCCXcc NSXCCXCC NSXCCxCc | N2 => N1/N4 => N3 |
|        |                                     |                   |
| Nxc    | NSXCCXCC NSXccXCC NSXCCXCC NSXcCxCC | N2 => N1/N4 => N3 |

## Binding Start and Stop

Available in Gridware Cluster Scheduler.

The `-bstart` and `-bstop` allow to define a start and stop position within a topology string where binding should be done. The specification of start and stop is optional, in case of absence the topology strings first and last unit are implicit start and stop positions. If sorting of topology strings is enabled with `-bsort` then the start and stop position is searched within the sorted topology.

In principle any unit type can be specified as start and/or stop where capital letters (S, C, E, N, Y, X) denote completely unused units and the corresponding lowercase letters (s, c, e, n, y, x) represent partially or fully utilized nodes.

Defining a start position can make sense for units where the granularity is bigger than the requested unit (`-bunit`) to avoid or favor utilized areas of a compute node.

***Example: Avoid binding within free/utilized sockets***

The bold areas within the topology string show Sockets that are completely free. The square brackets show the range within binding will happen.

| Description              | Topology                      |
|--------------------------|-------------------------------|
| unsorted                 | NSCCcc**SCCCCSCCCC**SCcCC     |
|                          |                               |
| after sort with -bsort S | **NSCCCCSCCCC**SCcCCSCCss     |
|                          |                               |
| -bstart S -bstop s       | **N\[SCCCCSCCCC\]**SCcCCSCCss |
|                          |                               |
| -bstart s -bstop S       | **NSCCCCSCCCC**\[SCcCCSCCss\] |

Note: If a specified start position for binding cannot be found, then no valid binding can be found. If no end position after start can be found then the end of the topology string is the implicit end for binding.

Sorting and start and stop positions help especially to group tasks of a parallel job so that they can share L2/L3 caches, Sockets, or NUMA nodes. In mixed clusters with big parallel and sequential jobs, it can be favorable to direct the sequential jobs to already utilized areas within a node topology.

## Binding Instance

The `-binstance` switch defines the instance that realizes the core binding on a machine. Jobs can distinguish the selected binding method by evaluating the environment variable SGE_BINDING_INSTANCE that is exported into the job environment showing the name of the selected instance.

The following binding instances are possible:

- *set* - The binding is applied by Cluster Scheduler. How this is achieved depends on the underlying operating system of the execution host where the submitted job will be started.
- *env* - Cluster Scheduler will not do the core binding, but the job can do the binding using the information provided by the environment variable SGE_BINDING_CPUSET or SGE_BIDNING_TOPOLOGY.
- *pe* - The binding instance pe causes the binding information to be written into the fourth column of the `pe_hostfile`. Here logical socket and core IDs will be printed. Numbering starts at 0 and numbers have no holes. Socket and core IDs are separated by the comma character (:), and those number pairs are separated by colon character (,). ‘0:0,0:1’ means core 0 and 1 on socket 0. Depending in the MPI implementation this addition column will be evaluated to do the core binding. Some MPI implementations require additional switches or environment variables to enable that functionality or socket/core pairs need to be converted into a different format and passed either via switches to the MPI runtime or via *rankfile* file mechanism. Consult your MPI documentation for details.

Independent of the binding instance and if a job is requesting binding, the following environment variables will be available within the job environment:

- SGE_BINDING_INSTANCE - shows the selected binding instance or NONE if no binding is applied.
- SGE_BINDING_TOPOLOGY - shows the topology string of the host where a job or task is running. Bound units are shown in lowercase letters. Without binding NONE is shown.
- SGE_BINDING_CPUSET - shows the cpuset string (as printed by HWLOC) or 0x0 if no binding affinity is applied.

## Configuration Parameters Influencing Binding

Within the global configuration the *binding_params* can set to define global default for the binding functionally. Following binding parameter are available:

- **enabled** - default is *true*. Allows disabling binding functionality.
- **implicit** - default is *false*. Allows enabling implicit binding requests for jobs that do not have explicit binding requests. For jobs with implicit  request the binding amount will correspond to the amount of slots. Implicit binding unit is defined by *default_unit*.
- **default_unit** - default is C. Defines the default binding unit for implicit binding requests if they are enabled via *implicit* parameter.
- **on_any_host** - default is *true*. Enables scheduling of jobs with binding request to hosts that do not report topology information or that do not support binding.
- **filter** - default is *NONE*. Can be set to *first_core* to disallow binding to the first core of the first socket. This reserves that core for other activities on compute nodes (other processes, interrupt handling, IO, ...)

## Notes and Caveats

***Example: Emulation of linear binding***

Strict linear binding, which was available in earlier versions of Grid Engine, is no longer supported as a binding strategy in xxQS_NAMExx. However, the same behavior can be emulated with the following binding request:

```bash
qsub -btype host -bunit C -bamount <slots_per_host> -bsort S -bstart S -bstop s
```

With this configuration:

- Free sockets are sorted together (-bsort S).
- Binding proceeds in a linear order: starting with the first available core on the first unused socket, and continuing until the last core of the last unused socket.

If a “hop” between two not necessarily consecutive sockets should be avoided, the request can be restricted by replacing `-bstop s` with:

```
-bstop S
```

This forces binding to stop at the end of the current socket, preventing allocation across disjoint sockets.

***Example: Explicit binding request for a job***

n earlier versions of Grid Engine, it was possible to explicitly request individual cores. Such requests only made sense if the job was scheduled to a specific host — or to a host with a predictable binding layout.

In xxQS_NAMExx, this behavior can be emulated for any binding unit by using a binding filter. The filter masks certain units, leaving the remainder available for selection by the packed binding strategy.

```bash
qsub -l m_topology=SCCCCSCCCC -bfilter ScCCCScCCC ...
```

In this example, the binding filter masks the first core of each socket. These masked cores will never be used for binding. The remaining cores are then eligible for packed binding, allowing the scheduler to allocate resources efficiently while respecting the filter.

# SEE ALSO

sge_intro(1), qconf(1), qstat(1), qrstat(1), qsub(1), qrsub(1)

# COPYRIGHT

See sge_intro(1) for a full statement of rights and permissions.

