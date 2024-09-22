---
title: sge_aliases
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_aliases - xxQS_NAMExx path aliases file format

# DESCRIPTION

The xxQS_NAMExx path aliasing facility provides administrators and users with the means to reflect complicated and 
in-homogeneous file system structures in distributed environments (such as user home directories mounted under 
different paths on different hosts) and to ensure that xxQS_NAMExx is able to locate the appropriate working 
directories for executing batch jobs.

There is a system global path aliasing file and a user local file. xxqs_name_sxx_aliases defines the format of both:

-   Blank lines and lines with a '#' sign in the first column are skipped.

-   Each line other than a blank line or a line lead by '#' has to contain four strings separated by any number of blanks or tabs.

-   The first string specifies a source-path, the second a submit-host, the third an execution-host and the fourth the source-path replacement.

-   Both the submit- and the execution-host entries may consist of only a '\*' sign which matches any host.

If the `-cwd` flag (and only if - otherwise the user's home directory on the execution host is selected to execute 
the job) to `qsub` was specified, the path aliasing mechanism is activated and the files are processed as follows:

-   After `qsub` has retrieved the physical current working directory path, the cluster global path aliasing file is read if
    present. The user path aliases file is read afterwards as if it were appended to the global file.

-   Lines not to be skipped are read from the top of the file one by one while the translations specified by those 
    lines are stored if necessary.

-   A translation is stored only if the submit-host entry matches the host qsub(1) is executed on and if the 
    source-path forms the initial part either of the current working directory or of the source-path replacements 
    already stored.

-   As soon as both files are read the stored path aliasing information is passed along with the submitted job.

-   On the execution host, the aliasing information will be evaluated. The leading part of the current working 
    directory will be replaced by the source-path replacement if the execution-host entry of the path alias matches 
    the executing host. **Note:** The current working directory string will be changed in this case and subsequent path
    aliases must match the replaced working directory path to be applied.

# EXAMPLES

The following is a simple example of a path aliasing file resolving problems with in-homogeneous paths if automount(8) is used:

    =====================================================
    # Path Aliasing File
    # src-path   sub-host   exec-host   replacement
    /tmp_mnt/    *          *           /
    # replaces any occurrence of /tmp_mnt/ by /
    # if submitting or executing on any host.
    # Thus paths on nfs server and clients are the same
    =====================================================

# FILES

For a complete list of common environment variables used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

## \<xxqs_name_sxx_root>/\<cell>/common/xxqs_name_sxx_aliases
global aliases file
    
## $HOME/.xxqs_name_sxx_aliases	
user local aliases file

# SEE ALSO

xxqs_name_sxx_intro(1), qsub(1)

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
