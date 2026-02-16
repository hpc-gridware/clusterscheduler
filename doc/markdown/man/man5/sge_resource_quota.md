---
title: sge_resource_quota
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_resource_quota - xxQS_NAMExx resource quota file format

# DESCRIPTION

Resource quota sets (rqs) are a flexible way to set a maximum resource consumption for any job requests. They 
are used by the scheduler to select the next possible jobs for running. The job request distinction is done by 
a set of user, project, cluster queue, host and pe filter criteria.

By using the resource quota sets administrators are allowed to define a fine granular resource quota configuration. 
This helps restricting some job requests to a lesser resource usage and granting other job requests a higher 
resource usage.

Note: Jobs requesting an Advance Reservation (AR) are not honored by Resource Quotas and are neither subject of 
the resulting limit, nor are debited in the usage consumption.

A list of currently configured rqs can be displayed via the qconf(1) `-srqsl` option. The contents of each 
listed rqs definition can be shown via the `-srqs` switch. The output follows the xxqs_name_sxx_resource_quota 
format description. New rqs can be created and existing can be modified via the `-arqs`, `-mrqs` and
`-drqs` options to qconf(1).

A resource quota set defines a maximum resource quota for a particular job request. All of the configured rule 
sets apply all of the time. This means that if multiple resource quota sets are defined, the most restrictive 
set is used.

Every resource quota set consist of one or more resource quota rules. These rules are evaluated in order, and 
the first rule that matches a specific request will be used. A resource quota set always results in at
most one effective resource quota rule for a specific request.

Note, xxQS_NAMExx allows backslashes (\\) be used to escape newline (\\newline) characters. The backslash and 
the newline are replaced with a space (" ") character before any interpretation.

# FORMAT

A resource quota set definition contains the following parameters:

## name

The resource quota set name.

## enabled

If set to *true* the resource quota set is active and will be considered for scheduling decisions. The default value is *false*.

## description

This description field is optional and can be set to arbitrary string. The default value is *NONE*.

## limit

Defines one resource quota rule. At least one limit entry is required. Multiple rules may be defined, each starting with the keyword "limit".

    limit [name <rule_name>] [filters] to <limit_expression>

The specification of a name and filters is optional. If no name is specified, the sequence number of the rule within the resource quote set is used to refer to a specific limit rule. If no filters are specified, the rule applies to all job independent of user, project, queue, host and pe request.

Rules are evaluated in definition order.

By default, limits apply to the entire filter scope collectively. For example:

    limit users user1,user2 to slots=100

means user1 and user2 together may use at most 100 slots.

To apply limits individually within a filter scope, use braces:

    limit users {user1,user2} to slots=100

In this case, user1 and user2 may each use 100 slots independently.

The following filter tags are supported:

* users  
  Contains a comma separated list of UNIX users or ACLs (see xxqs_name_sxx_access_list(5)). This parameter filters for jobs by a user in the list or one of the ACLs in the list. Any user not in the list will not be considered for the resource quota rule. The default value is '\*', which means any user. An ACL is differentiated from a UNIX user name by prefixing the ACL name with an '@' sign. To exclude a user or ACL from the rule, the name can be prefixed with the '!' sign. Defined UNIX user or ACL names need not be known in the xxQS_NAMExx configuration.

* projects  
  Contains a comma separated list of projects (see *xxqs_name_sxx_project*(5)). This parameter filters for jobs requesting a project in the list. Any project not in the list will not be considered for the resource quota rule. If no project filter is specified, all projects and jobs with no requested project match the rule. The value '\*' means all jobs with requested projects. To exclude a project from the rule, the name can be prefixed with the '!' sign. The value '!\*' means only jobs with no project requested.

* pes  
  Contains a comma separated list of PEs (see *sge_pe*(5)). This parameter filters for jobs requesting a pe in the list. Any PE not in the list will not be considered for the resource quota rule. If no pe filter is specified, all pe and jobs with no requested pe matches the rule. The value '\*' means all jobs with requested pe. To exclude a pe from the rule, the name can be prefixed with the '!' sign. The value '!\*' means only jobs with no pe requested.

* queues  
  Contains a comma separated list of cluster queues (see xxqs_name_sxx_queue_conf(5)). This parameter filters for jobs that may be scheduled in a queue in the list. Any queue not in the list will not be considered for the resource quota rule. The default value is '\*', which means any queue. To exclude a queue from the rule, the name can be prefixed with the '!' sign.

* hosts  
  Contains a comma separated list of host or host groups (see xxqs_name_sxx_host_conf(5) and xxqs_name_sxx_hostgroup(5)). This parameter filters for jobs that may be scheduled in a host in the list or a host contained in a host group in the list. Any host not in the list will not be considered for the resource quota rule. The default value is '\*', which means any hosts. To exclude a host or host group from the rule, the name can be prefixed with the

## to  

This mandatory field defines the quota for resource attributes for this rule. The quota is expressed by one or more limit definitions separated by commas. The configuration allows two kind of limits definitions

* static limits  
  A static limits sets static values as quotas. Each limits consists of a complex attribute followed by an "=" sign and the value specification compliant with the complex attribute type (see *xxqs_name_sxx_complex*(5)).

* dynamic limits  
  A dynamic limit is a simple algebraic expression used to derive the limit value. To be dynamic, the formula can reference a complex attribute whose value is used for the calculation of the resulting limit. The formula expression syntax is that of a sum of weighted complex values, that is:

      {w1|$complex1[*w1]}[{+|-}{w2|$complex2[*w2]}[{+|-}...]]

  The weighting factors (w1, ...) are positive integers or floating point numbers in double precision. The complex values (complex1, ...) are specified by the name defined as type INT or DOUBLE in the complex list (see xxqs_name_sxx_complex(5)). Note: Dynamic limits can only configured for a host-specific rule.

Please note that resource quotas are not enforced as job resource limits. Limiting for example h_vmem in a resource quota set does not result in a memory limit being set for job execution.

# EXAMPLES

The following is the simplest form of a resource quota set. It restricts all users together to the maximal use of 100 slots in the whole cluster.

    {
       name         max_u_slots
       description  "All users together can use a maximum of 100 slots"
       enabled      true
       limit        to slots=100
    }

The next example restricts user1 and user2 individually to 6g virtual_free and all other users each to the maximal use of 4g 
virtual_free on every host in host group @lx_hosts.

    {
       name         max_virtual_free_on_lx_hosts
       description  "resource quota for virtual_free restriction"
       enabled      true
       limit        users {user1,user2} hosts {@lx_host} to virtual_free=6g
       limit        users {*} hosts {@lx_host} to virtual_free=4g
    }

The next example shows the use of a dynamic limit. It restricts all users together to a maximum use of the 
double size of num_proc.

    {
       name         max_slots_on_every_host
       enabled      true
       limit        hosts {*} to slots=$num_proc*2
    }

# SEE ALSO

xxqs_name_sxx_intro(1), xxqs_name_sxx_access_list(5), xxqs_name_sxx_complex(5), xxqs_name_sxx_host_conf(5),
xxqs_name_sxx_hostgroup(5), qconf(1), qquota(1), xxqs_name_sxx_project(5).

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
