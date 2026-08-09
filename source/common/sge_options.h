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
 *  Portions of this code are Copyright 2011 Univa Inc.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The `*_OPT` enum naming every command line option of every client
 */

#include "uti/sge_component.h"

enum {
   NO_OPT = 0,   ///< not an option; the zero value used to mean "none"
   a_OPT,   ///< `-a`: request a start time
   A_OPT,   ///< `-A`: account string in accounting record
   c_OPT,   ///< `-c`: define type of checkpointing for job
   cat_OPT,   ///< `-cat`: the job category, used by DRMAA rather than by a command line client
   cl_OPT,   ///< `-cl`: define type of checkpointing for job (shares the `-c` usage text)
   cell_OPT,   ///< `-cell`: the cell to operate on, overriding `SGE_CELL`
   C_OPT,   ///< `-C`: define command prefix for job script
   e_OPT,   ///< `-e`: enable
   pe_OPT,   ///< `-pe`: request slot range for parallel jobs
   f_OPT,   ///< `-f`: full output
   h_OPT,   ///< `-h`: assign holds for jobs or tasks

   hard_OPT,   ///< `-hard`: consider following requests "hard"
   i_OPT,   ///< `-i`: specify standard input stream file(s)
   j_OPT,   ///< `-j`: merge stdout and stderr stream of job
   js_OPT,   ///< `-js`: share tree or functional job share
   jsv_OPT,   ///< `-jsv`: job submission verification script to be used
   l_OPT,   ///< `-l`: request the given resources
   m_OPT,   ///< `-m`: define mail notification events
   M_OPT,   ///< `-M`: notify these e-mail addresses
   N_OPT,   ///< `-N`: specify job name
   o_OPT,   ///< `-o`: specify standard output stream path(s)
   p_OPT,   ///< `-p`: define job's relative priority
   q_OPT,   ///< `-q`: bind job to queue(s)

   r_OPT,   ///< `-r`: whether the job is rerunnable
   res_OPT,   ///< `-res`: show requested resources of job(s)
   s_OPT,   ///< `-s`: suspend
   scope_OPT,   ///< `-scope`: switch the request scope
   shell_OPT,   ///< `-shell`: start command with or without wrapping `<loginshell> -c`
   soft_OPT,   ///< `-soft`: consider following requests as soft
   sync_OPT,   ///< `-sync`: wait for job to end and return exit code
   S_OPT,   ///< `-S`: command interpreter to be used
   t_OPT,   ///< `-t`: create a job-array with these tasks
   u_OPT,   ///< `-u`: specify a list of users
   v_OPT,   ///< `-v`: export these environment variables
   verify_OPT,   ///< `-verify`: do not submit just verify
   V_OPT,   ///< `-V`: export all environment variables
   JOB_ID_OPR,   ///< operand: jobid's (and taskid's) of jobs to be altered
   JOB_TASK_OPR,   ///< operand: a job id, optionally with an array task range

   SCRIPT_OPR,   ///< operand: the job script and its arguments
   help_OPT,   ///< `-help`: print this help
   cwd_OPT,   ///< `-cwd`: use current working directory
   ext_OPT,   ///< `-ext`: view also scheduling attributes
   notify_OPT,   ///< `-notify`: notify job before killing/suspending it
   now_OPT,   ///< `-now`: start job immediately or not at all
   b_OPT,   ///< `-b`: handle command as binary
   wd_OPT,   ///< `-wd`: use working_directory

   masterq_OPT,   ///< `-masterq`: bind master task to queue(s)
   d_OPT,   ///< `-d`: disable
   us_OPT,   ///< `-us`: unsuspend
   hold_jid_OPT,   ///< `-hold_jid`: define jobnet interdependencies
   hold_jid_ad_OPT,   ///< `-hold_jid_ad`: define jobnet array interdependencies
   JQ_DEST_OPR,   ///< operand: a queue destination, `queue` or `queue@host`
   ac_OPT,   ///< `-ac`: add context variable(s)
   ah_OPT,   ///< `-ah`: add an administrative host
   am_OPT,   ///< `-am`: add user to manager list
   ao_OPT,   ///< `-ao`: add user to operator list
   aq_OPT,   ///< `-aq`: add a new cluster queue

   au_OPT,   ///< `-au`: add user(s) to userset list(s)
   Au_OPT,   ///< `-Au`: add userset from file or directory
   Aq_OPT,   ///< `-Aq`: add queue(s) from a file or directory
   cq_OPT,   ///< `-cq`: clean queue
   dc_OPT,   ///< `-dc`: delete context variable(s)
   dh_OPT,   ///< `-dh`: delete administrative host
   dm_OPT,   ///< `-dm`: delete user from manager list
   do_OPT,   ///< `-do`: delete user from operator list
   dq_OPT,   ///< `-dq`: delete cluster queue(s)
   Dq_OPT,   ///< `-Dq`: del cqueue objects from file/directory (CS-2305)
   du_OPT,   ///< `-du`: delete user(s) from userset list(s)
   Du_OPT,   ///< `-Du`: del userset objects from file/directory (CS-2310)
   ke_OPT,   ///< `-ke`: shutdown execution daemon(s)

   mc_OPT,   ///< `-mc`: modify complex attributes
   mconf_OPT,   ///< `-mconf`: modify configurations
   mq_OPT,   ///< `-mq`: modify a queue
   sc_OPT,   ///< `-sc`: show complex attributes
   sconf_OPT,   ///< `-sconf`: show configurations
   sh_OPT,   ///< `-sh`: show a list of all administrative hosts
   sm_OPT,   ///< `-sm`: show a list of all managers

   so_OPT,   ///< `-so`: show a list of all operators
   sq_OPT,   ///< `-sq`: show queue configuration
   sql_OPT,   ///< `-sql`: show a list of all queues
   su_OPT,   ///< `-su`: show userset
   sul_OPT,   ///< `-sul`: show a list of all userset lists
   jid_OPT,   ///< `-jid`: jobs to be printed
   gc_OPT,   ///< `-gc`: dummy from qconf to qmaster to get complex
   ae_OPT,   ///< `-ae`: add an exec host using a template
   Ae_OPT,   ///< `-Ae`: add exec host(s) from a file or directory

   as_OPT,   ///< `-as`: add a submit host
   de_OPT,   ///< `-de`: delete exec host(s)
   De_OPT,   ///< `-De`: del exec hosts from file/directory (CS-2304)
   ds_OPT,   ///< `-ds`: delete submit host
   Mc_OPT,   ///< `-Mc`: modify complex attributes from file
   me_OPT,   ///< `-me`: modify exec server
   Me_OPT,   ///< `-Me`: modify exec host(s) from a file or directory
   sel_OPT,   ///< `-sel`: show a list of all exec servers
   se_OPT,   ///< `-se`: show given exec server
   ss_OPT,   ///< `-ss`: show a list of all submit hosts
   km_OPT,   ///< `-km`: shut the qmaster down

   ks_OPT,   ///< `-ks`: shut the scheduler down
   ap_OPT,   ///< `-ap`: add pe object
   mp_OPT,   ///< `-mp`: mod pe object
   dp_OPT,   ///< `-dp`: del pe object
   Dp_OPT,   ///< `-Dp`: del pe objects from file/directory (CS-2301)
   sp_OPT,   ///< `-sp`: show pe object
   spl_OPT,   ///< `-spl`: show pe object list
   sconfl_OPT,   ///< `-sconfl`: show list of local configurations
   dconf_OPT,   ///< `-dconf`: delete local configuration
   Dconf_OPT,   ///< `-Dconf`: delete local configuration(s) from file/directory (CS-2311)
   Fmt_OPT,   ///< `-Fmt`: -fmt plain|json: output/input serialization format (CS-2313a)
   starthist_OPT,   ///< `-starthist`: flush history

   Mq_OPT,   ///< `-Mq`: modify queue(s) from a file or directory
   aconf_OPT,   ///< `-aconf`: add configurations
   nostart_commd_OPT,   ///< `-nostart-commd`: obsolete; the commd no longer exists
   sep_OPT,   ///< `-sep`: show a list of all licensed processors
   Aconf_OPT,   ///< `-Aconf`: add configurations from file_list or directory
   Mconf_OPT,   ///< `-Mconf`: modify configurations from file_list or directory
   clear_OPT,   ///< `-clear`: skip previous definitions for job

   AT_OPT,   ///< `-AT`: read commandline input from file
   Ap_OPT,   ///< `-Ap`: add pe object from file
   Mp_OPT,   ///< `-Mp`: mod pe object from file
   tsm_OPT,   ///< `-tsm`: trigger scheduler monitoring
   msconf_OPT,   ///< `-msconf`: modify SGE scheduler configuration
   Msconf_OPT,   ///< `-Msconf`: mofify SGE scheduler configuration from file
   aus_OPT,   ///< `-aus`: SGE add user
   Aus_OPT,   ///< `-Aus`: SGE add user from file
   mus_OPT,   ///< `-mus`: SGE modify user
   Mus_OPT,   ///< `-Mus`: SGE modify user from file
   dus_OPT,   ///< `-dus`: SGE delete user
   Dus_OPT,   ///< `-Dus`: SGE delete users from file/directory (CS-2308)
   sus_OPT,   ///< `-sus`: SGE show user

   susl_OPT,   ///< `-susl`: SGE show user list
   aprj_OPT,   ///< `-aprj`: SGE add project
   Aprj_OPT,   ///< `-Aprj`: SGE add project from file
   Mprj_OPT,   ///< `-Mprj`: SGE modify project from file
   mprj_OPT,   ///< `-mprj`: SGE modify project
   dprj_OPT,   ///< `-dprj`: SGE delete project
   Dprj_OPT,   ///< `-Dprj`: SGE delete projects from file/directory (CS-2309)
   sprj_OPT,   ///< `-sprj`: SGE show project
   sprjl_OPT,   ///< `-sprjl`: SGE show project list
   mstree_OPT,   ///< `-mstree`: SGE modify sharetree
   Mstree_OPT,   ///< `-Mstree`: SGE modify sharetree from file
   astree_OPT,   ///< `-astree`: SGE add sharetree
   Astree_OPT,   ///< `-Astree`: SGE add sharetree from file
   dstree_OPT,   ///< `-dstree`: SGE delete sharetree
   sstree_OPT,   ///< `-sstree`: SGE show sharetree
   sst_OPT,   ///< `-sst`: SGE show a formated sharetree

   mu_OPT,   ///< `-mu`: edit userset object (not only SGE)
   Mu_OPT,   ///< `-Mu`: modify userset from file
   dl_OPT,   ///< `-dl`: SGE deadline initiation
   P_OPT,   ///< `-P`: SGE Project
   ot_OPT,   ///< `-ot`: SGE override tickets option

   /* added for checkpointing */
   ackpt_OPT,   ///< `-ackpt`: add ckpt element
   Ackpt_OPT,   ///< `-Ackpt`: add ckpt element from file
   dckpt_OPT,   ///< `-dckpt`: delete ckpt element
   Dckpt_OPT,   ///< `-Dckpt`: delete ckpt elements listed in file/directory (CS-2300)
   mckpt_OPT,   ///< `-mckpt`: modify ckpt element
   Mckpt_OPT,   ///< `-Mckpt`: modify ckpt element from file
   sckpt_OPT,   ///< `-sckpt`: show ckpt element
   sckptl_OPT,   ///< `-sckptl`: show all ckpt elements
   ckptobj_OPT,   ///< `-ckptobj`: -ckpt in qsub

   dul_OPT,   ///< `-dul`: "-dul <user_set>," in qconf
   display_OPT,   ///< `-display`: -display option for qsh
   sss_OPT,   ///< `-sss`: show scheduler state
   sick_OPT,   ///< `-sick`: show deficient configurations
   ssconf_OPT,   ///< `-ssconf`: show scheduler configuration

   /* calendar management */
   acal_OPT,   ///< `-acal`: add new calendar interactively
   Acal_OPT,   ///< `-Acal`: add new calendar from file
   mcal_OPT,   ///< `-mcal`: modify calendar interactively
   Mcal_OPT,   ///< `-Mcal`: modify calendar from file
   dcal_OPT,   ///< `-dcal`: remove calendar
   Dcal_OPT,   ///< `-Dcal`: remove calendars listed in file/directory (CS-2299)
   scal_OPT,   ///< `-scal`: show calendar
   scall_OPT,   ///< `-scall`: show calendar list
   w_OPT,   ///< `-w`: warn mode concerning verification of schedulability

   /* share tree node */
   astnode_OPT,   ///< `-astnode`: SGE add share tree node
   dstnode_OPT,   ///< `-dstnode`: SGE delete share tree node
   mstnode_OPT,   ///< `-mstnode`: SGE modify share tree node
   sstnode_OPT,   ///< `-sstnode`: SGE show share tree node
   rsstnode_OPT,   ///< `-rsstnode`: SGE show share tree node and its children

   /* verbosity */
   verbose_OPT,   ///< `-verbose`: verbose option for q(r)sh
   inherit_OPT,   ///< `-inherit`: inherit option for qrsh, inherit existing job $JOB_ID
   nostdin_OPT,   ///< `-nostdin`: nostdin option for qrsh, pass as -n option to rsh
   noshell_OPT,   ///< `-noshell`: noshell option for qrsh, pass as noshell option to qrsh_starter
   pty_OPT,   ///< `-pty`: pty option for qrsh, start job in a pty
   x11_OPT,   ///< `-x11`: X11 forwarding for builtin IJS mode (qrsh/qlogin)

   /* add/set/delete/modify sge objects */
   mattr_OPT,   ///< `-mattr`: modify a sublist of an object
   rattr_OPT,   ///< `-rattr`: overwrite a sublist
   dattr_OPT,   ///< `-dattr`: delete some elements of a sublist
   aattr_OPT,   ///< `-aattr`: add a element to a sublist
   Mattr_OPT,   ///< `-Mattr`: modifiy a sublist from file
   Rattr_OPT,   ///< `-Rattr`: overwrite a sublist from file
   Dattr_OPT,   ///< `-Dattr`: aelete a sublist from file
   Aattr_OPT,   ///< `-Aattr`: add a element to a sublist from file
   sobjl_OPT,   ///< `-sobjl`: show object list which matches conf value
   purge_OPT,   ///< `-purge`: delete element which value matches given string

   /* added for host groups */
   ahgrp_OPT,   ///< `-ahgrp`: add new host group entry
   Ahgrp_OPT,   ///< `-Ahgrp`: add new host group entry from file
   dhgrp_OPT,   ///< `-dhgrp`: delete host group entry
   Dhgrp_OPT,   ///< `-Dhgrp`: delete host groups from file/directory (CS-2306)
   mhgrp_OPT,   ///< `-mhgrp`: modify host group entry
   shgrp_OPT,   ///< `-shgrp`: show host group entry
   shgrp_tree_OPT,   ///< `-shgrp_tree`: show host group entry as tree
   shgrp_resolved_OPT,   ///< `-shgrp_resolved`: show host group entry with resolved hostlist
   shgrpl_OPT,   ///< `-shgrpl`: show host group entry list
   Mhgrp_OPT,   ///< `-Mhgrp`: modify host group entry from file

   /* added for event clients */
   secl_OPT,   ///< `-secl`: show event client list
   kec_OPT,   ///< `-kec`: kill event client

   cu_OPT,   ///< `-cu`: SGEEE sharetree - clear all user/project usage
   R_OPT,   ///< `-R`: SGEEE sharetree - clear all user/project usage

   /* added for resource quota sets */
   srqs_OPT,   ///< `-srqs`: show resource quota set
   srqsl_OPT,   ///< `-srqsl`: show resource quota set list
   arqs_OPT,   ///< `-arqs`: add resource quota set
   Arqs_OPT,   ///< `-Arqs`: add resource quota set from file
   mrqs_OPT,   ///< `-mrqs`: modfiy resource quota set
   Mrqs_OPT,   ///< `-Mrqs`: modify resource quota set from file
   drqs_OPT,   ///< `-drqs`: delete resource quota set
   Drqs_OPT,   ///< `-Drqs`: delete resource quota sets from file/directory (CS-2307)
   ar_OPT,   ///< `-ar`: advanced resservation option
   he_OPT,   ///< `-he`: error handling for qrsub
   explain_OPT,   ///< `-explain`: explain error in qrstat
   xml_OPT,   ///< `-xml`: generate xml outout
   terse_OPT,   ///< `-terse`: tersed output
   at_OPT,   ///< `-at`: add/start thread
   kt_OPT,   ///< `-kt`: kill/terminate thread

   tc_OPT,   ///< `-tc`: task concurrency

   suspend_remote_OPT,   ///< `-suspend_remote`: parameter for qrsh to toggle the suspend behavior

   ace_OPT,   ///< `-ace`: add ce object
   Ace_OPT,   ///< `-Ace`: add ce object
   mce_OPT,   ///< `-mce`: mod ce object
   Mce_OPT,   ///< `-Mce`: mod ce object
   dce_OPT,   ///< `-dce`: del ce object
   Dce_OPT,   ///< `-Dce`: del ce objects from file/directory (CS-2303)
   sce_OPT,   ///< `-sce`: show ce object
   scel_OPT,   ///< `-scel`: show ce object list

   stl_OPT,   ///< `-stl`: show thread pool list
   dept_OPT,  ///< `-dept`: set job's department
   scatl_OPT,   ///< `-scatl`: show list of all categories
   scat_OPT,   ///< `-scat`: show category

   btype_OPT,   ///< `-btype`: set type of binding
   bunit_OPT,   ///< `-bunit`: set binding unit
   bfilter_OPT,   ///< `-bfilter`: specifies binding filter to mask binding units
   bsort_OPT,   ///< `-bsort`: enables and specifies binding sort order
   bstart_OPT,   ///< `-bstart`: defines the start position for binding
   bstop_OPT,   ///< `-bstop`: defines the stop position for binding
   bstrategy_OPT,   ///< `-bstrategy`: defines the binding strategy
   bamount_OPT,   ///< `-bamount`: defines the number of binding units to be used
   binstance_OPT,   ///< `-binstance`: defines the instance applying the binding

   when_OPT,   ///< `-when`: for qalter, whether the change applies to a running job or only after rescheduling
   par_OPT,   ///< `-par`: set the parallel job allocation rule
   ectx_OPT, ///< execution context
   fmt_OPT, ///< output format

   /* role management */
   arole_OPT,   ///< add role interactively
   Arole_OPT,   ///< add role from file
   drole_OPT,   ///< delete role
   Drole_OPT,   ///< delete roles listed in file/directory (CS-2302)
   mrole_OPT,   ///< modify role interactively
   Mrole_OPT,   ///< modify role from file
   srole_OPT,   ///< show role
   srolel_OPT,  ///< show all role names

   /* CS-23xx: bulk export (-S<obj> name|dir) — save objects to a file or directory.
    * NOTE: append-only and kept in this order; the sge_options[][] rows in
    * sge_options.cc must stay positionally aligned with these enum values. */
   Scal_OPT,    ///< export calendar(s)
   Sckpt_OPT,   ///< export checkpoint env(s)
   Sce_OPT,     ///< export complex entry/entries
   Se_OPT,      ///< export exec host(s)
   Shgrp_OPT,   ///< export host group(s)
   Sp_OPT,      ///< export parallel environment(s)
   Sprj_OPT,    ///< export project(s)
   Sq_OPT,      ///< export cluster queue(s)
   Srole_OPT,   ///< export role(s)
   Srqs_OPT,    ///< export resource quota set(s)
   Su_OPT,      ///< export userset(s)
   Suser_OPT,   ///< export user(s)
   Sstree_OPT,  ///< export the share tree (singleton)
   Ssconf_OPT,  ///< export the scheduler configuration (singleton)
   Sconf_OPT,   ///< export global + host configuration(s)
};

/* macros used in parsing */
/** @brief Is option @p opt accepted by client @p who?
 *
 * @param opt one of the `*_OPT` enumerators above
 * @param who a `prog_number`, e.g. `QSUB` or `QCONF`
 */
#define VALID_OPT(opt,who) (sge_options[opt][who])

/** @brief Which command line option each client accepts
 *
 * One row per `*_OPT` enumerator, one column per client, in the order of the
 * `prog_number` values. Read it through #VALID_OPT rather than directly.
 */
extern unsigned short sge_options[][ALL_OPT + 1];
