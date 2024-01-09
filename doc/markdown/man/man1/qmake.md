---
title: qmake
section: 1
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

qmake - distributed parallel make, scheduling by xxQS_NAMExx.

# SYNTAX

**qmake** \[ **options** \] -- \[ **gmake options** \]

# DESCRIPTION

*Qmake* is a parallel, distributed *make*(1) utility. Scheduling of the
parallel *make* tasks is done by xxQS_NAMExx. It is based on *gmake*
(GNU make), version 4.4. Both xxQS_NAMExx and *gmake* command line
options can be specified. They are separated by "--".

All xxQS_NAMExx options valid with *qsub*(1) or *qrsh*(1) can be
specified with *qmake* - see *submit*(1) for a description of all
xxQS_NAMExx command line options. The *make*(1) manual page describes
the *gmake* command line syntax.

The syntax of *qmake* makefiles corresponds to *gmake* and is described
in the "GNU Make Manual".

A typical qmake call will use the xxQS_NAMExx command line options -cwd
to have a scheduled make started in the current working directory on the
execution host, -v PATH if the xxQS_NAMExx environment is not setup in
the users .cshrc or .profile shell resource file and request slots in a
parallel environment (see *sge_pe*(5) ).

If no resource request (xxQS_NAMExx command line option -l) is
specified, qmake will use the environment variable SGE_ARCH to request
the same architecture for task execution as has the submit host. If
SGE_ARCH is set, the architecture specified in SGE_ARCH will be
requested by inserting the option -l arch=$SGE_ARCH into the command
line options. If SGE_ARCH is not set, the make tasks can be executed on
any available architecture. As this is critical for typical make
(compile) jobs, a warning will be output.

*qmake* has two different modes for allocating xxQS_NAMExx resources for
the parallel execution of tasks:

1\. Allocation of resources using a parallel environment. If the -pe
option is used on the qmake command line, a parallel job is scheduled by
xxQS_NAMExx. The make rules are executed as tasks within this parallel
job.

2\. Dynamic allocation of resources. If no parallel environment is
requested when submitting a qmake job, each make rule will generate an
individual xxQS_NAMExx qrsh job. All resource requests given to qmake
will be inherited by the jobs processing the make rules.

In dynamic allocation mode, additional resource requests for individual
rules can be specified by preceding the rule by the definition of an
environment variable SGE_RREQ. The rule then takes the form
SGE_RREQ="\<request>" \<rule>, e.g. SGE_RREQ="-l lic=1" cc -c ... If
such makefile rules are executed in a make utility other than qmake, the
environment variable SGE_RREQ will be set in the environment established
for the rule's execution - without any effect.

# EXAMPLES

    qmake -cwd -v PATH -pe compiling 1-10 --

will request between 1 and 10 slots in parallel environment "compiling".
If the SGE_ARCH environment variable is set to the machines
architecture, a resource request will be inserted into the qmake command
line to start the qmake job on the same architecture as the submit host.
The *make* tasks will inherit the complete environment of the calling
shell. It will execute as many parallel tasks as slots have been granted
by xxQS_NAMExx.

    qmake -l arch=sol-sparc -cwd -v PATH -- -j 4

will submit each make rule as an individual qrsh job. A maximum of 4
tasks will be processed in parallel. The qmake job will be started on a
machine of architecture sol-sparc, this resource request will also be
inherited by the make tasks, i.e. all jobs created for the execution of
make tasks will request the architecture sol-sparc.

If the following Makefile is submitted with the above command line,
additional resource requests will be made for individual rules: For the
compile and link rules, compiler licenses (comp) and linker licenses
(link) will be requested, in addition to the resource request made for
the whole job (-l arch=sol-sparc) on the command line.

    all: test

    clean:
    	rm -f test main.o functions.o

    test: main.o functions.o
    	SGE_RREQ="-l link=1" ld -o test main.o functions.o

    main.o: main.c
    	SGE_RREQ="-l comp=1" cc -c -DALIASPATH=

    functions.o: functions.c
    	SGE_RREQ="-l comp=1" cc -c -DALIASPATH=

The command line

    qmake -cwd -v PATH -l arch=sol-sparc64 -pe make 3 --

will request 3 parallel *make* tasks to be executed on hosts of
architecture "sol-sparc64". The submit may be done on a host of any
architecture.

The shell script

    #!/bin/sh
    qmake -inherit -- 

can be submitted by

    qsub -cwd -v PATH -pe make 1-10 [further xxqs_name_sxx options] <script>

*Qmake* will inherit the resources granted for the job submitted above
under parallel environment "make".

# ENVIRONMENTAL VARIABLES

xxQS_NAME_Sxx_ROOT  
Specifies the location of the xxQS_NAMExx standard configuration files.

xxQS_NAME_Sxx_CELL  
If set, specifies the default xxQS_NAMExx cell. To address a xxQS_NAMExx
cell *qmake* uses (in the order of precedence):

> The name of the cell specified in the environment variable
> xxQS_NAME_Sxx_CELL, if it is set.
>
> The name of the default cell, i.e. **default**.

xxQS_NAME_Sxx_DEBUG_LEVEL  
If set, specifies that debug information should be written to stderr. In
addition the level of detail in which debug information is generated is
defined.

SGE_ARCH  
The architecture of the submit host. If this variable is set in the
submission environment, qmake will request the given architecture for
job execution (see DESCRIPTION above).

# KNOWN PROBLEMS

## Slow NFS server

Very low file server performance may lead to problems on depending
files.

Example: Host a compiles a.c to a.o, host b compiles b.c to b.o, host c
shall link program c from a.o and b.o. In case of very bad NFS
performance, host c might not yet see files a.o and b.o.

## Multiple commands in one rule

If multiple commands are executed in one rule, the makefile has to
ensure that they are handled as one command line.

Example:

    libx.a:
    cd x
    ar ru libx.a x.o

Building libx.a will fail, if the commands are executed in parallel (and
possibly on different hosts). Write the following instead:

    libx.a:
    cd x ; ar ru libx.a x.o

or

    libx.a:
    cd x ; \
    ar ru libx.a x.o

# SEE ALSO

*submit*(1) , *sge_pe*(5) as well as *make*(1) (GNU make manpage) and
*The GNU Make Manual* in \<xxqs_name_sxx_root>/3rd_party/qmake.

# COPYRIGHT

*Qmake* contains portions of Gnu Make (*gmake*), which is the copyright
of the Free Software Foundation, Inc., Boston, MA, and is protected by
the Gnu General Public License.  
See *xxqs_name_sxx_intro*(1) and the information provided in
\<xxqs_name_sxx_root>/3rd_party/qmake for a statement of further rights
and permissions.
