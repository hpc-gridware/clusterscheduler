#!/bin/bash -p
#___INFO__MARK_BEGIN_NEW__
###########################################################################
#
#  Copyright 2026 HPC-Gridware GmbH
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
###########################################################################
#___INFO__MARK_END_NEW__
#
# Prolog and epilog for the tasks of parallel jobs (shepherd_cmd wrapper)
#
# WHAT IT DOES
#   OCS/GCS runs the queue prolog and epilog only for the master task of a
#   parallel job. For every other task (started with "qrsh -inherit") execd
#   writes "prolog=none" and "epilog=none" into the shepherd configuration of
#   the task.
#
#   Set as the shepherd_cmd, this script runs before every shepherd. For a
#   task whose job asked for it, it replaces those "none" lines with the
#   commands configured below. Then it starts the real shepherd.
#
# SETUP (administrator)
#   1. Choose in the configuration block below what the tasks run:
#        TASK_PROLOG=queue       the same prolog the master task runs (from the
#                                queue, else the host, else the global
#                                configuration) - one script for both, which
#                                tells a task from the master task itself (see
#                                "MASTER TASK OR TASK?" below)
#        TASK_PROLOG="root@/opt/site/task_prolog.sh"
#                                a separate script for the tasks
#        TASK_PROLOG=""          off (the default)
#      The same for TASK_EPILOG.
#      With "queue" every task start asks qmaster ("qconf -sq", "qconf
#      -sconf"), so the execution hosts must be administrative or submit
#      hosts. A qmaster which does not answer delays the task by up to 30
#      seconds (3 queries of at most 10 s), after which it runs without hook. Only choose "queue" once the prolog is written for tasks:
#      otherwise any job can make it run once per task.
#   2. Make this script the shepherd_cmd:
#        qconf -mconf <host>       (or "global")
#        shepherd_cmd  /path/to/shepherd_wrapper.sh
#
# MASTER TASK OR TASK? (in the prolog/epilog)
#   SGE_PE_TASK_ID is set in a task (e.g. "1.node7") and not in the master
#   task:
#        if [ -n "$SGE_PE_TASK_ID" ]; then ...task...; else ...master...; fi
#   A job can set SGE_PE_TASK_ID itself for its master task (qsub -v). Where
#   that matters (checks, security), ask the file execd wrote instead:
#        if grep -q '^pe_task_id=' "$SGE_JOB_SPOOL_DIR/config"; then ...task...
#
# USE (job)
#   qsub -v SGE_PER_TASK_PROLOG=1 -v SGE_PER_TASK_EPILOG=1 ...
#   Accepted values for "on": 1 t true y yes on (any case).
#   The job only switches a hook on. WHICH command runs is always decided by
#   the configuration block below, never by the job.
#
# SIDE EFFECT: THE TASK GETS A PTY (pty=1)
#   The shepherd cannot start a prolog or epilog in a task without a pty: it
#   fails with "fd for in is not 0", the task ends with exit status 7 and the
#   queue goes into error state (builtin_starter.cc, verified on OCS 9.1.6).
#   MPI launchers never ask for a pty ("qrsh -inherit -nostdin -V"), so this
#   script sets pty=1 whenever it adds a hook. Consequence: the stderr of the
#   task arrives merged into its stdout at the "qrsh -inherit" which started
#   it. All bytes are otherwise unchanged.
#
# SAFETY RULES
#   - The real shepherd is ALWAYS started. execd treats a shepherd_cmd that
#     exits as a failed job, so nothing below may end the script early.
#   - Only lines execd wrote as "none" are changed, plus "pty".
#   - The configuration is only touched if it is a plain file (no symbolic
#     link, no hard link), and it is replaced in one step (write a new file
#     with the same mode and owner, then rename it over the old one).
#   - Works with and without an admin user (admin_user in the bootstrap
#     file): execd then owns the spool files as that user and starts this
#     script with it as effective user; the files keep that owner.
#   - What was done is logged to shepherd_wrapper.log in the spool directory.

# ============================== configuration ==============================

TASK_PROLOG=""    # queue | "root@/opt/site/task_prolog.sh" | "" (off)
TASK_EPILOG=""    # queue | "root@/opt/site/task_epilog.sh" | "" (off)

# ===========================================================================

# execd starts this script as root with its own environment. Keep the loader
# and the shell free of anything injected.
unset LD_PRELOAD LD_AUDIT LD_LIBRARY_PATH BASH_ENV ENV
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

# The architecture directory of the Grid Engine binaries (e.g. lx-amd64).
ARCH=""
case "$SGE_ROOT" in
	/*) ARCH=$("$SGE_ROOT/util/arch" 2>/dev/null) ;;
esac

# execd starts us in the spool directory of the job or task.
CONFIG=config                  # shepherd configuration, written by execd
ENVIRONMENT=environment        # environment of the job, written by execd
LOG=shepherd_wrapper.log

# The hooks to add; filled in by decide_hooks.
NEW_PROLOG=""
NEW_EPILOG=""

# ------------------------------- helpers -----------------------------------

log() {
	echo "$(date '+%Y-%m-%d %H:%M:%S') $*" >> "$LOG" 2>/dev/null
}

# get FILE KEY: the value of the last "KEY=..." line (the shepherd, too,
# uses the last one).
get() {
	sed -n "s/^$2=//p" "$1" 2>/dev/null | tail -n 1
}

# job_wants VARIABLE: did the job switch this hook on?
job_wants() {
	local value
	value=$(get "$ENVIRONMENT" "$1")
	case "${value,,}" in
		1|t|true|y|yes|on) return 0 ;;
	esac
	return 1
}

# hook_is_none KEY: did execd write KEY=none (and nothing else) for this task?
# A hook set by other means is left alone.
hook_is_none() {
	local all nones
	all=$(grep -c "^$1=" "$CONFIG")
	nones=$(grep -ic "^$1=none$" "$CONFIG")
	[ "$all" -gt 0 ] && [ "$all" -eq "$nones" ]
}

# is_plain_file FILE: a regular file, not a symbolic link, one hard link.
# Root must not rewrite something planted in the spool directory.
is_plain_file() {
	[ -f "$1" ] && [ ! -L "$1" ] && [ "$(stat -c %h "$1" 2>/dev/null)" = 1 ]
}

# owner_of FILE / mode_and_owner_of FILE: numeric owner and group, and with
# the permission bits, for comparing two files.
owner_of() {
	stat -c %u:%g "$1" 2>/dev/null
}
mode_and_owner_of() {
	stat -c %a:%u:%g "$1" 2>/dev/null
}

# qconf_value KEY ARGS...: the value of KEY in what "qconf ARGS" prints, empty
# when qconf fails or does not answer within 10 seconds.
qconf_value() {
	local key=$1
	shift
	SGE_SINGLE_LINE=1 timeout 10 "$SGE_ROOT/bin/$ARCH/qconf" "$@" 2>/dev/null |
		sed -n "s/^$key[[:space:]]\{1,\}//p" | tail -n 1
}

# master_hook KEY: the prolog or epilog (KEY) the master task runs on this
# host, found the way execd finds it: the queue instance, else the host
# configuration, else the global configuration. Empty for none.
master_hook() {
	local key=$1 queue host value
	queue=$(get "$CONFIG" queue)
	host=$(get "$CONFIG" host)

	value=$(qconf_value "$key" -sq "$queue@$host")
	if [ -z "$value" ] || [ "${value,,}" = none ]; then
		value=$(qconf_value "$key" -sconf "$host")
		if [ -z "$value" ]; then
			value=$(qconf_value "$key" -sconf global)
		fi
	fi
	if [ "${value,,}" = none ]; then
		value=""
	fi
	echo "$value"
}

# task_hook SETTING KEY: the command to add for KEY (prolog or epilog) from
# the configured SETTING (TASK_PROLOG or TASK_EPILOG).
task_hook() {
	if [ "$1" = queue ]; then
		master_hook "$2"
	else
		echo "$1"
	fi
}

# fits_on_one_line VALUE: the command must not break the config line.
fits_on_one_line() {
	case "$1" in
		*$'\n'*) return 1 ;;
	esac
	return 0
}

# ------------------------------- the steps ---------------------------------

# decide_hooks: set NEW_PROLOG / NEW_EPILOG for the hooks to add, or leave
# them empty when nothing is to be changed.
decide_hooks() {
	is_plain_file "$CONFIG"      || return
	is_plain_file "$ENVIRONMENT" || return
	grep -q '^pe_task_id=' "$CONFIG" || return   # tasks only, never the master
	grep -q '^pty=' "$CONFIG"        || return   # a hook needs the pty line

	local command
	if [ -n "$TASK_PROLOG" ] && job_wants SGE_PER_TASK_PROLOG &&
		hook_is_none prolog; then
		command=$(task_hook "$TASK_PROLOG" prolog)
		if [ -n "$command" ] && fits_on_one_line "$command"; then
			NEW_PROLOG=$command
		fi
	fi
	if [ -n "$TASK_EPILOG" ] && job_wants SGE_PER_TASK_EPILOG &&
		hook_is_none epilog; then
		command=$(task_hook "$TASK_EPILOG" epilog)
		if [ -n "$command" ] && fits_on_one_line "$command"; then
			NEW_EPILOG=$command
		fi
	fi
}

# rewrite_config: write the configuration with the new hooks and pty=1 into
# a new file, then rename it over the old one. On any failure the old file
# stays as execd wrote it.
rewrite_config() {
	local task new line
	task=$(get "$CONFIG" pe_task_id)

	new=$(mktemp ./.config.XXXXXX) || {
		log "task $task: cannot create a temporary file, nothing changed"
		return
	}

	while IFS= read -r line || [ -n "$line" ]; do
		case "$line" in
			prolog=*) [ -n "$NEW_PROLOG" ] && line="prolog=$NEW_PROLOG" ;;
			epilog=*) [ -n "$NEW_EPILOG" ] && line="epilog=$NEW_EPILOG" ;;
			pty=*)    line="pty=1" ;;
		esac
		printf '%s\n' "$line"
	done < "$CONFIG" > "$new"

	# The new file must look like the old one: same mode, same owner and group.
	# With admin_user set in the bootstrap file, execd owns the spool files as
	# that user and starts this script with it as effective user, so the new
	# file already has the right owner. With admin_user "none" everything is
	# root's. Only a differing owner needs chown.
	chmod --reference="$CONFIG" "$new"
	if [ "$(owner_of "$new")" != "$(owner_of "$CONFIG")" ]; then
		chown --reference="$CONFIG" "$new"
	fi

	if [ "$(mode_and_owner_of "$new")" = "$(mode_and_owner_of "$CONFIG")" ] &&
		mv -f "$new" "$CONFIG"; then
		log "task $task: prolog=${NEW_PROLOG:-unchanged}" \
			"epilog=${NEW_EPILOG:-unchanged} pty=1"
	else
		# Remove the temporary file created above - and only that: its name
		# must be the one mktemp made in this spool directory, which only
		# execd's user (root or the admin user) can write.
		case "$new" in
			./.config.??????) rm -f -- "$new" ;;
		esac
		log "task $task: rewriting the configuration failed, nothing changed"
	fi
}

# start_shepherd: become the real shepherd.
#   - "exec" keeps the process id and signal mask execd gave this script.
#   - The name "sge_shepherd-<job_id>" is how execd finds its shepherds again
#     after a restart.
start_shepherd() {
	local job_id
	job_id=$(get "$CONFIG" job_id)
	case "$job_id" in
		''|*[!0-9]*) job_id=0 ;;
	esac
	exec -a "sge_shepherd-$job_id" "$SGE_ROOT/bin/$ARCH/sge_shepherd" "$@"
}

# --------------------------------- main -------------------------------------

decide_hooks
if [ -n "$NEW_PROLOG$NEW_EPILOG" ]; then
	rewrite_config
fi
start_shepherd "$@"
