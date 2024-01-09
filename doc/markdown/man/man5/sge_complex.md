---
title: sge_complex
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_complex - xxQS_NAMExx complexes configuration file format

# DESCRIPTION

*Complex* reflects the format of the xxQS_NAMExx complex configuration.
The definition of complex attributes provides all pertinent information
concerning the resource attributes a user may request for a xxQS_NAMExx
job via the *qsub*(1) **-l** option and for the interpretation of these
parameters within the xxQS_NAMExx system.

The xxQS_NAMExx complex object defines all entries which are used for
configuring the global, the host, and queue object. The system has a set
of pre defined entries, which are assigned to a host or queue per
default. In a addition can the user define new entries and assign them
to one or multiple objects. Each load value has to have its
corresponding complex entry object, which defines the type and the
relational operator for it.

## **defining resource attributes**

The complex configuration should not be accessed directly. In order to
add or modify complex entries, the *qconf*(1) options -Mc and -mc
should be used instead. While the -Mc option takes a *complex*
configuration file as an argument and overrides the current
configuration, the -mc option bring up an editor filled in with the
current *complex* configuration.

The provided list contains all definitions of resource attributes in the
system. Adding a new entry means to provide: name, shortcut, type,
relop, requestable, consumable, default, and urgency. The fields are
described below. Changing one is easily done by updating the field to
change and removing an entry by deleting its definition. An attribute
can only be removed, when it is not referenced in a host or queue object
anymore. Also does the system have a set of default resource attributes
which are always attached to a host or queue. They cannot be deleted nor
can the type of such an attribute be changed.

## **working with resource attributes**

Before a user can request a resource attribute it has to be attached to
the global, host, or cqueue object. The resource attribute exists only
for the objects, it got attached to ( if it is attached to the global
object(qconf -me global), it exits system wide, host object: only on
that host (qconf -me NAME): cqueue object: only on that cqueue (qconf
-mq NAME)).

When the user attached a resource attribute to an object, one also has
to assign a value to it; the resource limit. Another way to get a
resource attribute value is done by configuring a load sensor for that
attribute.

## **Default queue resource attributes**

In its default form it contains a selection of parameters in the queue
configuration as defined in *queue_conf*(5). The queue configuration
parameters being requestable for a job by the user in principal are:

    qname
    hostname
    notify
    calendar
    min_cpu_interval 
    tmpdir
    seq_no
    s_rt
    h_rt
    s_cpu
    h_cpu
    s_data
    h_data
    s_stack
    h_stack
    s_core
    h_core
    s_rss
    h_rss

## **Default host resource attributes**

The standard set of host related attributes consists of two categories.
he first category is built by several queue configuration attributes
which are particularly suitable to be managed on a host basis. These
attributes are:

    slots
    s_vmem
    h_vmem
    s_fsize
    h_fsize

(please refer to *queue_conf*(5) for details).

**Note:** Defining these attributes in the host complex is no
contradiction to having them also in the queue configuration. It allows
maintaining the corresponding resources on a host level and at the same
time on a queue level. Total virtual free memory (h_vmem) can be managed
for a host, for example, and a subset of the total amount can be
associated with a queue on that host.

The second attribute category in the standard host complex are the
default load values Every *xxqs_name_sxx_execd*(8) periodically reports
load to *xxqs_name_sxx_qmaster*(8). The reported load values are either
the standard xxQS_NAMExx load values such as the CPU load average (see
*uptime*(1)) or load values defined by the xxQS_NAMExx administration
(see the **load_sensor** parameter in the cluster configuration
*xxqs_name_sxx_conf*(5) and the *xxQS_NAMExx Installation and
Administration Guide* for details). The characteristics definition for
the standard load values is part of the default host complex, while
administrator defined load values require extension of the host complex.
Please refer to the file \<xxqs_name_sxx_root>/doc/load_parameters.asc
for detailed information on the standard set of load values.

## **Overriding attributes**

One attribute can be assigned to the global object, host object, and
queue object at the same time. On the host level it might get its value
from the user defined resource limit and a load sensor. In case that the
attribute is a consumable, we have in addition to the resource limit and
its load report on host level also the internal usage, which the system
keeps track of. The merge is done as follows:

In general an attribute can be overridden on a lower level - global by
hosts and queues - hosts by queues and load values or resource limits on
the same level.

We have one limitation for overriding attributes based on its relational
operator:

!=, == operators can only be overridden on the same level, but not on a
lower level. The user defined value always overrides the load value.

\>=, \>, \<=, \< operators can only be overridden, when the new value is
more restrictive than the old one.

In the case of a consumable on host level, which has also a load sensor,
the system checks for the current usage, and if the internal accounting
is more restrictive than the load sensor report, the internal value is
kept; if the load sensor report is more restrictive, that one is kept.

Note, xxQS_NAMExx allows backslashes (\\) be used to escape newline
(\\newline) characters. The backslash and the newline are replaced with
a space (" ") character before any interpretation.

# FORMAT

The principal format of a *complex* configuration is that of a tabulated
list. Each line starting with a '#' character is a comment line. Each
line despite comment lines define one element of the complex. A element
definition line consists of the following 8 column entries per line (in
the order of appearance):

## **name**

The name of the complex element to be used to request this attribute for
a job in the *qsub*(1) **-l** option. A complex attribute name (see
*complex_name* in *sge_types*(1)) may appear only once across all
complexes, i.e. the complex attribute definition is unique.

## **shortcut**

A shortcut for **name** which may also be used to request this attribute
for a job in the *qsub*(1) **-l** option. An attribute **shortcut** may
appear only once across all complexes, so as to avoid the possibility of
ambiguous complex attribute references.

## **type**

This setting determines how the corresponding values are to be treated
xxQS_NAMExx internally in case of comparisons or in case of load scaling
for the load complex entries:

-   With **INT** only raw integers are allowed.

-   With **DOUBLE** floating point numbers in double precision (decimal
    and scientific notation) can be specified.

-   With **TIME** time specifiers are allowed. Refer to *queue_conf*(5)
    for a format description.

-   With **MEMORY** memory size specifiers are allowed. Refer to
    *queue_conf*(5) for a format description.

-   With **BOOL** the strings TRUE and FALSE are allowed. When used in a
    load formula (refer to *sched_conf*(5) ) TRUE and FALSE get mapped
    into '1' and '0'.

-   With **STRING** all strings are allowed and is used for wildcard
    regular boolean expression matching. Please see *sge_types*(1)
    manpage for **expression** definition.

<!-- -->

    Examples:
     -l arch="*x*|sol*"  : 
          results in "arch=lx-x86" OR "arch=lx-amd64" 
             OR "arch=sol-amd64" OR ... 
     -l arch="sol-x??"  : 
          results in "arch=sol-x86" OR "arch=sol-x64" OR ...
     -l arch="lx2[246]-x86"  : 
          results in "arch=lx22-x86" OR "arch=lx24-x86"  
             OR "arch=lx26-x86"
     -l arch="lx2[4-6]-x86"  : 
          results in "arch=lx24-x86" OR "arch=lx25-x86"  
             OR "arch=lx26-x86"
     -l arch="lx2[24-6]-x86"  : 
          results in "arch=lx22-x86" OR "arch=lx24-x86"  
             OR "arch=lx25-x86" OR "arch=lx26-x86"
     -l arch="!lx-x86&!sol-amd64"  : 
          results in NEITHER "arch=lx-x86" NOR "arch=sol-amd64"
     -l arch="lx2[4|6]-amd64"  : 
          results in "arch=lx24-amd64" OR "arch=lx26-amd64"  

-   **CSTRING** is like **STRING** except comparisons are case
    insensitive.

-   **RESTRING** is like **STRING** and it will be deprecated in the
    future.

-   **HOST** is like **CSTRING** but the expression must match a valid
    hostname.

## **relop**

The **relation operator.** The relation operator is used when the value
requested by the user for this parameter is compared against the
corresponding value configured for the considered queues. If the result
of the comparison is false, the job cannot run in this queue. Possible
relation operators are "==", "\<", ">", "\<=", ">=" and "EXCL". The only
valid operator for string type attributes is "==".

The "EXCL" relation operator implements exclusive scheduling and is only
valid for consumable boolean type attributes. Exclusive means the result
of the comparison is only true if a job requests to be exclusive and no
other exclusive or non-exclusive jobs uses the complex. If the job does
not request to be exclusive and no other exclusive job uses the complex
the comparison is also true.

## **requestable**

The entry can be used in a *qsub*(1) resource request if this field is
set to 'y' or 'yes'. If set to 'n' or 'no' this entry cannot be used by
a user in order to request a queue or a class of queues. If the entry is
set to 'forced' or 'f' the attribute has to be requested by a job or it
is rejected.

To enable resource request enforcement the existence of the resource has
to be defined. This can be done on a cluster global, per host and per
queue basis. The definition of resource availability is performed with
the complex_values entry in *host_conf*(5) and *queue_conf*(5).

## **consumable**

The **consumable** parameter can be set to either 'yes' ('y'
abbreviated), 'no' ('n') or 'JOB' ('j'). It can be set to 'yes' and
'JOB' only for numeric attributes (INT, DOUBLE, MEMORY, TIME - see
**type** above). If set to 'yes' or 'JOB' the consumption of the
corresponding resource can be managed by xxQS_NAMExx internal
bookkeeping. In this case xxQS_NAMExx accounts for the consumption of
this resource for all running jobs and ensures that jobs are only
dispatched if the xxQS_NAMExx internal bookkeeping indicates enough
available consumable resources. Consumables are an efficient means to
manage limited resources such a available memory, free space on a file
system, network bandwidth or floating software licenses.

A consumable defined by 'y' is a per slot consumables which means the
limit is multiplied by the number of slots being used by the job before
being applied. In case of 'j' the consumable is a per job consumable.
This resource is debited as requested (without multiplication) from the
allocated master queue. The resource needs not be available for the
slave task queues.

Consumables can be combined with default or user defined load parameters
(see *xxqs_name_sxx_conf*(5) and *host_conf*(5)), i.e. load values can
be reported for consumable attributes or the consumable flag can be set
for load attributes. The xxQS_NAMExx consumable resource management
takes both the load (measuring availability of the resource) and the
internal bookkeeping into account in this case, and makes sure that
neither of both exceeds a given limit.

To enable consumable resource management the basic availability of a
resource has to be defined. This can be done on a cluster global, per
host and per queue basis while these categories may supersede each other
in the given order (i.e. a host can restrict availability of a cluster
resource and a queue can restrict host and cluster resources). The
definition of resource availability is performed with the
**complex_values** entry in *host_conf*(5) and *queue_conf*(5). The
**complex_values** definition of the "global" host specifies cluster
global consumable settings. To each consumable complex attribute in a
**complex_values** list a value is assigned which denotes the maximum
available amount for that resource. The internal bookkeeping will
subtract from this total the assumed resource consumption by all running
jobs as expressed through the jobs' resource requests.

**Note:** Jobs can be forced to request a resource and thus to specify
their assumed consumption via the 'force' value of the **requestable**
parameter (see above).

**Note also:** A default resource consumption value can be pre-defined
by the administrator for consumable attributes not explicitly requested
by the job (see the **default** parameter below). This is meaningful
only if requesting the attribute is not enforced as explained above.

See the *xxQS_NAMExx Installation and Administration Guide* for examples
on the usage of the consumable resources facility.

## **default**

Meaningful only for consumable complex attributes (see **consumable**
parameter above). xxQS_NAMExx assumes the resource amount denoted in the
**default** parameter implicitly to be consumed by jobs being dispatched
to a host or queue managing the consumable attribute. Jobs explicitly
requesting the attribute via the *-l* option to *qsub*(1) override this
default value.

## **urgency**

The urgency value allows influencing job priorities on a per resource
base. The urgency value effects the addend for each resource when
determining the resource request related urgency contribution. For
numeric type resource requests the addend is the product of the urgency
value, the jobs assumed slot allocation and the per slot request as
specified via *-l* option to *qsub*(1). For string type requests the
resources urgency value is directly used as addend. Urgency values are
of type real. See under *sge_priority*(5) for an overview on job
priorities.

# SEE ALSO

*xxqs_name_sxx_intro*(1), *xxqs_name_sxx_types*(1), *qconf*(1),
*qsub*(1), *uptime*(1), *xxqs_name_sxx_host_conf*(5), *xxqs_name_sxx_queue_conf*(5),
*xxqs_name_sxx_execd*(8), *xxqs_name_sxx_qmaster*(8)  
*xxQS_NAMExx Installation and Administration Guide.*

# COPYRIGHT

See *xxqs_name_sxx_intro*(1) for a full statement of rights and
permissions.
