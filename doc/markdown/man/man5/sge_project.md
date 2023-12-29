---
title: sge_project
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_project - xxQS_NAMExx project entry file format

# DESCRIPTION

Jobs can be submitted to projects, and a project can be assigned with a
certain level of importance via the functional or the override policy.
This level of importance is then inherited by the jobs executing under
that project.

A list of currently configured projects can be displayed via the
*qconf* (1) **-sprjl** option. The contents of each enlisted project
definition can be shown via the **-sprj** switch. The output follows the
*project* format description. New projects can be created and existing
can be modified via the **-aprj**, **-mprj** and **-dprj** options to
*qconf* (1).

Note, xxQS_NAMExx allows backslashes (\\) be used to escape newline
(\\newline) characters. The backslash and the newline are replaced with
a space (" ") character before any interpretation.

# FORMAT

A project definition contains the following parameters:

## **name**

The project name as defined for *project_name* in *sge_types* (1).

## **oticket**

The amount of override tickets currently assigned to the project.

## **fshare**

The current functional share of the project.

## **acl**

A list of user access lists (ACLs - see *xxqs_name_sxx_access_list* (5)) referring to
those users being allowed to submit jobs to the project.

If the **acl** parameter is set to NONE, all users are allowed to submit
jobs to the project except for those listed in **xacl** parameter
described below.

## **xacl**

A list of user access lists (ACLs - see *xxqs_name_sxx_access_list* (5)) referring to
those users being not allowed to submit jobs to the project.

# SEE ALSO

*xxqs_name_sxx_intro* (1), *xxqs_name_sxx_types* (1), *qconf* (1),
*xxqs_name_sxx_access_list* (5).

# COPYRIGHT

See *xxqs_name_sxx_intro* (1) for a full statement of rights and
permissions.
