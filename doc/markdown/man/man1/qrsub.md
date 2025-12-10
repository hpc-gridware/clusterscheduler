---
title: qrsub
section: 1
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

`qrsub` - Submit an Advance Reservation (AR) to xxQS_NAMExx.

# SYNTAX

`qrsub` \[`-a` *date_time*\] \[`-A` *account_string*\] \[`-ckpt` *ckpt_name*\] \[`-d` *time*\] \[`-e` *date_time*\] 
\[`-he` *yes*\|*no*\] \[`-help`\] \[`-l` *resource_list*\] \[`-now`\] \[`-M` *user*\[@*host*\],...\] 
\[`-N` *ar_name*\] \[`-w` e \| v \] \[`-pe` *pe_name* *slot_range*\] \[`-q` *wc_queue_list*\] \[`-u` *wc_user_list*\]

# DESCRIPTION

`Qrsub` provides a means for operators, managers or users referenced in the ACL (see xxqs_name_sxx_access_list(5)) 
*arusers* to create an Advance Reservation (AR) in the xxQS_NAMExx queuing system. ARs allow to reserve
particular consumable resources for future use. These reserved resources are only available for jobs requesting the 
AR and the scheduler ensures the availability of the resources when the start time is reached. Job requesting the AR 
can only use the reserved consumable resources.

During AR submit time the xxQS_NAMExx queuing system selects the best suited queues for the AR request and then 
reserves the desired amount of resources. For a reservation, all queues that are not in orphaned state are considered 
as suited. Only if the AR request can be fulfilled, the AR will be granted.

ARs will be deleted either automatically when the end time is reached, or manually using `qrdel`. In both cases, 
first all jobs requesting the AR will be removed and then AR itself. Already granted ARs can be shown
with `qrstat`.

`Qrsub` and `qrdel` operations can only be executed from submit hosts.

Note: To make AR behavior predictable, it is necessary to have reserved resources available at the time of AR start. 
This is done by keeping jobs with an unlimited runtime limit separated from ARs, and not considering resources used 
by such jobs for reservation.

Note: Resource Quotas are not considered for AR queue selection and nor for jobs requesting an AR.

When an AR was successfully added to the xxQS_NAMExx queuing system `qrsub` returns a unique integer ID referring 
the newly created AR. The highest AR ID is 9999999. If the highest ID is reached, a wraparound happens and the 
next unused ID, starting with 1, will be used.

For `qrsub`, the administrator and the user may define default request files (analogous to xxQS_NAMExx_request for 
`qsub`), which can contain any of the possible command line options. A cluster wide default request file is optional. 
If such a default  request file is used, it must be placed under 
*$xxQS_NAME_Sxx_ROOT/$xxQS_NAME_Sxx_CELL/common/sge_ar_request* (global defaults file).  
A user private default request file is optional. If it is used, it must be placed under  
*$HOME/.sge_ar_request* (user private defaults file).

# OPTIONS

## -a *date_time*
Defines the activation (start) date and time of an AR. The option is not mandatory. If omitted, the current 
*date_time* is assumed. Either a duration or end date_time must also be specified. For details about
date_time please see sge_types(1)

## -A *account_string*  
Identifies the account to which the resource reservation of the AR should be charged. For *account_string* 
value details please see the *name* definition in sge_types(1). In the absence of this parameter xxQS_NAMExx 
will place the default account string "xxqs_name_sxx" in the accounting record of the AR.

## -ckpt *ckpt_name*
Selects the checkpointing environment (see xxqs_name_sxx_checkpoint(5)) the AR jobs may request. Using this 
option guarantees queues that only providing this checkpoint environment will be reserved.

## -d *time*
Defines the duration of the AR. The use of `-d` *time* is optional if `-e` *date_time* is requested. For 
details about *time* definition please see xxqs_name_sxx_types(1).

## -e *date_time*  
Defines the end date and time of an AR. The use of `-e` *date_time* is optional if `-d` *time* is requested. 
For details about *date_time* definition please see xxqs_name_sxx_types(1).

## -he *y*\[*es*\]\|*n*\[*o*\]  
Specifies the behavior when the AR goes into an error state. The AR goes into error state when a reserved host 
goes into unknown state, a queue error happens, or when a queue is disabled or suspended.

A hard error, `-he yes`, means as long as the AR is in error state no jobs using the AR will be scheduled. If 
soft error, `-he no`, is specified the AR stays usable with the remaining resources. By default, soft error 
handling is used.  

## -help  
Prints a list of all options.

## -l *resource*=*value*,...  
Creates an AR in a xxQS_NAMExx queue, providing the given resource request list. xxqs_name_sxx_complex(5) 
describes how a list of available *resources* and their associated valid *value* specifiers can be obtained.  
There may be multiple `-l` switches in a single command.

## -m *b*\|*e*\|*a*\|*n*  
Defines or redefines under which circumstances mail is to be sent to the AR owner or to the users defined with 
the `-M` option described below. The option arguments have the following meaning:

* *b*: Mail is sent at the beginning of the AR
* *e*: Mail is sent at the end of the AR 
* *a*: Mail is sent when the AR goes into error state
* *n*: No mail is sent, default for qrsub

## -M *user*\[@*host*\],...  
Defines or redefines the list of users to which the qmaster sends mail.

## -masterq *wc_queue_list*
Only meaningful for a parallel AR request together with the `-pe` option. This option is used to reserve the 
proper queues to match this request if it would be requested by a `qsub`. A more detailed description of
*wc_queue_list* can be found in xxqs_name_sxx_types(1).

## -now *y*\[*es*\]\|*n*\[*o*\]  
This options impacts the queues selection for reservation. With the `-now y` option, only queues with the 
qtype "INTERACTIVE" assigned will be considered for reservation. `-now n` is the default for `qrsub`.

## -N *name*  
The name of the AR. The name, if requested, must conform to *name* as defined in xxqs_name_sxx_types(1). 
Invalid names will be denied at submit time.

## -w *e*\|*v*  
Specifies the validation level applied to the AR request.

The specifiers *e* and *v* define the following validation modes:

* *v*: verify - does not submit the AR but prints extensive validation report
* *e*: error - rejects request if requirements cannot fulfilled, default for `qrsub`

## -pe *parallel_env* *n*\[-\[*m*\]\] \| [-\[*m*\]\],...  
Parallel programming environment (PE) to select for the AR queue reservation. Please see the details of a PE 
in xxqs_name_sxx_pe(5).

## -q *wc_queue_list*
Defines or redefines a list of cluster queues, queue domains or queue instances, that may be reserved by the AR. 
Please find a description of *wc_queue_list* in xxqs_name_sxx_types(1). This parameter has all the properties of 
a resource request and will be merged with requirements derived from the `-l` option described above.

## -u \[*username* \| @*access_list*\],...  
Defines the users allowed to submit jobs requesting the AR. The access is specified by a comma separated list 
containing UNIX users or ACLs (see xxqs_name_sxx_access_list(5)). prefixing the ACL name with an '@' sign.  
By default only the AR owner is allowed to submit jobs requesting the AR.

Note: Only queues, where all users specified in the list have access, are considered for reservation (see
xxqs_name_sxx_queue_conf(5)).

# ENVIRONMENTAL VARIABLES

For a complete list of common environment variables used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# FILES

For a complete list of files used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

## $xxQS_NAME_Sxx_ROOT/$xxQS_NAME_Sxx_CELL/common/xxqs_name_sxx_ar_request
global defaults file

## $HOME/.xxqs_name_sxx__ar_request
user private defaults file

# SEE ALSO

qrdel(1), qrstat(1), qsub(1), xxqs_name_sxx_types(1), xxqs_name_sxx_checkpoint(5), xxqs_name_sxx_complex(5), 
xxqs_name_sxx_queue_conf(5), xxqs_name_sxx_pe(5), xxqs_name_sxx_resource_quota(5).

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
