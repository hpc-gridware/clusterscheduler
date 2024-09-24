---
title: sge_request
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_request - xxQS_NAMExx default request definition file format

# DESCRIPTION

*xxqs_name_sxx_request* reflects the format of the files to define default request profiles. If available, 
default request files are read and processed during job submission before any submit options embedded in the job 
script and before any options in the qsub(1) or qsh(1) command-line are considered. Thus, the command-line and 
embedded script options may overwrite the settings in the default request files (see qsub(1) or qsh(1) for details).

There is a cluster global, a user private and a working directory local default request definition file. The 
working directory local default request file has the highest precedence and is followed by the user private and 
then the cluster global default request file.

Note, that the `-clear` option to qsub(1) or qsh(1) can be used to discard any previous settings at any time in 
a default request file, in the embedded script flags or in a qsub(1) or qsh(1) command-line option.

The format of the default request definition files is:

-   The default request files may contain an arbitrary number of lines. Blank lines and lines with a '#' sign in 
    the first column are skipped.

-   Each line not to be skipped may contain any qsub(1) option as described in the xxQS_NAMExx Reference Manual. 
    More than one option per line is allowed. The batch script file and argument options to the batch script are 
    not considered as qsub(1) options and thus are not allowed in a default request file.

# EXAMPLES

The following is a simple example of a default request definition file:

    =====================================================
    # Default Requests File

    # request group to be sun4 and a CPU-time of 5hr
    -l arch=sun4,s_cpu=5:0:0

    # don't restart the job in case of system crashes
    -r n
    =====================================================

Having defined a default request definition file like this and submitting a job as follows:

    qsub test.sh

would have precisely the same effect as if the job was submitted with:

    qsub -l arch=sun4,s_cpu=5:0:0 -r n test.sh

# FILES

For a complete list of common environment variables used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

## <xxqs_name_sxx_root>/<cell>/common/xxqs_name_sxx_request
global defaults file
    
## $HOME/.xxqs_name_sxx_request	
user private defaults file
    
## $cwd/.xxqs_name_sxx_request	
cwd directory defaults file

# SEE ALSO

xxqs_name_sxx_intro(1), qsh(1), qsub(1), 

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
