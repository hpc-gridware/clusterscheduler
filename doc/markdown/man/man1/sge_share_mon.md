---
title: sge_share_mon
section: 1
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

`sge_share_mon` - Shows details about xxQS_NAMExx share tree part of the scheduler policy system.

# SYNTAX

sge_share_mon \[-c count\] \[-d delimiter\] \[-f field \[,field,...] ] \] \[-h\] 
              \[-i interval\] \[-l delimiter\] \[-m output_mode\] \[-n\] 
              \[-o output_file\] \[-r delimiter\] \[-s string_format\] 
              \[-t\] \[-u\] \[-x\] \[node_names ...\]

# DESCRIPTION

The `sge_share_mon` command displays the current status of the xxQS_NAMExx share tree nodes. Without any options selected, the command
will display a formatted view of the share tree every 15 seconds.

If node names are specified, only the matching nodes will be displayed. If no node names are specified,
all nodes in the share tree will be displayed. The output can be customised using various options, such as
the fields to be printed, the output format and the collection interval.

# OPTIONS

## -c *count*

Specifies the number of collections to be made. The default is *infinite*, the command will run until it is
interrupted by the user.

## -d *delimiter*

Specifies the delimiter between columns in the output. The default is a *tab* character.

## -f *field* \[,*field*,...\]

Specifies the fields to be printed in the output. The fields can be specified by their names. Available names are
specified in the section OUTPUT FORMAT below. The default is to print all fields.

## -h

Prints a header containing the field names before the first collection.

## -help  

Prints a listing of all options.

## -i *interval*

Specifies the interval in seconds between collections. The default is 15 seconds.

## -l *delimiter*

Specifies the delimiter between nodes in the output. The default is a carriage return character.

## -m *output_mode*

Specifies the output file fopen mode. The default is "w" (write). Other modes like "a" (append) can be used.

## -n

If specified, the output will be in name=value format. This is useful for parsing the output programmatically.

## -o *output_file*

Specifies the output file where the results will be written. If not specified, the output will be written to
stdout. 

## -r *delimiter*

Specifies the delimiter between collection records in the output. The default is a carriage return character.

## -s *string_format*

Specifies the format of displayed strings. The default is "%s", which means that strings will be printed as they are.

## -t

If specified, the command will show formatted times in the output. This is useful for displaying time-related
information in a human-readable format.

## -u

If specified, the command will show decayed usage (since timestamp) in nodes. This means that the output will
include information about how much share usage has decayed over time for each node in the share tree.

## -x

If specified, the command will exclude non-leaf nodes from the output. This means that only the leaf nodes of
the share tree will be displayed, which can be useful for focusing on the actual resources being used.

# OUTPUT FORMAT

The output format of `sge_share_mon` can be customized using the `-f` option. The available fields are:

* *curr_time*: the time of the last status collection (either in seconds since epoch or formatted if `-t` is used)
* *usage_time*: the time of the last time the usage was updated
* *node_name*: the name of the node
* *user_name*: the name of the user if this is a user node
* *project_name*: the name of the project if this is a project node
* *shares*: the number of shares assigned to the node
* *job_count*: the number of active jobs associated to this node
* *level%*: the share percentage of this node amongst its siblings
* *total%*: the overall share percentage of this node amongst all nodes
* *long_target_share*: the long term target share that we are trying to achieve
* *short_target_share*: the short term target share that we are trying to achieve in order to meet the long term target
* *actual_share*: the actual share that the node is receiving based on usage
* *usage*: the combined and decayed usage for this node

Additionally, all nodes also contain the following fields:

* *cpu*: the accumulated and decayed CPU time for this node
* *mem*: the accumulated and decayed memory usage for this node. This represents the amount of virtual memory used by 
  processes multiplied by the user and system CPU time. The value is expressed in gigabyte seconds.
* *io*: the accumulated and decayed I/O usage for this node
* *ltcpu*: the total accumulated CPU time for this node
* *ltmem*: the total accumulated memory usage (in gigabyte seconds) for this node
* *ltio*: the total accumulated I/O usage for this node

# ENVIRONMENTAL VARIABLES

For a complete list of common environment variables used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# FILES

For a complete list of files used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# SEE ALSO

xxqs_name_sxx_intro(1), xxqs_name_sxx_sched_conf(5)

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
