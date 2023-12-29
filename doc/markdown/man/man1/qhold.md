---
title: qhold
section: 1
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

qhold - hold back xxQS_NAMExx jobs from execution

# SYNTAX

**qhold** \[ **-h** {**u**\|**o**\|**s**},... \] \[ **-help** \] \[
**job/task_id_list** \]

**qhold** \[ **-h** {**u**\|**o**\|**s**},... \] \[ **-help** \] **-u
user_list**

# DESCRIPTION

*Qhold* provides a means for a user/operator/manager to place so called
*holds* on one or more jobs pending to be scheduled for execution. As
long as any type of hold is assigned to a job, the job is not eligible
for scheduling.

Holds can be removed with the *qrls* (1) or the *qalter* (1) command.

There are three different types of holds:

user  
User holds can be assigned and removed by managers, operators and the
owner of the jobs.

operator  
Operator holds can be assigned and removed by managers and operators.

system  
System holds can be assigned and removed by managers only.

If no hold type is specified with the **-h** option (see below) the user
hold is assumed by default.

An alternate way to assign holds to jobs is the *qsub* (1) or the
*qalter* (1) command (see the *-h* option).

# OPTIONS

-h {u\|o\|s},...  
Assign a u(ser), o(perator) or s(system) hold or a combination thereof
to one or more jobs.

-help  
Prints a listing of all options.

-u username,...  
Changes are only made on those jobs which were submitted by users
specified in the list of usernames. Managers are allowed to use the
**qhold -u "\*"** command to set a hold for all jobs of all users.

If a user uses the **-u** switch, the user may specify an additional
*job/task_id_list*.

job/task_id_list  
Specified by the following form:

*job_id\[.task_range\]\[,job_id\[.task_range\],...\]*

If present, the *task_range* restricts the effect of the *qhold*
operation to the array job task range specified as suffix to the job id
(see the **-t** option to *qsub* (1) for further details on array jobs).

The task range specifier has the form n\[-m\[:s\]\]. The range may be a
single number, a simple range of the form n-m or a range with a step
size.

Instead of *job/task_id_list* it is possible to use the keyword 'all' to
modify the hold state for all jobs of the current user.

# ENVIRONMENTAL VARIABLES

xxQS_NAME_Sxx_ROOT  
Specifies the location of the xxQS_NAMExx standard configuration files.

xxQS_NAME_Sxx_CELL  
If set, specifies the default xxQS_NAMExx cell. To address a xxQS_NAMExx
cell *qhold* uses (in the order of precedence):

> The name of the cell specified in the environment variable
> xxQS_NAME_Sxx_CELL, if it is set.
>
> The name of the default cell, i.e. **default**.

xxQS_NAME_Sxx_DEBUG_LEVEL  
If set, specifies that debug information should be written to stderr. In
addition the level of detail in which debug information is generated is
defined.

xxQS_NAME_Sxx_QMASTER_PORT  
If set, specifies the tcp port on which *xxqs_name_sxx_qmaster* (8) is
expected to listen for communication requests. Most installations will
use a services map entry for the service "sge_qmaster" instead to define
that port.

# SEE ALSO

*xxqs_name_sxx_intro* (1), *qalter* (1), *qrls* (1), *qsub* (1).

# COPYRIGHT

See *xxqs_name_sxx_intro* (1) for a full statement of rights and
permissions.
