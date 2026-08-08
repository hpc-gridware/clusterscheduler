#pragma once
/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 *
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 *
 *  Sun Microsystems Inc., March, 2001
 *
 *
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 *
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 *
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *   Copyright: 2001 by Sun Microsystems, Inc.
 *
 *   All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Declarations, state bits and job type flags of the job object
 *
 * @see sge_job.cc
 */

#include "uti/sge_htable.h"
#include "uti/sge_dstring.h"

#include "gdi/ocs_gdi_Packet.h"

#include "sgeobj/cull/sge_job_JB_L.h"
#include "sgeobj/cull/sge_job_JG_L.h"
#include "sgeobj/cull/sge_job_JRS_L.h"
#include "sgeobj/cull/sge_job_PN_L.h"
#include "sgeobj/cull/sge_job_ref_JRE_L.h"

/**
 * @name Job states
 *
 * Bits of `JAT_state`, and of `JAT_status` / `PET_status`. Which field a bit
 * belongs to matters: #JSUSPENDED_ON_THRESHOLD and #JFINISHED share a value
 * and are told apart only by the field they appear in.
 * @{
 */
#define JIDLE                                0x00000000 ///< no state bit set at all
/* #define JENABLED                             0x00000008 */
#define JHELD                                0x00000010 ///< a hold keeps the job from being scheduled
#define JMIGRATING                           0x00000020 ///< the job is being moved to another queue
#define JQUEUED                              0x00000040 ///< the job is waiting to be dispatched
#define JRUNNING                             0x00000080 ///< the job is running
#define JSUSPENDED                           0x00000100 ///< the job was suspended
#define JTRANSFERING                         0x00000200 ///< the job was dispatched and is starting up
#define JDELETED                             0x00000400 ///< a delete request was accepted for the job
#define JWAITING                             0x00000800 ///< the job waits for something, e.g. its start time
#define JEXITING                             0x00001000 ///< the job finished and is being cleaned up
#define JWRITTEN                             0x00002000 ///< the job was written to the spool
#define JWAITING4OSJID                       0x00004000 ///< execd: job waits for getting its ASH/JOBID
#define JERROR                               0x00008000 ///< execd: shepherd reports job exit but there are still processes

/// The job was suspended because a load threshold was exceeded; only ever in `JAT_state`
#define JSUSPENDED_ON_THRESHOLD              0x00010000
/**
 * @brief The job finished; only ever in `JAT_status` and `PET_status`
 *
 * qmaster delays removing the job until the scheduler no longer needs it.
 *
 * @note Shares its value with #JSUSPENDED_ON_THRESHOLD. The two are told apart
 *       purely by which field they appear in.
 */
#define JFINISHED                            0x00010000
/// execd: prevents slave jobs from getting started
#define JSLAVE                               0x00020000

/**
 * @brief Display-only bit marking a retained finished task (CS-1908)
 *
 * Set by #jatask_combine_state_and_status_for_output when `JAT_status` is
 * #JFINISHED, so the state character renders as `f` rather than the `x` a
 * retention row got before CS-1908 - `x` means "in the process of exiting",
 * which a retained row is not.
 *
 * @warning Never persist this bit onto `JAT_state`. It exists only inside the
 *          transient value the combine helper returns for rendering. Its value
 *          is disjoint from every real state bit, so an accidental write would
 *          stay invisible to code masking against the real ones - and
 *          therefore hard to find.
 */
#define JFINISHED_DISPLAY                    0x00040000

/// A request for the job was deferred and will be applied when it is rescheduled
#define JDEFERRED_REQ                        0x00100000
/** @} */

/**
 * @brief What a `qalter -h` request does to `JB_hold`
 *
 * One value combines a target - which hold types are addressed - with a
 * command - whether they are added, removed or set. The low four bits carry
 * the target, the bits from #MINUS_H_CMD_SUB upwards the command.
 *
 * `qalter -h {u|s|o|n}` maps onto it like this:
 *
 * | Written | Means |
 * |---|---|
 * | `u` | `SET\|USER` |
 * | `o` | `SET\|OPERATOR` |
 * | `s` | `SET\|SYSTEM` |
 * | `n` | `SUB\|USER\|SYSTEM\|OPERATOR` |
 * | `+u` `+o` `+s` | `ADD\|`the matching target |
 * | `-u` `-o` `-s` | `SUB\|`the matching target |
 *
 * The first four are the POSIX spelling and overwrite; the `+`/`-` forms are
 * the Grid Engine extension and add or remove.
 */
enum {
   MINUS_H_TGT_USER     = 1, ///< the user hold; removing it needs at least the job owner
   MINUS_H_TGT_OPERATOR = 2, ///< the operator hold; removing it needs at least an operator
   MINUS_H_TGT_SYSTEM   = 4, ///< the system hold; removing it needs at least a manager
   MINUS_H_TGT_JA_AD    = 8, ///< the array dependency hold; removed automatically

   MINUS_H_TGT_ALL      = 15, ///< all four targets at once
   MINUS_H_TGT_NONE     = 31, ///< not a target; marks a request that names none

   MINUS_H_CMD_ADD = (0<<4), ///< adds the targetted flags
   MINUS_H_CMD_SUB = (1<<4), ///< removes the targetted flags
   MINUS_H_CMD_SET = (2<<4)  ///< overwrites the hold state with the targetted flags
};

/// The `-w` letters, in the order of the values below
#define OPTION_VERIFY_STR "nwevp"
/**
 * @brief Values for `JB_verify_suitable_queues`, i.e. how far `qsub -w` goes
 *
 * The index of a value in #OPTION_VERIFY_STR is the letter the user writes.
 */
enum {
   SKIP_VERIFY = 0,     ///< `-w n` no expendable verifications will be done
   WARNING_VERIFY,      ///< `-w w` qmaster will warn about these jobs, but submit will succeed
   ERROR_VERIFY,        ///< `-w e` qmaster rejects jobs that are not schedulable (default)
   JUST_VERIFY,         ///< `-w v` just verify at qmaster but do not submit
   POKE_VERIFY          ///< `-w p` verify with all resource utilizations in place (poke)
};

/// What `qsub -sync` makes the submitting client wait for
typedef enum {
   SYNC_UNINITIALIZED = 0x00000000, ///< the switch was not given
   SYNC_NO            = 0x00000001, ///< do not wait at all
   SYNC_JOB_END       = 0x00000002, ///< wait until the job finished
   SYNC_JOB_START     = 0x00000004, ///< wait until the job started
} sync_switch_t;

/// When a `qalter` change takes effect
typedef enum {
   QALTER_WHEN_ON_RESCHEDULE = 0, ///< only once the job is rescheduled
   QALTER_WHEN_NOW                ///< immediately, on the running job
} qalter_when_t;

/**
 * @name Scheduling constants
 *
 * A POSIX priority runs from -1023 to 1024, but the object model stores it in
 * an unsigned field, so #BASE_PRIORITY is added on the way in and subtracted
 * on the way out.
 * @{
 */
#define BASE_PRIORITY  1024   ///< added to a signed priority to make it storable unsigned

#define PRI_ITOU(x) ((x)+BASE_PRIORITY) ///< signed priority to the stored unsigned form
#define PRI_UTOI(x) ((x)-BASE_PRIORITY) ///< stored unsigned form back to the signed priority

#define PRIORITY_OFFSET 8         ///< bit position the priority starts at within the sort key
#define NEWCOMER_FLAG     0x1000000 ///< marks a job that has not been through a scheduling run yet

#define MAX_JOBS_EXCEEDED 0x8000000 ///< forced negative sign bit: the job limit was hit
#define ALREADY_SCANNED   0x4000000 ///< the job was already looked at in this pass
#define PRIORITY_MASK     0xffff00  ///< the priority bits of the sort key
#define SUBPRIORITY_MASK  0x0000ff  ///< the tie breaking bits below the priority
#define JOBS_SCANNED_PER_PASS 10    ///< how many jobs one pass looks at before yielding
/** @} */

/// qstat: the job is suspended because its queue was suspended by a subordinate relation
#define JSUSPENDED_ON_SUBORDINATE            0x00002000
/// qstat: the same, but through a slot wise subordinate relation
#define JSUSPENDED_ON_SLOTWISE_SUBORDINATE   0x00004000

/// Reserved `JB_context` key holding the interoperable object reference of an interactive job
#define CONTEXT_IOR "IOR"
/// Reserved `JB_context` key naming the job this one was started from
#define CONTEXT_PARENT "PARENT"

/**
 * @name Macros to handle flag JB_type
 *
 * `JB_type` is a bit field. The `JOB_TYPE_*` values are the bits, the
 * `JOB_TYPE_SET_*` / `_CLEAR_*` / `_IS_*` macros are how the bits are written
 * and read, and the `JOB_TYPE_STR_*` values are how each appears in output.
 *
 * The four interactive kinds are mutually exclusive, which is what
 * #JOB_TYPE_QXXX_MASK is for: setting one clears the others.
 * @{
 */

/// submitted with `-now y`, so it runs at once or not at all
#define JOB_TYPE_IMMEDIATE  0x01UL
/// an interactive job started by `qsh`
#define JOB_TYPE_QSH        0x02UL
/// an interactive login session started by `qlogin`
#define JOB_TYPE_QLOGIN     0x04UL
/// a remote command started by `qrsh`
#define JOB_TYPE_QRSH       0x08UL
/// a remote login started by `qrsh` without a command
#define JOB_TYPE_QRLOGIN    0x10UL
/// do not put the job in error state when it fails to start
#define JOB_TYPE_NO_ERROR   0x20UL

/// submitted via `qsub -b y` or `qrsh [-b y]`: the job is a binary, not a script
#define JOB_TYPE_BINARY     0x40UL

/// an array job, submitted with `qsub -t`
#define JOB_TYPE_ARRAY      0x80UL
/// do a raw exec, submitted with `qsub -noshell`
#define JOB_TYPE_NO_SHELL   0x100UL

/// The mutually exclusive interactive kinds; setting one clears the rest
#define JOB_TYPE_QXXX_MASK \
   (JOB_TYPE_QSH | JOB_TYPE_QLOGIN | JOB_TYPE_QRSH | JOB_TYPE_QRLOGIN | JOB_TYPE_NO_ERROR)

/// How #JOB_TYPE_IMMEDIATE is written in qstat and accounting output
#define JOB_TYPE_STR_IMMEDIATE  "IMMEDIATE"
/// How #JOB_TYPE_QSH is written in qstat and accounting output
#define JOB_TYPE_STR_QSH        "INTERACTIVE"
/// How #JOB_TYPE_QLOGIN is written in qstat and accounting output
#define JOB_TYPE_STR_QLOGIN     "QLOGIN"
/// How #JOB_TYPE_QRSH is written in qstat and accounting output
#define JOB_TYPE_STR_QRSH       "QRSH"
/// How #JOB_TYPE_QRLOGIN is written in qstat and accounting output
#define JOB_TYPE_STR_QRLOGIN    "QRLOGIN"
/// How #JOB_TYPE_NO_ERROR is written in qstat and accounting output
#define JOB_TYPE_STR_NO_ERROR   "NO_ERROR"

/// Clear #JOB_TYPE_IMMEDIATE in `jb_now`
#define JOB_TYPE_CLEAR_IMMEDIATE(jb_now) \
   jb_now = jb_now & ~JOB_TYPE_IMMEDIATE

/// Set #JOB_TYPE_IMMEDIATE in `jb_now`
#define JOB_TYPE_SET_IMMEDIATE(jb_now) \
   jb_now =  jb_now | JOB_TYPE_IMMEDIATE

/// Make `jb_now` #JOB_TYPE_QSH, clearing the other interactive kinds
#define JOB_TYPE_SET_QSH(jb_now) \
   jb_now = (jb_now & (~JOB_TYPE_QXXX_MASK)) | JOB_TYPE_QSH

/// Make `jb_now` #JOB_TYPE_QLOGIN, clearing the other interactive kinds
#define JOB_TYPE_SET_QLOGIN(jb_now) \
   jb_now = (jb_now & (~JOB_TYPE_QXXX_MASK)) | JOB_TYPE_QLOGIN

/// Make `jb_now` #JOB_TYPE_QRSH, clearing the other interactive kinds
#define JOB_TYPE_SET_QRSH(jb_now) \
   jb_now = (jb_now & ~JOB_TYPE_QXXX_MASK) | JOB_TYPE_QRSH

/// Make `jb_now` #JOB_TYPE_QRLOGIN, clearing the other interactive kinds
#define JOB_TYPE_SET_QRLOGIN(jb_now) \
   jb_now = (jb_now & ~JOB_TYPE_QXXX_MASK) | JOB_TYPE_QRLOGIN

/// Set #JOB_TYPE_BINARY in `jb_now`
#define JOB_TYPE_SET_BINARY(jb_now) \
   jb_now = jb_now | JOB_TYPE_BINARY

/// Clear #JOB_TYPE_BINARY in `jb_now`
#define JOB_TYPE_CLEAR_BINARY(jb_now) \
   jb_now = jb_now & ~JOB_TYPE_BINARY

/// Set #JOB_TYPE_ARRAY in `jb_now`
#define JOB_TYPE_SET_ARRAY(jb_now) \
   jb_now = jb_now | JOB_TYPE_ARRAY

/// Clear #JOB_TYPE_NO_ERROR in `jb_now`
#define JOB_TYPE_CLEAR_NO_ERROR(jb_now) \
   jb_now = jb_now & ~JOB_TYPE_NO_ERROR

/// Set #JOB_TYPE_NO_ERROR in `jb_now`
#define JOB_TYPE_SET_NO_ERROR(jb_now) \
   jb_now =  jb_now | JOB_TYPE_NO_ERROR

/// Set #JOB_TYPE_NO_SHELL in `jb_now`
#define JOB_TYPE_SET_NO_SHELL(jb_now) \
   jb_now =  jb_now | JOB_TYPE_NO_SHELL

/// Clear #JOB_TYPE_NO_SHELL in `jb_now`
#define JOB_TYPE_CLEAR_NO_SHELL(jb_now) \
   jb_now =  jb_now & ~JOB_TYPE_NO_SHELL

/// Clear #JOB_TYPE_BINARY in `jb_now`
#define JOB_TYPE_UNSET_BINARY(jb_now) \
   jb_now = jb_now & ~JOB_TYPE_BINARY

/// Clear #JOB_TYPE_NO_SHELL in `jb_now`
#define JOB_TYPE_UNSET_NO_SHELL(jb_now) \
   jb_now =  jb_now & ~JOB_TYPE_NO_SHELL

/// Is #JOB_TYPE_IMMEDIATE set in `jb_now`?
#define JOB_TYPE_IS_IMMEDIATE(jb_now)      (jb_now & JOB_TYPE_IMMEDIATE)
/// Is #JOB_TYPE_QSH set in `jb_now`?
#define JOB_TYPE_IS_QSH(jb_now)            (jb_now & JOB_TYPE_QSH)
/// Is #JOB_TYPE_QLOGIN set in `jb_now`?
#define JOB_TYPE_IS_QLOGIN(jb_now)         (jb_now & JOB_TYPE_QLOGIN)
/// Is #JOB_TYPE_QRSH set in `jb_now`?
#define JOB_TYPE_IS_QRSH(jb_now)           (jb_now & JOB_TYPE_QRSH)
/// Is #JOB_TYPE_QRLOGIN set in `jb_now`?
#define JOB_TYPE_IS_QRLOGIN(jb_now)        (jb_now & JOB_TYPE_QRLOGIN)
/// Is #JOB_TYPE_BINARY set in `jb_now`?
#define JOB_TYPE_IS_BINARY(jb_now)         (jb_now & JOB_TYPE_BINARY)
/// Is #JOB_TYPE_ARRAY set in `jb_now`?
#define JOB_TYPE_IS_ARRAY(jb_now)          (jb_now & JOB_TYPE_ARRAY)
/// Is #JOB_TYPE_NO_ERROR set in `jb_now`?
#define JOB_TYPE_IS_NO_ERROR(jb_now)       (jb_now & JOB_TYPE_NO_ERROR)
/// Is #JOB_TYPE_NO_SHELL set in `jb_now`?
#define JOB_TYPE_IS_NO_SHELL(jb_now)       (jb_now & JOB_TYPE_NO_SHELL)
/** @} */


bool job_is_enrolled(const lListElem *job,
                     uint32_t ja_task_number);

uint32_t job_get_ja_tasks(const lListElem *job);

uint32_t job_get_not_enrolled_ja_tasks(const lListElem *job);

uint32_t job_get_enrolled_ja_tasks(const lListElem *job);

uint32_t job_get_submit_ja_tasks(const lListElem *job);

lListElem *job_enroll(lListElem *job, lList **answer_list, uint32_t task_number);

void job_unenroll(lListElem *job, lList **answer_list, lListElem **ja_task);

void job_delete_not_enrolled_ja_task(lListElem *job, lList **answer_list,
                                     uint32_t ja_task_number);

uint32_t job_count_pending_tasks(const lListElem *job, bool count_all);

bool job_has_soft_requests(lListElem *job);

bool job_is_ja_task_defined(const lListElem *job, uint32_t ja_task_number);

void job_set_hold_state(lListElem *job,
                        lList **answer_list, uint32_t ja_task_id,
                        uint32_t new_hold_state);

uint32_t job_get_hold_state(lListElem *job, uint32_t ja_task_id);

/* int job_add_job(lList **job_list, char *name, lListElem *job, int check,
                 int hash, htable* Job_Hash_Table); */

/**
 * @brief Print a job list for debugging
 *
 * @param job_list the list to print
 *
 * @warning Declared here but defined nowhere in the tree, and never called.
 */
void job_list_print(lList *job_list);

lListElem *job_get_ja_task_template(const lListElem *job, uint32_t ja_task_id);

lListElem *job_get_ja_task_template_hold(const lListElem *job,
                                         uint32_t ja_task_id,
                                         uint32_t hold_state);

lListElem *job_get_ja_task_template_pending(const lListElem *job,
                                            uint32_t ja_task_id);

lListElem *job_search_task(const lListElem *job, lList **answer_list, uint32_t ja_task_id);
lListElem *job_create_task(lListElem *job, lList **answer_list, uint32_t ja_task_id);

int job_list_add_job(lList **job_list, const char *name, lListElem *job,
                     int check);

uint32_t job_get_ja_task_hold_state(const lListElem *job, uint32_t ja_task_id);

void job_destroy_hold_id_lists(const lListElem *job, lList *id_list[16]);

void job_create_hold_id_lists(const lListElem *job, lList *id_list[16],
                              uint32_t hold_state[16]);

const char *job_get_shell_start_mode(const lListElem *queue, const char *conf_shell_start_mode);

bool job_is_array(const lListElem *job);

bool job_is_parallel(const lListElem *job);

bool job_is_tight_parallel(const lListElem *job, const lList *pe_list);

bool job_might_be_tight_parallel(const lListElem *job, const lList *pe_list);

void job_get_submit_task_ids(const lListElem *job, uint32_t *start,
                             uint32_t *end, uint32_t *step);

int job_set_submit_task_ids(lListElem *job, uint32_t start, uint32_t end,
                            uint32_t step);

uint32_t job_get_smallest_unenrolled_task_id(const lListElem *job);

uint32_t job_get_smallest_enrolled_task_id(const lListElem *job);

uint32_t job_get_biggest_unenrolled_task_id(const lListElem *job);

uint32_t job_get_biggest_enrolled_task_id(const lListElem *job);

int job_list_register_new_job(const lList *job_list, uint32_t max_jobs,
                              int force_registration);

/**
 * @brief Render an array task list as a range string
 *
 * @param task_list the tasks to render
 * @param[out] range_string receives the rendered ranges
 *
 * @warning Declared here but defined nowhere in the tree, and never called.
 */
void jatask_list_print_to_string(const lList *task_list, dstring *range_string);

lList* ja_task_list_split_group(lList **task_list);

int job_initialize_id_lists(lListElem *job, lList **answer_list);

void job_initialize_env(lListElem *job,
                        lList **answer_list,
                        const lList* path_alias_list,
                        const char *unqualified_hostname,
                        const char *qualified_hostname);

const char* job_get_env_string(const lListElem *job, const char *variable);

void job_set_env_string(lListElem *job, const char *variable,
                        const char *value);

void job_check_correct_id_sublists(lListElem *job, lList **answer_list);

const char *job_get_id_string(uint32_t job_id, uint32_t ja_task_id,
                              const char *pe_task_id, dstring *buffer);

const char *job_get_job_key(uint32_t job_id, dstring *buffer);

const char *job_get_key(uint32_t job_id, uint32_t ja_task_id,
                        const char *pe_task_id, dstring *buffer);

const char *jobscript_get_key(const lListElem *jep, dstring *buffer);

char *jobscript_parse_key(char *key,const char **exec_file);

bool job_parse_key(char *key, uint32_t *job_id, uint32_t *ja_task_id,
                   char **pe_task_id, bool *only_job);

bool job_is_pe_referenced(const lListElem *job, const lListElem *pe);

bool job_is_ckpt_referenced(const lListElem *job, const lListElem *ckpt);

void job_get_state_string(char *str, uint32_t op);

void job_add_parent_id_to_context(lListElem *job);

int job_check_qsh_display(const lListElem *job,
                          lList **answer_list,
                          bool output_warning);

int job_check_owner(const ocs::gdi::Packet *packet, uint32_t job_id, lList *master_job_list);

int job_resolve_host_for_path_list(const lListElem *job, lList **answer_list, int name);

const lListElem *
job_get_request(const lListElem *job, const char *centry_name);

const lListElem *
job_get_hard_request(const lListElem *job, const char *name, bool is_master_task);

bool
job_get_contribution(const lListElem *job, lList **answer_list, const char *name, double *value,
                     const lListElem *complex_definition, bool is_master_task);
bool
job_get_contribution_by_scope(const lListElem *job, lList **answer_list, const char *name, double *value,
                              const lListElem *complex_definition, uint32_t scope);

void
adjust_slave_task_debit_slots(const lListElem *pe, int &slave_debit_slots);

/* unparse functions */
bool sge_unparse_string_option_dstring(dstring *category_str, const lListElem *job_elem,
                               int pos, const char *option);

bool sge_unparse_ulong_option_dstring(dstring *category_str, const lListElem *job_elem,
                               int pos, const char *option);

bool sge_unparse_binding_dstring(dstring *category_str, const lListElem *job, int pos);

bool sge_unparse_pe_dstring(dstring *category_str, const lListElem *job_elem, int pe_pos, int range_pos,
                            const char *option);

bool sge_unparse_resource_list_dstring(dstring *category_str, lList *resource_list, const char *option);

bool sge_unparse_queue_list_dstring(dstring *category_str, lList *queue_list, const char *option);

bool sge_unparse_acl_dstring(dstring *category_str, const char *owner, const char *group, const lList *grp_list,
                             const lList *acl_list, const char *option);

bool job_verify(const lListElem *job, lList **answer_list, bool do_cull_verify);
bool job_verify_submitted_job(lListElem *job, lList **answer_list);

bool job_get_wallclock_limit(uint64_t *limit, const lListElem *jep);

bool
job_is_binary(const lListElem *job);

bool
job_set_binary(lListElem *job, bool is_binary);

bool
job_is_no_shell(const lListElem *job);

bool
job_set_no_shell(lListElem *job, bool is_no_shell);

bool
job_set_owner_and_group(lListElem *job, uint32_t uid, uint32_t gid,
                        const char *user, const char *grouprp, int amount, ocs_grp_elem_t *grp_array);

void
job_get_ckpt_attr(std::ostream &os, uint32_t op);

bool
job_get_ckpt_attr(uint32_t op, dstring *string);

bool
job_get_verify_attr(uint32_t op, dstring *string);

void
set_context(lList *jbctx, lListElem *job);

bool
job_parse_validation_level(int *level, const char *input, int prog_number, lList **answer_list);

bool
job_is_requesting_consumable(lListElem *jep, const char *resource_name);

/**
 * @brief Give a job a default binding element
 *
 * @param jep the job to initialise
 * @return true when the element was created
 *
 * @warning Declared here but defined nowhere in the tree, and never called.
 */
bool
job_init_binding_elem(lListElem *jep);

// Defines and Functions for Job Resource Sets (JRS_Type)

/**
 * @name Job resource set scopes
 *
 * A job may state different requests for its parts. A sequential job only has
 * the global scope; a parallel job may additionally request something specific
 * for its master task and something else for its slave tasks.
 * @{
 */
#define JRS_SCOPE_GLOBAL 0 ///< requests that apply to the whole job
#define JRS_SCOPE_MASTER 1 ///< requests that apply to a parallel job's master task
#define JRS_SCOPE_SLAVE  2 ///< requests that apply to a parallel job's slave tasks
/** @} */

/**
 * @brief The name of a scope, usable in a constant expression
 *
 * @param scope one of the `JRS_SCOPE_*` values
 * @return `global`, `master`, `slave`, or `unknown`
 */
constexpr const char *scope_to_string(const int scope) {
   switch (scope) {
      case JRS_SCOPE_GLOBAL:
         return "global";
      case JRS_SCOPE_MASTER:
         return "master";
      case JRS_SCOPE_SLAVE:
         return "slave";
      default:
         return "unknown";
   }
}

bool job_parse_scope_string(const char *scope, char &scope_id);
const char *job_scope_name(uint32_t scope_id);
const char *job_scope_name(const lListElem *scope_ep);
std::string get_scope_list_name(uint32_t scope, int nm, bool with_colon = false);

const lListElem *job_get_request_set(const lListElem *job, uint32_t scope);
lListElem *job_get_request_setRW(lListElem *job, uint32_t scope);
lListElem *job_get_or_create_request_setRW(lListElem *job, uint32_t scope);

bool job_request_set_remove_duplicates(lListElem *job);
bool job_request_set_has_queue_requests(const lListElem *job);

const lListElem *job_get_highest_hard_request(const lListElem *job, const char *request_name);

const lList *job_get_resource_list(const lListElem *job, uint32_t scope, bool hard);
const lList *job_get_queue_list(const lListElem *job, uint32_t scope, bool hard);

const lList *job_get_hard_resource_list(const lListElem *job);
const lList *job_get_hard_resource_list(const lListElem *job, uint32_t scope);
const lList *job_get_soft_resource_list(const lListElem *job);
const lList *job_get_soft_resource_list(const lListElem *job, uint32_t scope);

const lList *job_get_hard_queue_list(const lListElem *job);
const lList *job_get_hard_queue_list(const lListElem *job, uint32_t scope);
const lList *job_get_soft_queue_list(const lListElem *job);
const lList *job_get_soft_queue_list(const lListElem *job, uint32_t scope);
const lList *job_get_master_hard_queue_list(const lListElem *job);

lList *job_get_resource_listRW(lListElem *job, uint32_t scope, bool hard);
lList *job_get_hard_resource_listRW(lListElem *job);
lList *job_get_hard_resource_listRW(lListElem *job, uint32_t scope);
lList *job_get_soft_resource_listRW(lListElem *job);
lList *job_get_soft_resource_listRW(lListElem *job, uint32_t scope);
lList *job_get_queue_listRW(lListElem *job, uint32_t scope, bool hard);
lList *job_get_hard_queue_listRW(lListElem *job);
lList *job_get_hard_queue_listRW(lListElem *job, uint32_t scope);
lList *job_get_soft_queue_listRW(lListElem *job);
lList *job_get_soft_queue_listRW(lListElem *job, uint32_t scope);
lList *job_get_master_hard_queue_listRW(lListElem *job);

void job_set_resource_list(lListElem *job, lList *resource_list, uint32_t scope, bool hard);
void job_set_hard_resource_list(lListElem *job, lList *resource_list);
void job_set_hard_resource_list(lListElem *job, lList *resource_list, uint32_t scope);
void job_set_soft_resource_list(lListElem *job, lList *resource_list);
void job_set_soft_resource_list(lListElem *job, lList *resource_list, uint32_t scope);

void job_set_queue_list(lListElem *job, lList *queue_list, uint32_t scope, bool hard);
void job_set_hard_queue_list(lListElem *job, lList *queue_list);
void job_set_hard_queue_list(lListElem *job, lList *queue_list, uint32_t scope);
void job_set_soft_queue_list(lListElem *job, lList *queue_list);
void job_set_soft_queue_list(lListElem *job, lList *queue_list, uint32_t scope);
void job_set_master_hard_queue_list(lListElem *job, lList *queue_list);

const char *job_get_allocation_rule(const lListElem *job, uint32_t scope);
void job_set_allocation_rule(lListElem *job, const char *allocation_rule, uint32_t scope);

const char *
job_get_effective_command_line(const lListElem *job, dstring *dstr, const char *client);

void
job_set_command_line(lListElem *job, const char *client);

void
job_set_command_line(lListElem *job, int argc, const char *argv[]);

void
job_set_sync_options(lListElem *job, uint32_t sync_options);

std::string
job_get_sync_options_string(const lListElem *job);

bool
job_is_visible(const ocs::gdi::Packet *packet, const char *owner, bool is_manager);

void
job_normalize_priority(lListElem *jep, uint32_t priority);

lList *
gdil_make_host_unique(const lList *gdil_in);

uint32_t
jatask_combine_state_and_status_for_output(const lListElem *job, const lListElem *jatep);

bool
job_parse_when_string(const char *input, qalter_when_t &when);

bool
job_is_when_now(const lListElem *job);
