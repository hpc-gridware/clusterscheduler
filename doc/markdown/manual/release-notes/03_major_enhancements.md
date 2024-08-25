# Major Enhancements 

## qconf support to add/modify/delete/show complex entries individually

`Qconf` also allows you to add, modify, delete and display complexes individually using the new `-ace`, `-Ace`, 
`-mce`, `-Mce`, `-sce` and `-scel` switches. Previously this was only possible as a group command for the whole 
complex set with `-mq`. More information can be found in the qconf(1) man page or by running `qconf -help`.

(Available in Open Cluster Scheduler and Gridware Cluster Scheduler)

## Added support to supplementary group IDs in user, operator and manager lists. 

Additionally, to user and primary group names, it is now possible to specify supplementary group IDs in user, operator,
and manager lists. User lists can be specified in host, queue, configuration, and parallel environment objects to allow
or reject access. They are also used in resource quota sets or in combination with advance reservations.

The ability to specify supplementary groups allows administrators to set up a cluster so that access to users can be 
assigned based on their group membership. This is especially useful in environments where users are members of multiple 
groups and access to resources is based on group membership.

Furthermore, it is now possible to add group IDs to user lists if supplementary groups exist without a specific name. 
This can be helpful in environments where tools or applications are tagged with supplementary groups that are not 
managed via directory services like LDAP or NIS and therefore have no name.

Please note that the evaluation of supplementary groups is disabled by default. To enable it, set the parameter
`ENABLE_SUP_GRP_EVAL` to `1` in the `qmaster_params` of the cluster configuration. Enabling this feature can 
significantly impact the performance of directory services, especially in large clusters with many users and 
groups. Therefore, it is recommended to test the performance impact in a test environment before enabling it 
in a production environment. Enabling caching services like `nscd` can help reduce the performance impact.

(Available in Gridware Cluster Scheduler only)

## New internal architecture to support multiple Data Stores

The internal data architecture of `sge_qmaster` has been changed to support multiple data stores. This change 
does not have a major impact currently and is not visible to the user. However, it is a prerequisite for future 
enhancements that will significantly improve the scalability and performance of the cluster.
This will allow different processing components within `sge_qmaster` to use separate data stores, thereby 
enhancing the performance of the cluster in large environments.

(Available in Open Cluster Scheduler and Gridware Cluster Scheduler)
