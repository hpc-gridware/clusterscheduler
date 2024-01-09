---
title: sge_user
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_user - xxQS_NAMExx user entry file format

# DESCRIPTION

A user entry is used to store ticket and usage information on a per-user basis. Maintaining user entries for all 
users participating in a xxQS_NAMExx system is required if xxQS_NAMExx is operated under a user share tree policy.

If the `enforce_user` cluster configuration parameter is set to *auto*, a user object for the submitting user will 
be created automatically during job submission, if one does not already exist. The `auto_user_oticket`, 
`auto_user_fshare`, `auto_user_default_project`, and `auto_user_delete_time` cluster configuration parameters will 
be used as default attributes of the new user object.

A list of currently configured user entries can be displayed via the qconf(1) `-suserl` option. The contents of each 
enlisted user entry can be shown via the `-suser` switch. The output follows the `user` format description. New 
user entries can be created and existing can be modified via the `-auser`, `-muser` and `-duser` options to qconf(1).

Note, xxQS_NAMExx allows backslashes (\\) be used to escape newline characters. The backslash and the newline are 
replaced with a space (" ") character before any interpretation.

# FORMAT

A user entry contains four parameters:

## name

The user name as defined for *user_name* in sge_types(1).

## oticket

The amount of override tickets currently assigned to the user.

## fshare

The current functional share of the user.

## default_project

The default project of the user.

## delete_time

**Note:** Deprecated, may be removed in future release. The wall-clock time when this user will be deleted, 
expressed as the number of seconds elapsed since January 1, 1970. If set to zero, the affected user is a permanent 
user. If set to one, the user currently has active jobs. For additional information about automatically created
users, see the `enforce_user` and `auto_user_delete_time` parameters in sge_conf(5).

# SEE ALSO

xxqs_name_sxx_intro(1), xxqs_name_sxx_types(1), qconf(1), sge_conf(5)

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
