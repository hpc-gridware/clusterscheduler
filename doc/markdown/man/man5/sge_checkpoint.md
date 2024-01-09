---
title: sge_checkpoint
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_checkpoint - xxQS_NAMExx checkpointing environment configuration file
format

# DESCRIPTION

Checkpointing is a facility to save the complete status of an executing
program or job and to restore and restart from this so called checkpoint
at a later point of time if the original program or job was halted, e.g.
through a system crash.

xxQS_NAMExx provides various levels of checkpointing support (see
*xxqs_name_sxx_ckpt*(1)). The checkpointing environment described here
is a means to configure the different types of checkpointing in use for
your xxQS_NAMExx cluster or parts thereof. For that purpose you can
define the operations which have to be executed in initiating a
checkpoint generation, a migration of a checkpoint to another host or a
restart of a checkpointed application as well as the list of queues
which are eligible for a checkpointing method.

Supporting different operating systems may easily force xxQS_NAMExx to
introduce operating system dependencies for the configuration of the
checkpointing configuration file and updates of the supported operating
system versions may lead to frequently changing implementation details.
Please refer to the \<xxqs_name_sxx_root>/ckpt directory for more
information.

Please use the *-ackpt*, *-dckpt*, *-mckpt* or *-sckpt* options to the
*qconf*(1) command to manipulate checkpointing environments from the
command-line or use the corresponding *qmon*(1) dialogue for X-Windows
based interactive configuration.

Note, xxQS_NAMExx allows backslashes (\\) be used to escape newline
(\\newline) characters. The backslash and the newline are replaced with
a space (" ") character before any interpretation.

# FORMAT

The format of a *checkpoint* file is defined as follows:

## **ckpt_name**

The name of the checkpointing environment as defined for *ckpt_name* in
*sge_types*(1).

*qsub*(1) **-ckpt** switch or for the *qconf*(1) options mentioned
above.

## **interface**

The type of checkpointing to be used. Currently, the following types are
valid:

hibernator  
The Hibernator kernel level checkpointing is interfaced.

cpr  
The SGI kernel level checkpointing is used.

cray-ckpt  
The Cray kernel level checkpointing is assumed.

transparent  
xxQS_NAMExx assumes that the jobs submitted with reference to this
checkpointing interface use a checkpointing library such as provided by
the public domain package *Condor*.

userdefined  
xxQS_NAMExx assumes that the jobs submitted with reference to this
checkpointing interface perform their private checkpointing method.

application-level  
Uses all of the interface commands configured in the checkpointing
object like in the case of one of the kernel level checkpointing
interfaces (*cpr*, *cray-ckpt*, etc.) except for the **restart_command**
(see below), which is not used (even if it is configured) but the job
script is invoked in case of a restart instead.

## **ckpt_command**

A command-line type command string to be executed by xxQS_NAMExx in
order to initiate a checkpoint.

## **migr_command**

A command-line type command string to be executed by xxQS_NAMExx during
a migration of a checkpointing job from one host to another.

## **restart_command**

A command-line type command string to be executed by xxQS_NAMExx when
restarting a previously checkpointed application.

## **clean_command**

A command-line type command string to be executed by xxQS_NAMExx in
order to cleanup after a checkpointed application has finished.

## **ckpt_dir**

A file system location to which checkpoints of potentially considerable
size should be stored.

## **ckpt_signal**

A Unix signal to be sent to a job by xxQS_NAMExx to initiate a
checkpoint generation. The value for this field can either be a symbolic
name from the list produced by the *-l* option of the *kill*(1) command
or an integer number which must be a valid signal on the systems used
for checkpointing.

## **when**

The points of time when checkpoints are expected to be generated. Valid
values for this parameter are composed by the letters *s*, *m*, *x* and
*r* and any combinations thereof without any separating character in
between. The same letters are allowed for the *-c* option of the
*qsub*(1) command which will overwrite the definitions in the used
checkpointing environment. The meaning of the letters is defined as
follows:

19. A job is checkpointed, aborted and if possible migrated if the
    corresponding *xxqs_name_sxx_execd*(8) is shut down on the job's
    machine.

20. Checkpoints are generated periodically at the *min_cpu_interval*
    interval defined by the queue (see *xxqs_name_sxx_queue_conf*(5)) in which a job
    executes.

21. A job is checkpointed, aborted and if possible migrated as soon as
    the job gets suspended (manually as well as automatically).

22. A job will be rescheduled (not checkpointed) when the host on which
    the job currently runs went into unknown state and the time interval
    *reschedule_unknown* (see *xxqs_name_sxx_conf*(5)) defined in the
    global/local cluster configuration will be exceeded.

# RESTRICTIONS

**Note**, that the functionality of any checkpointing, migration or
restart procedures provided by default with the xxQS_NAMExx distribution
as well as the way how they are invoked in the *ckpt_command*,
*migr_command* or *restart_command* parameters of any default
checkpointing environments should not be changed or otherwise the
functionality remains the full responsibility of the administrator
configuring the checkpointing environment. xxQS_NAMExx will just invoke
these procedures and evaluate their exit status. If the procedures do
not perform their tasks properly or are not invoked in a proper fashion,
the checkpointing mechanism may behave unexpectedly, xxQS_NAMExx has no
means to detect this.

# SEE ALSO

*xxqs_name_sxx_intro*(1), *xxqs_name_sxx_ckpt*(1),
*xxqs_name_sxx\_\_types*(1), *qconf*(1), *qmod*(1), *qsub*(1),
*xxqs_name_sxx_execd*(8).

# COPYRIGHT

See *xxqs_name_sxx_intro*(1) for a full statement of rights and
permissions.
