---
title: qconf
section: 1
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

`qconf` - xxQS_NAMExx Queue Configuration

# SYNTAX

`qconf` *\<options\>*

# DESCRIPTION

`Qconf` allows the system administrator to add, delete, and modify the current xxQS_NAMExx configuration, including 
queue management, host management, complex management and user management. `Qconf` also allows you to examine the 
current queue configuration for existing queues.

Some *\<options\>* of `qconf` start up an editor indicated by the environment variable *EDITOR*. If such a variable 
is not set then as default the vi(1) editor will be started.

`Qconf` allows the use of the backslash, '\\', character at the end of a line to indicate that the next line is a 
continuation of the current line. When displaying settings, such as the output of one of the `-s*` options, `qconf` 
will break up long lines (lines greater than 80 characters) into smaller lines using backslash line continuation 
where possible. Lines will only be broken up at commas or whitespace. This feature can be disabled by setting the 
*SGE_SINGLE_LINE* environment variable.

Operations of `qconf` (add, modify or delete) will not have an effect on ongoing decisions of the *scheduler* 
component in xxqs_name_sxx_qmaster(8). Changes will be effective with the next scheduling cycle. The same applies
for execution, configuration and queue settings which will be in effect for the next job that is
started on/in the corresponding object. 

Unless denoted otherwise, `qconf` *\<options\>* and the corresponding operations are available to all users
with a valid account but some of them require root/manager privileges. 

# OPTIONS


## -aattr *obj_spec* *attr_name* *val* *obj_instance*, ...  
Allows adding specifications to a single configuration list attribute in multiple instances of an object with a 
single command. Currently supported objects are the queue, the host, the host group, the parallel environment, 
the resource quota sets and the checkpointing interface configuration being specified as 
*queue*, *exechost*, *hostgroup*, *pe*, *rqs* or *ckpt* in *obj_spec*. 

For the *obj_spec* *queue* the *obj_instance* can be a cluster queue name, a queue domain name or a queue instance 
name. Find more information concerning different queue names in xxqs_name_sxx_types(1). Depending on the type of 
the *obj_instance* this adds to the cluster queues attribute sublist the cluster queues implicit default 
configuration value or the queue domain configuration value or queue instance configuration value. 
The queue *load_thresholds* parameter is an example of a list attribute. With the `-aattr` option, entries can be 
added to such lists, while they can be deleted with `-dattr`, modified with `-mattr`, and replaced with `-rattr`. 

For the *obj_spec* *rqs* the *obj_instance* is a unique identifier for a specific rule. The identifier consists 
of a rule-set name and either the number of the rule in the list, or the name of the rule, separated by a /.  

The name of the configuration attribute to be enhanced is specified with *attr_name* followed by *val* as a 
*name=value* pair. The comma separated list of object instances (e.g., the list of queues) to which the changes have 
to be applied are specified at the end of the command.  

The following restriction applies: For the *exechost* object the *load_values* attribute cannot be modified 
(see xxqs_name_sxx_host_conf(5)). Requires root/manager privileges.

## -Aattr *obj_spec* *fname* *obj_instance*, ...

Adds attributes to an object.

Similar to `-aattr` (see below) but takes specifications for the object attributes to be enhanced from file named
*fname*. As opposed to `-aattr`, multiple attributes can be enhanced. Their specification has to be enlisted in
*fname* following the file format of the corresponding object (see xxqs_name_sxx_queue_conf(5) for the queue, 
for example). Requires root/manager privileges.

## -acal *calendar_name*
Adds a new calendar definition to the xxQS_NAMExx environment. Calendars are used in xxQS_NAMExx for defining
availability and unavailability schedules of queues. The format of a calendar definition is described in
xxqs_name_sxx_calendar_conf(5).

With the calendar name given in the option argument `qconf` will open a temporary file and start up an editor.
After entering the calendar definition and closing the editor the new calendar is checked and registered with
xxqs_name_sxx_qmaster(8). Requires root/manager privileges.

## -Acal *fname*
Adds a new calendar definition to the xxQS_NAMExx environment. Calendars are used in xxQS_NAMExx for defining
availability and unavailability schedules of queues. The format of a calendar definition is described in
calendar_conf(5). The calendar definition is taken from the file *fname*. Requires root/manager privileges.

## -ackpt *ckpt_name*
Adds a checkpointing environment under the name *ckpt_name* to the list of checkpointing environments maintained
by xxQS_NAMExx and to be usable to submit checkpointing jobs (see xxqs_name_sxx_checkpoint(5) for details on
the format of a checkpointing environment definition). `qconf` retrieves a default checkpointing environment
configuration and executes an editor to allow you to customize the checkpointing environment configuration. Upon
exit from the editor, the checkpointing environment is registered with xxqs_name_sxx_qmaster(8).
Requires root/manager privileges.

## -Ackpt *fname*
Add the checkpointing environment as defined in *fname* (see xxqs_name_sxx_checkpoint(5)) to the list of supported
checkpointing environments. Requires root/manager privileges.

## -aconf *host*, ...
Successively adds configurations (see xxqs_name_sxx_conf(5)) For the hosts in the comma separated *host* list.
For each *host*, an editor is invoked and the configuration for the host can be entered. The configuration is
registered with xxqs_name_sxx_qmaster(8) after saving the file and quitting the editor. Requires root/manager
privileges.

## -Aconf *fname_list*
Add the configurations (see xxqs_name_sxx_conf(5)) specified in the files enlisted in the comma separated
*fname_list*. The configuration is added for the host that is identical to the file name. Requires root/manager
privileges.

## -ae \[*host_template*\]
Adds a host to the list of xxQS_NAMExx execution hosts. If a queue is configured on a host this host is
automatically added to the xxQS_NAMExx execution host list. Adding execution hosts explicitly offers the
advantage to be able to specify parameters like load scale values with the registration of the execution host.
However, these parameters can be modified (from their defaults) at any later time via the `-me` option
described below.  
If the *host_template* argument is present, `qconf` retrieves the configuration of the specified execution
host from xxqs_name_sxx_qmaster(8) or a generic template otherwise. The template is then stored in a file and
`qconf` executes an editor to change the entries in the file. The format of the execution host specification is
described in xxqs_name_sxx_host_conf(5). When the changes are saved in the editor and the editor is quit them the new
execution host is registered with xxqs_name_sxx_qmaster(8). Requires root/manager privileges.

## -Ae *fname*
Add the execution host defined in *fname* to the xxQS_NAMExx cluster. The format of the execution host specification
is described in xxqs_name_sxx_host_conf(5). Requires root/manager privileges.

## -ah *hostname*,...
Adds hosts *hostname* to the xxQS_NAMExx trusted host list. A host must be in this list to execute administrative
xxQS_NAMExx commands, the sole exception to this being the execution of `qconf` on the xxqs_name_sxx_qmaster(8)
node. The default xxQS_NAMExx installation procedures add all designated execution hosts (see the `-ae`
option above) to the xxQS_NAMExx trusted host list automatically. Requires root/manager privileges.

## -ahgrp *group*
Adds a new host group with the name specified in *group*. This command invokes an editor that allows to
make required changes. After the editor is quit the new host group entry is registered. Requires
root/manager privileges.

## -Ahgrp *fname*
Add the host group configuration defined in *fname*. The file format of *fname* must comply to the format specified
in xxqs_name_sxx_hostgroup(5). Requires root/manager privileges.

## -am *user*,...
Adds the indicated users to the xxQS_NAMExx manager list. Requires root/manager privileges.

## -ao *user*,...
Adds the indicated users to the xxQS_NAMExx operator list. Requires root/manager privileges.

## -ap *pe_name*
Adds a new parallel environment (PE) description under the name *pe_name* to the list of parallel environments
maintained by xxQS_NAMExx and to be usable to submit parallel jobs (see xxqs_name_sxx_pe(5) for details
on the format of a PE definition. `Qconf` retrieves a default PE configuration and executes an editor
to allow you to customize the PE configuration. Upon exit from the editor, the PE is registered with
xxqs_name_sxx_qmaster(8). Requires root/manager privileges.

## -Ap *fname*
Add the parallel environment (PE) defined in *fname* to the xxQS_NAMExx cluster. Requires root/manager privileges.

## -aprj
Adds a project description to the list of registered projects (see xxqs_name_sxx_project(5)). `Qconf` retrieves
a template project configuration and executes an editor to allow you to customize the new project.
Upon exit from the editor, the specified project is registered with xxqs_name_sxx_qmaster(8). Requires
root/manager privileges.

## -Aprj *fname*
Adds the project description defined in *fname* to the list of registered projects (see xxqs_name_sxx_project(5)).
Requires root/manager privileges.

## -aq \[*queue_name*\]
`Qconf` retrieves the default cluster queue configuration (see xxqs_name_sxx_queue_conf(5)) and executes an editor
to allow you to customize the cluster queue configuration. Upon exit from the editor, the queue is registered with
xxqs_name_sxx_qmaster(8). A minimal configuration requires only that the queue name and queue hostlist be set.
Requires root/manager privileges.

## -Aq *fname*
Add the queue defined in *fname* to the xxQS_NAMExx cluster. Requires root/manager privileges.

## -arqs \[*rqs_name*\]
Adds a resource quota set (RQS) description under the names *rqs_name* to the list of quota sets maintained
by xxQS_NAMExx (see xxqs_name_sxx_resource_quota(5) for details on the format of a RQS definition). `Qconf`
retrieves a default RQS configuration and executes and editor to allow you to customize the RQS configuration.
Upon exit from the editor, the RQS is registered with xxqs_name_sxx_qmaster(8). Requires root/manager privileges.

## -Arqs *fname*
Add the resource quota set (RQS) defined in the file named *fname* to the xxQS_NAMExx cluster. Requires
root/manager privileges.

## -as *hostname*, ...
Add hosts *hostname* to the list of hosts allowed to submit xxQS_NAMExx jobs and control their behavior only.
Requires root/manager privileges.

## -astnode *node_path*=*shares*, ...
Adds the specified share tree node(s) to the share tree (see xxqs_name_sxx_share_tree(5)). The *node_path* is
a hierarchical path (*\[/\]node_name\[\[/.\]node_name...\]*) specifying the location of the new node in the
share tree. The base name of the node_path is the name of the new node. The node is initialized to the number
of specified shares. Requires root/manager privileges.

## -astree
Adds the definition of a share tree to the system (see xxqs_name_sxx_share_tree(5)). A template share tree is
retrieved and an editor is invoked for modifying the share tree definition. Upon exiting the editor, the
modified data is registered with xxqs_name_sxx_qmaster(8). Requires root/manager privileges.

## -Astree *fname*
Adds the definition of a share tree to the system (see xxqs_name_sxx_share_tree(5)) from the file *fname*.
Requires root or manager privileges.

## -at *thread_name*
Activates an additional thread in the xxqs_name_sxx_qmaster(8) process. *thread_name* might be only *'scheduler'*.
The corresponding thread is only started when it is not already running. There might be only one scheduler in the
master process at the same time.

## -au *user*,... *acl_name*,...
Adds users to xxQS_NAMExx access control lists (ACL). Those lists are used for object usage authentication.
Requires root/manager/operator privileges.

## -Au *fname*
Add the user access list (ACL) defined in *fname** to xxQS_NAMExx. User lists are used for queue usage authentication.
Requires root/manager/operator privileges.

## -auser
Adds a user to the list of registered users (see xxqs_name_sxx_user*(5)). This command invokes an editor
for showing a template user. The new user is registered after changing the entry and exiting the editor. Requires
root/manager privileges.

## -Auser *fname*
Add the user defined in *fname* to the xxQS_NAMExx cluster. The format of the user specification is described
in xxqs_name_sxx_user(5). Requires root/manager privileges.

## -cb
This parameter can be used since xxQS_NAMExx version 6.2u5 in combination with the command line switch `-sep`.
In that case the output of the corresponding command will contain information about the added job to core binding
functionality. If *-cb* switch is not used then *-sep* will behave as in xxQS_NAMExx version 6.2u4 and below.

Please note that this command-line switch will be removed from xxQS_NAMExx with the next major release.

## -clearusage
Clears all user and project usage from the share tree. All usage will be initialized back to zero.

## -cq *wc_queue_list*
Cleans queue from jobs which haven't been reaped. Primarily a development tool. Requires root/manager/operator
privileges. Find a description of *wc_queue_list* in xxqs_name_sxx_types(1).

## -dattr *obj_spec* *attr_name* *val* *obj_instance*
Allows deleting specifications in a single configuration list attribute in multiple instances of an object with a
single command. Find more information concerning *obj_spec* and *obj_instance* in the description of `-aattr`.

## -Dattr *obj_spec* *fname* *obj_instance*, ...
Deletes attributes from an object. Similar to `-dattr` (see below) but the definition of the list
attributes from which entries are to be deleted is contained in the file named *fname*. As opposed to `-dattr`,
multiple attributes can be modified. Their specification has to be enlisted in *fname* following
the file format of the corresponding object (see xxqs_name_sxx_queue_conf(5) for the queue, for example).  
Requires root/manager privileges.

## -dcal *calendar_name*,...
Deletes the specified calendar definition from xxQS_NAMExx. Requires root/manager privileges.

## -dckpt *ckpt_name*
Deletes the specified checkpointing environment. Requires root/manager privileges.

## -dconf *host*,...
The local configuration entries for the specified hosts are deleted from the configuration list.
Requires root/manager privilege.

## -de *host_name*,...
Deletes hosts from the xxQS_NAMExx execution host list. Requires root|manager privileges.

## -dh *host_name*,...
Deletes hosts from the xxQS_NAMExx trusted host list. The host on which xxqs_name_sxx_qmaster(8) is currently
running cannot be removed from the list of administrative hosts. Requires root/manager privileges.

## -dhgrp *group*
Deletes host group configuration with the name specified with *group*. Requires root/manager privileges.

## -dm *user*\[,*user*,...\]
Deletes managers from the manager list. It is not possible to delete the admin user or the user root
from the manager list. Requires root/manager privileges.

## -do *user*\[,*user*,...\]
Deletes operators from the operator list. It is not possible to delete the admin user or the user root
from the operator list. Requires root or manager privileges.

## -dp *pe_name*
Deletes the specified parallel environment (PE). Requires root/manager privileges.

## -dprj *project*,...
Deletes the specified project(s). Requires root/manager privileges.

## -dq *queue_name*,...
Removes the specified queue(s). Active jobs will be allowed to run to completion. Requires root/manager privileges.

## -drqs *rqs_name_list*
Deletes the specified resource quota sets (RQS). Requires root/manager privileges.

## -ds host_name,... \<delete submit host>
Deletes hosts from the xxQS_NAMExx submit host list. Requires root or
manager privileges.

## -dstnode *node_path*,...
Deletes the specified share tree node(s). The *node_path* is a hierarchical path
(\[/\]node_name\[\[/.\]node_name...\]) specifying the location of the node to be deleted in the share tree.
Requires root/manager privileges.

## -dstree
Deletes the current share tree. Requires root/manager privileges.

## -du *user*,... *acl_name*,...
Deletes one or more users from one or more xxQS_NAMExx access control lists (ACLs). Requires
root/manager/operator privileges.

## -dul *acl_name*,...
Deletes one or more access control lists (ACLs) from the system. Requires root/manager/operator privileges.

## -duser
Deletes the specified user(s) from the list of registered users. Requires root/manager privileges.

## -help
Prints a listing of all options.

## -km
Forces the master component/process xxqs_name_sxx_qmaster(8) to terminate in a controlled fashion.

## -ke[j] *host*,...|**all**
`-ke` and `-kej` terminate the execution component/daemon xxqs_name_sxx_execd(8) on the specified execution *hosts*.
In difference to `-ke`, the `-kej` variant aborts all jobs running on the specified machines, prior to the
termination of the corresponding xxqs_name_sxx_execd(8). Instead of a *host* or *host* list it is possible to
specify the keyword **all** which will shut down all registered xxqs_name_sxx_execd(8) components/daemons at
xxqs_name_sxx_qmaster(8). Job abort via triggered by `-kej` will result in **dr** state of running jobs until the
xxqs_name_sxx_execd(8) process ist running again. Requires root/manager privileges.

## -kec *id*,... | **all**
Used to shut down event clients registered at xxqs_name_sxx_qmaster(8). The comma separated event client list
specifies the event clients to be addressed by the `-kec`. If the keyword **all** is specified instead of an
event client list, all running event clients except special clients like the *scheduler* thread are terminated.
Requires root/manager privilege.

## -ks | -kt *name*
Terminates a thread in the xxqs_name_sxx_qmaster(8) component/process. Currently, it is only
supported to shut down the **scheduler** thread. The command will only be successful if the corresponding thread
is running.`-ks` is an equivalent for `-kt scheduler`.
Requires root/manager privileges.

## -mattr *obj_spec* *attr_name* *val* *obj_instance*
Allows changing a single configuration attribute in multiple instances of an object with a single command. Find more
information concerning *obj_spec* and *obj_instance* in the description of `-aattr`

## -Mattr *obj_spec* *fname* *obj_instance*, ...
Modifies attributes of an object. Similar to `-mattr` (see below) but takes specifications for the object attributes
to be modified from file named *fname*. As opposed to `-mattr`, multiple attributes can be modified. Their
specification has to be enlisted in *fname* following the file format of the corresponding object
(see xxqs_name_sxx_queue_conf(5) for the queue, for example).  
Requires root/manager privileges.

## -mc
The complex configuration (see xxqs_name_sxx_complex(5)) is retrieved, an editor is executed and on exit the
changed complex configuration is registered with xxqs_name_sxx_qmaster(8) upon exit of the editor. Requires
root/manager privilege.

## -Mc *fname*
Overwrites the complex configuration by the contents of *fname*. The argument file must comply to the format
specified in xxqs_name_sxx_complex(5). Requires root or manager privilege.

## -mckpt *ckpt_name*
Retrieves the current configuration for the specified checkpointing environment, executes an editor
and registers the new configuration with the xxqs_name_sxx_qmaster(8). Refer to xxqs_name_sxx_checkpoint(5) for
details on the checkpointing environment configuration format. Requires root/manager privilege.

## -Mckpt *fname*
Overwrite an existing checkpointing environment with the definitions in *fname* (see xxqs_name_sxx_checkpoint(5)).
The name attribute in *fname* has to match an existing checkpointing environment. Requires root/manager
privileges.

## -mcal *calendar_name*
The specified calendar definition (see xxqs_name_sxx_calendar_conf(5)) is retrieved, an editor is executed
and the changed calendar definition is registered with xxqs_name_sxx_qmaster(8) upon exit of the editor.
Requires root/manager privilege.

## -Mcal *fname*
Overwrites the calendar definition as specified in *fname*. The argument file must comply to the format described in
xxqs_name_sxx_calendar_conf(5). Requires root or manager privilege.

## -mconf \[*host*,... \| **global**\]
The configuration for the specified host is retrieved, an editor is executed and the changed configuration is
registered with xxqs_name_sxx_qmaster(8) upon exit of the editor. If the optional host argument is omitted or if
the special host name **global** is specified, the global configuration is modified. The format of the
configuration is described in xxqs_name_sxx_conf(5). Requires root/manager privilege.

## -Mconf *fname_list*
Modify the configurations (see xxqs_name_sxx_conf(5)) specified in the files enlisted in the comma separated
*fname_list*. The configuration is modified for the host that is identical to the file name. Requires
root/manager privileges.

## -me *hostname*
Retrieves the current configuration for the specified execution host, executes an editor and registers the changed
configuration with xxqs_name_sxx_qmaster(8) upon exit from the editor. The format of the execution host
configuration is described in xxqs_name_sxx_host_conf(5). Requires root/manager privilege.

## -Me *fname*
Overwrites the execution host configuration for the specified host with the contents of *fname*, which must
comply to the format defines in xxqs_name_sxx_host_conf(5). Requires root or manager privilege.

## -mhgrp *group*
The host group entries for the host group specified in *group* are retrieved and an editor is invoked for modifying
the host group configuration. By closing the editor, the modified data is registered with xxqs_name_sxx_qmaster(8).
The format of the host group configuration is described in xxqs_name_sxx_hostgroup(5). Requires
root/manager privilege.

## -Mhgrp *fname*
Allows changing of host group configuration with a single command. All host group configuration entries contained
in *fname* will be applied. Configuration entries not contained in *fname* will be deleted. The
file format of *fname* must comply to the format specified in xxqs_name_sxx_hostgroup(5).

## -mp *pe_name*
Retrieves the current configuration for the specified parallel environment (PE), executes an editor
indicated by the EDITOR environment variable) configuration with the xxqs_name_sxx_qmaster(8). Refer to
xxqs_name_sxx_pe(5) for details on the PE configuration format. Requires root/manager privilege.

## -Mp *fname*
Modifies a parallel environment. Same as `-mp` (see below) but instead of invoking an editor to modify
the PE configuration the file *fname* is considered to contain a changed configuration. Refer to
xxqs_name_sxx_pe(5) for details on the PE configuration format. Requires root/manager privilege.

## -mprj *project*
Data for the specific project is retrieved (see xxqs_name_sxx_project(5)) and an editor is invoked
for modifying the project definition. Upon exiting the editor, the modified data is registered.
Requires root/manager privileges.

## -Mprj *fname*
Modifies a project configuration. Same as `-mprj` (see below) but instead of invoking an editor to
modify the project configuration the file *fname* is considered to contain a changed configuration.
Refer to xxqs_name_sxx_project*(5) for details on the project configuration format.
Requires root/manager privilege.

## -mq *queuename*
Retrieves the current configuration for the specified cluster queue, executes an editor and registers the
new configuration with the xxqs_name_sxx_qmaster(8). Refer to xxqs_name_sxx_queue_conf(5) for details on
the queue configuration format. Requires root/manager privilege.

## -Mq *fname*
Modifies a queue configuration. Same as `-mq` (see below) but instead of invoking an editor to modify
the queue configuration the file *fname* is considered to contain a changed configuration.
Refer to xxqs_name_sxx_queue_conf(5) for details on the queue configuration format. Requires root/manager privilege.

## -mrqs \[*rqs_name*\]
Retrieves the resource quota set (RQS) configuration defined in *rqs_name*, or if *rqs_name* is not given,
retrieves all resource quota sets, executes an editor and registers the new configuration with the
xxqs_name_sxx_qmaster(8). Refer to xxqs_name_sxx_resource_quota(5) for details on the RQS configuration format.
Requires root/manager privilege.

## -Mrqs *fname* \[*mrqs_name*\]
Modifies a resource quota set (RQS) configuration. Same as `-mrqs` (see below) but instead of invoking an editor to
modify the RQS configuration, the file *fname* is considered to contain a changed configuration. The name of the rule
set in *fname* must be the same as rqs_name. If *rqs_name* is not set, all rule sets are
overwritten by the rule sets in *fname* Refer to xxqs_name_sxx_resource_quota(5) for details on the RQS configuration
format. Requires root/manager privilege.

## -msconf
The current scheduler configuration (see xxqs_name_sxx_sched_conf(5)) is retrieved, an editor is executed
and the changed configuration is registered with xxqs_name_sxx_qmaster(8) upon exit of the editor. Requires
root/manager privilege.

## -Msconf *fname*
Modifies the scheduler configuration. The current scheduler configuration (see xxqs_name_sxx_sched_conf(5))
is overridden with the configuration specified in the file. Requires root/manager privilege.

## -mstnode *node_path*=*shares*,...
Modifies the specified share tree node(s) in the share tree (see xxqs_name_sxxshare_tree(5)). The *node_path*
is a hierarchical path (*\[/\]node_name\[\[/.\]node_name...\])* specifying the location of an existing node in
the share tree. The node is set to the number of specified *shares*. Requires root/manager privileges.

## -mstree
Modifies the definition of the share tree (see xxqs_name_sxx_share_tree(5)). The present share tree is retrieved
and an editor is invoked for modifying the share tree definition. Upon exiting the editor, the modified data
is registered with xxqs_name_sxx_qmaster(8). Requires root/manager privileges.

## -Mstree *fname*
Modifies the definition of the share tree (see xxqs_name_sxx_share_tree(5)). The modified sharetree is read from
file fname. Requires root/manager privileges.

## -mu *acl_name*
Retrieves the current configuration for the specified user access list, executes an editor and registers the new
configuration with the xxqs_name_sxx_qmaster(8). Requires root/manager privilege.

## -Mu *fname*
Takes the user access list (ACL) defined in *fname* to overwrite any existing ACL with the same name.
See xxqs_name_sxx_access_list(5) for information on the ACL configuration format. Requires root or manager privilege.

## -muser user
Data for the specific user is retrieved (see xxqs_name_sxx_user(5)) and an editor is invoked for modifying the
user definition. Upon exiting the editor, the modified data is registered at xxqs_name_sxx_qmaster(8). Requires
root/manager privilege.

## -Muser *fname*
Modify the user defined in *fname* in the xxQS_NAMExx cluster. The format of the user specification is described in
xxqs_name_sxx_user(5). Requires root/manager privileges.

## -purge *queue* *attr_nm*,... *obj_spec*  

Delete the values of the attributes defined in *attr_nm* from the object defined in *obj_spec*. *Obj_spec* can 
be a **queue_instance** or **queue_domain**. The names of the attributes are described in xxqs_name_sxx_queue_conf(1).  
This operation only works on a single queue instance or domain. It cannot be used on a cluster queue. 

In the case where the *obj_spec* is **queue@@hostgroup**, the attribute values defined in *attr_nm* which are set 
for the indicated host group are deleted, but not those which are set for the hosts contained by that host group. 

If the *attr_nm* is *'\*'*, all attribute values set for the given queue instance or domain are deleted.  

The main difference between `-dattr` and `-purge` is that `-dattr` removes a value from a single list attribute, 
whereas `-purge` removes one or more overriding attribute settings from a cluster queue configuration. With
`-purge`, the entire attribute is deleted for the given queue instance or queue domain.

## -rattr *obj_spec* *attr_name* *val* *obj_instance*,...

Allows replacing a single configuration list attribute in multiple instances of an object with a single command. 
Find more information concerning *obj_spec* and *obj_instance* in the description of `-aattr`.
Requires root/manager privilege.

## -Rattr *obj_spec* *fname* *obj_instance*, ...
Replace object attributes. Similar to `-rattr` (see below) but the definition of the list attributes whose content is
to be replace is contained in the file named *fname*. As opposed to `-rattr`, multiple attributes can be modified.
Their specification has to be enlisted in *fname* following the file format of the corresponding object
(see xxqs_name_sxx_queue_conf(5) for the queue, for example). Requires root/manager privileges.

## -rsstnode *node_path*,...  
Recursively shows the name and shares of the specified share tree node(s) and the names and shares of its child nodes. 
(see xxqs_name_sxx_share_tree(5)). The *node_path* is a hierarchical path (\[/\]node_name\[\[/.\]node_name...\]) 
specifying the location of a node in the share tree.

## -sc
Display the complex configuration.

## -scal *calendar_name*
Display the configuration of the specified calendar.

## -scall 
Show a list of all calendars currently defined.

## -sckpt *ckpt_name*
Display the configuration of the specified checkpointing environment.

## -sckptl
Show a list of names of all checkpointing environments currently configured.

## -sconf \[*host*,...\ | **global**\] 
If the optional comma separated host list argument is omitted or the special string **global** is given, the global 
configuration is displayed. The configuration in effect on a certain host is the merger of the global
configuration and the host specific local configuration. The format of the configuration is described in 
xxqs_name_sxx_conf(5).

## -sconfl
Display a list of hosts for which configurations are available. The special host name **global** refers to the 
global configuration.

## -sds 
Displays detached settings in the cluster configuration.

## -se *hostname*
Displays the definition of the specified execution host.

## -secl
Displays the xxQS_NAMExx event client list.

## -sel  
Displays the xxQS_NAMExx execution host list.

## -sep
**Note:** Deprecated, may be removed in future release.

Displays a list of virtual processors. This value is taken from the underlying OS and it depends on underlying 
hardware and operating system whether this value represents sockets, cores or supported threads.

If this option is used in combination with `-cb` parameter then two additional columns will be shown in the output 
for the number of sockets and number of cores. Currently, xxQS_NAMExx will enlist these values only if the
corresponding operating system of execution host is Linux under kernel >= 2.6.16, or Solaris 10. Other operating 
systems or versions might be supported with the future update releases. In case these values won't be retrieved, 
'0' will be displayed.

## -sh
Displays the xxQS_NAMExx administrative host list.

## -shgrp *group*
Displays the host group entries for the group specified in *group*.

## -shgrpl 
Displays a name list of all currently defined host groups which have a valid host group configuration.

## -shgrp_tree *group*
Shows a tree like structure of a host group.

## -shgrp_resolved *group* 
Shows a list of all hosts which are part of the definition of host group. If the host group definition contains 
sub host groups than also these groups are resolved and the hostnames are printed.

## -srqs \[*rqs_name_list*\]  
Show the definition of the resource quota sets (RQS) specified by the argument list.

## -srqsl
Show a list of all currently defined resource quota (RQS) sets.

## -sm 
Displays the managers list.

## -so
Displays the operator list.

## -sobjl *obj_spec* *attr_name* *val*  
Shows a list of all configuration objects for which *val* matches at least one configuration value of the attributes 
whose name matches with *attr_name*.

*obj_spec* can be **queue** or **queue_domain** or **queue_instance** or **exechost**. **Note** When **queue_domain** 
or **queue_instance** is specified as *obj_spec* matching is only done with the attribute overriding concerning 
the host group or the execution host. In this case queue domain names resp. queue instances are returned.

*attr_name* can be any of the configuration file keywords enlisted in xxqs_name_sxx_queue_conf(5) or 
xxqs_name_sxx_host_conf(5). Also wildcards can be used to match multiple attributes.

*val* can be an arbitrary string or a wildcard expression.

## -sp *pe_name*
Show the definition of the parallel environment (PE) specified by *pe_name*.

## -spl 
Show a list of all currently defined parallel environment (PE) objects.

## -sprj *project*
Shows the definition of the specified project (see xxqs_name_sxx_project(5)).

## -sprjl
Shows the list of all currently defined projects.

## -sq *wc_queue_list*
Displays one or multiple cluster queues or queue instances. A description of *wc_queue_list* can be found in
xxqs_name_sxx_types(1).

## -sql
Shows a list of all currently defined cluster queues.

## -ss
Displays the xxQS_NAMExx submit host list.

## -ssconf  
Displays the current scheduler configuration in the format explained in xxqs_name_sxx_sched_conf(5).

## -sstnode *node_path*,...
Shows the name and shares of the specified share tree node(s) (see xxqs_name_sxx_share_tree(5)). The *node_path* 
is a hierarchical path (**\[/\]node_name\[\[/.\]node_name...\])** specifying the location of a node in the share tree.

## -sstree
Shows the definition of the share tree (see xxqs_name_sxx_share_tree(5)).

## -sst
Shows the definition of the share tree in a tree view (see xxqs_name_sxx_share_tree(5)).

## -sss
Currently displays the host on which the xxQS_NAMExx scheduler is active or an error message if no scheduler 
is running.

## -su *acl_name*
Displays a xxQS_NAMExx access control list (ACL).

## -sul  
Displays a list of names of all currently defined xxQS_NAMExx access control lists (ACL).

## -suser user,...
Shows the definition of the specified user(s) (see xxqs_name_sxx_user(5)).

## -suserl 
Shows the list of all currently defined users.

## -tsm  
The xxQS_NAMExx scheduler is forced by this option to print trace messages of its next scheduling run to the file
*\<xxQS_NAME_Sxx_ROOT\>/\<xxQS_NAME_Sxx_CELL\>/common/schedd_runlog*. The messages indicate the reasons for jobs and queues not 
being selected in that run. Requires root/manager privileges.

**Note** The reasons for job requirements being invalid with respect to resource availability of queues are 
displayed using the format as described for the `qstat` *-F* option (see description of
**Full Format** in section **OUTPUT FORMATS** of the qstat(1) manual page.

# ENVIRONMENTAL VARIABLES

## xxQS_NAME_Sxx_ROOT  
Specifies the location of the xxQS_NAMExx standard configuration files.

## xxQS_NAME_Sxx_CELL
If set, specifies the default xxQS_NAMExx cell. To address a xxQS_NAMExx cell `qacct` uses (in the order of precedence):

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

## xxQS_NAME_Sxx_SINGLE_LINE  
If set, indicates that long lines should not be broken up using backslashes. This setting is useful for scripts 
which expect one entry per line.

# FILES

## \<xxQS_NAME_Sxx_ROOT\>/\<xxQS_NAME_Sxx_CELL\>/common/act_qmaster
xxQS_NAMExx master host file

## \<xxQS_NAME_Sxx_ROOT\>/\<xxQS_NAME_Sxx_CELL\>/common/schedd_runlog
file containing trace message of the scheduling component after a call of `qconf -tsm`

# SEE ALSO

xxqs_name_sxx_intro(1), qstat(1), qsub(1), xxqs_name_sxx_checkpoint(5), xxqs_name_sxx_complex(5),
xxqs_name_sxx_conf(5), xxqs_name_sxx_host_conf(5), xxqs_name_sxx_pe(5), xxqs_name_sxx_queue_conf(5), 
xxqs_name_sxx_execd(8), xxqs_name_sxx_qmaster(8), xxqs_name_sxx_resource_quota(5)

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
