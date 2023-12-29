---
title: sge_intro
section: 1
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxQS_NAMExx - a facility for executing UNIX jobs on remote machines

# DESCRIPTION

xxQS_NAMExx is a facility for executing UNIX batch jobs (shell scripts)
on a pool of cooperating workstations. Jobs are queued and executed
remotely on workstations at times when those workstations would
otherwise be idle or only lightly loaded. The work load is distributed
among the workstations in the cluster corresponding to the load
situation of each machine and the resource requirements of the jobs.

User level checkpointing programs are supported and a transparent
checkpointing mechanism is provided (see *xxqs_name_sxx_ckpt* (1)).
Checkpointing jobs migrate from workstation to workstation without user
intervention on load demand. In addition to batch jobs, interactive jobs
and parallel jobs can also be submitted to xxQS_NAMExx.

# USER INTERFACE

The xxQS_NAMExx user interface consists of several programs which are
described separately.

*qacct* (1)  
*qacct* extracts arbitrary accounting information from the cluster
logfile.

*qalter* (1)  
*qalter* changes the characteristics of already submitted jobs.

*qconf* (1)  
*qconf* provides the user interface for configuring, modifying, deleting
and querying queues and the cluster configuration.

*qdel* (1)  
*qdel* provides the means for a user/operator/manager to cancel jobs.

*qhold* (1)  
*qhold* holds back submitted jobs from execution.

*qhost* (1)  
*qhost* displays status information about xxQS_NAMExx execution hosts.

*qlogin* (1)  
*qlogin* initiates a telnet or similar login session with automatic
selection of a low loaded and suitable host.

*qmake* (1)  
*qmake* is a replacement for the standard Unix *make* facility. It
extends make by its ability to distribute independent make steps across
a cluster of suitable machines.

*qmod* (1)  
*qmod* allows the owner(s) of a queue to suspend and enable all queues
associated with his machine (all currently active processes in this
queue are also signaled) or to suspend and enable jobs executing in the
owned queues.

*qmon* (1)  
*qmon* provides a Motif command interface to all xxQS_NAMExx functions.
The status of all or a private selection of the configured queues is
displayed on-line by changing colors at corresponding queue icons.

*qquota* (1)  
*qquota* provides a status listing of all currently used resource quotas
(see *xxqs_name_sxx_resource_quota* (1).)

*qresub* (1)  
*qresub* creates new jobs by copying currently running or pending jobs.

*qrls* (1)  
*qrls* releases holds from jobs previously assigned to them e.g. via
*qhold* (1) (see above).

*qrdel* (1)  
*qrdel* provides the means to cancel advance reservations.

*qrsh* (1)  
*qrsh* can be used for various purposes such as providing remote
execution of interactive applications via xxQS_NAMExx comparable to the
standard Unix facility rsh, to allow for the submission of batch jobs
which, upon execution, support terminal I/O (standard/error output and
standard input) and terminal control, to provide a batch job submission
client which remains active until the job has finished or to allow for
the xxQS_NAMExx-controlled remote execution of the tasks of parallel
jobs.

*qrstat* (1)  
*qrstat* provides a status listing of all advance reservations in the
cluster.

*qrsub* (1)  
*qrsub* is the user interface for submitting a advance reservation to
xxQS_NAMExx.

*qselect* (1)  
*qselect* prints a list of queue names corresponding to specified
selection criteria. The output of *qselect* is usually fed into other
xxQS_NAMExx commands to apply actions on a selected set of queues.

*qsh* (1)  
*qsh* opens an interactive shell (in an *xterm* (1)) on a low loaded
host. Any kind of interactive jobs can be run in this shell.

*qstat* (1)  
*qstat* provides a status listing of all jobs and queues associated with
the cluster.

*qsub* (1)  
*qsub* is the user interface for submitting a job to xxQS_NAMExx.

# SEE ALSO

*xxqs_name_sxx_ckpt* (1), *qacct* (1), *qalter* (1), *qconf* (1),
*qdel* (1), *qhold* (1), *qhost* (1), *qlogin* (1), *qmake* (1),
*qmod* (1), *qmon* (1), *qresub* (1), *qrls* (1), *qrsh* (1),
*qselect* (1), *qsh* (1), *qstat* (1), *qsub* (1), *xxQS_NAMExx
Installation Guide,* *xxQS_NAMExx Administration Guide,* *xxQS_NAMExx
User's Guide.*

# COPYRIGHT

Copyright: 2008 by Sun Microsystems, Inc.

Parts of the manual page texts are Copyright 2011 Univa Corporation.
