---
title: sge_ckpt
section: 1
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxQS_NAMExx Checkpointing - the xxQS_NAMExx checkpointing mechanism and
checkpointing support

# DESCRIPTION

xxQS_NAMExx supports two levels of checkpointing: the user level and a
operating system provided transparent level. User level checkpointing
refers to applications, which do their own checkpointing by writing
restart files at certain times or algorithmic steps and by properly
processing these restart files when restarted.

Transparent checkpointing has to be provided by the operating system and
is usually integrated in the operating system kernel. An example for a
kernel integrated checkpointing facility is the Hibernator package from
Softway for SGI IRIX platforms.

Checkpointing jobs need to be identified to the xxQS_NAMExx system by
using the *-ckpt* option of the *qsub1* () command. The argument to this
flag refers to a so called checkpointing environment, which defines the
attributes of the checkpointing method to be used (see *checkpoint5* ()
for details). Checkpointing environments are setup by the *qconf1* ()
options *-ackpt*, *-dckpt*, *-mckpt* and *-sckpt*. The *qsub1* () option
*-c* can be used to overwrite the *when* attribute for the referenced
checkpointing environment.

If a queue is of the type CHECKPOINTING, jobs need to have the
checkpointing attribute flagged (see the **-ckpt** option to *qsub1* ())
to be permitted to run in such a queue. As opposed to the behavior for
regular batch jobs, checkpointing jobs are aborted under conditions, for
which batch or interactive jobs are suspended or even stay unaffected.
These conditions are:

-   Explicit suspension of the queue or job via *qmod1* () by the
    cluster administration or a queue owner if the *x* occasion
    specifier (see *qsub1* () *-c* and *checkpoint5* ()) was assigned to
    the job.

-   A load average value exceeding the suspend threshold as configured
    for the corresponding queues (see *queue_conf5* ().)

-   Shutdown of the xxQS_NAMExx execution daemon
    *xxqs_name_sxx_execd8* () being responsible for the checkpointing
    job.

After abortion, the jobs will migrate to other queues unless they were
submitted to one specific queue by an explicit user request. The
migration of jobs leads to a dynamic load balancing. **Note:** The
abortion of checkpointed jobs will free all resources (memory, swap
space) which the job occupies at that time. This is opposed to the
situation for suspended regular jobs, which still cover swap space.

# RESTRICTIONS

When a job migrates to a queue on another machine at present no files
are transferred automatically to that machine. This means that all files
which are used throughout the entire job including restart files,
executables and scratch files must be visible or transferred explicitly
(e.g. at the beginning of the job script).

There are also some practical limitations regarding use of disk space
for transparently checkpointing jobs. Checkpoints of a transparently
checkpointed application are usually stored in a checkpoint file or
directory by the operating system. The file or directory contains all
the text, data, and stack space for the process, along with some
additional control information. This means jobs which use a very large
virtual address space will generate very large checkpoint files. Also
the workstations on which the jobs will actually execute may have little
free disk space. Thus it is not always possible to transfer a
transparent checkpointing job to a machine, even though that machine is
idle. Since large virtual memory jobs must wait for a machine that is
both idle, and has a sufficient amount of free disk space, such jobs may
suffer long turnaround times.

# SEE ALSO

*xxqs_name_sxx_intro1* (,) *qconf1* (,) *qmod1* (,) *qsub1* (,)
*checkpoint5* (,) *xxQS_NAMExx Installation and Administration Guide,*
*xxQS_NAMExx User's Guide*

# COPYRIGHT

See *xxqs_name_sxx_intro1* () for a full statement of rights and
permissions.
