---
title: sge_priority
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

sge_priority - xxQS_NAMExx job priorities

# DESCRIPTION

xxQS_NAMExx provide means for controlling job dispatch and run-time
priorities. The dispatch priority indicates the importance of pending
jobs compared with each other and determines the order in which
xxQS_NAMExx dispatches jobs to queue instances. The run-time priority
determines the CPU allocation that the operating system assigns to jobs.

## **JOBS DISPATCH PRIORITY**

A job's dispatch priority is affected by a number of factors:

-   the identity of the submitting user

-   the project under which the job is submitted (or alternatively, the
    default project of the submitting user)

-   any resources requested by the job

-   the job's submit time

-   the job's initiation deadline time (if specified)

-   the -p priority specified for the job (also known as the POSIX
    priority "pprio")

The effect of each of these is governed by the overall policy setup,
which is split into three top-level contributions. Each of these is
configured through the *sched_conf*(5) parameters **weight_priority**,
**weight_ticket** and **weight_urgency**. These three parameters control
to what degree POSIX priority, ticket policy, and urgency policy are in
effect. To facilitate interpretation, the raw priorities
("tckts"/"urg"/"ppri") are normalized ("ntckts"/"nurg"/"npprior") before
they are used to calculate job priorities ("prio"). Normalization maps
each raw urgency/ticket/priority value into a range between 0 and 1.

npprior = normalized(ppri) nurg = normalized(urg) ntckts =
normalized(tckts)

prio = weight_priority \* pprio + weight_urgency \* nurg + weight_ticket
\* ntckts

The higher a job's priority value, the earlier it gets dispatched.

The urgency policy defines an urgency value for each job. The urgency
value

urg = rrcontr + wtcontr + dlcontr

consists of the resource requirement contribution ("rrcontr"), the
waiting time contribution ("wtcontr") and the deadline contribution
("dlcontr").

The resource requirement contribution is adding up all resource
requirements of a job into a single numeric value.

rrcontr = Sum over all(hrr)

with an "hrr" for each hard resource request. Depending on the resource
type two different methods are used to determine the value to be used
for "hrr" here. For numeric type resource requests, the "hrr" represents
how much of a resource a job requests (on a per-slot basis for pe jobs)
and how "important" this resource is considered in comparison to other
resources. This is expressed by the formula:

hrr = rurg \* assumed_slot_allocation \* request

where the resource's urgency value ("rurg") is as specified under
**urgency** in *complex*(5), the job's assumed_slot_allocation
represents the number of slots supposedly assigned to the job, and the
per-slot request is that which was specified using the -l *qsub*(1)
option. For string type requests the formula is simply

hrr = "rurg"

and directly assigns the resource urgency value as specified under
**urgency** in *complex*(5).

The waiting time contribution represents a weighted weighting time of
the jobs

wtcontr = waiting_time \* weight_waiting_time

with the waiting time in seconds and the **weight_waiting_time** value
as specified in *sched_conf*(5).

The deadline contribution has an increasing effect as jobs approach
their deadline initiation time (see the -dl option in *qsub*(1)). It is
defined as the quotient of the **weight_deadline** value from
*sched_conf*(5) and the (steadily decreasing) free time in seconds
until deadline initiation time

dlcontr = weight_deadline / free_time

or is set to 0 for non-deadline jobs. After the deadline passes, the
value is static and equal to weight_deadline.

The ticket policy unites functional, override and share tree policies in
the ticket value ("tckts"), as is defined as the sum of the specific
ticket values ("ftckt"/"otckt"/"stckt") for each sub-policy (functional,
override, share):

tckts = ftckt + otckt + stckt

The ticket policies provide a broad range of means for influencing both
job dispatch and runtime priorities on a per job, per user, per project,
and per department basis. See the xxQS_NAMExx Installation and
Administration Guide for details.

## **JOB RUN-TIME PRIORITY**

The run-time priority can be dynamically adjusted in order to meet the
goals set with the ticket policy. Dynamic run-time priority adjustment
can be turned off (default) globally using **reprioritize_interval** in
*sched_conf*(5) If no dynamic run-time priority adjustment is done at a
host level, the **priority** specification in *queue_conf*(5) is in
effect.

Note that urgency and POSIX priorities do **NOT** affect runtime
priority.

# SEE ALSO

*xxqs_name_sxx_intro*(1), *xxqs_name_sxx_complex*(5), *qstat*(1), *qsub*(1),
*xxqs_name_sxx_sched_conf*(5), *sge_conf*(5) *xxQS_NAMExx Installation and
Administration Guide*

# COPYRIGHT

See *xxqs_name_sxx_intro*(1) for a full statement of rights and
permissions.
