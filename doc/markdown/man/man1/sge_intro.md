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

xxQS_NAMExx is a facility for executing UNIX batch jobs (shell scripts) on a pool of cooperating workstations. 
Jobs are queued and executed remotely on workstations at times when those workstations would otherwise be idle or 
only lightly loaded. The work load is distributed among the workstations in the cluster corresponding to the load
situation of each machine and the resource requirements of the jobs.

User level checkpointing programs are supported and a transparent checkpointing mechanism is provided 
(see *xxqs_name_sxx_ckpt*(1)). Checkpointing jobs migrate from workstation to workstation without user
intervention on load demand. In addition to batch jobs, interactive jobs and parallel jobs can also be submitted to 
xxQS_NAMExx.

# USER INTERFACE

The xxQS_NAMExx user interface consists of several programs which are described separately.

## qacct(1)  
`qacct` extracts arbitrary accounting information from the cluster logfile.

## qalter(1)  
`qalter` changes the characteristics of already submitted jobs.

## qconf(1)  
`qconf` provides the user interface for configuring, modifying, deleting and querying queues and the 
cluster configuration.

## qdel(1)  
`qdel` provides the means for a user/operator/manager to cancel jobs.

## qhold(1)  
`qhold` holds back submitted jobs from execution.

## qhost(1)  
`qhost` displays status information about xxQS_NAMExx execution hosts.

## qlogin(1)  
`qlogin` initiates a ssh or similar login session with automatic selection of a low loaded and suitable host.

## qmake(1)  
`qmake` is a replacement for the standard Unix make(1) facility. It extends make by its ability to distribute 
independent make steps across a cluster of suitable machines.

## qmod(1)  
`qmod` allows the owner(s) of a queue to suspend and enable all queues associated with his machine (all currently 
active processes in this queue are also signaled) or to suspend and enable jobs executing in the owned queues.

## qping(1)
`qping` checks application status of xxQS_NAMExx daemons.

## qquota(1)  
`qquota` provides a status listing of all currently used resource quotas.

## qrdel(1)
`qrdel` deletes xxQS_NAMExx advance reservations.

## qresub(1)  
`qresub` creates new jobs by copying currently running or pending jobs.

## qrls(1)
`qrls` releases a job hold state.

## qrdel(1)  
`qrdel` provides the means to cancel advance reservations.

## qrsh(1)  
`qrsh` can be used for various purposes such as providing remote execution of interactive applications via 
xxQS_NAMExx comparable to the standard Unix facility rsh, to allow for the submission of batch jobs which, 
upon execution, support terminal I/O (standard/error output and standard input) and terminal control, to provide 
a batch job submission client which remains active until the job has finished or to allow for the 
xxQS_NAMExx-controlled remote execution of the tasks of parallel jobs.

## qrstat(1)  
`qrstat` provides a status listing of all advance reservations in the cluster.

## qrsub(1)  
`qrsub` is the user interface for submitting an advance reservation to xxQS_NAMExx.

## qselect(1)  
`qselect` prints a list of queue names corresponding to specified selection criteria. The output of `qselect` 
is usually fed into other xxQS_NAMExx commands to apply actions on a selected set of queues.

## qsh(1)  
`qsh` opens an interactive shell (in a xterm(1)) on a low loaded host. Any kind of interactive jobs can be run 
in this shell.

## qstat(1)  
`qstat` provides a status listing of all jobs and queues associated with the cluster.

## qsub(1)  
`qsub` is the user interface for submitting a job to xxQS_NAMExx.

# ENVIRONMENT

## xxQS_NAME_Sxx_ROOT
Specifies the location of the xxQS_NAMExx standard configuration files.

## xxQS_NAME_Sxx_CELL
If set, specifies the default xxQS_NAMExx cell. To address a xxQS_NAMExx cell commands use (in the order of precedence):

* The name of the cell specified in the environment variable xxQS_NAME_Sxx_CELL, if it is set.
* The name of the default cell, i.e. **default**.

## xxQS_NAME_Sxx_DEBUG_LEVEL
If set, specifies that debug information should be written to stderr. In addition to the level of detail in which
debug information is generated is defined.

## xxQS_NAME_Sxx_QMASTER_PORT
If set, specifies the TCP port on which xxqs_name_sxx_qmaster(8) is expected to listen for communication requests.
Most installations will use a services map entry instead to define that port.

## xxQS_NAME_Sxx_EXECD_PORT
If set, specifies the tcp port on which xxqs_name_sxx_execd(8) is expected to listen for communication requests.
Most installations will use a services map entry instead to define that port.

# FILES

## \<xxQS_NAME_Sxx_ROOT\>/\<xxQS_NAME_Sxx_CELL\>/common/act_qmaster
xxQS_NAMExx master host file

## \<xxQS_NAME_Sxx_ROOT\>/\<xxQS_NAME_Sxx_CELL\>/common/accounting
xxQS_NAMExx default accounting file

## \<xxQS_NAME_Sxx_ROOT\>/\<xxQS_NAME_Sxx_CELL\>/common/schedd_runlog
File containing trace message of the scheduling component after a call of `qconf -tsm`

# SEE ALSO

qacct(1), qalter(1), qconf(1), qdel(1), qhold(1), qhost(1), qlogin(1), qmake(1), qmod(1), qping(1), qquota(1), 
qrdel(1), qresub(1), qrls(1), qrsh(1), qrstat(1), qrsub(1), qselect(1), qsh(1), qstat(1), qsub(1)

# COPYRIGHT

Copyright: 2008 by Sun Microsystems, Inc.

Parts of the manual page texts are Copyright 2011 Univa Corporation.

Parts of the manual page texts are Copyright 2024 HPC-Gridware GmbH.

