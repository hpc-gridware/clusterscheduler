---
title: qhold
section: 1
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

`qhold` - hold back xxQS_NAMExx jobs from execution

# SYNTAX

`qhold` \[`-h` {*u*\|*o*\|*s*},...\] \[`-help`\] \[*job/task_id_list*\]

`qhold` \[`-h` {*u*\|*o*\|*s*},...\] \[`-help`\] `-u` *user_list*

# DESCRIPTION

`qhold` provides a means for a user/operator/manager to place so-called *holds* on one or more jobs pending to 
be scheduled for execution. As long as any type of hold is assigned to a job, the job is not eligible for scheduling.

Holds can be removed with the qrls(1) or the qalter(1) command.

There are three different types of holds:

* **User** holds can be assigned and removed by managers, operators and the owner of the jobs.
* **Operator** holds can be assigned and removed by managers and operators.
* **System** holds can be assigned and removed by managers only.

If no hold type is specified with the `-h` option (see below) the user hold is assumed by default.

An alternate way to assign holds to jobs is the qsub(1) or the qalter(1) command (see the `-h` option).

# OPTIONS

## -h {*u*\|*o*\|*s*},...  
Assign a u(ser), o(perator) or s(system) hold or a combination thereof to one or more jobs.

## -help  
Prints a listing of all options.

## -u *username*,...  
Changes are only made on those jobs which were submitted by users specified in the list of usernames. 
Managers are allowed to use the `qhold -u "\*"` command to set a hold for all jobs of all users.

If a user uses the `-u` switch, the user may specify an additional *job/task_id_list*.

## job/task_id_list  
Specified by the following form:

> *job_id*\[.*task_range*\]\[,*job_id*\[.*task_range*\], ...\]*

If present, the *task_range* restricts the effect of the `qhold` operation to the array job task range specified 
as suffix to the job id (see the `-t` option to qsub(1) for further details on array jobs).

The task range specifier has the form *n*\[-*m*\[:*s*\]\]. The range may be a single number, a simple range of 
the form *n*-*m* or a range with a step size *s*.

Instead of *job/task_id_list* it is possible to use the keyword **all** to modify the hold state for all jobs of 
the current user.

# ENVIRONMENTAL VARIABLES

For a complete list of common environment variables used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# FILES

For a complete list of common environment variables used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# SEE ALSO

xxqs_name_sxx_intro(1), qalter(1), qrls(1), qsub(1).

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
