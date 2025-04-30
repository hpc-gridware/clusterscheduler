---
title: sge_qstat
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

sge_qstat - xxQS_NAMExx default qstat file format

# DESCRIPTION

sge_qstat defines the command line switches that will be used by qstat by default. If available, the default 
sge_qstat file is read and processed by qstat(1).

There is a cluster global and a user private sge_qstat file. The user private file has the highest precedence and 
is followed by the cluster global sge_qstat file. Command line switches used with qstat(1) override all switches 
contained in the user private or cluster global sge_qstat file.

The format of the default files is:

-   The default sge_qstat file may contain an arbitrary number of lines. Blank lines and lines with a '#' sign 
    at the first column are skipped. Each line not to be skipped may contain any qstat(1) option as described 
    in the xxQS_NAMExx Reference Manual. More than one option per line is allowed.

# EXAMPLES

The following is a simple example of a default sge_qstat file:

    =====================================================
    # Just show me my own running and suspended jobs
    -s rs -u $user
    =====================================================

Having defined a default sge_qstat file like this and using qstat as follows:

    qstat 

has the same effect as if qstat was executed with:

    qstat -s rs -u <current_user>

# FILES

For a complete list of common environment variables used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

## \<xxQS_NAME_Sxx_ROOT\>/\<xxQS_NAME_Sxx_CELL\>/common/sge_qstat 
global defaults file
    
## $HOME/.sge_qstat	
user private defaults file

# SEE ALSO

xxqs_name_sxx_intro(1), qstat(1), 

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
