---
title: qrdel
section: 1
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

`qrdel` - delete xxQS_NAMExx Advance Reservations (AR)

# SYNTAX

`qrdel` \[`-f`\] \[`-help`\] \[`-u` *wc_user_list*\] *wc_ar_list*

# DESCRIPTION

`Qrdel` provides a means for a user/operator/manager to delete one or more advance reservations (AR). 
A manager/operator can delete ARs belonging to any user, while a regular user can only delete his or her
own ARs. If a manager wants to delete another user's AR, the manager can specify the AR id. 

By default, `qrdel` *wc_ar_name* will delete only the ARs belonging to that user. A manager is able to delete another 
user's AR via `-u` *wc_user_list*. Jobs referring to an AR tagged for deletion will also be removed. 
Only if all jobs referring to an AR are removed from the xxQS_NAMExx database will the AR also be removed.

`Qrdel` deletes ARs in the order in which the AR identifiers are presented. Find additional information 
concerning *wc_user_list* and *wc_ar_list* in xxqs_name_sxx_types(1).

# OPTIONS

## -f  
Force action for AR. The AR and the jobs using the AR are deleted from the xxQS_NAMExx queuing system even if the 
xxqs_name_sxx_execd(8) controlling the AR jobs does not respond to the delete request sent by the 
xxqs_name_sxx_qmaster(8). Users which have neither xxQS_NAMExx manager nor operator status can
only use the `-f` option for their own ARs.

## -help  
Prints a list of all options.

## -u *wc_user_list*
Deletes only those ARs which were submitted by users specified in the list of usernames. For managers, it is 
possible to use `qrdel -u "\*"` to delete all ARs for all users. If a manager wants to delete a specific
AR for a user, he has to specify the user and the AR id. If no AR is specified, all ARs belonging to that user are deleted.

## *wc_ar_list*
A list of AR ID's that should be deleted

# ENVIRONMENTAL VARIABLES

For a complete list of common environment variables used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# FILES

For a complete list of files used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# SEE ALSO

xxqs_name_sxx_intro(1), qrstat(1), qrsub(1), qsub(1), xxqs_name_sxx_qmaster(8), xxqs_name_sxx_execd(8).

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
