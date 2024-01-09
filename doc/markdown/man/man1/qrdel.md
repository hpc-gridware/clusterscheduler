---
title: qrdel
section: 1
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

qrdel - delete xxQS_NAMExx Advance Reservations (AR)

# SYNTAX

**qrdel** **\[-f\]** **\[-help** **\[-u wc_user_list\]** **wc_ar_list**

# DESCRIPTION

*Qrdel* provides a means for a user/operator/manager to delete one or
more Advance Reservations (AR). A manager/operator can delete ARs
belonging to any user, while a regular user can only delete his or her
own ARs. If a manager wants to delete another user's AR, the manager can
specify the AR id. By default, "qrdel wc_ar_name" will delete only the
ARs belonging to that user. A manager is able to delete another user's
AR via "-u wc_user_list". Jobs referring to an AR tagged for deletion
will also be removed. Only if all jobs referring to an AR are removed
from the xxQS_NAMExx database will the AR also be removed.

*Qrdel* deletes ARs in the order in which the AR identifiers are
presented. Find additional information concerning *wc_user_list* and
*wc_ar_list* in *sge_types*(1).

# OPTIONS

-f  
Force action for AR. The AR and the jobs using the AR are deleted from
the xxQS_NAMExx queuing system even if the *xxqs_name_sxx_execd*(8)
controlling the AR jobs does not respond to the delete request sent by
the *xxqs_name_sxx_qmaster*(8).  
Users which have neither xxQS_NAMExx manager nor operator status can
only use the **-f** option for their own ARs.

-help  
Prints a list of all options.

-u wc_user_list  
Deletes only those ARs which were submitted by users specified in the
list of usernames. For managers, it is possible to use **qrdel -u "\*"**
to delete all ARs for all users. If a manager wants to delete a specific
AR for a user, he has to specify the user and the AR id. If no AR is
specified, all ARs belonging to that user are deleted.

wc_ar_list  
A list of AR id's that should be deleted

# ENVIRONMENTAL VARIABLES

xxQS_NAME_Sxx_ROOT  
Specifies the location of the xxQS_NAMExx standard configuration files.

xxQS_NAME_Sxx_CELL  
If set, specifies the default xxQS_NAMExx cell. To address a xxQS_NAMExx
cell *qrdel* uses (in the order of precedence):

> The name of the cell specified in the environment variable
> xxQS_NAME_Sxx_CELL, if it is set.
>
> The name of the default cell, i.e. **default**.

xxQS_NAME_Sxx_DEBUG_LEVEL  
If set, specifies that debug information should be written to stderr. In
addition the level of detail in which debug information is generated is
defined.

xxQS_NAME_Sxx_QMASTER_PORT  
If set, specifies the tcp port on which the *xxqs_name_sxx_qmaster*(8)
is expected to listen for communication requests. Most installations
will use a service map entry for the service "sge_qmaster" instead of
defining the port.

# FILES

    <xxqs_name_sxx_root>/<cell>/common/act_qmaster
    	xxQS_NAMExx master host file

# SEE ALSO

*xxqs_name_sxx_intro*(1), *qrstat*(1), *qrsub*(1), *qsub*(1),
*xxqs_name_sxx_qmaster*(8), *xxqs_name_sxx_execd*(8).

# COPYRIGHT

See *xxqs_name_sxx_intro*(1) for a full statement of rights and
permissions.
