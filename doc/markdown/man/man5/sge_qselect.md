---
title: sge_qselect
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

sge_qselect - xxQS_NAMExx default qselect file format

# DESCRIPTION

sge_qselect defines the command line switches that will be used by qselect by default. If available, the default 
sge_qselect file is read and processed by qselect(1).

There is a cluster global and a user private sge_qselect file. The user private file has the highest precedence and 
is followed by the cluster global sge_qselect file. Command line switches used with qselect(1) override all switches 
contained in the user private or cluster global sge_qselect file.

The format of the default files is:

-   The default sge_qselect file may contain an arbitrary number of lines. Blank lines and lines with a '#' sign 
    at the first column are skipped. Each line not to be skipped may contain any qselect(1) option as described 
    in the xxQS_NAMExx Reference Manual. More than one option per line is allowed.

# EXAMPLES

The following is a simple example of a default sge_qselect file:

    =====================================================
    # Just show me my own running and suspended jobs
    -sdv
    =====================================================

Having defined a default sge_qselect file like this and using qselect as follows:

    qselect 

has the same effect as if qselect was executed with:

    qselect -sdv

# FILES

For a complete list of common environment variables used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

## <xxqs_name_sxx_root>/<cell>/common/sge_qselect 
global defaults file
    
## $HOME/.sge_qselect	
user private defaults file

# SEE ALSO

xxqs_name_sxx_intro(1), qselect(1), 

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
