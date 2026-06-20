---
title: qconf
section: 1
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

`qconf` - xxQS_NAMExx command to customise configuration objects and get or change the state of cluster components.  

# SYNTAX

`qconf` *\<options\>*

# DESCRIPTION

`Qconf` allows the system administrator to add, delete, and modify the current xxQS_NAMExx configuration, including 
queue management, host management, complex management and user management. `Qconf` also allows you to examine the 
current queue configuration for existing queues.

Some *\<options\>* of `qconf` start up an editor indicated by the environment variable *EDITOR*. If such a variable 
is not set then as default the vi(1) editor will be started.

`Qconf` allows the use of the backslash, '\\', character at the end of a line to indicate that the next line is a 
continuation of the current line. When displaying settings, such as the output of one of the `-s...` options, `qconf` 
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

## -a*obj* \[*name*\]
Adds an object interactively: `qconf` opens a template (or, where noted, an existing
configuration) in an editor (the one named by `$EDITOR`) and registers the result on
exit — the interactive counterpart of the file-based `-A<obj>`, and an upsert
(`-a<obj>` and `-m<obj>` are interchangeable: an object whose name already exists is
modified rather than rejected). `-a<obj>` is available for `-acal` (calendar),
`-ackpt` (checkpointing environment), `-ace` (complex entries), `-ae` (execution
host), `-ahgrp` (host group), `-ap` (parallel environment), `-aprj` (project), `-aq`
(cluster queue), `-arole` (role), `-arqs` (resource quota set) and `-auser` (user),
plus `-astree` (share tree) and `-aconf` (cluster/host configuration).

Most variants take an optional object name; if it is omitted a generic template named
`template` is offered for editing, and if it is given it pre-fills the template's name
field. The object's configuration follows the format of its man page:

| Option | Object | Argument | Format |
|--------|--------|----------|--------|
| `-acal`   | calendar | \[*calendar_name*\] | xxqs_name_sxx_calendar_conf(5) |
| `-ackpt`  | checkpointing environment | \[*ckpt_name*\] | xxqs_name_sxx_checkpoint(5) |
| `-ace`    | complex entries | \[*ce_name*\] | xxqs_name_sxx_complex(5) |
| `-ae`     | execution host | \[*host_template*\] | xxqs_name_sxx_host_conf(5) |
| `-ahgrp`  | host group | *group* | xxqs_name_sxx_hostgroup(5) |
| `-ap`     | parallel environment | \[*pe_name*\] | xxqs_name_sxx_pe(5) |
| `-aprj`   | project | \[*name*\] | xxqs_name_sxx_project(5) |
| `-aq`     | cluster queue | \[*queue_name*\] | xxqs_name_sxx_queue_conf(5) |
| `-arole`  | role | \[*role_name*\] | xxqs_name_sxx_role(5) |
| `-arqs`   | resource quota set | \[*rqs_name*\] | xxqs_name_sxx_resource_quota(5) |
| `-auser`  | user | \[*name*\] | xxqs_name_sxx_user(5) |
| `-astree` | share tree | *(none)* | xxqs_name_sxx_share_tree(5) |
| `-aconf`  | cluster/host configuration | *host*,... | xxqs_name_sxx_conf(5) |

Object-specific notes:

- `-ae` opens a copy of the named execution host's configuration when *host_template*
  is given, or a generic template otherwise.
- `-aconf` invokes an editor for each host in the comma-separated list and adds (or
  modifies) that host's local configuration.
- `-astree` is a singleton and takes no name argument; it creates the share tree, or
  modifies it if one already exists (`-astree` and `-mstree` are interchangeable).
- Usersets (ACLs) have no template-editor add: create one from a file with `-Au`, or
  add a user to an ACL with `-au` (which creates the ACL if it does not yet exist;
  see below). The scheduler configuration always exists and has no add form.

Requires root/manager privileges.

## -A*obj* *fname*|*dir*
Adds objects from a file or a directory of files. `-A<obj>` is an upsert and is
interchangeable with `-M<obj>`: if the argument is a file, the single object it
contains is added, or modified if an object of that name already exists; if it is a
directory, every (non-hidden) regular file in it is added or modified and a one-line
summary is printed. See `-M<obj>` above for the full description, the per-object
file-format table and the object-specific notes (`-Aconf` basename keying, `-Ae` host
resolution, `-Arqs` rule-set semantics, the `-Astree` singleton). `-A<obj>` is
available for the same objects as `-M<obj>`: `-Acal`, `-Ackpt`, `-Ace`, `-Ae`,
`-Ahgrp`, `-Ap`, `-Aprj`, `-Aq`, `-Arole`, `-Arqs`, `-Au` (userset/ACL), `-Auser`,
`-Astree` (share tree) and `-Aconf` (cluster/host configuration).

`-Astree` takes a single *fname* (the share tree is a singleton); the scheduler
configuration always exists and has no add form. See also `-dry`, `-strict` and the
EXIT STATUS section. Requires root/manager privileges (`-Au` also accepts operator
privilege).

## -ah *hostname*,...
Adds hosts *hostname* to the xxQS_NAMExx trusted host list. A host must be in this list to execute administrative
xxQS_NAMExx commands, the sole exception to this being the execution of `qconf` on the xxqs_name_sxx_qmaster(8)
node. The default xxQS_NAMExx installation procedures add all designated execution hosts (see the `-ae`
option above) to the xxQS_NAMExx trusted host list automatically. Requires root/manager privileges.

## -am *user*,...
Adds the indicated users to the xxQS_NAMExx manager list. Requires root/manager privileges.

## -ao *user*,...
Adds the indicated users to the xxQS_NAMExx operator list. Requires root/manager privileges.

## -as *hostname*, ...
Add hosts *hostname* to the list of hosts allowed to submit xxQS_NAMExx jobs and control their behavior only.
Requires root/manager privileges.

## -astnode *node_path*=*shares*, ...
Adds the specified share tree node(s) to the share tree (see xxqs_name_sxx_share_tree(5)). The *node_path* is
a hierarchical path (*\[/\]node_name\[\[/.\]node_name...\]*) specifying the location of the new node in the
share tree. The base name of the node_path is the name of the new node. The node is initialized to the number
of specified shares. Requires root/manager privileges.

## -at *thread_name*
Activates an additional thread in the xxqs_name_sxx_qmaster(8) process. *thread_name* might be only *'scheduler'*.
The corresponding thread is only started when it is not already running. There might be only one scheduler in the
master process at the same time.

## -au *user*,... *acl_name*,...
Adds users to xxQS_NAMExx access control lists (ACL). Those lists are used for object usage authentication.
Requires root/manager/operator privileges.

## -cb
This parameter can be used since xxQS_NAMExx version 6.2u5 in combination with the command line switch `-sep`.
In that case the output of the corresponding command will contain information about the added job to core binding
functionality. If *-cb* switch is not used then *-sep* will behave as in xxQS_NAMExx version 6.2u4 and below.

Please note that this command-line switch will be removed from xxQS_NAMExx with the next major release.

## -dry
Modifier for the add, modify and delete operations. The requested actions are validated and reported (for example
`[dry-run] would add "name"`) but nothing is sent to xxqs_name_sxx_qmaster(8). Useful for checking a directory of
definitions before applying it.

## -strict
Modifier for the file/directory based add and modify operations (`-Acal`/`-Mcal` and the equivalents for other
objects). When a directory is given, all files are read and validated first and the batch is applied only if every
file is valid; if any file fails to parse, nothing is sent to xxqs_name_sxx_qmaster(8). This is a client-side
pre-apply check, not a multi-object transaction: once validation passes, a failure while sending an already
validated batch can still leave earlier objects applied.

## -S*obj* *name*|*dir*
Exports (saves) objects to re-importable files, the inverse of the `-A`/`-M` bulk add/modify operations.
`-S<obj>` is available for the importable configuration objects: `-Scal` (calendar), `-Sckpt` (checkpointing
environment), `-Sce` (complex entries), `-Se` (exec host), `-Shgrp` (host group), `-Sp` (parallel environment),
`-Sprj` (project), `-Sq` (cluster queue), `-Srole` (role), `-Srqs` (resource quota set), `-Su` (userset) and
`-Suser` (user), plus the two singletons `-Sstree` (share tree) and `-Ssconf` (scheduler configuration), and the
bespoke `-Sconf` (cluster/host configuration).

If the argument is an existing directory or ends with a `/`, `-S<obj>` runs in **directory mode**: every object of
that type is written to its own file inside the directory, each file named after the object; the directory is
created if it does not exist. Otherwise the argument is treated as a single object name and that one object is
written to a file of that name in the current directory. Each exported file uses the same format the matching
`-A<obj>`/`-M<obj>` reads, so it re-imports unchanged; for example `qconf -Sq queues/` followed by `qconf -Aq
queues/` round-trips every queue.

By default an existing destination file is left untouched and skipped with a warning; use `-f` to overwrite. Under
`-fmt json` each file is written as JSON with a `.json` suffix. An object whose name is not a valid file name (for
example, one containing `/` or beginning with `-`), or two objects in a directory export whose names differ only in
case, are reported as an error and not written. The singletons `-Sstree` and `-Ssconf` take a file name only — a
directory argument is rejected. `-Sconf` takes a single host name (or `global`) or a directory; in directory mode
it writes the global configuration (file `global`) and every host's local configuration (one file per host). Unlike
`-sconf`, `-Sconf` does not accept a comma-separated host list. Export is read-only and does not require
root/manager privileges.

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

## -d*obj* *name*,...
Deletes objects by a comma-separated list of names — the name-list counterpart of the
file/directory `-D<obj>` deletes. `-d<obj>` is available for: `-dcal` (calendar),
`-dckpt` (checkpointing environment), `-dce` (complex entries), `-de` (execution
host), `-dhgrp` (host group), `-dp` (parallel environment), `-dprj` (project), `-dq`
(cluster queue), `-drole` (role), `-drqs` (resource quota set), `-dul` (userset/ACL),
`-duser` (user) and `-dconf` (cluster/host configuration). The share tree and
scheduler configuration are singletons with no name-list delete (use `-dstree`).

More than one object may be deleted at once by giving a comma-separated list of names:

| Option | Object | Argument |
|--------|--------|----------|
| `-dcal`   | calendar | *calendar_name*,... |
| `-dckpt`  | checkpointing environment | *ckpt_name*,... |
| `-dce`    | complex entries | *ce_name*,... |
| `-de`     | execution host | *host_name*,... |
| `-dhgrp`  | host group | *group*,... |
| `-dp`     | parallel environment | *pe_name*,... |
| `-dprj`   | project | *project*,... |
| `-dq`     | cluster queue | *queue_name*,... |
| `-drole`  | role | *role_name*,... |
| `-drqs`   | resource quota set | *rqs_name_list* |
| `-dul`    | userset (ACL) | *acl_name*,... |
| `-duser`  | user | *user_list* |
| `-dconf`  | cluster/host configuration | *host*,... |

Object-specific notes:

- `-dconf` deletes the local configuration entries of the named host(s).
- `-dq` removes the cluster queue(s) but lets active jobs run to completion.
- `-drole` deletion is refused if a role is still referenced as a parent by another
  role.
- `-dul` deletes whole usersets (ACLs); to remove individual users *from* an ACL use
  `-du` (see below) instead.

Requires root/manager privileges (`-dul` also accepts operator privilege).

## -D*obj* *fname*|*dir*
Deletes objects named in a file or a directory of files — the file/directory
counterpart of the comma-separated `-d<obj>` name-list deletes, and a companion to
`-A<obj>`/`-M<obj>`/`-S<obj>`. Each file is read, the object named in it is deleted,
and the rest of the file content is ignored, so a file produced by `-S<obj>` (or any
`-A<obj>`/`-M<obj>` input file) can be passed directly. `-D<obj>` is available for:
`-Dcal` (calendar), `-Dckpt` (checkpointing environment), `-Dce` (complex entries),
`-De` (execution host), `-Dhgrp` (host group), `-Dp` (parallel environment), `-Dprj`
(project), `-Dq` (cluster queue), `-Drole` (role), `-Drqs` (resource quota set),
`-Du` (userset/ACL), `-Duser` (user) and `-Dconf` (cluster/host configuration). The
share tree and scheduler configuration are singletons with no `-D` form (use
`-dstree`).

If the argument is a directory, every (non-hidden) regular file in it is processed as
one bulk delete: it is confirmed interactively unless `-f` is given, and `-dry`
previews it without deleting anything. Deletion is idempotent — an object named in a
file that no longer exists is reported and skipped rather than treated as an error.
Each object is identified by the name field of its file:

| Option | Object | Name taken from |
|--------|--------|-----------------|
| `-Dcal`   | calendar | `calendar_name` field |
| `-Dckpt`  | checkpointing environment | `ckpt_name` field |
| `-Dce`    | complex entries | `name` field |
| `-De`     | execution host | `hostname` field (resolved) |
| `-Dhgrp`  | host group | `group_name` field |
| `-Dp`     | parallel environment | `pe_name` field |
| `-Dprj`   | project | object name |
| `-Dq`     | cluster queue | `qname` field |
| `-Drole`  | role | `name` field |
| `-Drqs`   | resource quota set | rule-set name(s) |
| `-Du`     | userset (ACL) | object name |
| `-Duser`  | user | object name |
| `-Dconf`  | cluster/host configuration | file *basename* (host) |

Object-specific notes:

- `-Dconf` keys on the host name taken from the file *basename* (not a field in the
  file), matching `-Aconf`/`-Mconf`.
- `-De` resolves each host name before deleting it.
- `-Drole` deletion is refused if a role is still referenced as a parent by another
  role.
- `-Dq` lets active jobs run to completion.

Requires root/manager privileges (`-Du` also accepts operator privilege).

## -dh *host_name*,...
Deletes hosts from the xxQS_NAMExx trusted host list. The host on which xxqs_name_sxx_qmaster(8) is currently
running cannot be removed from the list of administrative hosts. Requires root/manager privileges.

## -dm *user*\[,*user*,...\]
Deletes managers from the manager list. It is not possible to delete the admin user or the user root
from the manager list. Requires root/manager privileges.

## -do *user*\[,*user*,...\]
Deletes operators from the operator list. It is not possible to delete the admin user or the user root
from the operator list. Requires root or manager privileges.

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

## -f
Modifier for the delete operations. Suppresses the interactive confirmation prompt that a directory based delete
(`-Dcal *dir*` and the equivalents for other objects) otherwise shows before removing multiple objects.

## -fmt *plain*|*json*
Selects the serialization format for the show (`-s*`) and file based add/modify (`-A*`/`-M*`) operations.
`plain` (the default) is the traditional flatfile ASCII format. `json` emits/reads a structured JSON
representation driven by the same object field definitions, with values typed natively (numbers, booleans,
nested arrays). How memory and time valued attributes are rendered is controlled by `-fmtval` (see below).
The interactive editor based operations (`-m*`) always use the plain format. This modifier may appear
anywhere on the command line.

## -fmtval *compact*|*numeric*
Controls how memory and time valued attributes are rendered in `-fmt json` output (it has no effect on the
plain format). `compact` (the default) keeps the human readable unit and colon notation also used by the plain
format, for example `1.5G` for a memory value and `0:5:0` for a time value. `numeric` renders the same values
as plain numbers in their base unit, that is bytes for memory and seconds for time. An unlimited value is
always emitted as the string `INFINITY`. On input, both forms (as well as a native number) are accepted
regardless of this setting. This modifier may appear anywhere on the command line.

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

## -m*obj* \[*name*\]
Retrieves the current configuration of an object, opens it in an editor (the one
named by the `$EDITOR` environment variable), and on exit registers the changed
configuration with xxqs_name_sxx_qmaster(8) — the interactive counterpart of the
file-based `-M<obj>`. `-m<obj>` is available for the same configuration objects as
`-S<obj>`/`-M<obj>`: `-mcal` (calendar), `-mckpt` (checkpointing environment),
`-mce` (complex entries), `-me` (execution host), `-mhgrp` (host group), `-mp`
(parallel environment), `-mprj` (project), `-mq` (cluster queue), `-mrole` (role),
`-mrqs` (resource quota set), `-mu` (userset/ACL) and `-muser` (user), plus the
singletons `-mstree` (share tree) and `-msconf` (scheduler configuration), and
`-mconf` (cluster/host configuration).

Most variants take the name of the object to edit. If no object of that name exists
yet, a generic template (pre-filled with the given name) is offered for editing and
the object is added on exit — `-m<obj>` and `-a<obj>` are interchangeable. The
object's configuration follows the format of its man page:

| Option | Object | Argument | Format |
|--------|--------|----------|--------|
| `-mcal`   | calendar | *calendar_name* | xxqs_name_sxx_calendar_conf(5) |
| `-mckpt`  | checkpointing environment | *ckpt_name* | xxqs_name_sxx_checkpoint(5) |
| `-mce`    | complex entries | *ce_name* | xxqs_name_sxx_complex(5) |
| `-me`     | execution host | *hostname* | xxqs_name_sxx_host_conf(5) |
| `-mhgrp`  | host group | *group* | xxqs_name_sxx_hostgroup(5) |
| `-mp`     | parallel environment | *pe_name* | xxqs_name_sxx_pe(5) |
| `-mprj`   | project | *project* | xxqs_name_sxx_project(5) |
| `-mq`     | cluster queue | *queuename* | xxqs_name_sxx_queue_conf(5) |
| `-mrole`  | role | *role_name* | xxqs_name_sxx_role(5) |
| `-mrqs`   | resource quota set | \[*rqs_name*\] | xxqs_name_sxx_resource_quota(5) |
| `-mu`     | userset (ACL) | *acl_name* | xxqs_name_sxx_access_list(5) |
| `-muser`  | user | *user* | xxqs_name_sxx_user(5) |
| `-mstree` | share tree | *(none)* | xxqs_name_sxx_share_tree(5) |
| `-msconf` | scheduler configuration | *(none)* | xxqs_name_sxx_sched_conf(5) |
| `-mconf`  | cluster/host configuration | \[*host*,... \| **global**\] | xxqs_name_sxx_conf(5) |

Object-specific notes:

- `-mconf` edits the configuration of the given host(s); if the argument is omitted
  or **global**, the global configuration is edited. A configuration that does not
  yet exist is created.
- `-mrqs` edits the named resource quota set, or all rule sets if *rqs_name* is
  omitted.
- `-mstree` and `-msconf` are singletons and take no name argument; `-mstree`
  creates the share tree if none exists (`-mstree` and `-astree` are
  interchangeable).

Requires root/manager privilege.

## -M*obj* *fname*|*dir*
Adds or modifies objects from a file or a directory of files — the file-based
counterpart of the interactive `-m<obj>` editors and the exact inverse of the
`-S<obj>` exporters. `-M<obj>` is available for the same configuration objects as
`-S<obj>`: `-Mcal` (calendar), `-Mckpt` (checkpointing environment), `-Mce` (complex
entries), `-Me` (execution host), `-Mhgrp` (host group), `-Mp` (parallel
environment), `-Mprj` (project), `-Mq` (cluster queue), `-Mrole` (role), `-Mrqs`
(resource quota set), `-Mu` (userset/ACL) and `-Muser` (user), plus the singletons
`-Mstree` (share tree) and `-Msconf` (scheduler configuration), and `-Mconf`
(cluster/host configuration).

If the argument is a file, the single object it contains is modified, or added if no
object of that name exists yet — `-M<obj>` and `-A<obj>` are interchangeable
upserts. If the argument is a directory, every (non-hidden) regular file in it is
applied the same way and a one-line summary of the number of objects modified/added
and failed is printed. See also `-dry`, `-strict` and the EXIT STATUS section. Files
written by `-S<obj>` re-import unchanged through `-M<obj>`. Requires root/manager
privileges. Each object's file must comply to the format of its configuration man
page:

| Option | Object | File format |
|--------|--------|-------------|
| `-Mcal`   | calendar | xxqs_name_sxx_calendar_conf(5) |
| `-Mckpt`  | checkpointing environment | xxqs_name_sxx_checkpoint(5) |
| `-Mce`    | complex entries | xxqs_name_sxx_complex(5) |
| `-Me`     | execution host | xxqs_name_sxx_host_conf(5) |
| `-Mhgrp`  | host group | xxqs_name_sxx_hostgroup(5) |
| `-Mp`     | parallel environment | xxqs_name_sxx_pe(5) |
| `-Mprj`   | project | xxqs_name_sxx_project(5) |
| `-Mq`     | cluster queue | xxqs_name_sxx_queue_conf(5) |
| `-Mrole`  | role | xxqs_name_sxx_role(5) |
| `-Mrqs`   | resource quota set | xxqs_name_sxx_resource_quota(5) |
| `-Mu`     | userset (ACL) | xxqs_name_sxx_access_list(5) |
| `-Muser`  | user | xxqs_name_sxx_user(5) |
| `-Mstree` | share tree | xxqs_name_sxx_share_tree(5) |
| `-Msconf` | scheduler configuration | xxqs_name_sxx_sched_conf(5) |
| `-Mconf`  | cluster/host configuration | xxqs_name_sxx_conf(5) |

Object-specific notes:

- `-Me` resolves each host name before the host is applied.
- `-Mconf` keys each configuration on the host name taken from the file *basename*
  (not a field in the file); `-Mconf global` modifies the global configuration. It
  also accepts a comma-separated *fname_list*.
- `-Mrqs` modifies each rule set in the file, adding it if absent, and leaves every
  other rule set unchanged. Its optional trailing *rqs_list* argument is
  **deprecated** and may be removed in a future release: place only the rule sets to
  apply in the file instead. The argument does not apply when the argument is a
  directory.
- `-Mstree` and `-Msconf` are singletons — they take a single *fname*, not a
  directory; `-Mstree` creates the share tree if none exists.

## -mstnode *node_path*=*shares*,...
Modifies the specified share tree node(s) in the share tree (see xxqs_name_sxxshare_tree(5)). The *node_path*
is a hierarchical path (*\[/\]node_name\[\[/.\]node_name...\])* specifying the location of an existing node in
the share tree. The node is set to the number of specified *shares*. Requires root/manager privileges.

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

## -sce *ce_name*
Show the definition of the complex entry specified by *ce_name*.

## -scel
Show a list of all currently defined complex entry names. To get the full complex configuration use `-sc'`

## -scal *calendar_name*
Display the configuration of the specified calendar.

## -scall 
Show a list of all calendars currently defined.

## -scat *cat_id*
Display characteristics of the given category ID *cat_id*. IDs of existing categories can be obtained from the output of `-scatl`.

## -scatl
Show a list of all categories currently defined. (see xxqs_name_sxx_category(5))

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

## -srole *role_name*
Show the definition of the role specified by *role_name*. Refer to xxqs_name_sxx_role(5) for details
on the role configuration format.

## -srolel
Show a list of all currently defined roles.

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

# EXIT STATUS
`qconf` exits with status 0 on success and a non-zero status on error. For the file/directory based add, modify and
delete operations the exit status reflects the number of failed objects: when a directory contains files that fail
to be applied (or, with `-strict`, when any file in the batch is invalid), `qconf` reports the failures and exits
non-zero while still having applied the objects that succeeded (unless `-strict` prevented the whole batch).

# ENVIRONMENTAL VARIABLES

For a complete list of common environment variables used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

## xxQS_NAME_Sxx_SINGLE_LINE  
If set, indicates that long lines should not be broken up using backslashes. This setting is useful for scripts 
which expect one entry per line.

# FILES

For a complete list of files used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# SEE ALSO

xxqs_name_sxx_intro(1), qstat(1), qsub(1), xxqs_name_sxx_checkpoint(5), xxqs_name_sxx_complex(5),
xxqs_name_sxx_conf(5), xxqs_name_sxx_host_conf(5), xxqs_name_sxx_pe(5), xxqs_name_sxx_queue_conf(5), 
xxqs_name_sxx_execd(8), xxqs_name_sxx_qmaster(8), xxqs_name_sxx_resource_quota(5)

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
