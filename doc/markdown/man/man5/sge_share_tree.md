---
title: sge_share_tree
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_share_tree - xxQS_NAMExx share tree file format

# DESCRIPTION

The share tree defines the long-term resource entitlements of
users/projects and of a hierarchy of arbitrary groups thereof.

The current share tree can be displayed via the *qconf*(1) **-sstree**
option. The output follows the *share_tree* format description. A share
tree can be created and an existing can be modified via the **-astree**
and **-mstree** options to *qconf*(1). The **-sst** option shows a
formatted share tree (tree view). Individual share tree nodes can be
created, modified, deleted, or shown via the **-astnode**, **-dstnode**,
**-mstnode**, and **-sstnode** options to *qconf*(1).

Note, xxQS_NAMExx allows backslashes (\\) be used to escape newline
(\\newline) characters. The backslash and the newline are replaced with
a space (" ") character before any interpretation.

# FORMAT

The format of a share tree file is defined as follows:

-   A new node starts with the attribute **id**, and equal sign and the
    numeric identification number of the node. Further attributes of
    that node follow until another **id**-keyword is encountered.

-   The attribute **type** defines, if a sharetree node references a
    user (type=0), or a project (type=1)

-   The attribute **childnodes** contains a comma separated list of
    child nodes to this node.

-   The parameter **name** refers to an arbitrary name for the node or
    to a corresponding user (see *xxqs_name_sxx_user*(5)) or project (see
    *xxqs_name_sxx_project*(5)) if the node is a leaf node of the share tree. The
    name for the root node of the tree is "Root" by convention.

-   The parameter **shares** defines the share of the node among the
    nodes with the same parent node.

-   A user leaf node named 'default' can be defined as a descendant of a
    *project*(5)) node in the share tree. The default node defines the
    number of shares for users who are running in the project, but who
    do not have a user node defined under the project. The default user
    node is a convenient way of specifying a single node for all users
    which should receive an equal share of the project resources. The
    default node may be specified by itself or with other *user*(5))
    nodes at the same level below a project. All users, whether
    explicitly specified as a user node or those which map to the
    'default' user node must have a corresponding *user*(5)) object
    defined in order to get shares. Do not configure a *user*(5))
    object named 'default'.

# EXAMPLES

Jobs of projects P1 and P2 get 50 shares, all other jobs get 10 shares.

    id=0
    name=Root
    type=0
    shares=1
    childnodes=1,2,3
    id=1
    name=P1
    type=1
    shares=50
    childnodes=NONE
    id=2
    name=P2
    type=1
    shares=50
    childnodes=NONE
    id=3
    name=default
    type=0
    shares=10
    childnodes=NONE

# SEE ALSO

*xxqs_name_sxx_intro*(1), *qconf*(1), *xxqs_name_sxx_project*(5), *xxqs_name_sxx_user*(5).

# COPYRIGHT

See *xxqs_name_sxx_intro*(1) for a full statement of rights and
permissions.
