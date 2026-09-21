---
title: qrstat
section: 1
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

`qrstat` - show the status of xxQS_NAMExx Advance Reservations (AR)

# SYNTAX

`qrstat` \[`-ar` *ar_id*,...\] \[`-help`\] \[`-u` *user*,...\] \[`-explain`\]

# DESCRIPTION

`qrstat` shows the current status of the available xxQS_NAMExx ARs. The selection option `-ar` allows you to 
get information about specific AR.

The administrator and the user may define files which can contain any of the options described below. 
A cluster-wide sge_qrstat file may be placed under *$xxQS_NAME_Sxx_ROOT/$xxQS_NAME_Sxx_CELL/common/sge_qrstat*
The user private file is searched at the location *$HOME/.sge_qrstat*. The home directory request file has the 
highest precedence over the cluster global file. Command line can be used to override the flags contained in
the files.

# OPTIONS

## `-ar` *ar_id*,...  
Prints various information about the ARs identified by given *ar_id* list.

## `-explain`  
Displays the reason for the error state of an AR. Possible reasons are the unknown state of a host or queue instance.

The output format for the alarm reasons is one line per reason.

## `-help`
Prints a listing of all options.

## `-u` *user*,...  
Display information only for those ARs created by the users from the given user list.

The string *$user* is a placeholder for the current username. An asterisk "\*" can be used as username wildcard 
to request that all users' ARs be displayed. `qrstat` without explicit use of the `-u` switch will behave the same
way as if `-u $user` was specified.

## `-xml`  
This option can be used with all other options and changes the output to XML. The used schemas are referenced in 
the XML output. The output is printed to *stdout*.  

# OUTPUT FORMATS

Depending on the presence or absence of the `-ar` option there are two output formats need to be differentiated.

## Advance Reservation Summary (without -ar)

Following the header line, a section for each AR is provided. The  columns contain information for

* the AR id.
* the name of the AR.
* the current state of the AR. One of following states 'wWrEd".
  * w - waiting without error
  * W - warning (effective - waiting with error)
  * r - running
  * E - error (effective - running with error)
  * d - deleted 
* the start time of the AR.
* the end time of the AR.
* the duration of the AR.

## Detailed Format (with -ar)

The output contains two columns. The first one contains all AR attributes. The second one the corresponding value.

An attribute which does not apply to a reservation is left out. A reservation which requests no
parallel environment, for instance, has no *granted_parallel_environment* line at all. The
attributes are printed in the order below.

* *id* - the identifier of the AR, assigned when the AR was submitted.
* *name* - the name of the AR, as given with `qrsub -N`. Empty if the AR was submitted without one.
* *owner* - the user who submitted the AR.
* *state* - the current state of the AR, one of the letters listed for the summary format above.
* *start_time* and *end_time* - the beginning and the end of the reservation.
* *duration* - the length of the reservation, as *hours*:*minutes*:*seconds*.
* *message* - the reason the AR is in error state, one line per reason. Printed only while the AR
  has such a reason. This is the same information the `-explain` option adds to the summary format.
* *submission_time* - the time the AR was submitted.
* *group* - the UNIX group of the AR owner.
* *account* - the account string given with `qrsub -A`.
* *binding* - the core binding the AR requested, as a comma separated list of the parameters the
  `qrsub` switches `-bamount`, `-btype`, `-bunit`, `-bstrategy`, `-bstart`, `-bstop`, `-bsort` and
  `-binstance` set, for example *bamount=2,btype=core,bunit=core*.
* *resource_list* - the resources the AR requested, as *name*=*value* pairs, in the form they were
  given to `qrsub -l`.
* *error_handling* - *true* when the AR was submitted with hard error handling, `qrsub -he yes`.
  Not printed for the default, soft error handling.
* *exec_binding_list* - the cores the AR holds, one entry per execution host, as a topology string.
  Lower case letters are the cores the AR holds, upper case letters the ones it does not.
* *granted_resources_list* - the resource maps the AR holds, one line per execution host, with the
  identifiers of the granted instances. See below.
* *exec_queue_list* - the queue instances the AR reserved and the number of slots it holds in each,
  as *queue*=*slots*.
* *granted_parallel_environment* - the parallel environment the AR was granted, followed by the
  slot range it was submitted with, in the form *pe_name* slots *range*.
* *master hard queue_list* - the queues the AR requested for its master task, as given with
  `qrsub -masterq`.
* *checkpoint_name* - the checkpointing environment the AR requested, as given with `qrsub -ckpt`.
* *mail_options* - when mail about the AR is sent, in the letters `qrsub -m` accepts.
* *mail_list* - the addresses mail about the AR is sent to, as *user*@*host*.
* *acl_list* and *xacl_list* - the users and access lists which may, and which may not, submit jobs
  into the AR, as given with `qrsub -u`.

*granted_resources_list* is printed with one line per execution host:

    granted_resources_list         node01: gpu=2(gpu0 gpu1)
                                   node02: gpu=1(gpu3)

The attribute name appears on the first line only; the lines which follow are indented to the same
column. The value is written the way a resource map is written in a host's *complex_values*, so an
amount on its own means the AR holds that many instances without naming them.

# ENVIRONMENTAL VARIABLES

For a complete list of common environment variables used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# FILES

For a complete list of files used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# SEE ALSO

xxqs_name_sxx_intro(1), qrsub(1), qrdel(1), qsub(1),

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
