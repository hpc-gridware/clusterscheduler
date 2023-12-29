---
title: qconf
section: 1
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

qconf - xxQS_NAMExx Queue Configuration

# SYNTAX

**qconf options**

# DESCRIPTION

*Qconf* allows the system administrator to add, delete, and modify the
current xxQS_NAMExx configuration, including queue management, host
management, complex management and user management. *Qconf* also allows
you to examine the current queue configuration for existing queues.

*Qconf* allows the use of the backslash, '\\', character at the end of a
line to indicate that the next line is a continuation of the current
line. When displaying settings, such as the output of one of the -s\*
options, *qconf* will break up long lines (lines greater than 80
characters) into smaller lines using backslash line continuation where
possible. Lines will only be broken up at commas or whitespace. This
feature can be disabled by setting the SGE_SINGLE_LINE environment
variable.

# OPTIONS

Unless denoted otherwise, the following options and the corresponding
operations are available to all users with a valid account.

-Aattr obj_spec fname obj_instance,...  

*\<add to object attributes>*

  
Similar to **-aattr** (see below) but takes specifications for the
object attributes to be enhanced from file named **fname**. As opposed
to **-aattr**, multiple attributes can be enhanced. Their specification
has to be enlisted in **fname** following the file format of the
corresponding object (see *queue_conf* (5) for the queue, for
example).  
Requires root/manager privileges.

-Acal fname \<add calendar>  
Adds a new calendar definition to the xxQS_NAMExx environment. Calendars
are used in xxQS_NAMExx for defining availability and unavailability
schedules of queues. The format of a calendar definition is described in
*calendar_conf* (5).

The calendar definition is taken from the file **fname**. Requires root/
manager privileges.

-Ackpt fname \<add ckpt. environment>  
Add the checkpointing environment as defined in **fname** (see
*checkpoint* (5)) to the list of supported checkpointing environments.
Requires root or manager privileges.

-Aconf fname_list \<add configurations>  
Add the configurations (see *xxqs_name_sxx_conf* (5)) specified in the
files enlisted in the comma separated **fname_list**. The configuration
is added for the host that is identical to the file name.  
Requires root or manager privileges.

-Ae fname \<add execution host>  
Add the execution host defined in **fname** to the xxQS_NAMExx cluster.
The format of the execution host specification is described in
*host_conf* (5). Requires root or manager privileges.

-Ahgrp fname \<add host group configuration>  
Add the host group configuration defined in **fname**. The file format
of **fname** must comply to the format specified in *hostgroup* (5).
Requires root or manager privileges.

-Arqs fname \<add RQS configuration>  
Add the resource quota set (RQS) defined in the file named **fname** to
the xxQS_NAMExx cluster. Requires root or manager privileges.

-Ap fname \<add PE configuration>  
Add the parallel environment (PE) defined in **fname** to the
xxQS_NAMExx cluster. Requires root or manager privileges.

-Aprj fname \<add new project>  
Adds the project description defined in **fname** to the list of
registered projects (see *project* (5)). Requires root or manager
privileges.

-Aq fname \<add new queue>  
Add the queue defined in **fname** to the xxQS_NAMExx cluster. Requires
root or manager privileges.

-Au fname \<add an ACL>  
Add the user access list (ACL) defined in **fname** to xxQS_NAMExx. User
lists are used for queue usage authentication. Requires
root/manager/operator privileges.

-cb  
This parameter can be used since xxQS_NAMExx version 6.2u5 in
combination with the command line switch **-sep**. In that case the
output of the corresponding command will contain information about the
added job to core binding functionality.

If **-cb** switch is not used then **-sep** will behave as in GE version
6.2u4 and below.

Please note that this command-line switch will be removed from
xxQS_NAMExx with the next major release.

-Dattr obj_spec fname obj_instance,...  

*\<del. from object attribs>*

  
Similar to **-dattr** (see below) but the definition of the list
attributes from which entries are to be deleted is contained in the file
named **fname**. As opposed to **-dattr**, multiple attributes can be
modified. Their specification has to be enlisted in **fname** following
the file format of the corresponding object (see *queue_conf* (5) for
the queue, for example).  
Requires root/manager privileges.

-Mattr obj_spec fname obj_instance,...  

*\<mod. object attributes>*

  
Similar to **-mattr** (see below) but takes specifications for the
object attributes to be modified from file named **fname**. As opposed
to **-mattr**, multiple attributes can be modified. Their specification
has to be enlisted in **fname** following the file format of the
corresponding object (see *queue_conf* (5) for the queue, for
example).  
Requires root/manager privileges.

-Mc fname \<modify complex>  
Overwrites the complex configuration by the contents of **fname**. The
argument file must comply to the format specified in *complex* (5).
Requires root or manager privilege.

-Mcal fname \<modify calendar>  
Overwrites the calendar definition as specified in **fname**. The
argument file must comply to the format described in
*calendar_conf* (5). Requires root or manager privilege.

-Mckpt fname \<modify ckpt. environment>  
Overwrite an existing checkpointing environment with the definitions in
**fname** (see *checkpoint* (5)). The name attribute in **fname** has to
match an existing checkpointing environment. Requires root or manager
privileges.

-Mconf fname_list \<modify configurations>  
Modify the configurations (see *xxqs_name_sxx_conf* (5)) specified in
the files enlisted in the comma separated **fname_list**. The
configuration is modified for the host that is identical to the file
name.  
Requires root or manager privileges.

-Me fname \<modify execution host>  
Overwrites the execution host configuration for the specified host with
the contents of **fname**, which must comply to the format defines in
*host_conf* (5). Requires root or manager privilege.

-Mhgrp fname \<modify host group configuration>  
Allows changing of host group configuration with a single command. All
host group configuration entries contained in **fname** will be applied.
Configuration entries not contained in **fname** will be deleted. The
file format of **fname** must comply to the format specified in
*hostgroup* (5).

-Mrqs fname \[mrqs_name\] \<modify RQS configuration>  
Same as **-mrqs** (see below) but instead of invoking an editor to
modify the RQS configuration, the file **fname** is considered to
contain a changed configuration. The name of the rule set in **fname**
must be the same as rqs_name. If rqs_name is not set, all rule sets are
overwritten by the rule sets in **fname** Refer to
*xxqs_name_sxx_resource_quota* (5) for details on the RQS configuration
format. Requires root or manager privilege.

-Mp fname \<modify PE configuration>  
Same as **-mp** (see below) but instead of invoking an editor to modify
the PE configuration the file **fname** is considered to contain a
changed configuration. Refer to *xxqs_name_sxx_pe* (5) for details on
the PE configuration format. Requires root or manager privilege.

-Mprj fname \<modify project configuration>  
Same as **-mprj** (see below) but instead of invoking an editor to
modify the project configuration the file **fname** is considered to
contain a changed configuration. Refer to *project* (5) for details on
the project configuration format. Requires root or manager privilege.

-Mq fname \<modify queue configuration>  
Same as **-mq** (see below) but instead of invoking an editor to modify
the queue configuration the file **fname** is considered to contain a
changed configuration. Refer to *queue_conf* (5) for details on the
queue configuration format. Requires root or manager privilege.

-Msconf fname \<modify scheduler configuration from file>  
The current scheduler configuration (see *sched_conf* (5)) is overridden
with the configuration specified in the file. Requires root or manager
privilege.

-Mstree fname \<modify share tree>  
Modifies the definition of the share tree (see *share_tree* (5)). The
modified sharetree is read from file fname. Requires root or manager
privileges.

-Mu fname \<modify ACL>  
Takes the user access list (ACL) defined in **fname** to overwrite any
existing ACL with the same name. See *access_list* (5) for information
on the ACL configuration format. Requires root or manager privilege.

-Muser fname \<modify user>  
Modify the user defined in **fname** in the xxQS_NAMExx cluster. The
format of the user specification is described in *user* (5). Requires
root or manager privileges.

-Rattr obj_spec fname obj_instance,...  

*\<replace object attribs>*

  
Similar to **-rattr** (see below) but the definition of the list
attributes whose content is to be replace is contained in the file named
**fname**. As opposed to **-rattr**, multiple attributes can be
modified. Their specification has to be enlisted in **fname** following
the file format of the corresponding object (see *queue_conf* (5) for
the queue, for example).  
Requires root/manager privileges.

-aattr obj_spec attr_name val obj_instance,...  

*\<add to object attributes>*

  
Allows adding specifications to a single configuration list attribute in
multiple instances of an object with a single command. Currently
supported objects are the queue, the host, the host group, the parallel
environment, the resource quota sets and the checkpointing interface
configuration being specified as *queue* , *exechost* , *hostgroup* ,
*pe* , *rqs* or *ckpt* in **obj_spec**. For the obj_spec *queue* the
obj_instance can be a cluster queue name, a queue domain name or a queue
instance name. Find more information concerning different queue names in
*sge_types* (1). Depending on the type of the obj_instance this adds to
the cluster queues attribute sublist the cluster queues implicit default
configuration value or the queue domain configuration value or queue
instance configuration value. The queue **load_thresholds** parameter is
an example of a list attribute. With the **-aattr** option, entries can
be added to such lists, while they can be deleted with **-dattr**,
modified with **-mattr**, and replaced with **-rattr**.  
For the obj_spec *rqs* the obj_instance is a unique identifier for a
specific rule. The identifier consists of a rule-set name and either the
number of the rule in the list, or the name of the rule, separated by a
/  
The name of the configuration attribute to be enhanced is specified with
**attr_name** followed by **val** as a *name=value* pair. The comma
separated list of object instances (e.g., the list of queues) to which
the changes have to be applied are specified at the end of the
command.  
The following restriction applies: For the *exechost* object the
**load_values** attribute cannot be modified (see *host_conf* (5)).  
Requires root or manager privileges.

-acal calendar_name \<add calendar>  
Adds a new calendar definition to the xxQS_NAMExx environment. Calendars
are used in xxQS_NAMExx for defining availability and unavailability
schedules of queues. The format of a calendar definition is described in
*calendar_conf* (5).

With the calendar name given in the option argument *qconf* will open a
temporary file and start up the text editor indicated by the environment
variable EDITOR (default editor is *vi* (1) if EDITOR is not set). After
entering the calendar definition and closing the editor the new calendar
is checked and registered with *xxqs_name_sxx_qmaster* (8). Requires
root/manager privileges.

-ackpt ckpt_name \<add ckpt. environment>  
Adds a checkpointing environment under the name **ckpt_name** to the
list of checkpointing environments maintained by xxQS_NAMExx and to be
usable to submit checkpointing jobs (see *checkpoint* (5) for details on
the format of a checkpointing environment definition). *Qconf* retrieves
a default checkpointing environment configuration and executes *vi* (1)
(or $EDITOR if the EDITOR environment variable is set) to allow you to
customize the checkpointing environment configuration. Upon exit from
the editor, the checkpointing environment is registered with
*xxqs_name_sxx_qmaster* (8). Requires root/manager privileges.

-aconf host,... \<add configuration>  
Successively adds configurations (see *xxqs_name_sxx_conf* (5)) For the
hosts in the comma separated *fname_list*. For each host, an editor
($EDITOR indicated or *vi* (1)) is invoked and the configuration for the
host can be entered. The configuration is registered with
*xxqs_name_sxx_qmaster* (8) after saving the file and quitting the
editor.  
Requires root or manager privileges.

-ae \[host_template\] \<add execution host>  
Adds a host to the list of xxQS_NAMExx execution hosts. If a queue is
configured on a host this host is automatically added to the xxQS_NAMExx
execution host list. Adding execution hosts explicitly offers the
advantage to be able to specify parameters like load scale values with
the registration of the execution host. However, these parameters can be
modified (from their defaults) at any later time via the **-me** option
described below.  
If the **host_template** argument is present, *qconf* retrieves the
configuration of the specified execution host from
*xxqs_name_sxx_qmaster* (8) or a generic template otherwise. The
template is then stored in a file and *qconf* executes *vi* (1) (or the
editor indicated by $EDITOR if the EDITOR environment variable is set)
to change the entries in the file. The format of the execution host
specification is described in *host_conf* (5). When the changes are
saved in the editor and the editor is quit the new execution host is
registered with *xxqs_name_sxx_qmaster* (8). Requires root/manager
privileges.

-ah hostname,... \<add administrative host>  
Adds hosts **hostname** to the xxQS_NAMExx trusted host list (a host
must be in this list to execute administrative xxQS_NAMExx commands, the
sole exception to this being the execution of *qconf* on the
*xxqs_name_sxx_qmaster* (8) node). The default xxQS_NAMExx installation
procedures usually add all designated execution hosts (see the **-ae**
option above) to the xxQS_NAMExx trusted host list automatically.
Requires root or manager privileges.

-ahgrp group \<add host group configuration>  
Adds a new host group with the name specified in **group.** This command
invokes an editor (either *vi* (1) or the editor indicated by the EDITOR
environment variable). The new host group entry is registered after
changing the entry and exiting the editor. Requires root or manager
privileges.

-arqs \[rqs_name\] \<add new RQS>  
Adds one or more *Resource Quota Set* (RQS) description under the names
**rqs_name** to the list of RQSs maintained by xxQS_NAMExx (see
*xxqs_name_sxx_resource_quota* (5) for details on the format of a RQS
definition). *Qconf* retrieves a default RQS configuration and executes
*vi* (1) (or $EDITOR if the EDITOR environment variable is set) to allow
you to customize the RQS configuration. Upon exit from the editor, the
RQS is registered with *xxqs_name_sxx_qmaster* (8). Requires root or
manager privileges.

-am user,... \<add managers>  
Adds the indicated users to the xxQS_NAMExx manager list. Requires root
or manager privileges.

-ao user,... \<add operators>  
Adds the indicated users to the xxQS_NAMExx operator list. Requires root
or manager privileges.

-ap pe_name \<add new PE>  
Adds a *Parallel Environment* (PE) description under the name
**pe_name** to the list of PEs maintained by xxQS_NAMExx and to be
usable to submit parallel jobs (see *xxqs_name_sxx_pe* (5) for details
on the format of a PE definition). *Qconf* retrieves a default PE
configuration and executes *vi* (1) (or $EDITOR if the EDITOR
environment variable is set) to allow you to customize the PE
configuration. Upon exit from the editor, the PE is registered with
*xxqs_name_sxx_qmaster* (8). Requires root/manager privileges.

-at thread_name \<activates thread in master>  
Activates an additional thread in the master process. **thread_name**
might be either "scheduler" or "jvm". The corresponding thread is only
started when it is not already running. There might be only one
scheduler and only one JVM thread in the master process at the same
time.

-aprj \<add new project>  
Adds a project description to the list of registered projects (see
*project* (5)). *Qconf* retrieves a template project configuration and
executes *vi* (1) (or $EDITOR if the EDITOR environment variable is set)
to allow you to customize the new project. Upon exit from the editor,
the template is registered with *xxqs_name_sxx_qmaster* (8). Requires
root or manager privileges.

-aq \[queue_name\] \<add new queue>  
*Qconf* retrieves the default queue configuration (see *queue_conf* (5))
and executes *vi* (1) (or $EDITOR if the EDITOR environment variable is
set) to allow you to customize the queue configuration. Upon exit from
the editor, the queue is registered with *xxqs_name_sxx_qmaster* (8). A
minimal configuration requires only that the queue name and queue
hostlist be set. Requires root or manager privileges.

-as hostname,... \<add submit hosts>  
Add hosts **hostname** to the list of hosts allowed to submit
xxQS_NAMExx jobs and control their behavior only. Requires root or
manager privileges.

-astnode node_path=shares,... \<add share tree node>  
Adds the specified share tree node(s) to the share tree (see
*share_tree* (5)). The **node_path** is a hierarchical path
(**\[/\]node_name\[\[/.\]node_name...\]**) specifying the location of
the new node in the share tree. The base name of the node_path is the
name of the new node. The node is initialized to the number of specified
shares. Requires root or manager privileges.

-astree \<add share tree>  
Adds the definition of a share tree to the system (see
*share_tree* (5)). A template share tree is retrieved and an editor
(either *vi* (1) or the editor indicated by $EDITOR) is invoked for
modifying the share tree definition. Upon exiting the editor, the
modified data is registered with *xxqs_name_sxx_qmaster* (8). Requires
root or manager privileges.

-Astree fname \<add share tree>  
Adds the definition of a share tree to the system (see *share_tree* (5))
from the file fname. Requires root or manager privileges.

-au user,... acl_name,... \<add users to ACLs>  
Adds users to xxQS_NAMExx user access lists (ACLs). User lists are used
for queue usage authentication. Requires root/manager/operator
privileges.

-Auser fname \<add user>  
Add the user defined in **fname** to the xxQS_NAMExx cluster. The format
of the user specification is described in *user* (5). Requires root or
manager privileges.

-auser \<add user>  
Adds a user to the list of registered users (see *user* (5)). This
command invokes an editor (either *vi* (1) or the editor indicated by
the EDITOR environment variable) for a template user. The new user is
registered after changing the entry and exiting the editor. Requires
root or manager privileges.

-clearusage \<clear sharetree usage>  
Clears all user and project usage from the sharetree. All usage will be
initialized back to zero.

-cq wc_queue_list \<clean queue>  
Cleans queue from jobs which haven't been reaped. Primarily a
development tool. Requires root/manager/operator privileges. Find a
description of wc_queue_list in *sge_types* (1).

-dattr obj_spec attr_name val obj_instance,...  

*\<delete in object attribs>*

  
Allows deleting specifications in a single configuration list attribute
in multiple instances of an object with a single command. Find more
information concerning obj_spec and obj_instance in the description of
**-aattr**

-dcal calendar_name,... \<delete calendar>  
Deletes the specified calendar definition from xxQS_NAMExx. Requires
root/manager privileges.

-dckpt ckpt_name \<delete ckpt. environment>  
Deletes the specified checkpointing environment. Requires root/manager
privileges.

-dconf host,... \<delete local configuration>  
The local configuration entries for the specified hosts are deleted from
the configuration list. Requires root or manager privilege.

-de host_name,... \<delete execution host>  
Deletes hosts from the xxQS_NAMExx execution host list. Requires root or
manager privileges.

-dh host_name,... \<delete administrative host>  
Deletes hosts from the xxQS_NAMExx trusted host list. The host on which
*xxqs_name_sxx_qmaster* (8) is currently running cannot be removed from
the list of administrative hosts. Requires root or manager privileges.

-dhgrp group \<delete host group configuration>  
Deletes host group configuration with the name specified in **group.**
Requires root or manager privileges.

-drqs rqs_name_list \<delete RQS>  
Deletes the specified resource quota sets (RQS). Requires root or
manager privileges.

-dm user\[,user,...\] \<delete managers>  
Deletes managers from the manager list. Requires root or manager
privileges. It is not possible to delete the admin user or the user root
from the manager list.

-do user\[,user,...\] \<delete operators>  
Deletes operators from the operator list. Requires root or manager
privileges. It is not possible to delete the admin user or the user root
from the operator list.

-dp pe_name \<delete parallel environment>  
Deletes the specified parallel environment (PE). Requires root or
manager privileges.

-dprj project,... \<delete projects>  
Deletes the specified project(s). Requires root/manager privileges.

-dq queue_name,... \<delete queue>  
Removes the specified queue(s). Active jobs will be allowed to run to
completion. Requires root or manager privileges.

-ds host_name,... \<delete submit host>  
Deletes hosts from the xxQS_NAMExx submit host list. Requires root or
manager privileges.

-dstnode node_path,... \<delete share tree node>  
Deletes the specified share tree node(s). The **node_path** is a
hierarchical path (**\[/\]node_name\[\[/.\]node_name...\]**) specifying
the location of the node to be deleted in the share tree. Requires root
or manager privileges.

-dstree \<delete share tree>  
Deletes the current share tree. Requires root or manager privileges.

-du user,... acl_name,... \<delete users from ACL>  
Deletes one or more users from one or more xxQS_NAMExx user access lists
(ACLs). Requires root/manager/operator privileges.

-dul acl_name,... \<delete user lists>  
Deletes one or more user lists from the system. Requires
root/manager/operator privileges.

"-duser  
Deletes the specified user(s) from the list of registered users.
Requires root or manager privileges.

-help  
Prints a listing of all options.

-k{m\|s\|e\[j\] {host,...\|all}} \<shutdown xxQS_NAMExx>  
**Note:** The **-ks** switch is deprecated, may be removed in future
release. Please use the **-kt** switch instead.  
Used to shutdown xxQS_NAMExx components (daemons). In the form **-km**
*xxqs_name_sxx_qmaster* (8) is forced to terminate in a controlled
fashion. In the same way the **-ks** switch causes termination of the
scheduler thread. Shutdown of running *xxqs_name_sxx_execd* (8)
processes currently registered is initiated by the **-ke** option. If
**-kej** is specified instead, all jobs running on the execution hosts
are aborted prior to termination of the corresponding
*xxqs_name_sxx_execd* (8). The comma separated host list specifies the
execution hosts to be addressed by the **-ke** and **-kej** option. If
the keyword **all** is specified instead of a host list, all running
*xxqs_name_sxx_execd* (8) processes are shutdown. Job abortion,
initiated by the **-kej** option will result in **dr** state for all
running jobs until *xxqs_name_sxx_execd* (8) is running again.  
Requires root or manager privileges.

"-kt  
Terminates a thread in the master process. Currently it is only
supported to shutdown the "scheduler" and the "jvm" thread. The command
will only be successful if the corresponding thread is running.

-kec {id,...\|all} \<kill event client>  
Used to shutdown event clients registered at
*xxqs_name_sxx_qmaster* (8). The comma separated event client list
specifies the event clients to be addressed by the **-kec** option. If
the keyword **all** is specified instead of an event client list, all
running event clients except special clients like the scheduler thread
are terminated. Requires root or manager privilege.

-mattr obj_spec attr_name val obj_instance,...  

*\<modify object attributes>*

  
Allows changing a single configuration attribute in multiple instances
of an object with a single command. Find more information concerning
obj_spec and obj_instance in the description of **-aattr**

-mc \<modify complex>  
The complex configuration (see *complex* (5)) is retrieved, an editor is
executed (either *vi* (1) or the editor indicated by $EDITOR) and the
changed complex configuration is registered with
*xxqs_name_sxx_qmaster* (8) upon exit of the editor. Requires root or
manager privilege.

-mcal calendar_name \<modify calendar>  
The specified calendar definition (see *calendar_conf* (5)) is
retrieved, an editor is executed (either *vi* (1) or the editor
indicated by $EDITOR) and the changed calendar definition is registered
with *xxqs_name_sxx_qmaster* (8) upon exit of the editor. Requires root
or manager privilege.

-mckpt ckpt_name \<modify ckpt. environment>  
Retrieves the current configuration for the specified checkpointing
environment, executes an editor (either *vi* (1) or the editor indicated
by the EDITOR environment variable) and registers the new configuration
with the *xxqs_name_sxx_qmaster* (8). Refer to *checkpoint* (5) for
details on the checkpointing environment configuration format. Requires
root or manager privilege.

-mconf \[host,...\|global\] \<modify configuration>  
The configuration for the specified host is retrieved, an editor is
executed (either *vi* (1) or the editor indicated by $EDITOR) and the
changed configuration is registered with *xxqs_name_sxx_qmaster* (8)
upon exit of the editor. If the optional host argument is omitted or if
the special host name **global is specified, the** global configuration
is modified. The format of the configuration is described in
*xxqs_name_sxx_conf* (5).  
Requires root or manager privilege.

-me hostname \<modify execution host>  
Retrieves the current configuration for the specified execution host,
executes an editor (either *vi* (1) or the editor indicated by the
EDITOR environment variable) and registers the changed configuration
with *xxqs_name_sxx_qmaster* (8) upon exit from the editor. The format
of the execution host configuration is described in *host_conf* (5).
Requires root or manager privilege.

-mhgrp group \<modify host group configuration>  
The host group entries for the host group specified in **group** are
retrieved and an editor (either *vi* (1) or the editor indicated by the
EDITOR environment variable) is invoked for modifying the host group
configuration. By closing the editor, the modified data is registered.
The format of the host group configuration is described in
*hostgroup* (5). Requires root or manager privileges.

-mrqs \[rqs_name\] \<modify RQS configuration>  
Retrieves the resource quota set (RQS)configuration defined in rqs_name,
or if rqs_name is not given, retrieves all resource quota sets, executes
an editor (either *vi* (1) or the editor indicated by the EDITOR
environment variable) and registers the new configuration with the
*xxqs_name_sxx_qmaster* (8). Refer to *xxqs_name_sxx_resource_quota* (5)
for details on the RQS configuration format. Requires root or manager
privilege.

-mp pe_name \<modify PE configuration>  
Retrieves the current configuration for the specified *parallel
environment* (PE), executes an editor (either *vi* (1) or the editor
indicated by the EDITOR environment variable) and registers the new
configuration with the *xxqs_name_sxx_qmaster* (8). Refer to
*xxqs_name_sxx_pe* (5) for details on the PE configuration format.
Requires root or manager privilege.

-mprj project \<modify project>  
Data for the specific project is retrieved (see *project* (5)) and an
editor (either *vi* (1) or the editor indicated by $EDITOR) is invoked
for modifying the project definition. Upon exiting the editor, the
modified data is registered. Requires root or manager privileges.

-mq queuename \<modify queue configuration>  
Retrieves the current configuration for the specified queue, executes an
editor (either *vi* (1) or the editor indicated by the EDITOR
environment variable) and registers the new configuration with the
*xxqs_name_sxx_qmaster* (8). Refer to *queue_conf* (5) for details on
the queue configuration format. Requires root or manager privilege.

-msconf \<modify scheduler configuration>  
The current scheduler configuration (see *sched_conf* (5)) is retrieved,
an editor is executed (either *vi* (1) or the editor indicated by
$EDITOR) and the changed configuration is registered with
*xxqs_name_sxx_qmaster* (8) upon exit of the editor. Requires root or
manager privilege.

-mstnode node_path=shares,... \<modify share tree node>  
Modifies the specified share tree node(s) in the share tree (see
*share_tree* (5)). The **node_path is a hierarchical path**
(**\[/\]node_name\[\[/.\]node_name...\])** specifying the location of an
existing node in the share tree. The node is set to the number of
specified **shares.** Requires root or manager privileges.

-mstree \<modify share tree>  
Modifies the definition of the share tree (see *share_tree* (5)). The
present share tree is retrieved and an editor (either *vi* (1) or the
editor indicated by $EDITOR) is invoked for modifying the share tree
definition. Upon exiting the editor, the modified data is registered
with *xxqs_name_sxx_qmaster* (8). Requires root or manager privileges.

-mu acl_name \<modify user access lists>  
Retrieves the current configuration for the specified user access list,
executes an editor (either *vi* (1) or the editor indicated by the
EDITOR environment variable) and registers the new configuration with
the *xxqs_name_sxx_qmaster* (8). Requires root or manager privilege.

-muser user \<modify user>  
Data for the specific user is retrieved (see *user* (5)) and an editor
(either *vi* (1) or the editor indicated by the EDITOR environment
variable) is invoked for modifying the user definition. Upon exiting the
editor, the modified data is registered. Requires root or manager
privileges.

-purge queue attr_nm,... obj_spec  

*\<purge divergent attribute settings>*

  
Delete the values of the attributes defined in **attr_nm from the **
object defined in **obj_spec. Obj_spec can be "queue_instance"** or
"queue_domain". The names of the attributes are described in
*queue_conf* (1).  
This operation only works on a single queue instance or domain. It
cannot be used on a cluster queue. In the case where the **obj_spec is**
"queue@@hostgroup", the attribute values defined in **attr_nm which
are** set for the indicated hostgroup are deleted, but not those which
are set for the hosts contained by that hostgroup. If the **attr_nm** is
'\*', all attribute values set for the given queue instance or domain
are deleted.  
The main difference between -dattr and -purge is that -dattr removes a
value from a single list attribute, whereas -purge removes one or more
overriding attribute settings from a cluster queue configuration. With
-purge, the entire attribute is deleted for the given queue instance or
queue domain.

-rattr obj_spec attr_name val obj_instance,...  

*\<replace object attributes>*

  
Allows replacing a single configuration list attribute in multiple
instances of an object with a single command. Find more information
concerning obj_spec and obj_instance in the description of **-aattr
.**  
Requires root or manager privilege.

-rsstnode node_path,... \<show share tree node>  
Recursively shows the name and shares of the specified share tree
node(s) and the names and shares of its child nodes. (see
*share_tree* (5)). The **node_path is a hierarchical path**
(**\[/\]node_name\[\[/.\]node_name...\])** specifying the location of a
node in the share tree.

-sc \<show complexes>  
Display the complex configuration.

-scal calendar_name \<show calendar>  
Display the configuration of the specified calendar.

-scall \<show calendar list>  
Show a list of all calendars currently defined.

-sckpt ckpt_name \<show ckpt. environment>  
Display the configuration of the specified checkpointing environment.

-sckptl \<show ckpt. environment list>  
Show a list of the names of all checkpointing environments currently
configured.

-sconf \[host,...\|global\] \<show configuration>  
Print the global or local (host specific) configuration. If the optional
comma separated host list argument is omitted or the special string
**global is** given, the global configuration is displayed. The
configuration in effect on a certain host is the merger of the global
configuration and the host specific local configuration. The format of
the configuration is described in *xxqs_name_sxx_conf* (5).

-sconfl \<show configuration list>  
Display a list of hosts for which configurations are available. The
special host name **global refers to the** global configuration.

-sds \<show detached settings>  
Displays detached settings in the cluster configuration.

-se hostname \<show execution host>  
Displays the definition of the specified execution host.

-sel \<show execution hosts>  
Displays the xxQS_NAMExx execution host list.

-secl \<show event clients>  
Displays the xxQS_NAMExx event client list.

-sep \<show licensed processors>  
**Note:** Deprecated, may be removed in future release.

Displays a list of virtual processors. This value is taken from the
underlying OS and it depends on underlying hardware and operating system
whether this value represents sockets, cores or supported threads.

If this option is used in combination with **-cb parameter then two**
additional columns will be shown in the output for the number of sockets
and number of cores. Currently SGE will enlist these values only if the
corresponding operating system of execution host is Linux under kernel
\>= 2.6.16, or Solaris 10. Other operating systems or versions might be
supported with the future update releases. In case these values won't be
retrieved, '0' character will be displayed.

-sh \<show administrative hosts>  
Displays the xxQS_NAMExx administrative host list.

-shgrp group \<show host group config.>  
Displays the host group entries for the group specified in **group.**

-shgrpl \<show host group lists>  
Displays a name list of all currently defined host groups which have a
valid host group configuration.

-shgrp_tree group \<show host group tree>  
Shows a tree like structure of host group.

-shgrp_resolved group \<show host group hosts>  
Shows a list of all hosts which are part of the definition of host
group. If the host group definition contains sub host groups than also
these groups are resolved and the hostnames are printed.

-srqs \[rqs_name_list\] \<show RQS configuration>  
Show the definition of the *resource quota sets* (RQS) specified by the
argument.

-srqsl \<show RQS-list>  
Show a list of all currently defined *resource quota sets***s (RQSs).**

-sm \<show managers>  
Displays the managers list.

-so \<show operators>  
Displays the operator list.

-sobjl obj_spec attr_name val \<show object list>  
Shows a list of all configuration objects for which val matches at least
one configuration value of the attributes whose name matches with
attr_name.

Obj_spec can be "queue" or "queue_domain" or "queue_instance" or
"exechost". Note: When "queue_domain" or "queue_instance" is specified
as obj_spec matching is only done with the attribute overridings
concerning the host group or the execution host. In this case queue
domain names resp. queue instances are returned.

Attr_name can be any of the configuration file keywords enlisted in
*queue_conf* (5) or *host_conf* (5). Also wildcards can be used to match
multiple attributes.

Val can be an arbitrary string or a wildcard expression.

-sp pe_name \<show PE configuration>  
Show the definition of the *parallel environment* (PE) specified by the
argument.

-spl \<show PE-list>  
Show a list of all currently defined *parallel environment***s (PEs).**

-sprj project \<show project>  
Shows the definition of the specified project (see *project* (5)).

-sprjl \<show project list>  
Shows the list of all currently defined projects.

-sq wc_queue_list \<show queues>  
Displays one or multiple cluster queues or queue instances. A
description of wc_queue_list can be found in *sge_types* (1).

-sql \<show queue list>  
Show a list of all currently defined cluster queues.

-ss \<show submit hosts>  
Displays the xxQS_NAMExx submit host list.

-ssconf \<show scheduler configuration>  
Displays the current scheduler configuration in the format explained in
*sched_conf* (5).

-sstnode node_path,... \<show share tree node>  
Shows the name and shares of the specified share tree node(s) (see
*share_tree* (5)). The **node_path is a hierarchical path**
(**\[/\]node_name\[\[/.\]node_name...\])** specifying the location of a
node in the share tree.

-sstree \<show share tree>  
Shows the definition of the share tree (see *share_tree* (5)).

-sst \<show formatted share tree>  
Shows the definition of the share tree in a tree view (see
*share_tree* (5)).

-sss \<show scheduler status>  
Currently displays the host on which the xxQS_NAMExx scheduler is active
or an error message if no scheduler is running.

-su acl_name \<show user ACL>  
Displays a xxQS_NAMExx user access list (ACL).

-sul \<show user lists>  
Displays a list of names of all currently defined xxQS_NAMExx user
access lists (ACLs).

-suser user,... \<show user>  
Shows the definition of the specified user(s) (see *user* (5)).

-suserl \<show users>  
Shows the list of all currently defined users.

-tsm \<trigger scheduler monitoring>  
The xxQS_NAMExx scheduler is forced by this option to print trace
messages of its next scheduling run to the file
*\<xxqs_name_sxx_root>/\<cell>/common/schedd_runlog***.** The messages
indicate the reasons for jobs and queues not being selected in that run.
Requires root or manager privileges.

Note, that the reasons for job requirements being invalid with respect
to resource availability of queues are displayed using the format as
described for the *qstat* (1) **-F option (see description of ** **Full
Format in section OUTPUT FORMATS of the** *qstat* (1) manual page.

# ENVIRONMENTAL VARIABLES

xxQS_NAME_Sxx_ROOT  
Specifies the location of the xxQS_NAMExx standard configuration files.

xxQS_NAME_Sxx_CELL  
If set, specifies the default xxQS_NAMExx cell. To address a xxQS_NAMExx
cell *qconf* uses (in the order of precedence):

> The name of the cell specified in the environment variable
> xxQS_NAME_Sxx_CELL, if it is set.
>
> The name of the default cell, i.e. **default.**

xxQS_NAME_Sxx_DEBUG_LEVEL  
If set, specifies that debug information should be written to stderr. In
addition the level of detail in which debug information is generated is
defined.

xxQS_NAME_Sxx_QMASTER_PORT  
If set, specifies the tcp port on which *xxqs_name_sxx_qmaster* (8) is
expected to listen for communication requests. Most installations will
use a services map entry instead to define that port.

xxQS_NAME_Sxx_EXECD_PORT  
If set, specifies the tcp port on which *xxqs_name_sxx_execd* (8) is
expected to listen for communication requests. Most installations will
use a services map entry instead to define that port.

SGE_SINGLE_LINE  
If set, indicates that long lines should not be broken up using
backslashes. This setting is useful for scripts which expect one entry
per line.

# RESTRICTIONS

Modifications to a queue configuration do not affect an active queue,
taking effect on next invocation of the queue (i.e., the next job).

# FILES

    <xxqs_name_sxx_root>/<cell>/common/act_qmaster
    	xxQS_NAMExx master host file

# SEE ALSO

*xxqs_name_sxx_intro* (1), *qstat* (1), *checkpoint* (5), *complex* (5),
*xxqs_name_sxx_conf* (5), *host_conf* (5), *xxqs_name_sxx_pe* (5),
*queue_conf* (5), *xxqs_name_sxx_execd* (8),
*xxqs_name_sxx_qmaster* (8), *xxqs_name_sxx_resource_quota* (5)

# COPYRIGHT

See *xxqs_name_sxx_intro* (1) for a full statement of rights and
permissions.
