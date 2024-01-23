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

# ENVIRONMENTAL VARIABLES

For a complete list of common environment variables used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# FILES

For a complete list of files used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# SEE ALSO

xxqs_name_sxx_intro(1), qrsub(1), qrdel(1), qsub(1),

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
