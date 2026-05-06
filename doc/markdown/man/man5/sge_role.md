---
title: sge_role
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_role - xxQS_NAMExx role configuration file format

# DESCRIPTION

Roles are the central building block of the xxQS_NAMExx Role-Based Access Control (RBAC) system.
A role defines a named, centrally managed collection of permissions that specifies which operations
are authorized on which resource types and under which conditions.

Roles are assigned to users via user access lists (*user_list*) and may inherit permissions from
other roles through a directed acyclic role hierarchy (*parent_role_list*). Authorization decisions
are derived dynamically at runtime from the effective permission set of the role, including all
transitively inherited permissions, and are enforced by xxqs_name_sxx_qmaster(8) according to the
default-deny principle: any operation not explicitly permitted by a matching rule is denied.

Roles are managed with the `-arole`, `-Arole`, `-mrole`, `-Mrole`, `-drole`, `-srole`, and `-srolel`
options of qconf(1).

Note, xxQS_NAMExx allows backslashes (\\) be used to escape newline (\\newline) characters. The
backslash and the newline are replaced with a space (" ") character before any interpretation.

# FORMAT

The format of a xxqs_name_sxx_role file is defined as follows:

## name

The unique name of the role as defined for *object_name* in xxqs_name_sxx_types(1). Used as the
argument to the `-arole`, `-drole`, `-mrole`, and `-srole` options of qconf(1).

## enabled

Controls whether the role may be used directly to authorize incoming requests. Valid values are
`TRUE` and `FALSE`.

When set to `FALSE`, requests that specify this role are rejected. A disabled role that appears in
another role's *parent_role_list* still contributes its permissions to that child role via
inheritance. This allows base roles to be defined that are only used through inheritance and never
directly.

## user_list

A comma-separated list of user access list names (see xxqs_name_sxx_access_list(5)) whose members
are authorized to use this role to submit requests, provided the role is enabled. A user must be a
member of at least one of the referenced access lists to assume the role.

If set to `NONE` (the default), no user is authorized to use the role. Unlike other configuration
objects, roles do not support an *xuser_list* parameter: access must be explicitly granted and
cannot be implicitly revoked.

Deletion of an access list referenced by a role's *user_list* is refused until the reference is
removed.

## parent_role_list

A comma-separated list of role names from which this role inherits all permissions transitively.
Inheritance is transitive: a role acquires the permissions of all ancestor roles in the hierarchy.
The role hierarchy must form a directed acyclic graph (DAG); cyclic references are not permitted
and are rejected during validation.

Permissions are evaluated in depth-first, left-to-right order starting from the role itself, then
parent roles in declaration order. The first matching permission rule encountered determines the
authorization outcome.

If set to `NONE` (the default), no permissions are inherited.

Deletion of a role that is still referenced in another role's *parent_role_list* is refused until
the reference is removed.

## perm_list

A comma-separated list of RBAC permission rules that define the access rights granted by this role.
Each rule consists of exactly six colon-separated characteristics:

```
<source>:<origin>:<operation>:<object_type>:<object_key>:<value_constraint>
```

A request is authorized if at least one rule in the effective permission set (own rules plus all
transitively inherited rules) matches all six characteristics of the request. The first matching
rule encountered determines the outcome; no further rules are evaluated.

The keyword `NONE` defines an empty rule set that does not match any request.

The syntax of individual permission rules is described in detail in xxqs_name_sxx_role(5) once
CS-2028 (permission rule parser) is implemented.

# SEE ALSO

xxqs_name_sxx_intro(1), xxqs_name_sxx_types(1), qconf(1), xxqs_name_sxx_access_list(5),
xxqs_name_sxx_qmaster(8).

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
