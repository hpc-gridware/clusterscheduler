---
title: qselect
section: 1
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

`qselect` - select queues.

# SYNTAX

`qselect` \[`-help`\] \[`-l` *resource*=*val*,...\] \[`-pe` *pe_name*,...\] \[`-q` *wc_queue*,...\] 
\[`-s` {r\|p\|s\|z\|hu\|ho\|hs\|hj\|ha\|h}+\] \[`-U` *user*,...\]

# DESCRIPTION

`qselect` prints a list of xxQS_NAMExx queue names corresponding to selection criteria specified in the `qselect` 
arguments described below. The output of `qselect` can be fed into other xxQS_NAMExx commands to apply actions on the 
selected queue sets. For example together with the `-mqattr` option to qconf(1), `qselect` can be used to modify queue
attributes on a set of queues.

The administrator and the user may define files (see xxqs_name_sxx_qselect(5)), which can contain any of the options 
described below. A cluster-wide xxqs_name_sxx\_qselect file may be placed under 
\$xxQS_NAME_Sxx_ROOT/\$xxQS_NAME_Sxx_CELL/common/xxqs_name_sxx\_qselect. The user private file is searched at the location 
\$HOME/.xxqs_name_sxx\_qselect. The home directory request file has the highest precedence over the cluster global file. Command 
line can be used to override the flags contained in the files.

# OPTIONS

## -help  
Prints a listing of all options.

## -l *resource*\[=*value*\],...  
Defines the resources to be granted by the queues which should be included in the queue list output. Matching is 
performed on queues based on non-mutable resource availability information only. That means load values are always 
ignored except the so-called static load values (i.e. *arch*, *num\_proc*, *mem\_total*, *swap\_total* and 
*virtual\_total*) ones. Also, consumable utilization is ignored. If there are multiple `-l` resource requests they will 
be concatenated by a logical AND: a queue needs to offer all resources to be displayed.

## -pe *pe_name*,...  
Includes queues into the output which are attached to at least one of the parallel environments enlisted in the comma 
separated option argument.

## -q *wc_queue*,...  
Directly specifies the wildcard expression queue list to be included in the output. This option usually is only 
meaningful in conjunction with another `qselect` option to extract a subset of queue names from a list given by `-q`. 
Description of *wc_queue* can be found in xxqs_name_sxx_types(1).

## -qs {a\|c\|d\|o\|s\|u\|A\|C\|D\|E\|S}  
This option allows to filter for queue instances in certain states.

## -sdv
This switch is available in Gridware Cluster Scheduler only.

This option is only effective if the executing user has no administrative rights.

If the `-sdv` option is used, the output of the command will be restricted to a department specific view. This means
that following parts of the output will be suppressed:

* queue instance information where the user has no access rights
* queue instance information if the corresponding host is not accessible by the user

The department specific view can be enforced by adding the `-sdv` switch to the *$HOME/.xxqs_name_sxx_qselect* file.
Administrators can enforce this behavior by adding this switch to the default *xxqs_name_sxx_qselect* files

The `-sdv` option is also available in the `qstat` command where it has the same effect and additionally suppresses
information about hosts and jobs where the user has no access rights.

## -U *user*,...  
Includes the queues to which the specified users have access in the `qselect` output.

# EXAMPLES

    % qselect -l arch=lx-amd64
    % qselect -l arch=lx-amd64 -U joga,ebablick 
    % qconf -mattr queue h_vmem=1GB `qselect -l arch=lx-amd64`

The first example prints the names of those queues residing on Linux machines. 

The second command in addition restricts the output to those queues with access permission for the users 
*joga* and *ebablick*. The third command changes the queue attribute *h\_vmem* to 1 Gigabyte on queues residing on 
Linux machines (see the qconf(1) manual page for details on the `-mattr` option and the xxqs_name_sxx_queue_conf(5) 
manual page on details of queue configuration entries).

# ENVIRONMENTAL VARIABLES

For a complete list of common environment variables used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# FILES

For a complete list of files used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# SEE ALSO

xxqs_name_sxx_intro(1), qconf(1), qmod(1), qstat(1), xxqs_name_sxx_qselect(5), xxqs_name_sxx_queue_conf(5),

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
