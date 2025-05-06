#ifndef __MSG_EXECD_H
#define __MSG_EXECD_H
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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/


#include "basis_types.h"

// clang-format off

/* CR: don't localize mail subject, until we send it in Mime format!
 *  The message definition is not l10n'ed (no _() macro used)!!!     */
#define MSG_MAIL_STARTSUBJECT_UUS     "Job-array task " sge_u32 "." sge_u32 " (" SFN ") Started"
#define MSG_MAIL_STARTSUBJECT_US      "Job " sge_u32 " (" SFN ") Started"


#define MSG_COM_ACK_UNKNOWN1_S                              _MESSAGE(29000, _("unknown ack event from host " SFN))
#define MSG_FILE_RECURSIVERMDIR_SS                          _MESSAGE(29001, _("recursive rmdir(" SFN "): " SFN))

#define MSG_JOB_XREGISTERINGJOBYATPTFDURINGSTARTUP_SU       _MESSAGE(29002, _(SFN " registering job \"" sge_u32 "\" at ptf during startup"))
#define MSG_FAILED                                          _MESSAGE(29003, _("failed"))
#define MSG_DELAYED                                         _MESSAGE(29004, _("delayed"))
#define MSG_JOB_XREGISTERINGJOBYTASKZATPTFDURINGSTARTUP_SUS _MESSAGE(29005, _(SFN " registering job \"" sge_u32 "\" task " SFN " at ptf during startup"))

#define MSG_SLAVE                                           _MESSAGE(29008, _("slave "))
#define MSG_COREDUMPED                                      _MESSAGE(29009, _("(core dumped) "))
#define MSG_WAITPIDNOSIGNOEXIT_PI                           _MESSAGE(29010, _("waitpid() returned for pid " pid_t_fmt " status %d unequal WIFSIGNALED/WIFEXITED"))
#define MSG_SHEPHERD_VSHEPHERDOFJOBWXDIEDTHROUGHSIGNALYZ_SUUSI    _MESSAGE(29011, _(SFN "shepherd of job " sge_u32 "." sge_u32 " died through signal " SFN "= %d"))
#define MSG_SHEPHERD_WSHEPHERDOFJOBXYEXITEDWITHSTATUSZ_SUUI       _MESSAGE(29012, _(SFN "shepherd of job " sge_u32 "." sge_u32 " exited with exit status = %d"))
#define MSG_JOB_MISSINGJOBXYINJOBREPORTFOREXITINGJOBADDINGIT_UU   _MESSAGE(29013, _("missing job " sge_u32 "." sge_u32 " in job report for exiting job - adding it"))
#define MSG_STATUS_LOADSENSORDIEDWITHSIGNALXY_SI            _MESSAGE(29014, _("load sensor died through signal " SFN "= %d"))
#define MSG_STATUS_LOADSENSOREXITEDWITHEXITSTATUS_I         _MESSAGE(29015, _("load sensor exited with exit status = %d"))
#define MSG_STATUS_MAILERDIEDTHROUGHSIGNALXY_SI             _MESSAGE(29016, _("mailer died through signal " SFN " = %d"))
#define MSG_STATUS_MAILEREXITEDWITHEXITSTATUS_I             _MESSAGE(29017, _("mailer exited with exit status = %d"))
#define MSG_JOB_REAPINGJOBXPTFCOMPLAINSY_US                 _MESSAGE(29018, _("reaping job \"" sge_u32 "\" ptf complains: " SFN))
#define MSG_JOB_CLEANUPJOBCALLEDWITHINVALIDPARAMETERS       _MESSAGE(29019, _("clean_up_job() called with invalid parameters"))
#define MSG_JOB_CANTFINDDIRXFORREAPINGJOBYZ_SS              _MESSAGE(29020, _("can't find directory " SFN " for reaping job " SFN))
#define MSG_JOB_CANTREADCONFIGFILEFORJOBXY_S                _MESSAGE(29021, _("can't read config file for job " SFN))
#define MSG_STATUS_ABNORMALTERMINATIONOFSHEPHERDFORJOBXY_S  _MESSAGE(29022, _("abnormal termination of shepherd for job " SFN ": no \"exit_status\" file"))
#define MSG_STATUS_ABNORMALTERMINATIONFOSHEPHERDFORJOBXYEXITSTATEFILEISEMPTY_S    _MESSAGE(29023, _("abnormal termination of shepherd for job " SFN ": \"exit_status\" file is empty"))
#define MSG_SHEPHERD_DIEDTHROUGHSIGNAL                      _MESSAGE(29024, _("shepherd died through signal"))
#define MSG_SHEPHERD_NOPIDFILE                              _MESSAGE(29025, _("no \"pid\" file for shepherd"))
#define MSG_SHEPHERD_EXITEDWISSTATUS_IS                     _MESSAGE(29026, _("shepherd exited with exit status %d: %s"))
#define MSG_JOB_CANTREADERRORFILEFORJOBXY_S                 _MESSAGE(29027, _("can't read error file for job " SFN))
#define MSG_JOB_CANTREADUSAGEFILEFORJOBXY_S                 _MESSAGE(29028, _("can't read usage file for job " SFN))
#define MSG_JOB_WXDIEDTHROUGHSIGNALYZ_SSI                   _MESSAGE(29029, _("job " SFN " died through signal " SFN " (%d)"))
#define MSG_JOB_CANTREADUSEDRESOURCESFORJOB                 _MESSAGE(29030, _("can't read used resources for job"))
#define MSG_JOB_CANTOPENJOBPIDFILEFORJOBXY_S                _MESSAGE(29031, _("can't open \"job_pid\" file for job " SFN))
#define MSG_SHEPHERD_REMOVEACKEDJOBEXITCALLEDWITHX_U        _MESSAGE(29032, _("remove_acked_job_exit called with " sge_u32 ".0"))
#define MSG_JOB_XYHASNOTASKZ_UUS                            _MESSAGE(29033, _("job " sge_u32 "." sge_u32 " has no task " SFQ))

#define MSG_FILE_CANTREMOVEDIRECTORY_SS                     _MESSAGE(29037, _("can't remove directory " SFQ ": " SFN))
#define MSG_SHEPHERD_ACKNOWLEDGEFORUNKNOWNJOBXYZ_UUS        _MESSAGE(29038, _("acknowledge for unknown job " sge_u32 "." sge_u32 "/" SFN))
#define MSG_SHEPHERD_NOMOREOLDJOBSAFTERSTARTUP               _MESSAGE(29039, _("no more old jobs after startup"))
#define MSG_SHEPHERD_CANTFINDACTIVEJOBSDIRXFORREAPINGJOBY_SU  _MESSAGE(29040, _("can't find active jobs directory " SFQ " for reaping job " sge_u32 ))
#define MSG_SHEPHERD_INCORRECTCONFIGFILEFORJOBXY_UU         _MESSAGE(29041, _("incorrect config file for job " sge_u32 "." sge_u32 ""))
#define MSG_SHEPHERD_CANTSTARTJOBXY_US                      _MESSAGE(29042, _("can't start job \"" sge_u32 "\": " SFN))
#define MSG_SHEPHERD_PROBLEMSAFTERSTART_DS                  _MESSAGE(29043, _("problems after job start \"" sge_u32 "\": " SFN))
#define MSG_SHEPHERD_JATASKXYISKNOWNREPORTINGITTOQMASTER    _MESSAGE(29044, _("ja-task \"" sge_u32 "." sge_u32 "\" is unknown - reporting it to qmaster"))
#define MSG_SHEPHERD_CKECKINGFOROLDJOBS                     _MESSAGE(29045, _("checking for old jobs"))
#define MSG_SHEPHERD_NOOLDJOBSATSTARTUP                     _MESSAGE(29046, _("no old jobs at startup"))
#define MSG_SHEPHERD_CANTGETPROCESSESFROMPSCOMMAND          _MESSAGE(29047, _("can't get processes from ps command"))
#define MSG_SHEPHERD_XISNOTAJOBDIRECTORY_S                  _MESSAGE(29048, _(SFQ " is not a job directory"))
#define MSG_SHEPHERD_FOUNDACTIVEJOBDIRXWHILEMISSINGJOBDIRREMOVING_S    _MESSAGE(29049, _("found active job directory " SFQ " while missing job directory - removing"))
#define MSG_SHEPHERD_CANTSTATXY_SS                          _MESSAGE(29050, _("can't stat " SFQ ": " SFN))
#define MSG_FILE_XISNOTADIRECTORY_S                         _MESSAGE(29051, _(SFQ " is not a directory"))
#define MSG_SHEPHERD_FOUNDDIROFJOBX_S                       _MESSAGE(29052, _("found directory of job " SFQ))
#define MSG_SHEPHERD_CANTREADPIDFILEXFORJOBYSTARTTIMEZX_SSSS _MESSAGE(29053, _("can't read pid file " SFQ " of shepherd for job " SFQ " - starttime: " SFN " cleaning up: " SFN))
#define MSG_SHEPHERD_MISSINGJOBXINJOBREPORTFOREXITINGJOB_U  _MESSAGE(29054, _("missing job \"" sge_u32 "\" in job report for exiting job"))
#define MSG_SHEPHERD_CANTREADPIDFROMPIDFILEXFORJOBY_SS      _MESSAGE(29055, _("can't read pid from pid file " SFQ " of shepherd for job " SFN))
#define MSG_SHEPHERD_SHEPHERDFORJOBXHASPIDYANDISZALIVE_SP   _MESSAGE(29056, _("shepherd for job " SFN " has pid \"" pid_t_fmt "\" and is alive"))
#define MSG_SHEPHERD_SHEPHERDFORJOBXHASPIDYANDISNOTALIVE_SP _MESSAGE(29057, _("shepherd for job " SFN " has pid \"" pid_t_fmt "\" and is not alive"))

#define MSG_SHEPHERD_INCONSISTENTDATAFORJOBX_U              _MESSAGE(29058, _("inconsistent data for job \"" sge_u32 "\""))
#define MSG_SHEPHERD_MISSINGJOBXYINJOBREPORT_UU             _MESSAGE(29059, _("Missing job " sge_u32 "." sge_u32 " in job report"))
#define MSG_SHEPHERD_CANTOPENPIDFILEXFORJOBYZ_SUU           _MESSAGE(29060, _("can't open pid file " SFQ " for job " sge_u32 "." sge_u32))
#define MSG_SHEPHERD_CANTOPENUSAGEFILEXFORJOBYZX_SUUS       _MESSAGE(29061, _("can't open usage file " SFQ " for job " sge_u32 "." sge_u32 ": " SFN))
#define MSG_SHEPHERD_EXECDWENTDOWNDURINGJOBSTART            _MESSAGE(29062, _("execd went down during job start"))
#define MSG_EXECD_ERRORREADINGPIDOFJOB_UU                   _MESSAGE(29063, _("error reading pid for job " sge_u32 "." sge_u32))
#define MSG_SHEPHERD_CKECKINGFOROLDJOBSAFTER                _MESSAGE(29064, _("checking for old jobs after configuration change"))
#define MSG_JR_ERRSTR_EXECDDONTKNOWJOB                      _MESSAGE(29068, _("execd doesn't know this job"))
#define MSG_EXECD_GOTACKFORPETASKBUTISNOTINSTATEEXITING_S   _MESSAGE(29069, _("get exit ack for pe task " SFN " but task is not in state exiting"))

#define MSG_PRIO_JOBXPIDYSETPRIORITYFAILURE_UUS             _MESSAGE(29072, _("job " sge_u32 " pid " sge_u32 " setpriority failure: " SFN))

#define MSG_WHERE_FAILEDTOBUILDWHERECONDITION               _MESSAGE(29077, _("failed to build where-condition"))
#define MSG_PRIO_PTFMINMAX_II                               _MESSAGE(29078, _("PTF_MAX_PRIORITY=%d, PTF_MIN_PRIORITY=%d"))

#define MSG_PRIO_SETPRIOFAILED_S                            _MESSAGE(29080, _("setpriority failed: " SFN))
#define MSG_ERROR_UNKNOWNERRORCODE                          _MESSAGE(29081, _("Unknown error code"))
#define MSG_ERROR_NOERROROCCURED                            _MESSAGE(29082, _("No error occurred"))
#define MSG_ERROR_INVALIDARGUMENT                           _MESSAGE(29083, _("Invalid argument"))
#define MSG_ERROR_JOBDOESNOTEXIST                           _MESSAGE(29084, _("Job does not exist"))

#define MSG_ERROR_UNABLETODUMPJOBUSAGELIST   _MESSAGE(29087, _("Unable to dump job usage list"))
#define MSG_LOAD_NOMEMINDICES          _MESSAGE(29088, _("failed retrieving memory indices"))
#define MSG_LOAD_NOPTFUSAGE_S          _MESSAGE(29089, _("ptf failed to determine job usage: " SFN))
#define MSG_SGETEXT_NO_LOAD            _MESSAGE(29090, _("can't get load values"))
#define MSG_JOB_TYPEMALLOC             _MESSAGE(29091, _("runtime type error or malloc failure in add_job_report"))
#define MSG_PARSE_USAGEATTR_SSU        _MESSAGE(29092, _("failed parsing " SFQ " passed as usage attribute " SFQ " of job " sge_u32))
#define MSG_EXECD_INVALIDUSERNAME_S    _MESSAGE(29093, _("invalid user name " SFQ))
#define MSG_EXECD_NOHOMEDIR_S          _MESSAGE(29094, _("missing home directory for user " SFQ))

#define MSG_JOB_TICKETFORMAT           _MESSAGE(29096, _("format error in ticket request"))
#define MSG_JOB_TICKETPASS2PTF_IS      _MESSAGE(29097, _("passing %d new tickets ptf complains: " SFN))
#define MSG_FILE_RMDIR_SS              _MESSAGE(29098, _("can't remove directory " SFQ ": " SFN))

#define MSG_FILE_CREATEDIR_SS          _MESSAGE(29100, _("can't create directory " SFN ": " SFN))
#define MSG_EXECD_NOSGID               _MESSAGE(29101, _("supplementary group ids could not be found in /proc"))
#define MSG_EXECD_NOPARSEGIDRANGE      _MESSAGE(29102, _("can not parse gid_range"))
#define MSG_EXECD_NOADDGID             _MESSAGE(29103, _("can not find an unused add_grp_id"))
#define MSG_MAIL_MAILLISTTOOLONG_U     _MESSAGE(29104, _("maillist for job " sge_u32 " too long"))
#define MSG_EXECD_NOXTERM              _MESSAGE(29105, _("unable to find xterm executable for interactive job, not configured"))

#define MSG_EXECD_NOSHEPHERD_SSS       _MESSAGE(29108, _("unable to find shepherd executable neither in architecture directory " SFN " nor in " SFN ": " SFN))
#define MSG_EXECD_NOSHEPHERDWRAP_SS    _MESSAGE(29109, _("unable to find shepherd wrapper command " SFN ": " SFN))
#define MSG_DCE_NOSHEPHERDWRAP_SS      _MESSAGE(29110, _("unable to find DCE shepherd wrapper command " SFN ": " SFN))
#define MSG_EXECD_NOCOSHEPHERD_SSS     _MESSAGE(29111, _("unable to find coshepherd executable neither in architecture directory " SFN " nor in " SFN ": " SFN))
#define MSG_EXECD_AFSCONFINCOMPLETE    _MESSAGE(29112, _("incomplete AFS configuration - set_token_cmd and token_extend_time must be configured"))
#define MSG_EXECD_NOCREATETOKENFILE_S  _MESSAGE(29113, _("can't create token file: " SFN))
#define MSG_EXECD_TOKENZERO            _MESSAGE(29114, _("AFS token does not exist or has zero length"))
#define MSG_EXECD_NOWRITETOKEN_S       _MESSAGE(29115, _("can't write token to token file: " SFN))
#define MSG_MAIL_STARTBODY_UUSSSSS     _MESSAGE(29116, _("Job-array task " sge_u32 "." sge_u32 " (" SFN ") Started\n User       = " SFN "\n Queue      = " SFN "\n Host       = " SFN "\n Start Time = " SFN))
#define MSG_MAIL_STARTBODY_USSSSS      _MESSAGE(29117, _("Job " sge_u32 " (" SFN ") Started\n User       = " SFN "\n Queue      = " SFN "\n Host       = " SFN "\n Start Time = " SFN))
#define MSG_EXECD_NOFORK_S             _MESSAGE(29119, _("fork failed: " SFN))
#define MSG_EXECD_NOSTARTSHEPHERD      _MESSAGE(29120, _("unable to start shepherd process"))

#define MSG_SYSTEM_CANTMAKETMPDIR      _MESSAGE(29122, _("can't make tmpdir"))
#define MSG_SYSTEM_CANTGETTMPDIR       _MESSAGE(29123, _("can't get tmpdir"))
#define MSG_SYSTEM_CANTOPENTMPDIR_S    _MESSAGE(29124, _("can't open tmpdir " SFN))
#define MSG_EXECD_UNABLETOFINDSCRIPTFILE_SS  _MESSAGE(29125, _("unable to find script file " SFN ": " SFN))
#define MSG_JOB_EXCEEDHLIM_USSFF       _MESSAGE(29126, _("job " sge_u32 " exceeds job hard limit " SFQ " of queue " SFQ " (%8.5f > limit:%8.5f) - sending SIGKILL"))
#define MSG_JOB_EXCEEDSLIM_USSFF       _MESSAGE(29127, _("job " sge_u32 " exceeds job soft limit " SFQ " of queue " SFQ " (%8.5f > limit:%8.5f) - sending SIGXCPU"))
#define MSG_EXECD_EXCEEDHWALLCLOCK_UU  _MESSAGE(29128, _("job " sge_u32 "." sge_u32 " exceeded hard wallclock time - initiate terminate method"))
#define MSG_EXECD_EXCEEDSWALLCLOCK_UU  _MESSAGE(29129, _("job " sge_u32 "." sge_u32 " exceeded soft wallclock time - initiate soft notify method"))
#define MSG_EXECD_NOADDGIDOPEN_SSS     _MESSAGE(29130, _("failed opening addgrpid file " SFN " of job " SFN ": " SFN))
#define MSG_JOB_NOREGISTERPTF_SS       _MESSAGE(29131, _("failed registering job " SFN " at ptf: " SFN))
#define MSG_EXECD_NOOSJOBIDOPEN_SSS    _MESSAGE(29132, _("failed opening os jobid file " SFN " of job " SFN ": " SFN))

#define MSG_COM_UNPACKFEATURESET       _MESSAGE(29134, _("unpacking featureset from job execution message"))
#define MSG_COM_UNPACKJOB              _MESSAGE(29135, _("unpacking job from job execution message"))

#define MSG_JOB_MISSINGQINGDIL_SU      _MESSAGE(29138, _("missing queue " SFQ " found in gdil of job " sge_u32))
#define MSG_EXECD_NOWRITESCRIPT_SIUS   _MESSAGE(29139, _("can't write script file " SFQ " wrote only %d of " sge_u32 " bytes: " SFN))
#define MSG_JOB_TASKWITHOUTJOB_U       _MESSAGE(29140, _("received task belongs to job " sge_u32 " but this job is not here"))
#define MSG_JOB_TASKNOTASKINJOB_UU     _MESSAGE(29141, _("received task belongs to job " sge_u32 " but this job is here but the JobArray task " sge_u32 " is not here"))
#define MSG_JOB_TASKNOSUITABLEJOB_U    _MESSAGE(29142, _("received task belongs to job " sge_u32 " but this job is not suited for starting tasks"))

#define MSG_JOB_NOFREEQ_USSS           _MESSAGE(29145, _("no free queue for job " sge_u32 " of user " SFN "@" SFN " (localhost = " SFN ")"))
#define MSG_JOB_INVALIDJATASK_REQUEST  _MESSAGE(29146, _("invalid task list in job start request"))
#define MSG_JOB_SAMEPATHSFORINPUTANDOUTPUT_SSS   _MESSAGE(29147, _("same paths given for stdin (" SFQ ") and " SFN " (" SFQ ")"))
#define MSG_DENIED_PETASKREQUEST_WRONG_USER_SS   _MESSAGE(29148, _("denied request of user " SFQ " to start a pe task in job of user " SFQ))
#define MSG_JOB_INITCKPTSHUTDOWN_U     _MESSAGE(29149, _("initiate checkpoint at shutdown: job " sge_u32 ""))
#define MSG_JOB_KILLSHUTDOWN_U         _MESSAGE(29150, _("killing job at shutdown: job " sge_u32 ""))
#define MSG_JOB_INITMIGRSUSPQ_U        _MESSAGE(29151, _("initiate migration at queue suspend for job " sge_u32))
#define MSG_JOB_SIGNALTASK_UUS         _MESSAGE(29152, _("SIGNAL jid: " sge_u32 " jatask: " sge_u32 " signal: " SFN))
#define MSG_EXECD_WRITESIGNALFILE_S    _MESSAGE(29153, _("error writing file " SFN " for signal transfer to shepherd"))
#define MSG_JOB_DELIVERSIGNAL_ISSIS    _MESSAGE(29154, _("failed to deliver signal %d to job " SFN " for " SFN " (shepherd with pid %d): " SFN))
#define MSG_JOB_INITMIGRSUSPJ_UU       _MESSAGE(29155, _("initiate migration at job suspend for job " sge_u32 " task " sge_u32 ""))

#define MSG_LS_STOPLS_S                _MESSAGE(29161, _("stopping load sensor " SFN))
#define MSG_LS_STARTLS_S               _MESSAGE(29162, _("starting load sensor " SFN))
#define MSG_LS_RESTARTLS_S             _MESSAGE(29163, _("restarting load sensor " SFN))
#define MSG_LS_NOMODTIME_SS            _MESSAGE(29165, _("can't get mod_time from load sensor file " SFN ": " SFN))
#define MSG_LS_FORMAT_ERROR_SS         _MESSAGE(29166, _("Format error of loadsensor " SFQ ". Received: \"%100s\""))
#define MSG_LS_USE_EXTERNAL_LS_S       _MESSAGE(29167, _("execd cannot retrieve load values on platform " SFQ " - please configure an external load sensor"))
#define MSG_FILE_REDIRECTFD_I          _MESSAGE(29168, _("can't redirect file descriptor #%d"))
#define MSG_EXECD_NOSTARTPTF           _MESSAGE(29172, _("could not start priority translation facility (ptf)"))
#define MSG_EXECD_STARTPDCANDPTF       _MESSAGE(29173, _("successfully started PDC and PTF"))
#define MSG_COM_CANTREGISTER_SS        _MESSAGE(29175, _("can't register at qmaster " SFQ ": " SFN))
#define MSG_COM_ERROR                  _MESSAGE(29176, _("abort qmaster registration due to communication errors"))
#define MSG_PARSE_INVALIDARG_S         _MESSAGE(29178, _("invalid command line argument " SFQ))
#define MSG_PARSE_TOOMANYARGS          _MESSAGE(29179, _("too many command line options"))

#define MSG_EXECD_CANT_GET_CONFIGURATION_EXIT      _MESSAGE(29186, _("can't get configuration qmaster - terminating"))
#define MSG_EXECD_REGISTERED_AT_QMASTER_S          _MESSAGE(29187, _("registered at qmaster host " SFQ))
#define MSG_EXECD_INVALIDJOBREQUEST_SS             _MESSAGE(29188, _("invalid job start order from commproc " SFQ " on host" SFQ))
#define MSG_EXECD_INVALIDTASKREQUEST_SS            _MESSAGE(29189, _("invalid pe task start order from commproc " SFQ " on host" SFQ))
#define MSG_EXECD_ENABLEDELEAYDJOBREPORTING        _MESSAGE(29190,    _("Reconnected to qmaster - enabled delayed job reporting period"))
#define MSG_EXECD_DISABLEDELEAYDJOBREPORTING       _MESSAGE(29191,    _("Delayed job reporting period finished"))
#define MSG_EXECD_ENABLEDELEAYDREPORTINGFORJOB_U   _MESSAGE(29192, _("Enable delayed job reporting for job " sge_u32 ""))

#define MSG_SGE_USAGE                        _MESSAGE(29193, _("usage: pdc [-snpgj] [-kK signo] [-iJPS secs] job_id [ ... ]"))
#define MSG_SGE_s_OPT_USAGE                  _MESSAGE(29194, _("show system data"  ))
#define MSG_SGE_n_OPT_USAGE                  _MESSAGE(29195, _("no output"  ))
#define MSG_SGE_p_OPT_USAGE                  _MESSAGE(29196, _("show process information"  ))
#define MSG_SGE_i_OPT_USAGE                  _MESSAGE(29197, _("interval in seconds (default is 2)"  ))
#define MSG_SGE_g_OPT_USAGE                  _MESSAGE(29198, _("produce output for gr_osview(1)"  ))
#define MSG_SGE_j_OPT_USAGE                  _MESSAGE(29199, _("provide job name for gr_osview display (used only with -g)"  ))
#define MSG_SGE_J_OPT_USAGE                  _MESSAGE(29200, _("job data collection interval in seconds (default is 0)"    ))
#define MSG_SGE_k_OPT_USAGE                  _MESSAGE(29201, _("kill job using signal signo"    ))
#define MSG_SGE_K_OPT_USAGE                  _MESSAGE(29202, _("kill job using signal signo (loop until all processes are dead)"    ))
#define MSG_SGE_P_OPT_USAGE                  _MESSAGE(29203, _("process data collection interval in seconds (default is 0)"    ))
#define MSG_SGE_S_OPT_USAGE                  _MESSAGE(29204, _("system data collection interval in seconds (default is 15)"    ))
#define MSG_SGE_JOBDATA                      _MESSAGE(29205, _("******** Job Data **********"))
#define MSG_SGE_SYSTEMDATA                   _MESSAGE(29206, _("******** System Data ***********"))
#define MSG_SGE_STATUS                       _MESSAGE(29207, _("********** Status **************"))
#define MSG_SGE_CPUUSAGE                     _MESSAGE(29208, _("CPU Usage:"))
#define MSG_SGE_SGEJOBUSAGECOMPARSION        _MESSAGE(29209, _("Job Usage Comparison"))
#define MSG_SGE_XISNOTAVALIDSIGNALNUMBER_S   _MESSAGE(29210, _(SFN " is not a valid signal number"))
#define MSG_SGE_XISNOTAVALIDINTERVAL_S       _MESSAGE(29211, _(SFN " is not a valid interval"))
#define MSG_SGE_XISNOTAVALIDJOBID_S          _MESSAGE(29212, _(SFN " is not a valid job_id"))
#define MSG_SGE_GROSVIEWEXPORTFILE           _MESSAGE(29213, _("gr_osview export file"))
#define MSG_SGE_PERMISSIONDENIED             _MESSAGE(29214, _("permission denied"))
#define MSG_SGE_NOJOBS                       _MESSAGE(29215, _("No jobs"))

#define MSG_SGE_NGROUPS_MAXOSRECONFIGURATIONNECESSARY    _MESSAGE(29216, _("NGROUPS_MAX <= 0: os reconfiguration necessary"))
#define MSG_SGE_PROCFSKILLADDGRPIDMALLOCFAILED           _MESSAGE(29217, _("procfs_kill_addgrpid(): malloc failed" ))
#define MSG_SGE_KILLINGPIDXY_PI                          _MESSAGE(29218, _("killing pid " pid_t_fmt "/%d" ))
#define MSG_SGE_DONOTKILLROOTPROCESSXY_PI                _MESSAGE(29219, _("do not kill root process " pid_t_fmt "/%d"   ))
#define MSG_SGE_PTDISPATCHPROCTOJOBMALLOCFAILED          _MESSAGE(29220, _("pt_dispatch_proc_to_job: malloc failed" ))

#define MSG_SYSTEMD_INITIALIZED_SSSS                     _MESSAGE(29230, _("systemd integration initialized, connected to dbus: " SFN ", running as " SFN ": " SFN))
#define MSG_EXECD_SYSTEMD_MOVE_SHEPHERD_TO_SCOPE_S       _MESSAGE(29231, _("systemd: moving shepherd to scope failed: " SFN))

// clang-format on

#endif /* __MSG_EXECD_H */

