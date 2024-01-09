---
title: qacct
section: 1
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

`qacct` - report and account for xxQS_NAMExx usage

# SYNOPSIS

`qacct` \[`-ar` \[*ar_id*\]\] \[`-A` *account*\] \[`-b` *begin_time*\] \[`-d` *days*\] \[`-e` *end_time*\] 
\[`-g` \[*group_id*\ | *group_name*\]\] \[ `-h` \[*hostname*\]\] \[`-help`\] 
\[`-j` \[*job_id* | *job_name* | *pattern*\]\] \[`-l` *attr*=*val*,...\] \[`-o` \[*owner*\]\]
\[`-pe` \[*pe_name*\]\] \[`-q` \[*wc_queue*\]\] \[`-slots` \[*slot_number*\]\] \[`-t` *task_id_range_list*\] 
\[`-P` \[*project*\]\] \[`-D` \[*department*\]\] \[`-f` *acct_filename*\]

# DESCRIPTION

The `qacct` utility scans the accounting data file (see xxqs_name_sxx_accounting(5)) and produces a summary of 
information for wall-clock time, cpu-time, and system time for the categories of hostname, queue-name, group-name, 
owner-name, job-name, job-ID and for the queues meeting the resource requirements as specified with the `-l` switch.
Combinations of each category are permitted. Alternatively, all or specific jobs can be listed with the `-j` switch. 
For example the search criteria could include summarizing for a queue and an owner, but not for two queues in the same 
request.

# OPTIONS

## -ar \[*ar_id*\]
The ID of the advance reservation for which usage is summarized. If `ar_id` is not given, accounting data is listed 
for each advance reservation separately.

## -A *account*  
The *account* for jobs to be summarized.

## -b *begin_time*  
*begin_time* specifies earliest start time for jobs to be summarized, in the format 
\[\[*CC*\]*YY*\]*MMDDhhmm*\[.*SS*\]. See also `-d` option. If *CC* is not specified, a *YY* of \< 70 means *20YY*.

## -d *days*  
The number of *days* to summarize and print accounting information on. If used together with the `-b` *begin_time* 
option (see above), jobs started within *begin_time* to *begin_time*+*days* are counted. If used together with 
the `-e` *end_time* (see below) option, count starts at
*end_time*-*days*.

## -e *end_time*  
The latest start time for jobs to be summarized, in the format \[\[*CC*\]*YY*\]*MMDDhhmm*\[.*SS*\]. 
See also `-d` option. If *CC* is not specified, a *YY* of \< 70 means *20YY*.

## \[-f *acct_filename*\]  
*acct_filename* specifies the accounting file to be used. If omitted, the system default accounting file is processed.

## -g \[*group_id* | *group_name*\]  
The numeric system group id (*group_id*) or the group alphanumeric name (*group_name*) of the job owners to be included 
in the accounting. If *group_id*/*group_name* is omitted, all groups are accounted.

## -h \[*hostname*\]  
The case-insensitive name of the host upon which accounting information is requested. If the *hostname* is omitted, 
totals for each host are listed separately.

## -help  
Display help information for the `qacct` command.

## -j \[*job_id* | *job_name* | *pattern*\]  
The name, an expression for matching names, or ID of the job during execution for which accounting information is 
printed. If neither a name nor an ID is given all jobs are enlisted. This option changes the output format of 
`qacct`. If activated, CPU times are no longer accumulated but the "raw" accounting information is printed in a 
formatted form instead. See accounting(5) for an explanation of the displayed information.

## -l *attr=val,...*  
A resource requirement specification which must be met by the queues in which the jobs being accounted were 
executing. The resource request is very similar to the one described in qsub(1).

## -o \[*owner*\]  
The name of the *owner* of the jobs for which accounting statistics are assembled. If the optional *owner* argument 
is omitted, a listing of the accounting statistics of all job owners being present in the accounting file is produced.

## -pe \[*pe_name*\]  
*pe_name* specifies the name of the parallel environment for which usage is summarized. If *pe_name* is not given, 
accounting data is listed for each parallel environment separately.

## -q \[*wc_queue_name*\]  
A expression for queues for which usage is summarized. All queue instances matching the expression will be listed. 
If no expression is specified, a cluster queue summary will be given.

## -slots \[*slot_number*\]  
The number of queue slots for which usage is summarized. If
*slot_number* is not given, accounting data is listed for each number
of queue slots separately.

## -t *task_id_range_list* 
Only available together with the `-j` option described above. 

The `-t` switch specifies the array job task range, for which accounting information should be printed. Syntax and 
semantics of *task_id_range_list* are identical to that one described under the `-t` option to qsub(1). 
Please see there also for further information on array jobs.

## -P \[*project*\]  
The name of the project for which usage is summarized. If *project* is not given, accounting data is listed for 
each owning project separately.

## -D \[*department*\]  
The name of the department for which usage is summarized. If *department* is not given, accounting data is listed 
for each owning department separately.

# ENVIRONMENTAL VARIABLES

## xxQS_NAME_Sxx_ROOT
Specifies the location of the xxQS_NAMExx standard configuration files.

## xxQS_NAME_Sxx_CELL
If set, specifies the default xxQS_NAMExx cell. To address a xxQS_NAMExx cell `qacct` uses (in the order of precedence):

* The name of the cell specified in the environment variable xxQS_NAME_Sxx_CELL, if it is set.
* The name of the default cell, i.e. **default**.

## xxQS_NAME_Sxx_DEBUG_LEVEL
If set, specifies that debug information should be written to stderr. In addition, the level of detail in which debug 
information is generated is defined.

## xxQS_NAME_Sxx_QMASTER_PORT
If set, specifies the TCP port on which xxqs_name_sxx_qmaster(8) is expected to listen for communication requests. 
Most installations will use a services map entry for the service *sge_qmaster* instead to define that port.

## xxQS_NAME_Sxx_EXECD_PORT
If set, specifies the tcp port on which xxqs_name_sxx_execd(8) is expected to listen for communication requests. 
Most installations will use a services map entry for the service *sge_execd* instead to define that port.

# FILES

## \<xxQS_NAME_Sxx_ROOT\>/\<xxQS_NAME_Sxx_CELL\>/common/accounting
xxQS_NAMExx default accounting file
    
## \<xxQS_NAME_Sxx_ROOT\>/\<xxQS_NAME_Sxx_CELL\>/common/act_qmaster
xxQS_NAMExx master host file

# SEE ALSO

xxqs_name_sxx_intro(1), qsub(1), xxqs_name_sxx_accounting(5), xxqs_name_sxx_qmaster(8)

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
