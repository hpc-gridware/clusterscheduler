---
title: sge_qhost
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

sge_qhost - xxQS_NAMExx default qhost file format

# DESCRIPTION

sge_qhost defines the command line switches that will be used by qhost by default. If available, the default sge_qhost file is read and processed by qhost(1).

There is a cluster global and a user private sge_qhost file. The user private file has the highest precedence and is followed by the cluster global sge_qhost file. Command line switches used with qhost(1) override all switches contained in the user private or cluster global sge_qhost file.

The format of the default files is:

* The default sge_qhost file may contain an arbitrary number of lines. 
* Blank lines and lines with a '#' sign at the first column are skipped. 
* Each line not to be skipped may contain any qhost(1) option as described in the xxQS_NAMExx Reference Manual. 
* More than one option per line is allowed.

# EXAMPLES

The following is a simple example of a default sge_qhost file:

    =====================================================================
    # display queues 
    -q
    # show only hosts/queues that allow access and jobs of the department
    -sdv
    =====================================================================

Having defined a default sge_qhost file like this and using qhost as follows:

    qhost

has the same effect as if qhost was executed with:

    qhost -q -sdv

# FILES

For a complete list of common environment variables used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

## \<xxQS_NAME_Sxx_ROOT\>/\<xxQS_NAME_Sxx_CELL\>/common/sge_qhost
global defaults file
    
## $HOME/.sge_qhost
user private defaults file

# SEE ALSO

xxqs_name_sxx_intro(1), qhost(1), 

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
