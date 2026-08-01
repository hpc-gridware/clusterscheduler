# Known Issues and Limitations

## *id* and *groups* Fail Inside Jobs on Distributions Using uutils Coreutils

Every process of a job carries one additional supplementary group id, taken from the *gid_range* of the
execution host configuration. That is how xxQS_NAMExx recognises which processes belong to a job. These
ids are deliberately unused ids of the system and therefore have no name in `/etc/group` or in a
directory service.

Commands that translate group ids into names notice this. On distributions that have replaced GNU
coreutils with the Rust rewrite (uutils) — among them Ubuntu 25.10 and newer — `id` reports

```
id: cannot find name for group ID 20001
```

and **exits with status 1**. GNU `id` prints the same id numerically and exits with 0. The `groups`
command fails in both implementations. Commands that work with numeric ids, such as `id -u`, `id -G` or
`ls -l`, are unaffected, and nothing about job execution itself changes.

This becomes visible when a job, prolog or epilog script evaluates the exit status of such a command, or
runs with `set -e`.

**What to do:**

- In scripts, prefer `id -un` over `id` and `id -G` over `groups` where the exit status matters. This is
  portable and works regardless of which coreutils implementation is installed.
- If the GNU behaviour is needed system-wide, the GNU implementation is still available on these
  distributions — on Ubuntu in the package *gnu-coreutils*, whose commands are prefixed with `gnu`
  (`gnuid`), while removing *coreutils-from-uutils* restores the previous default.
- The ids can also be given names, by creating a group for each id of the range on every execution host.
  Note that the range holds one id per job running on a host at the same time, so this can be a few
  hundred groups, that the names have to be identical on all hosts, and that the ids are then no longer
  unused ids in the sense described in xxqs_name_sxx_conf(5).

[//]: # (Each file has to end with two empty lines)

