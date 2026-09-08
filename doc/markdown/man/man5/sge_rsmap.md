---
title: sge_rsmap
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_rsmap - xxQS_NAMExx resource maps

# DESCRIPTION

A resource map is a complex of type *RSMAP* (see xxqs_name_sxx_complex(5)). It manages a fixed pool
of individually named instances of a resource on one host: GPUs, network interfaces, licence seats,
scratch areas, or anything else the scheduler has to both count and place.

An ordinary consumable answers the question "how much is left". A resource map also answers "which
ones". A job that is granted a resource map is told the identifiers of the instances it may use, and
those identifiers reach the job in its environment, so it can address the devices it was given.

A resource map is always consumable and can be attached only to a host, not to a queue. It appears
in the *complex_values* of a host or of the global host (see xxqs_name_sxx_host_conf(5)).

# DEFINING THE COMPLEX

A resource map is defined like any other complex, with `qconf -mc` or `qconf -ace`:

    name    shortcut  type    relop  requestable  consumable  default  urgency
    gpu     gpu       RSMAP   <=     YES          HOST        0        0

Three of those columns are constrained:

*type*
: *RSMAP*.

*relop*
: `<=`. A resource map is consumable, and a consumable must be `<=` or `EXCL`; `EXCL` is reserved
  for booleans.

*consumable*
: *YES*, *JOB* or *HOST*, never *NO*. Which of the three to choose is described under "How often the
  amount is taken".

Defining the complex only creates the attribute. The instances are configured per host.

# CONFIGURING A RESOURCE MAP ON A HOST

The value in a host's *complex_values* (see xxqs_name_sxx_host_conf(5)) has the form

    <name>=<amount>(<id-spec> <id-spec> ...)

where each *id-spec* is a bare identifier (`gpu0`), an integer range (`1-3`, expanded to individual
ids when it is read), or an identifier followed by a characteristics block. The *amount* must equal
the number of instances the list contains.

    qconf -mattr exechost complex_values "gpu=2(gpu0 gpu1)" node01

An identifier may contain any character except whitespace, `,`, `=`, `(`, `)`, `[` and `]`. There is
no quoting mechanism.

## The short form

The id list may be left out:

    gpu=4

The instances are then named `0` to *amount*-1, so this is equivalent to `gpu=4(0-3)`, and it is
stored and displayed in that longer form. Instances created this way carry no characteristics. An
amount of `0` means the host provides no instance at all.

How many ids may be created this way is limited by *MAX_RSMAP_IDS* in *qmaster_params* (default 512,
see xxqs_name_sxx_conf(5)). A larger amount is rejected and has to be written as an explicit id
list. Setting *MAX_RSMAP_IDS* to `0` rejects the short form altogether.

## Repeated identifiers

An identifier may appear more than once, to model N-way sharing of one physical resource:

    gpu=8(0 0 0 0 1 1 1 1)

This host has two physical GPUs, each usable by four jobs at once. The scheduler sees eight
instances, so eight single-instance jobs can run, but only two identifiers are ever handed out.

If any occurrence of a repeated identifier carries characteristics, every occurrence must carry the
same set; the comparison ignores their order. A bare occurrence and an annotated occurrence of the
same identifier are rejected. Repeating an identical annotation is accepted and deduplicated.

# CHARACTERISTICS

A characteristic attaches typed metadata to one instance: the device files of a GPU, its on-board
memory, its affinity to a set of cores, the bandwidth of an interface. It is written in a bracketed
list after the identifier:

    <id>[<char-name>=<char-value>,<char-name>=<char-value>,...]

    gpu=2(gpu0[devices=/dev/nvidia0:rw,memory=80G] gpu1[devices=/dev/nvidia1:rw,memory=40G])

Each *char-name* must itself be a complex, defined before it is used. The referenced complex
supplies the type, so `memory=80G` is read as *MEMORY* and `bandwidth=100000` as *INT*. A complex of
type *RSMAP* cannot be used as a characteristic. A *char-value* may contain any byte except `,`,
`]`, whitespace, `=`, `(` and `)`; there is no quoting or escape mechanism.

Characteristics cannot be attached to a range, so the identifiers have to be listed one by one.
Whitespace inside the brackets is ignored, so a long definition can be continued with `\`:

    complex_values gpu=2(gpu0[devices=/dev/nvidia0:rw,memory=80G,\
                              affinity_mask=SCCCCCCCCScccccccc] \
                         gpu1[devices=/dev/nvidia1:rw,memory=40G,\
                              affinity_mask=SccccccccSCCCCCCCC])

Most characteristics are metadata that only a prolog or the job itself interprets. One is read by
xxQS_NAMExx: *devices*, described under DEVICE ISOLATION.

Characteristics are available in Gridware Cluster Scheduler only.

# REQUESTING A RESOURCE MAP

A request names the resource map and an amount:

    qsub -l gpu=2 ...

The job is granted two instances and the scheduler chooses which.

## How often the amount is taken

The *consumable* setting of the complex decides how often the requested amount is granted:

*consumable YES*
: Per slot. A parallel job with eight slots requesting `-l gpu=1` is granted eight instances.

*consumable JOB*
: Once for the whole job, on the master host.

*consumable HOST*
: Once per host the job runs on.

A shared-GPU host of the shape shown above is normally *HOST* or *JOB*, so that a multithreaded job
can ask for the instances of one card in a single request.

# WHAT A JOB SEES

The identifiers granted on a host are exported to the job as

    SGE_HGR_<name>

with *name* the name of the resource map. The identifiers are separated by a single space, and an
identifier appears once for every instance granted of it. A job granted two different instances of
`gpu` sees

    SGE_HGR_gpu="gpu0 gpu1"

while a job granted four shares of one card on the host shown above sees

    SGE_HGR_gpu="0 0 0 0"

A prolog or a wrapper script normally turns this into whatever the application expects, for example
the device list of a GPU runtime, and has to allow for the repetition.

# DEVICE ISOLATION

Where an instance carries a *devices* characteristic and the execution host runs jobs under systemd,
a job is given access to the device files of the instances it was granted, and denied the rest.

The value is a list of device paths separated by `;`, each with an optional access mode:

    <path>[:<mode>];<path>[:<mode>];...

*mode* is `r`, `w` or `rw`. A path written without a mode is granted read access.

    qconf -mattr exechost complex_values \
      'gpu=2(gpu0[devices=/dev/nvidia0:rw;/dev/nvidiactl:r] \
             gpu1[devices=/dev/nvidia1:rw;/dev/nvidiactl:r])' node01

A job granted `gpu0` may read and write `/dev/nvidia0` and read `/dev/nvidiactl`, and cannot reach
`/dev/nvidia1` at all. Where one path appears on more than one granted instance, the widest of the
modes applies.

As soon as any device is allowed for a job, the device policy is closed: the job reaches the devices
listed for the instances it was granted, and in addition `/dev/null`, `/dev/zero`, `/dev/full`,
`/dev/random` and `/dev/urandom`. A job that does not request the resource map is not restricted by
it.

# LIMITATIONS

A resource map cannot be attached to a queue.

An identifier is meaningful only on the host it is configured on. Two hosts using the same
identifier are related only by convention.

A request cannot say which instances it wants; the scheduler chooses among the free ones.

# SEE ALSO

xxqs_name_sxx_intro(1), xxqs_name_sxx_qsub(1), xxqs_name_sxx_qconf(1), xxqs_name_sxx_complex(5),
xxqs_name_sxx_host_conf(5), xxqs_name_sxx_conf(5)

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
