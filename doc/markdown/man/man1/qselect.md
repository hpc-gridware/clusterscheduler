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

# OPTIONS

## -help  
Prints a listing of all options.

## -l resource\[=value\],...  
Defines the resources to be granted by the queues which should be included in the queue list output. Matching is 
performed on queues based on non-mutable resource availability information only. That means load values are always 
ignored except the so-called static load values (i.e. *arch*, *num_proc*, *mem_total*, *swap_total* and 
*virtual_total*) ones. Also, consumable utilization is ignored. If there are multiple `-l` resource requests they will 
be concatenated by a logical AND: a queue needs to offer all resources to be displayed.

## -pe pe_name,...  
Includes queues into the output which are attached to at least one of the parallel environments enlisted in the comma 
separated option argument.

## -q wc_queue,...  
Directly specifies the wildcard expression queue list to be included in the output. This option usually is only 
meaningful in conjunction with another `qselect` option to extract a subset of queue names from a list given by `-q`. 
Description of *wc_queue* can be found in xxqs_name_sxx_types(1).

## -qs {a\|c\|d\|o\|s\|u\|A\|C\|D\|E\|S}  
This option allows to filter for queue instances in certain states.

## -U user,...  
Includes the queues to which the specified users have access in the `qselect` output.

# EXAMPLES

    % qselect -l arch=lx-amd64
    % qselect -l arch=lx-amd64 -U joga,ebablick 
    % qconf -mattr queue h_vmem=1GB `qselect -l arch=lx-amd64`

The first example prints the names of those queues residing on Linux machines. 

The second command in addition restricts the output to those queues with access permission for the users 
*joga* and *ebablick*. The third command changes the queue attribute *h_vmem* to 1 Gigabyte on queues residing on 
Linux machines (see the qconf(1) manual page for details on the `-mattr` option and the xxqs_name_sxx_queue_conf(5) 
manual page on details of queue configuration entries).

# ENVIRONMENTAL VARIABLES

For a complete list of common environment variables used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# FILES

For a complete list of files used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# SEE ALSO

xxqs_name_sxx_intro(1), qconf(1), qmod(1), qstat(1), xxqs_name_sxx_queue_conf(5),

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
