---
title: sge_conf
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_conf - xxQS_NAMExx configuration files

# DESCRIPTION

*xxqs_name_sxx_conf* defines the global and local xxQS_NAMExx
configurations and can be shown/modified by *qconf* (1) using the
-sconf/-mconf options. Only root or the cluster administrator may modify
*xxqs_name_sxx_conf.*

At its initial start-up, *xxqs_name_sxx_qmaster* (8) checks to see if a
valid xxQS_NAMExx configuration is available at a well known location in
the xxQS_NAMExx internal directory hierarchy. If so, it loads that
configuration information and proceeds. If not,
*xxqs_name_sxx_qmaster* (8) writes a generic configuration containing
default values to that same location. The xxQS_NAMExx execution daemons
*xxqs_name_sxx_execd* (8) upon start-up retrieve their configuration
from *xxqs_name_sxx_qmaster* (8).

The actual configuration for both *xxqs_name_sxx_qmaster* (8) and
*xxqs_name_sxx_execd* (8) is a superposition of a *global* configuration
and a *local* configuration pertinent for the host on which a master or
execution daemon resides. If a local configuration is available, its
entries overwrite the corresponding entries of the global configuration.
**Note:** The local configuration does not have to contain all valid
configuration entries, but only those which need to be modified against
the global entries.

Note: xxQS_NAMExx allows backslashes (\\) be used to escape newline
(\\newline) characters. The backslash and the newline are replaced with
a space (" ") character before any interpretation.

# FORMAT

The paragraphs that follow provide brief descriptions of the individual
parameters that compose the global and local configurations for a
xxQS_NAMExx cluster:

## **execd_spool_dir**

The execution daemon spool directory path. Again, a feasible spool
directory requires read/write access permission for root. The entry in
the global configuration for this parameter can be overwritten by
execution host local configurations, i.e. each *xxqs_name_sxx_execd* (8)
may have a private spool directory with a different path, in which case
it needs to provide read/write permission for the root account of the
corresponding execution host only.

Under **execd_spool_dir** a directory named corresponding to the
unqualified hostname of the execution host is opened and contains all
information spooled to disk. Thus, it is possible for the
**execd_spool_dir**s of all execution hosts to physically reference the
same directory path (the root access restrictions mentioned above need
to be met, however).

Changing the global **execd_spool_dir** parameter set at installation
time is not supported in a running system. If the change should still be
done it is required to restart all affected execution daemons. Please
make sure running jobs have finished before doing so, otherwise running
jobs will be lost.

The default location for the execution daemon spool directory is
$xxQS_NAME_Sxx_ROOT/$xxQS_NAME_Sxx_CELL/spool.

The global configuration entry for this value may be overwritten by the
execution host local configuration.

## **mailer**

**mailer** is the absolute pathname to the electronic mail delivery
agent on your system. It must accept the following syntax:

> mailer -s \<subject-of-mail-message> \<recipient>

Each *xxqs_name_sxx_execd* (8) may use a private mail agent. Changing
**mailer** will take immediate effect.

The default for **mailer** depends on the operating system of the host
on which the xxQS_NAMExx master installation was run. Common values are
/bin/mail or /usr/bin/Mail.

The global configuration entry for this value may be overwritten by the
execution host local configuration.

## **xterm**

**xterm** is the absolute pathname to the X Window System terminal
emulator, *xterm* (1).

Changing **xterm** will take immediate effect.

The default for **xterm** is /usr/bin/X11/xterm.

The global configuration entry for this value may be overwritten by the
execution host local configuration.

## **load_sensor**

A comma separated list of executable shell script paths or programs to
be started by *xxqs_name_sxx_execd* (8) and to be used in order to
retrieve site configurable load information (e.g. free space on a
certain disk partition).

Each *xxqs_name_sxx_execd* (8) may use a set of private **load_sensor**
programs or scripts. Changing **load_sensor** will take effect after two
load report intervals (see **load_report_time). A load sensor will be
restarted automatically if** the file modification time of the load
sensor executable changes.

The global configuration entry for this value may be overwritten by the
execution host local configuration.

In addition to the load sensors configured via **load_sensor,**
*xxqs_name_sxx_exec* (8) searches for an executable file named
*qloadsensor* in the execution host's xxQS_NAMExx binary directory path.
If such a file is found, it is treated like the configurable load
sensors defined in **load_sensor. This facility is intended for
pre-installing** a default load sensor.

## **prolog**

The executable path of a shell script that is started before execution
of xxQS_NAMExx jobs with the same environment setting as that for the
xxQS_NAMExx jobs to be started afterwards. An optional prefix "user@"
specifies the user under which this procedure is to be started. The
procedures standard output and the error output stream are written to
the same file used also for the standard output and error output of each
job. This procedure is intended as a means for the xxQS_NAMExx
administrator to automate the execution of general site specific tasks
like the preparation of temporary file systems with the need for the
same context information as the job. Each *xxqs_name_sxx_execd* (8) may
use a private prolog script. Correspondingly, the execution host local
configurations is can be overwritten by the queue configuration (see
*xxqs_name_sxx_queue_conf* (5) ). Changing **prolog will take** immediate effect.

The default for **prolog is the special value NONE, which prevents**
from execution of a prolog script.

The following special variables expanded at runtime can be used (besides
any other strings which have to be interpreted by the procedure) to
constitute a command line:

$host  
The name of the host on which the prolog or epilog procedures are
started.

$job_owner  
The user name of the job owner.

$job_id  
xxQS_NAMExx's unique job identification number.

$job_name  
The name of the job.

$processors  
The **processors string as contained in the queue configuration** (see
*xxqs_name_sxx_queue_conf* (5)) of the master queue (the queue in which the prolog and
epilog procedures are started).

$queue  
The cluster queue name of the master queue instance, i.e. the cluster
queue in which the prolog and epilog procedures are started.

$stdin_path  
The pathname of the stdin file. This is always /dev/null for prolog,
pe_start, pe_stop and epilog. It is the pathname of the stdin file for
the job in the job script. When delegated file staging is enabled, this
path is set to $fs_stdin_tmp_path. When delegated file staging is not
enabled, it is the stdin pathname given via DRMAA or qsub.

$stdout_path  

$stderr_path  
The pathname of the stdout/stderr file. This always points to the
output/error file. When delegated file staging is enabled, this path is
set to $fs_stdout_tmp_path/$fs_stderr_tmp_path. When delegated file
staging is not enabled, it is the stdout/stderr pathname given via DRMAA
or qsub.

$merge_stderr  
If merging of stderr and stdout is requested, this flag is "1",
otherwise it is "0". If this flag is 1, stdout and stderr are merged in
one file, the stdout file. Merging of stderr and stdout can be requested
via the DRMAA job template attribute 'drmaa_join_files' (see
*drmaa_attributes* (3) ) or the qsub parameter '-j y' (see *qsub* (1) ).

$fs_stdin_host  
When delegated file staging is requested for the stdin file, this is the
name of the host where the stdin file has to be copied from before the
job is started.

$fs_stdout_host  

$fs_stderr_host  
When delegated file staging is requested for the stdout/stderr file,
this is the name of the host where the stdout/stderr file has to be
copied to after the job has run.

$fs_stdin_path  
When delegated file staging is requested for the stdin file, this is the
pathname of the stdin file on the host $fs_stdin_host.

$fs_stdout_path  

$fs_stderr_path  
When delegated file staging is requested for the stdout/stderr file,
this is the pathname of the stdout/stderr file on the host
$fs_stdout_host/$fs_stderr_host.

$fs_stdin_tmp_path  
When delegated file staging is requested for the stdin file, this is the
destination pathname of the stdin file on the execution host. The prolog
script must copy the stdin file from $fs_stdin_host:$fs_stdin_path to
localhost:$fs_stdin_tmp_path to establish delegated file staging of the
stdin file.

$fs_stdout_tmp_path  

$fs_stderr_tmp_path  
When delegated file staging is requested for the stdout/stderr file,
this is the source pathname of the stdout/stderr file on the execution
host. The epilog script must copy the stdout file from
localhost:$fs_stdout_tmp_path to $fs_stdout_host:$fs_stdout_path (the
stderr file from localhost:$fs_stderr_tmp_path to
$fs_stderr_host:$fs_stderr_path) to establish delegated file staging of
the stdout/stderr file.

$fs_stdin_file_staging  

$fs_stdout_file_staging  

$fs_stderr_file_staging  
When delegated file staging is requested for the stdin/stdout/stderr
file, the flag is set to "1", otherwise it is set to "0" (see in
**delegated_file_staging how ** to enable delegated file staging). These
three flags correspond to the DRMAA job template attribute
'drmaa_transfer_files' (see *drmaa_attributes* (3) ).

The global configuration entry for this value may be overwritten by the
execution host local configuration.

Exit codes for the prolog attribute can be interpreted based on the
following exit values:

> 0: Success  
> 99: Reschedule job  
> 100: Put job in error state  
> Anything else: Put queue in error state

## **epilog**

The executable path of a shell script that is started after execution of
xxQS_NAMExx jobs with the same environment setting as that for the
xxQS_NAMExx jobs that has just completed. An optional prefix "user@"
specifies the user under which this procedure is to be started. The
procedures standard output and the error output stream are written to
the same file used also for the standard output and error output of each
job. This procedure is intended as a means for the xxQS_NAMExx
administrator to automate the execution of general site specific tasks
like the cleaning up of temporary file systems with the need for the
same context information as the job. Each *xxqs_name_sxx_execd* (8) may
use a private epilog script. Correspondingly, the execution host local
configurations is can be overwritten by the queue configuration (see
*xxqs_name_sxx_queue_conf* (5) ). Changing **epilog will take** immediate effect.

The default for **epilog is the special value NONE, which prevents**
from execution of a epilog script. The same special variables as for
**prolog can be** used to constitute a command line.

The global configuration entry for this value may be overwritten by the
execution host local configuration.

Exit codes for the epilog attribute can be interpreted based on the
following exit values:

> 0: Success  
> 99: Reschedule job  
> 100: Put job in error state  
> Anything else: Put queue in error state

## **shell_start_mode**

**Note:** Deprecated, may be removed in future release.  
This parameter defines the mechanisms which are used to actually invoke
the job scripts on the execution hosts. The following values are
recognized:

unix_behavior  
If a user starts a job shell script under UNIX interactively by invoking
it just with the script name the operating system's executable loader
uses the information provided in a comment such as \`#!/bin/csh' in the
first line of the script to detect which command interpreter to start to
interpret the script. This mechanism is used by xxQS_NAMExx when
starting jobs if *unix_behavior*** is defined as shell_start_mode.**

posix_compliant  
POSIX does not consider first script line comments such a \`#!/bin/csh'
as significant. The POSIX standard for batch queuing systems (P1003.2d)
therefore requires a compliant queuing system to ignore such lines but
to use user specified or configured default command interpreters
instead. Thus, if **shell_start_mode is set to** *posix_compliant***
xxQS_NAMExx will either use the command interpreter** indicated by the
**-S option of the** *qsub* (1) command or the **shell parameter of the
queue to be used (see** *xxqs_name_sxx_queue_conf* (5) for details).

script_from_stdin  
Setting the **shell_start_mode parameter either to ***posix_compliant*
or *unix_behavior*** requires you to set the umask in use for**
*xxqs_name_sxx_execd* (8) such that every user has read access to the
active_jobs directory in the spool directory of the corresponding
execution daemon. In case you have **prolog and epilog scripts
configured, they also need to be** readable by any user who may execute
jobs.  
If this violates your site's security policies you may want to set
**shell_start_mode** to *script_from_stdin***. This will force
xxQS_NAMExx to open the** job script as well as the epilog and prolog
scripts for reading into STDIN as root (if *xxqs_name_sxx_execd* (8) was
started as root) before changing to the job owner's user account. The
script is then fed into the STDIN stream of the command interpreter
indicated by the **-S option of the** *qsub* (1) command or the **shell
parameter of the queue to be used (see** *xxqs_name_sxx_queue_conf* (5) for
details).  
Thus setting **shell_start_mode to ***script_from_stdin*** also**
implies *posix_compliant*** behavior. Note, however, that** feeding
scripts into the STDIN stream of a command interpreter may cause trouble
if commands like *rsh* (1) are invoked inside a job script as they also
process the STDIN stream of the command interpreter. These problems can
usually be resolved by redirecting the STDIN channel of those commands
to come from /dev/null (e.g. rsh host date \< /dev/null). **Note also,
that any** command-line options associated with the job are passed to
the executing shell. The shell will only forward them to the job if they
are not recognized as valid shell options.

Changes to **shell_start_mode will take immediate effect.** The default
for **shell_start_mode is ***posix_compliant***.**

This value is a global configuration parameter only. It cannot be
overwritten by the execution host local configuration.

## **login_shells**

UNIX command interpreters like the Bourne-Shell (see *sh* (1)) or the
C-Shell (see *csh* (1)) can be used by xxQS_NAMExx to start job scripts.
The command interpreters can either be started as login-shells (i.e. all
system and user default resource files like .login or .profile will be
executed when the command interpreter is started and the environment for
the job will be set up as if the user has just logged in) or just for
command execution (i.e. only shell specific resource files like .cshrc
will be executed and a minimal default environment is set up by
xxQS_NAMExx - see *qsub* (1)). The parameter **login_shells contains a
comma separated list of the** executable names of the command
interpreters to be started as login-shells. Shells in this list are only
started as login shells if the parameter **shell_start_mode (see above)
is set to ***posix_compliant***.**

Changes to **login_shells will take immediate effect.** The default for
**login_shells is sh,bash,csh,tcsh,ksh.**

This value is a global configuration parameter only. It cannot be
overwritten by the execution host local configuration.

## **min_uid**

**min_uid places a lower bound on user IDs that may use the cluster.
Users** whose user ID (as returned by *getpwnam* (3)) is less than
**min_uid will not be allowed to run jobs on the cluster.**

Changes to **min_uid will take immediate effect.** The default for
**min_uid is 0.**

This value is a global configuration parameter only. It cannot be
overwritten by the execution host local configuration.

## **min_gid**

This parameter sets the lower bound on group IDs that may use the
cluster. Users whose default group ID (as returned by *getpwnam* (3)) is
less than **min_gid will not be allowed to run jobs on the cluster.**

Changes to **min_gid will take immediate effect.** The default for
**min_gid is 0.**

This value is a global configuration parameter only. It cannot be
overwritten by the execution host local configuration.

## **user_lists **

The **user_lists parameter contains a comma separated list of ** user
access lists as described in *access_list* (5). Each user contained in
at least one of the enlisted access lists has access to the cluster. If
the **user_lists parameter is set to** NONE (the default) any user has
access not explicitly excluded via the **xuser_lists parameter described
below.** If a user is contained both in an access list enlisted in
**xuser_lists** and **user_lists the user is denied access to the
cluster.**

Changes to **user_lists will take immediate effect**

This value is a global configuration parameter only. It cannot be
overwritten by the execution host local configuration.

## **xuser_lists **

The **xuser_lists parameter contains a comma separated list of ** user
access lists as described in *access_list* (5). Each user contained in
at least one of the enlisted access lists is denied access to the
cluster. If the **xuser_lists parameter is set to** NONE (the default)
any user has access. If a user is contained both in an access list
enlisted in **xuser_lists** and **user_lists (see above) the user is
denied access to the cluster.**

Changes to **xuser_lists will take immediate effect**

This value is a global configuration parameter only. It cannot be
overwritten by the execution host local configuration.

## **administrator_mail**

**administrator_mail specifies a comma separated list of the**
electronic mail address(es) of the cluster administrator(s) to whom
internally-generated problem reports are sent. The mail address format
depends on your electronic mail system and how it is configured; consult
your system's configuration guide for more information.

Changing **administrator_mail takes immediate effect.** The default for
**administrator_mail is an empty mail list.**

This value is a global configuration parameter only. It cannot be
overwritten by the execution host local configuration.

## **projects**

The **projects list contains all projects which are granted access** to
xxQS_NAMExx. User belonging to none of these projects cannot use
xxQS_NAMExx. If users belong to projects in the **projects list and
the** **xprojects list (see below), they also cannot use the system.**

Changing **projects takes immediate effect.** The default for **projects
is none.**

This value is a global configuration parameter only. It cannot be
overwritten by the execution host local configuration.

## **xprojects**

The **xprojects list contains all projects that are denied access ** to
xxQS_NAMExx. User belonging to one of these projects cannot use
xxQS_NAMExx. If users belong to projects in the **projects list (see
above) and the** **xprojects list, they also cannot use the system.**

Changing **xprojects takes immediate effect.** The default for
**xprojects is none.**

This value is a global configuration parameter only. It cannot be
overwritten by the execution host local configuration.

## **load_report_time**

System load is reported periodically by the execution daemons to
*xxqs_name_sxx_qmaster* (8). The parameter **load_report_time defines
the time interval between load** reports.

Each *xxqs_name_sxx_execd* (8) may use a different load report time.
Changing **load_report_time will** take immediate effect.

**Note: Be careful** when modifying **load_report_time. Reporting load
too frequently** might block *xxqs_name_sxx_qmaster* (8) especially if
the number of execution hosts is large. Moreover, since the system load
typically increases and decreases smoothly, frequent load reports hardly
offer any benefit.

The default for **load_report_time is 40 seconds.**

The global configuration entry for this value may be overwritten by the
execution host local configuration.

## **reschedule_unknown**

Determines whether jobs on hosts in unknown state are rescheduled and
thus sent to other hosts. Hosts are registered as unknown if
*xxqs_name_sxx_master* (8) cannot establish contact to the
*xxqs_name_sxx_execd* (8) on those hosts (see **max_unheard** ). Likely
reasons are a breakdown of the host or a breakdown of the network
connection in between, but also *xxqs_name_sxx_execd* (8) may not be
executing on such hosts.

In any case, xxQS_NAMExx can reschedule jobs running on such hosts to
another system. **reschedule_unknown** controls the time which
xxQS_NAMExx will wait before jobs are rescheduled after a host became
unknown. The time format specification is hh:mm:ss. If the special value
00:00:00 is set, then jobs will not be rescheduled from this host.

Rescheduling is only initiated for jobs which have activated the rerun
flag (see the **-r y** option of *qsub* (1) and the **rerun** option of
*queue_conf* (5)). Parallel jobs are only rescheduled if the host on
which their master task executes is in unknown state. The behavior of
**reschedule_unknown** for parallel jobs and for jobs without the rerun
flag be set can be adjusted using the **qmaster_params settings**
**ENABLE_RESCHEDULE_KILL** and **ENABLE_RESCHEDULE_SLAVE.**

Checkpointing jobs will only be rescheduled when the **when** option of
the corresponding checkpointing environment contains an appropriate
flag. (see *checkpoint* (5)). Interactive jobs (see *qsh* (1),
*qrsh* (1), are not rescheduled.

The default for **reschedule_unknown** is 00:00:00

The global configuration entry for this value may be over written by the
execution host local configuration.

## **max_unheard**

If *xxqs_name_sxx_qmaster* (8) could not contact or was not contacted by
the execution daemon of a host for **max_unheard seconds, all queues
residing on that particular host** are set to status unknown.
*xxqs_name_sxx_qmaster* (8), at least, should be contacted by the
execution daemons in order to get the load reports. Thus, **max_unheard
should by greater than the** **load_report_time (see above).**

Changing **max_unheard takes immediate effect.** The default for
**max_unheard is 5 minutes.**

This value is a global configuration parameter only. It cannot be
overwritten by the execution host local configuration.

## **loglevel**

This parameter specifies the level of detail that xxQS_NAMExx components
such as *xxqs_name_sxx_qmaster* (8) or *xxqs_name_sxx_execd* (8) use to
produce informative, warning or error messages which are logged to the
*messages*** files in the master and execution daemon** spool
directories (see the description of the **execd_spool_dir parameter
above). The following message** levels are available:

log_err  
All error events being recognized are logged.

log_warning  
All error events being recognized and all detected signs of potentially
erroneous behavior are logged.

log_info  
All error events being recognized, all detected signs of potentially
erroneous behavior and a variety of informative messages are logged.

Changing **loglevel will take immediate effect.**

The default for **loglevel is ***log_warning***.**

This value is a global configuration parameter only. It cannot be
overwritten by the execution host local configuration.

## **max_aj_instances**

This parameter defines the maximum amount of array task to be scheduled
to run simultaneously per array job. An instance of an array task will
be created within the master daemon when it gets a start order from the
scheduler. The instance will be destroyed when the array task finishes.
Thus the parameter provides control mainly over the memory consumption
of array jobs in the master and scheduler daemon. It is most useful for
very large clusters and very large array jobs. The default for this
parameter is 2000. The value 0 will deactivate this limit and will allow
the scheduler to start as many array job tasks as suitable resources are
available in the cluster.

Changing **max_aj_instances will take immediate effect.**

This value is a global configuration parameter only. It cannot be
overwritten by the execution host local configuration.

## **max_aj_tasks**

This parameter defines the maximum number of array job tasks within an
array job. *xxqs_name_sxx_qmaster* (8) will reject all array job
submissions which request more than **max_aj_tasks** array job tasks.
The default for this parameter is 75000. The value 0 will deactivate
this limit.

Changing **max_aj_tasks will take immediate effect.**

This value is a global configuration parameter only. It cannot be
overwritten by the execution host local configuration.

## **max_u\_jobs**

The number of active (not finished) jobs which each xxQS_NAMExx user can
have in the system simultaneously is controlled by this parameter. A
value greater than 0 defines the limit. The default value 0 means
"unlimited". If the **max_u\_jobs** limit is exceeded by a job
submission then the submission command exits with exit status 25 and an
appropriate error message.

Changing **max_u\_jobs will take immediate effect.**

This value is a global configuration parameter only. It cannot be
overwritten by the execution host local configuration.

## **max_jobs**

The number of active (not finished) jobs simultaneously allowed in
xxQS_NAMExx is controlled by this parameter. A value greater than 0
defines the limit. The default value 0 means "unlimited". If the
**max_jobs** limit is exceeded by a job submission then the submission
command exits with exit status 25 and an appropriate error message.

Changing **max_jobs will take immediate effect.**

This value is a global configuration parameter only. It cannot be
overwritten by the execution host local configuration.

## **max_advance_reservations**

The number of active (not finished) Advance Reservations simultaneously
allowed in xxQS_NAMExx is controlled by this parameter. A value greater
than 0 defines the limit. The default value 0 means "unlimited". If the
**max_advance_reservations** limit is exceeded by an Advance Reservation
request then the submission command exits with exit status 25 and an
appropriate error message.

Changing **max_advance_reservations will take immediate effect.**

This value is a global configuration parameter only. It cannot be
overwritten by the execution host local configuration.

## **enforce_project**

If set to *true***, users are required to request a project whenever**
submitting a job. See the **-P option to** *qsub* (1) for details.

Changing **enforce_project will take immediate effect.** The default for
**enforce_project is ***false***.**

This value is a global configuration parameter only. It cannot be
overwritten by the execution host local configuration.

## **enforce_user**

If set to *true***, a** *user* (5) must exist to allow for job
submission. Jobs are rejected if no corresponding user exists.

If set to *auto***, a** *user* (5) object for the submitting user will
automatically be created during job submission, if one does not already
exist. The **auto_user_oticket,** **auto_user_fshare,
auto_user_default_project, and** **auto_user_delete_time configuration
parameters will be used as** default attributes of the new *user* (5)
object.

Changing **enforce_user will take immediate effect.** The default for
**enforce_user is ***auto***.**

This value is a global configuration parameter only. It cannot be
overwritten by the execution host local configuration.

## **auto_user_oticket**

The number of override tickets to assign to automatically created
*user* (5) objects. User objects are created automatically if the
**enforce_user** attribute is set to *auto***.**

Changing **auto_user_oticket** will affect any newly created user
objects, but will not change user objects created in the past.

This value is a global configuration parameter only. It cannot be
overwritten by the execution host local configuration.

## **auto_user_fshare**

The number of functional shares to assign to automatically created
*user* (5) objects. User objects are created automatically if the
**enforce_user** attribute is set to *auto***.**

Changing **auto_user_fshare** will affect any newly created user
objects, but will not change user objects created in the past.

This value is a global configuration parameter only. It cannot be
overwritten by the execution host local configuration.

## **auto_user_default_project**

The default project to assign to automatically created *user* (5)
objects. User objects are created automatically if the **enforce_user**
attribute is set to *auto***.**

Changing **auto_user_default_project** will affect any newly created
user objects, but will not change user objects created in the past.

This value is a global configuration parameter only. It cannot be
overwritten by the execution host local configuration.

## **auto_user_delete_time**

The number of seconds of inactivity after which automatically created
*user* (5) objects will be deleted. User objects are created
automatically if the **enforce_user** attribute is set to *auto***. If
the user has no active or pending** jobs for the specified amount of
time, the object will automatically be deleted. A value of 0 can be used
to indicate that the automatically created user object is permanent and
should not be automatically deleted.

Changing **auto_user_delete_time** will affect the deletion time for all
users with active jobs.

This value is a global configuration parameter only. It cannot be
overwritten by the execution host local configuration.

## **set_token_cmd**

**Note:** Deprecated, may be removed in future release.  
This parameter is only present if your xxQS_NAMExx system is licensed to
support AFS.

**Set_token_cmd points to a command which sets and extends AFS** tokens
for xxQS_NAMExx jobs. In the standard xxQS_NAMExx AFS distribution, it
is supplied as a script which expects two command line parameters. It
reads the token from STDIN, extends the token's expiration time and sets
the token:

>     <set_token_cmd> <user> <token_extend_after_seconds>

As a shell script this command will call the programs:

>     - SetToken
>     - forge

which are provided by your distributor as source code. The script looks
as follows:

>     --------------------------------
>     #!/bin/sh
>     # set_token_cmd
>     forge -u $1 -t $2 | SetToken
>     --------------------------------

Since it is necessary for *forge*** to read the secret AFS server** key,
a site might wish to replace the **set_token_cmd script by a** command,
which connects to a custom daemon at the AFS server. The token must be
forged at the AFS server and returned to the local machine, where
*SetToken*** is executed.**

Changing **set_token_cmd will take immediate effect.** The default for
**set_token_cmd is none.**

The global configuration entry for this value may be overwritten by the
execution host local configuration.

## **pag_cmd**

**Note:** Deprecated, may be removed in future release.  
This parameter is only present if your xxQS_NAMExx system is licensed to
support AFS.

The path to your *pagsh*** is specified via this parameter.** The
*xxqs_name_sxx_shepherd* (8) process and the job run in a *pagsh***.
Please ask your AFS administrator** for details.

Changing **pag_cmd will take immediate effect.** The default for
**pag_cmd is none.**

The global configuration entry for this value may be overwritten by the
execution host local configuration.

## **token_extend_time**

**Note:** Deprecated, may be removed in future release.  
This parameter is only present if your xxQS_NAMExx system is licensed to
support AFS.

The **token_extend_time is the time period for which AFS tokens are
periodically ** extended. xxQS_NAMExx will call the token extension 30
minutes before the tokens expire until jobs have finished and the
corresponding tokens are no longer required.

Changing **token_extend_time will take immediate effect.** The default
for **token_extend_time is 24:0:0, i.e. 24 hours.**

The global configuration entry for this value may be overwritten by the
execution host local configuration.

## **shepherd_cmd**

  
Alternative path to the **shepherd_cmd binary. Typically used to call**
the shepherd binary by a wrapper script or command.

Changing **shepherd_cmd will take immediate effect. The default** for
**shepherd_cmd is none.**

The global configuration entry for this value may be overwritten by the
execution host local configuration.

## **gid_range**

The **gid_range** is a comma separated list of range expressions of the
form n-m (n as well as m are integer numbers greater than 99), where m
is an abbreviation for m-m. These numbers are used in
*xxqs_name_sxx_execd* (8) to identify processes belonging to the same
job.

Each *xxqs_name_sxx_execd* (8) may use a separate set up group ids for
this purpose. All number in the group id range have to be unused
supplementary group ids on the system, where the
*xxqs_name_sxx_execd* (8) is started.

Changing **gid_range will take immediate effect.** There is no default
for **gid_range. The administrator will have to** assign a value for
**gid_range during installation of xxQS_NAMExx.**

The global configuration entry for this value may be overwritten by the
execution host local configuration.

## **qmaster_params**

A list of additional parameters can be passed to the xxQS_NAMExx
qmaster. The following values are recognized:

ENABLE_ENFORCE_MASTER_LIMIT  
If this parameter is set then the **s_rt, h_rt limit of a running job **
are tested and executed by the *xxqs_name_sxx_qmaster* (8) when the
*xxqs_name_sxx_execd* (8) where the job is in unknown state.

After **s_rt or h_rt limit of a job is expired then the master daemon
will ** wait additional time defined by **DURATION_OFFSET (see **
*xxqs_name_sxx_sched_conf* (5)). If the execution daemon still cannot be contacted
when this additional time is elapsed, then the master daemon will force
the deletion of the job (see **-f of ** *qdel* (1)).

For jobs which will be deleted that way an accounting record will be
created. As usage the record will contain the last reported online
usage, when the execution daemon could contact qmaster. The **failed
state in the record ** will be set to 37 to indicate that the job was
terminated by a limit enforcement of master daemon.

After the restart of *xxqs_name_sxx_qmaster* (8) the limit enforcement
will at first be triggered after the double of the biggest
**load_report_interval interval defined in ** *sge_conf* (5) has been
elapsed. This will give the execution daemons enough time to reregister
at master daemon.

ENABLE_FORCED_QDEL_IF_UNKNOWN  
If this parameter is set then a deletion request for a job is
automatically interpreted as a forced deletion request (see **-f of **
*qdel* (1)) if the host, where the job is running is in unknown state.

ENABLE_FORCED_QDEL  
If this parameter is set, non-administrative users can force deletion of
their own jobs via the *-f*** option of** *qdel* (1). Without this
parameter, forced deletion of jobs is only allowed by the xxQS_NAMExx
manager or operator.

**Note: Forced deletion for jobs is executed differently depending** on
whether users are xxQS_NAMExx administrators or not. In case of
administrative users, the jobs are removed from the internal database of
xxQS_NAMExx immediately. For regular users, the equivalent of a normal
*qdel* (1) is executed first, and deletion is forced only if the normal
cancellation was unsuccessful.

FORBID_RESCHEDULE  
If this parameter is set, re-queuing of jobs cannot be initiated by the
job script which is under control of the user. Without this parameter
jobs returning the value 99 are rescheduled. This can be used to cause
the job to be restarted at a different machine, for instance if there
are not enough resources on the current one.

<!-- -->

FORBID_APPERROR  
If this parameter is set, the application cannot set itself to error
state. Without this parameter jobs returning the value 100 are set to
error state (and therefore can be manually rescheduled by clearing the
error state). This can be used to set the job to error state when a
starting condition of the application is not fulfilled before the
application itself has been started, or when a clean up procedure (e.g.
in the epilog) decides that it is necessary to run the job again, by
returning 100 in the prolog, pe_start, job script, pe_stop or epilog
script.

<!-- -->

DISABLE_AUTO_RESCHEDULING  
**Note:** Deprecated, may be removed in future release.  
If set to "true" or "1", the *reschedule_unknown*** parameter** is not
taken into account.

<!-- -->

ENABLE_RESCHEDULE_KILL  
If set to "true" or "1", the *reschedule_unknown*** parameter** affects
also jobs which have the rerun flag not activated (see the **-r y**
option of *qsub* (1) and the **rerun** option of *queue_conf* (5)), but
they are just finished as they can't be rescheduled.

<!-- -->

ENABLE_RESCHEDULE_SLAVE  
If set to "true" or "1" xxQS_NAMExx triggers job rescheduling also when
the host where the slave tasks of a parallel job executes is in unknown
state, if the *reschedule_unknown*** parameter is activated.**

<!-- -->

MAX_DYN_EC  
Sets the max number of dynamic event clients (as used by qsub -sync y
and by xxQS_NAMExx DRMAA API library sessions). The default is set
to 1000. The number of dynamic event clients should not be bigger than
half of the number of file descriptors the system has. The number of
file descriptors are shared among the connections to all exec hosts, all
event clients, and file handles that the qmaster needs.

<!-- -->

MONITOR_TIME  
Specifies the time interval when the monitoring information should be
printed. The monitoring is disabled by default and can be enabled by
specifying an interval. The monitoring is per thread and is written to
the messages file or displayed by the "qping -f" command line tool.
Example: MONITOR_TIME=0:0:10 generates and prints the monitoring
information approximately every 10 seconds. The specified time is a
guideline only and not a fixed interval. The interval that is actually
used is printed. In this example, the interval could be anything between
9 seconds and 20 seconds.

<!-- -->

LOG_MONITOR_MESSAGE  
Monitoring information is logged into the messages files by default.
This information can be accessed via by *qping* (1). If monitoring is
always enabled, the messages files can become quite large. This switch
disables logging into the messages files, making *qping -f* the only
source of monitoring data.

<!-- -->

PROF_SIGNAL  
Profiling provides the user with the possibility to get system
measurements. This can be useful for debugging or optimization of the
system. The profiling output will be done within the messages file.

Enables the profiling for qmaster signal thread. (e.g. PROF_SIGNAL=true)

PROF_WORKER  
Enables the profiling for qmaster worker threads. (e.g.
PROF_WORKER=true)

<!-- -->

PROF_LISTENER  
Enables the profiling for qmaster listener threads. (e.g.
PROF_LISTENER=true)

<!-- -->

PROF_DELIVER  
Enables the profiling for qmaster event deliver thread. (e.g.
PROF_DELIVER=true)

<!-- -->

PROF_TEVENT  
Enables the profiling for qmaster timed event thread. (e.g.
PROF_TEVENT=true)

Please note, that the cpu utime and stime values contained in the
profiling output are not per thread cpu times. These cpu usage
statistics are per process statistics. So the printed profiling values
for cpu mean "cpu time consumed by sge_qmaster (all threads) while the
reported profiling level was active".

STREE_SPOOL_INTERVAL  
Sets the time interval for spooling the sharetree usage. The default is
set to 00:04:00. The setting accepts colon-separated string or seconds.
There is no setting to turn the sharetree spooling off. (e.g.
STREE_SPOOL_INTERVAL=00:02:00)

<!-- -->

"MAX_JOB_DELETION_TIME  
Sets the value of how long the qmaster will spend deleting jobs. After
this time, the qmaster will continue with other tasks and schedule the
deletion of remaining jobs at a later time. The default value is 3
seconds, and will be used if no value is entered. The range of valid
values is \> 0 and \<= 5. (e.g. MAX_JOB_DELETION_TIME=1)

<!-- -->

"gdi_timeout  
Sets how long the communication will wait for gdi send/receive
operations. The default value is set to 60 seconds. After this time, the
communication library will retry, if "gdi_retries" is configured,
receiving the gdi request. In case of not configured "gdi_retries" the
communication will return with a "gdi receive failure" (e.g.
gdi_timeout=120 will set the timeout time to 120 sec) Configuring no
gdi_timeout value, the value defaults to 60 sec.

<!-- -->

"gdi_retries  
Sets how often the gdi receive call will be repeated until the gdi
receive error appears. The default is set to 0. In this case the call
will be done 1 time with no retry. Setting the value to -1 the call will
be done permanently. In combination with gdi_timeout parameter it is
possible to configure a system with eg. slow NFS, to make sure that all
jobs will be submitted. (e.g. gdi_retries=4)

<!-- -->

"cl_ping  
Turns on/off a communication library ping. This parameter will create
additional debug output. This output shows information about the error
messages which are returned by communication and it will give
information about the application status of the qmaster. eg, if it's
unclear what's the reason for gdi timeouts, this may show you some
useful messages. The default value is false (off) (e.g cl_ping=false)

<!-- -->

"SCHEDULER_TIMEOUT  
Setting this parameter allows the scheduler GDI event acknowledge
timeout to be manually configured to a specific value. Currently the
default value is 10 minutes with the default scheduler configuration and
limited between 600 and 1200 seconds. Value is limited only in case of
default value. The default value depends on the current scheduler
configuration. The *SCHEDULER_TIMEOUT* value is specified in seconds.

<!-- -->

"jsv_timeout  
This parameter measures the response time of the server JSV. In the
event that the response time of the JSV is longer than the timeout value
specified, this will cause the JSV to be re-started. The default value
for the timeout is 10 seconds and if modified, must be greater than 0.
If the timeout has been reach, the JSV will only try to re-start once,
if the timeout is reached again an error will occur.

<!-- -->

"jsv_threshold  
The threshold of a JSV is measured as the time it takes to perform a
server job verification. If this value is greater than the user defined
value, it will cause logging to appear in the qmaster messages file at
the INFO level. By setting this value to 0, all jobs will be logged in
the qmaster messages file. This value is specified in milliseconds and
has a default value of 5000.

"OLD_RESCHEDULE_BEHAVIOR  
Beginning with version 8.0.0 of Univa Grid Engine the scheduling
behavior changed for jobs that are rescheduled by users. Rescheduled
jobs will not be put at the beginning of the pending job list anymore.
The submit time of those jobs is set to the end time of the previous
run. Due to that those rescheduled jobs will be appended at the end of
the pending job list as if a new job would have been submitted. To
achive the old behaviour the paramter *OLD_RESCHEDULE_BEHAVIOR* has to
be set. Please note that this parameter is declared as deprecated. So it
might be removed with next minor release.

"OLD_RESCHEDULE_BEHAVIOR_ARRAY_JOB  
Beginning with version 8.0.0 of Univa Grid Engine the scheduling
behavior changed for array job tasks that are rescheduled by users. As
soon as a array job task gets scheduled all remaining pending tasks of
that job will be put at the end of the pending job list. To achive the
old scheduling behavior the paramter *OLD_RESCHEDULE_BEHAVIOR_ARRAY_JOB*
has to be set. Please note that this parameter is declared as
deprecated. So it might be removed with next minor release.

<!-- -->

"ENABLE_SUBMIT_LIB_PATH  
Beginning with version 8.0.1p3 of Univa Grid Engine environment
variables like LD_PRELOAD, LD_LIBRARY_PATH and similar variables by
default may no longer be set via submit option -v or -V.

Setting these variables could be misused to execute malicious code from
user jobs, if the execution environment contained methods (e.g. prolog)
to be executed as the root user, or if the old interactive job support
(e.g. via ssh) was configured.

Should it be necessary to allow setting environment variables like
LD_LIBRARY_PATH via submit option -v or -V, this can be enabled again by
setting *ENABLE_SUBMIT_LIB_PATH* to TRUE.

In general the correct job environment should be set up in the job
script or in a prolog, making the use of the -v or -V option for this
purpose unnecessary.

Changing **qmaster_params** will take immediate effect, except
gdi_timeout, gdi_retries, cl_ping, these will take effect only for new
connections. The default for **qmaster_params** is none.

This value is a global configuration parameter only. It cannot be
overwritten by the execution host local configuration.

## **execd_params**

This is used for passing additional parameters to the xxQS_NAMExx
execution daemon. The following values are recognized:

ACCT_RESERVED_USAGE  
If this parameter is set to true, the usage of reserved resources is
used for the accounting entries **cpu**, **mem** and **io** instead of
the measured usage.

<!-- -->

ENABLE_WINDOMACC  
If this parameter is set to true, Windows Domain accounts (WinDomAcc)
are used on Windows hosts. These accounts require the use of
*sgepasswd* (1) (see also *sgepasswd* (5)). If this parameter is set to
false or is not set, local Windows accounts are used. On non-Windows
hosts, this parameter is ignored.

<!-- -->

IGNORE_NGROUPS_MAX_LIMIT  
If a user is assigned to NGROUPS_MAX-1 supplementary groups so that
xxQS_NAMExx is not able to add an addition one for job tracking then the
job will go into error state when it is started. Administrators that
want prevent the system doing so can set this parameter. In this case
the NGROUPS_MAX limit is ignored and the additional group (see
*gid_range*) is not set. As a result for those jobs no online usage will
be available. Also the parameter *ENABLE_ADDGRP_KILL* will have no
effect. Please note that it is not recommended to use this parameters.
Instead the group membership of the submit user should be reduced.

<!-- -->

KEEP_ACTIVE  
This value should only be set for debugging purposes. If set to true,
the execution daemon will not remove the spool directory maintained by
*xxqs_name_sxx_shepherd* (8) for a job.

<!-- -->

PTF_MIN_PRIORITY, PTF_MAX_PRIORITY  
The maximum/minimum priority which xxQS_NAMExx will assign to a job.
Typically this is a negative/positive value in the range of -20
(maximum) to 19 (minimum) for systems which allow setting of priorities
with the *nice* (2) system call. Other systems may provide different
ranges.  
The default priority range (varies from system to system) is installed
either by removing the parameters or by setting a value of -999.  
See the "messages" file of the execution daemon for the predefined
default value on your hosts. The values are logged during the startup of
the execution daemon.

<!-- -->

PROF_EXECD  
Enables the profiling for the execution daemon. (e.g. PROF_EXECD=true)

<!-- -->

NOTIFY_KILL  
The parameter allows you to change the notification signal for the
signal SIGKILL (see *-notify* option of *qsub* (1)). The parameter
either accepts signal names (use the *-l* option of *kill* (1)) or the
special value *none*. If set to *none*, no notification signal will be
sent. If it is set to *TERM*, for instance, or another signal name then
this signal will be sent as notification signal.

<!-- -->

NOTIFY_SUSP  
With this parameter it is possible to modify the notification signal for
the signal SIGSTOP (see *-notify* parameter of *qsub* (1)). The
parameter either accepts signal names (use the *-l* option of
*kill* (1)) or the special value *none*. If set to *none*, no
notification signal will be sent. If it is set to *TSTP*, for instance,
or another signal name then this signal will be sent as notification
signal.

<!-- -->

SHARETREE_RESERVED_USAGE  
**Note:** Deprecated, may be removed in future release.  
If this parameter is set to true, the usage of reserved resources is
taken for the xxQS_NAMExx share tree consumption instead of measured
usage.  
**Note:** When running tightly integrated jobs with
*SHARETREE_RESERVED_USAGE* set, and with having *accounting_summary*
enabled in the parallel environment, reserved usage will only be
reported by the master task of the parallel job. No per parallel task
usage records will be sent from execd to qmaster, which can
significantly reduce load on qmaster when running large tightly
integrated parallel jobs.

<!-- -->

USE_QSUB_GID  
If this parameter is set to true, the primary group id active when a job
was submitted will be set to become the primary group id for job
execution. If the parameter is not set, the primary group id as defined
for the job owner in the execution host *passwd* (5) file is used.  
The feature is only available for jobs submitted via *qsub* (1),
*qrsh* (1), and *qmake* (1) Also, it only works for *qrsh* (1) jobs and
*qmake* (1)) if rsh and rshd components are used which are provided with
xxQS_NAMExx (i.e., the **rsh_daemon** and **rsh_command** parameters may
not be changed from the default).

<!-- -->

S_DESCRIPTORS, H_DESCRIPTORS, S_MAXPROC, H_MAXPROC,  

S_MEMORYLOCKED, H_MEMORYLOCKED, S_LOCKS, H_LOCKS  
Specifies soft and hard resource limits as implemented by the
*setrlimit* (2) system call. See this manual page on your system for
more information. These parameters complete the list of limits set by
the RESOURCE LIMITS parameter of the queue configuration as described in
*queue_conf* (5). Unlike the resource limits in the queue configuration,
these resource limits are set for every job on this execution host. If a
value is not specified, the resource limit is inherited from the
execution daemon process. Because this would lead to unpredicted
results, if only one limit of a resource is set (soft or hard), the
corresponding other limit is set to the same value.  
*S_DESCRIPTORS* and *H_DESCRIPTORS* specify a value one greater than the
maximum file descriptor number that can be opened by any process of a
job.  
*S_MAXPROC* and *H_MAXPROC* specify the maximum number of processes that
can be created by the job user on this execution host  
*S_MEMORYLOCKED* and *H_MEMORYLOCKED* specify the maximum number of
bytes of virtual memory that may be locked into RAM.  
*S_LOCKS* and *H_LOCKS* specify the maximum number of file locks any
process of a job may establish.  
All of these values can be specified using the multiplier letters k, K,
m, M, g and G, see *sge_types* (1) for details.

INHERIT_ENV  
This parameter indicates whether the shepherd should allow the
environment inherited by the execution daemon from the shell that
started it to be inherited by the job it's starting. When true, any
environment variable that is set in the shell which starts the execution
daemon at the time the execution daemon is started will be set in the
environment of any jobs run by that execution daemon, unless the
environment variable is explicitly overridden, such as PATH or LOGNAME.
If set to false, each job starts with only the environment variables
that are explicitly passed on by the execution daemon, such as PATH and
LOGNAME. The default value is true.

<!-- -->

SET_LIB_PATH  
This parameter tells the execution daemon whether to add the xxQS_NAMExx
shared library directory to the library path of executed jobs. If set to
true, and INHERIT_ENV is also set to true, the xxQS_NAMExx shared
library directory will be prepended to the library path which is
inherited from the shell which started the execution daemon. If
INHERIT_ENV is set to false, the library path will contain only the
xxQS_NAMExx shared library directory. If set to false, and INHERIT_ENV
is set to true, the library path exported to the job will be the one
inherited from the shell which started the execution daemon. If
INHERIT_ENV is also set to false, the library path will be empty. After
the execution daemon has set the library path, it may be further altered
by the shell in which the job is executed, or by the job script itself.
The default value for SET_LIB_PATH is false.

<!-- -->

ENABLE_ADDGRP_KILL  
If this parameter is set then xxQS_NAMExx uses the supplementary group
ids (see *gid_range*) to identify all processes which are to be
terminated when a job is deleted, or when *xxqs_name_sxx_shepherd* (8)
cleans up after job termination.

<!-- -->

PDC_INTERVAL  
This parameter defines the interval how often the PDC (Portable Data
Collector) is executed by the execution daemon. The PDC is responsible
for enforcing the resource limits s_cpu, h_cpu, s_vmem and h_vmem (see
*xxqs_name_sxx_queue_conf* (5)) and job usage collection. The parameter can be set to
a time_specifier (see *sge_types* (5)) , to **PER_LOAD_REPORT** or to
**NEVER**. If this parameter is set to **PER_LOAD_REPORT** the PDC is
triggered in the same interval as **load_report_time** (see above). If
this parameter is set to **NEVER** the PDC run is never triggered. The
default is 1 second.  
**Note:** A PDC run is quite compute extensive may degrade the
performance of the running jobs. But if the PDC runs less often or never
the online usage can be incomplete or totally missing (for example
online usage of very short running jobs might be missing) and the
resource limit enforcement is less accurate or would not happen if PDC
is turned of completely.

<!-- -->

ENABLE_BINDING  
If this parameter is set then xxQS_NAMExx enables the core binding
module within the execution daemon to apply binding parameters that are
specified during submission time of a job. This parameter is not set per
default and therefore all binding related information will be ignored.
Find more information for job to core binding in the section *-binding*
of *qsub* (1).

Changing **execd_params** will take effect after it was propagated to
the execution daemons. The propagation is done in one load report
interval. The default for **execd_params** is none.

The global configuration entry for this value may be overwritten by the
execution host local configuration.

## **reporting_params**

Used to define the behavior of reporting modules in the xxQS_NAMExx
qmaster. Changes to the **reporting_params** takes immediate effect. The
following values are recognized:

accounting  
If this parameter is set to true, the accounting file is written. The
accounting file is prerequisite for using the **qacct** command.

<!-- -->

reporting  
If this parameter is set to true, the reporting file is written. The
reporting file contains data that can be used for monitoring and
analysis, like job accounting, job log, host load and consumables, queue
status and consumables and sharetree configuration and usage. Attention:
Depending on the size and load of the cluster, the reporting file can
become quite large. Only activate the reporting file if you have a
process running that will consume the reporting file! See
*reporting* (5) for further information about format and contents of the
reporting file.

<!-- -->

flush_time  
Contents of the reporting file are buffered in the xxQS_NAMExx qmaster
and flushed at a fixed interval. This interval can be configured with
the *flush_time* parameter. It is specified as a time value in the
format HH:MM:SS. Sensible values range from a few seconds to one minute.
Setting it too low may slow down the qmaster. Setting it too high will
make the qmaster consume large amounts of memory for buffering data.

<!-- -->

accounting_flush_time  
Contents of the accounting file are buffered in the xxQS_NAMExx qmaster
and flushed at a fixed interval. This interval can be configured with
the *accounting_flush_time* parameter. It is specified as a time value
in the format HH:MM:SS. Sensible values range from a few seconds to one
minute. Setting it too low may slow down the qmaster. Setting it too
high will make the qmaster consume large amounts of memory for buffering
data. Setting it to 00:00:00 will disable accounting data buffering; as
soon as data is generated, it will be written to the accounting file. If
this parameter is not set, the accounting data flush interval will
default to the value of the *flush_time* parameter.

<!-- -->

joblog  
If this parameter is set to true, the reporting file will contain job
logging information. See *reporting* (5) for more information about job
logging.

<!-- -->

sharelog  
The xxQS_NAMExx qmaster can dump information about sharetree
configuration and use to the reporting file. The parameter *sharelog*
sets an interval in which sharetree information will be dumped. It is
set in the format HH:MM:SS. A value of 00:00:00 configures qmaster not
to dump sharetree information. Intervals of several minutes up to hours
are sensible values for this parameter. See *reporting* (5) for further
information about sharelog.

<!-- -->

log_consumables  
This parameter controls writing of consumable resources to the reporting
file. When set to **log_consumables=true** information about all
consumable resources (their current usage and their capacity) will be
written to the reporting file, whenever a consumable resource changes
either in definition, or in capacity, or when the usage of an arbitrary
consumable resource changes. When **log_consumables is set to false
(default), only those variables will be written to the** reporting file,
that are configured in the **report_variables in the exec host
configuration** and whose definition or value actually changed. This
parameter is deprecated and will get removed in the next major release.
See *host_conf* (5) for further information about **report_variables.**

## **finished_jobs**

**Note:** Deprecated, may be removed in future release.  
xxQS_NAMExx stores a certain number of *just finished*** jobs to
provide** post mortem status information. The **finished_jobs parameter
defines the** number of finished jobs stored. If this maximum number is
reached, the eldest finished job will be discarded for every new job
added to the finished job list.

Changing **finished_jobs will take immediate effect.** The default for
**finished_jobs is 100.**

This value is a global configuration parameter only. It cannot be
overwritten by the execution host local configuration.

## **qlogin_daemon**

This parameter specifies the mechanism that is to be started on the
server side of a *qlogin* (1) request. Usually this is the builtin
mechanism. It's also possible to configure an external executable by
specifying the full qualified pathname, e.g. of the system's telnet
daemon.

Changing **qlogin_daemon will take immediate effect.** The default value
for **qlogin_daemon is builtin.**

The global configuration entry for this value may be overwritten by the
execution host local configuration.

Examples for the two allowed kinds of attributes are:  
qlogin_daemon builtin  
or  
qlogin_daemon /usr/sbin/in.telnetd

## **qlogin_command**

This is the command to be executed on the client side of a *qlogin* (1)
request. Usually this is the builtin qlogin mechanism. It's also
possible to configure an external mechanism, usually the absolute
pathname of the system's telnet client program. It is automatically
started with the target host and port number as parameters.

Changing **qlogin_command will take immediate effect.** The default
value for **qlogin_command is builtin.**

The global configuration entry for this value may be overwritten by the
execution host local configuration.

Examples for the two allowed kinds of attributes are:  
qlogin_command builtin  
or  
qlogin_command /usr/bin/telnetd

## **rlogin_daemon**

This parameter specifies the mechanism that is to be started on the
server side of a *qrsh* (1) request **without** a command argument to be
executed remotely. Usually this is the builtin mechanism. It's also
possible to configure an external executable by specifying the absolute
pathname, e.g. of the system's rlogin daemon.

Changing **rlogin_daemon** will take immediate effect. The default for
**rlogin_daemon** is builtin.

The global configuration entry for this value may be overwritten by the
execution host local configuration.

The allowed values are similar to the ones of the examples of
**qlogin_daemon.**

## **rlogin_command**

This is the mechanism to be executed on the client side of a *qrsh* (1)
request **without** a command argument to be executed remotely. Usually
this is the builtin mechanism. If no value is given, a specialized
xxQS_NAMExx component is used. The command is automatically started with
the target host and port number as parameters. The xxQS_NAMExx rlogin
client has been extended to accept and use the port number argument. You
can only use clients, such as *ssh***, which also understand this
syntax.**

Changing **rlogin_command** will take immediate effect. The default
value for **rlogin_command** is builtin.

The global configuration entry for this value may be overwritten by the
execution host local configuration.

In addition to the examples of **qlogin_command** , this value is
allowed:  
rsh_daemon none

## **rsh_daemon**

This parameter specifies the mechanism that is to be started on the
server side of a *qrsh* (1) request **with** a command argument to be
executed remotely. Usually this is the builtin mechanism. If no value is
given, a specialized xxQS_NAMExx component is used.

Changing **rsh_daemon** will take immediate effect. The default value
for **rsh_daemon** is builtin.

The global configuration entry for this value may be overwritten by the
execution host local configuration.

In addition to the examples of **qlogin_daemon** , this value is
allowed:  
rsh_daemon none

## **rsh_command**

This is the mechanism to be executed on the client side of a *qrsh* (1)
request **with** a command argument to be executed remotely. Usually
this is the builtin mechanism. If no value is given, a specialized
xxQS_NAMExx component is used. The command is automatically started with
the target host and port number as parameters like required for
*telnet* (1) plus the command with its arguments to be executed
remotely. The xxQS_NAMExx rsh client has been extended to accept and use
the port number argument. You can only use clients, such as *ssh***,
which also understand this syntax.**

Changing **rsh_command** will take immediate effect. The default value
for **rsh_command** is builtin.

The global configuration entry for this value may be overwritten by the
execution host local configuration.

In addition to the examples of **qlogin_command** , this value is
allowed:  
rsh_command none

## **delegated_file_staging**

This flag must be set to "true" when the prolog and epilog are ready for
delegated file staging, so that the DRMAA attribute
'drmaa_transfer_files' is supported. To establish delegated file
staging, use the variables beginning with "$fs\_..." in prolog and
epilog to move the input, output and error files from one host to the
other. When this flag is set to "false", no file staging is available
for the DRMAA interface. File staging is currently implemented only via
the DRMAA interface. When an error occurs while moving the input, output
and error files, return error code 100 so that the error handling
mechanism can handle the error correctly. (See also FORBID_APPERROR).

## **reprioritize**

**Note:** Deprecated, may be removed in future release.  
This flag enables or disables the reprioritization of jobs based on
their ticket amount. The **reprioritize_interval in ** *sched_conf* (5)
takes effect only if **reprioritize is set to true. To turn off ** job
reprioritization, the **reprioritize flag must be set to false ** and
the **reprioritize_interval to 0 which is the default.**

This value is a global configuration parameter only. It cannot be
overridden by the execution host local configuration.

## **jsv_url**

This setting defines a server JSV instance which will be started and
triggered by the *sge_qmaster* (8) process. This JSV instance will be
used to verify job specifications of jobs before they are accepted and
stored in the internal master database. The global configuration entry
for this value cannot be overwritten by execution host local
configurations.

Find more details concerning JSV in *jsv* (1) and *sge_request* (1).

The syntax of the **jsv_url is specified in ** *sge_types* (1).

## **jsv_allowed_mod**

If there is a server JSV script defined with **jsv_url parameter, then**
all *qalter* (1) or *qmon* (1) modification requests for jobs are
rejected by qmaster. With the **jsv_allowed_mod** parameter an
administrator has the possibility to allow a set of switches which can
then be used with clients to modify certain job attributes. The value
for this parameter has to be a comma separated list of JSV job parameter
names as they are documented in *qsub* (1) or the value **none to
indicate that no modification should be allowed. ** Please note that
even if **none is specified the switches -w and -t are** allowed for
qalter.

## **libjvm_path**

**libjvm_path is usually set during qmaster installation and points to
the absolute path of libjvm.so.** (or the corresponding library
depending on your architecture - e.g.
/usr/java/jre/lib/i386/server/libjvm.so) The referenced libjvm version
must be at least 1.5. It is needed by the JVM qmaster thread only. If
the Java VM needs additional starting parameters they can be set in
**additional_jvm_args. If the JVM thread is started at all can be
defined in the ** *bootstrap* (5) file. If libjvm_path is empty or an
incorrect path the JVM thread fails to start.

The global configuration entry for this value may be overwritten by the
execution host local configuration.

## **additional_jvm_args**

**additional_jvm_args is usually set during qmaster installation.**
Details about possible values **additional_jvm_args can be found in the
help output of the accompanying Java command. This setting is normally
not needed.**

The global configuration entry for this value may be overwritten by the
execution host local configuration.

# SEE ALSO

*xxqs_name_sxx_intro* (1), *csh* (1), *qconf* (1), *qsub* (1),
*xxqs_name_sxx_jsv* (1), *rsh* (1), *sh* (1), *getpwnam* (3), *drmaa_attributes* (3),
*xxqs_name_sxx_queue_conf* (5), *xxqs_name_sxx_sched_conf* (5), *sge_types* (1),
*xxqs_name_sxx_execd* (8), *xxqs_name_sxx_qmaster* (8),
*xxqs_name_sxx_shepherd* (8), *cron* (8), *xxQS_NAMExx Installation and
Administration Guide.*

# COPYRIGHT

See *xxqs_name_sxx_intro* (1) for a full statement of rights and
permissions.
