# NAME

gethostname -  
get local hostname.

gethostbyname -  
get local host information for specified hostname.

gethostbyaddr -  
get hostname via IP address.

getservbyname -  
get configured port number of service

# SYNTAX

**gethostname \[-help\|-name\|-aname\|-all\]**

**gethostbyname \[-help\|-name\|-aname\|-all\]** **\<name>**

**gethostbyaddr \[-help\|-name\|-aname\|-all\]** **\<ip>**

**getservbyname \[-help\|-number\]** **\<service>**

# DESCRIPTION

*gethostname* and *gethostbyname* are used to get the local resolved
host name. *gethostbyaddr* is used to get the hostname of a specified IP
address (dotted decimal notation). *getservbyname* can be used to get
the configured port number of a service (e.g. from /etc/services).

The hostname utils are primarily used by the xxQS_NAMExx installation
scripts. *gethostname* , *gethostbyname* and *gethostbyaddr* called
without any option will print out the hostname, all specified aliases,
and the IP address of the locally resolved hostname. Calling
*getservbyname* without any option will print out the full service
entry.

# OPTIONS

## **-help**

Prints a list of all options.

## **-name**

This option only reports the primary name of the host.

## **-aname**

If this option is set, the xxQS_NAMExx host alias file is used for host
name resolving. It is necessary to set the environment variable
xxQS_NAME_Sxx_ROOT and, if more than one cell is defined, also
xxQS_NAME_Sxx_CELL.

This option will print out the xxQS_NAMExx host name.

## **-all**

By using the **-all** option all available host information will be
printed. This information includes the host name, the xxQS_NAMExx host
name, all host aliases, and the IP address of the host.

## **-number**

This option will print out the port number of the specified service
name.

## **\<name>**

The host name for which the information is requested.

## **\<ip>**

The IP address (dotted decimal notation) for which the information is
requested.

## **\<service>**

The service name for which the information is requested (e.g. ftp,
sge_qmaster or sge_execd).

# EXAMPLES

The following example shows how to get the port number of the FTP
service:

>     >getservbyname -number ftp
>     21

The next example shows the output of gethostname -all when the host
alias file contains this line:

gridmaster extern_name extern_name.mydomain

The local host resolving must also provide the alias name "gridmaster".
Each xxQS_NAMExx host that wants to use the cluster must be able to
resolve the host name "gridmaster".

To setup an alias name, edit your /etc/hosts file or modify your NIS
setup to provide the alias for the NIS clients.

The host alias file must be readable from each host (use e.g. NFS shared
file location).

>     >gethostname -all
>     Hostname: extern_name.mydomain
>     SGE name: gridmaster
>     Aliases:  loghost gridmaster
>     Host Address(es): 192.168.143.99

# ENVIRONMENTAL VARIABLES

xxQS_NAME_Sxx_ROOT  
Specifies the location of the xxQS_NAMExx standard configuration files.

xxQS_NAME_Sxx_CELL  
If set, specifies the default xxQS_NAMExx cell.

# SEE ALSO

*xxqs_name_sxx_intro*(1), *host_aliases*(5),

# COPYRIGHT

See *xxqs_name_sxx_intro*(1) for a full statement of rights and
permissions.
