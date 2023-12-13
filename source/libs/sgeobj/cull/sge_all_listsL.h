#ifndef __SGE_ALL_LISTSL_H
#define __SGE_ALL_LISTSL_H

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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "basis_types.h"
#include "cull/cull.h"

/* Definition of new names */

#include "sgeobj/cull/sge_ack_ACK_L.h"
#include "sgeobj/cull/sge_advance_reservation_AR_L.h"
#include "sgeobj/cull/sge_advance_reservation_ARA_L.h"
#include "sgeobj/cull/sge_answer_AN_L.h"
#include "sgeobj/cull/sge_attr_ASTR_L.h"
#include "sgeobj/cull/sge_attr_AULNG_L.h"
#include "sgeobj/cull/sge_attr_ABOOL_L.h"
#include "sgeobj/cull/sge_attr_ATIME_L.h"
#include "sgeobj/cull/sge_attr_AMEM_L.h"
#include "sgeobj/cull/sge_attr_AINTER_L.h"
#include "sgeobj/cull/sge_attr_ASTRING_L.h"
#include "sgeobj/cull/sge_attr_ASTRLIST_L.h"
#include "sgeobj/cull/sge_attr_AUSRLIST_L.h"
#include "sgeobj/cull/sge_attr_APRJLIST_L.h"
#include "sgeobj/cull/sge_attr_ACELIST_L.h"
#include "sgeobj/cull/sge_attr_ASOLIST_L.h"
#include "sgeobj/cull/sge_attr_AQTLIST_L.h"
#include "sgeobj/cull/sge_binding_BN_L.h"
#include "sgeobj/cull/sge_calendar_CAL_L.h"
#include "sgeobj/cull/sge_calendar_CA_L.h"
#include "sgeobj/cull/sge_calendar_CQU_L.h"
#include "sgeobj/cull/sge_calendar_TMR_L.h"
#include "sgeobj/cull/sge_calendar_TM_L.h"
#include "sgeobj/cull/sge_centry_CE_L.h"
#include "sgeobj/cull/sge_ckpt_CK_L.h"
#include "sgeobj/cull/sge_conf_CONF_L.h"
#include "sgeobj/cull/sge_conf_CF_L.h"
#include "sgeobj/cull/sge_cqueue_CQ_L.h"
#include "sgeobj/cull/sge_ct_SCT_L.h"
#include "sgeobj/cull/sge_ct_REF_L.h"
#include "sgeobj/cull/sge_ct_CT_L.h"
#include "sgeobj/cull/sge_ct_CCT_L.h"
#include "sgeobj/cull/sge_ct_CTI_L.h"
#include "sgeobj/cull/sge_cull_xml_XMLA_L.h"
#include "sgeobj/cull/sge_cull_xml_XMLS_L.h"
#include "sgeobj/cull/sge_cull_xml_XMLH_L.h"
#include "sgeobj/cull/sge_cull_xml_XMLE_L.h"
#include "sgeobj/cull/sge_eejob_FCAT_L.h"
#include "sgeobj/cull/sge_eejob_SGEJ_L.h"
#include "sgeobj/cull/sge_event_EV_L.h"
#include "sgeobj/cull/sge_event_EVS_L.h"
#include "sgeobj/cull/sge_event_ET_L.h"
#include "sgeobj/cull/sge_event_request_EVR_L.h"
#include "sgeobj/cull/sge_feature_FES_L.h"
#include "sgeobj/cull/sge_helper_QAJ_L.h"
#include "sgeobj/cull/sge_hgroup_HGRP_L.h"
#include "sgeobj/cull/sge_host_EH_L.h"
#include "sgeobj/cull/sge_host_RU_L.h"
#include "sgeobj/cull/sge_host_AH_L.h"
#include "sgeobj/cull/sge_host_SH_L.h"
#include "sgeobj/cull/sge_host_HL_L.h"
#include "sgeobj/cull/sge_host_HS_L.h"
#include "sgeobj/cull/sge_href_HR_L.h"
#include "sgeobj/cull/sge_id_ID_L.h"
#include "sgeobj/cull/sge_japi_JJ_L.h"
#include "sgeobj/cull/sge_japi_JJAT_L.h"
#include "sgeobj/cull/sge_japi_NSV_L.h"
#include "sgeobj/cull/sge_ja_task_JAT_L.h"
#include "sgeobj/cull/sge_job_JB_L.h"
#include "sgeobj/cull/sge_job_JG_L.h"
#include "sgeobj/cull/sge_job_PN_L.h"
#include "sgeobj/cull/sge_job_ref_JRE_L.h"
#include "sgeobj/cull/sge_jsv_JSV_L.h"
#include "sgeobj/cull/sge_krb_KRB_L.h"
#include "sgeobj/cull/sge_krb_KTGT_L.h"
#include "sgeobj/cull/sge_loadsensor_LS_L.h"
#include "sgeobj/cull/sge_mailrec_MR_L.h"
#include "sgeobj/cull/sge_manop_UM_L.h"
#include "sgeobj/cull/sge_manop_UO_L.h"
#include "sgeobj/cull/sge_mesobj_QIM_L.h"
#include "sgeobj/cull/sge_message_SME_L.h"
#include "sgeobj/cull/sge_message_MES_L.h"
#include "sgeobj/cull/sge_multi_MA_L.h"
#include "sgeobj/cull/sge_order_OR_L.h"
#include "sgeobj/cull/sge_order_OQ_L.h"
#include "sgeobj/cull/sge_order_RTIC_L.h"
#include "sgeobj/cull/sge_pack_PACK_L.h"
#include "sgeobj/cull/sge_parse_SPA_L.h"
#include "sgeobj/cull/sge_path_alias_PA_L.h"
#include "sgeobj/cull/sge_pe_PE_L.h"
#include "sgeobj/cull/sge_pe_task_PET_L.h"
#include "sgeobj/cull/sge_pe_task_PETR_L.h"
#include "sgeobj/cull/sge_pe_task_FPET_L.h"
#include "sgeobj/cull/sge_permission_PERM_L.h"
#include "sgeobj/cull/sge_ptf_JL_L.h"
#include "sgeobj/cull/sge_ptf_JO_L.h"
#include "sgeobj/cull/sge_ptf_JP_L.h"
#include "sgeobj/cull/sge_qeti_QETI_L.h"
#include "sgeobj/cull/sge_qexec_RT_L.h"
#include "sgeobj/cull/sge_qinstance_QU_L.h"
#include "sgeobj/cull/sge_qref_QR_L.h"
#include "sgeobj/cull/sge_range_RN_L.h"
#include "sgeobj/cull/sge_report_REP_L.h"
#include "sgeobj/cull/sge_report_JR_L.h"
#include "sgeobj/cull/sge_report_LIC_L.h"
#include "sgeobj/cull/sge_report_LR_L.h"
#include "sgeobj/cull/sge_resource_quota_RQS_L.h"
#include "sgeobj/cull/sge_resource_quota_RQR_L.h"
#include "sgeobj/cull/sge_resource_quota_RQRF_L.h"
#include "sgeobj/cull/sge_resource_quota_RQRL_L.h"
#include "sgeobj/cull/sge_resource_quota_RQL_L.h"
#include "sgeobj/cull/sge_resource_utilization_RDE_L.h"
#include "sgeobj/cull/sge_resource_utilization_RUE_L.h"
#include "sgeobj/cull/sge_schedd_conf_PARA_L.h"
#include "sgeobj/cull/sge_schedd_conf_SC_L.h"
#include "sgeobj/cull/sge_select_queue_LDR_L.h"
#include "sgeobj/cull/sge_select_queue_QRL_L.h"
#include "sgeobj/cull/sge_sharetree_STN_L.h"
#include "sgeobj/cull/sge_spooling_SPC_L.h"
#include "sgeobj/cull/sge_spooling_SPR_L.h"
#include "sgeobj/cull/sge_spooling_SPT_L.h"
#include "sgeobj/cull/sge_spooling_SPTR_L.h"
#include "sgeobj/cull/sge_str_ST_L.h"
#include "sgeobj/cull/sge_str_STU_L.h"
#include "sgeobj/cull/sge_subordinate_SO_L.h"
#include "sgeobj/cull/sge_suser_SU_L.h"
#include "sgeobj/cull/sge_time_event_TE_L.h"
#include "sgeobj/cull/sge_ulong_ULNG_L.h"
#include "sgeobj/cull/sge_usage_UA_L.h"
#include "sgeobj/cull/sge_userprj_PR_L.h"
#include "sgeobj/cull/sge_userprj_UU_L.h"
#include "sgeobj/cull/sge_userprj_UPU_L.h"
#include "sgeobj/cull/sge_userprj_UPP_L.h"
#include "sgeobj/cull/sge_userset_US_L.h"
#include "sgeobj/cull/sge_userset_UE_L.h"
#include "sgeobj/cull/sge_userset_JC_L.h"
#include "sgeobj/cull/sge_var_VA_L.h"
#include "sgeobj/cull/sge_proc_PRO_L.h"

#ifdef  __cplusplus
extern "C" {
#endif

#ifdef  __cplusplus
}
#endif
#if defined(__SGE_GDI_LIBRARY_HOME_OBJECT_FILE__)
#ifdef  __cplusplus
extern "C" {
#endif

lNameSpace nmv[] = {

/*  
   1. unique keq of the first element in the descriptor 
   2. number of elements in the descriptor 
   3. array with names describing the fields of the descriptor
   4. pointer to the descriptor
   1.              2.   3.   4.
*/
   {JB_LOWERBOUND, JBS, JBN, JB_Type},        /* job */
   {QU_LOWERBOUND, QUS, QUN, QU_Type},        /* Queue Instance list */
   {EH_LOWERBOUND, EHS, EHN, EH_Type},        /* exec host */
   {AH_LOWERBOUND, AHS, AHN, AH_Type},        /* admin host */
   {SH_LOWERBOUND, SHS, SHN, SH_Type},        /* submit host */
   {AN_LOWERBOUND, ANS, ANN, AN_Type},        /* gdi acknowledge format */
   {HL_LOWERBOUND, HLS, HLN, HL_Type},        /* load of an exec host */
   {HS_LOWERBOUND, HSS, HSN, HS_Type},        /* scaling of and exec host */
   {ET_LOWERBOUND, ETS, ETN, ET_Type},        /* event */
   {EV_LOWERBOUND, EVS, EVN, EV_Type},        /* event client */
   {EVS_LOWERBOUND, EVSS, EVSN, EVS_Type},    /* subscribed event list */
   {CE_LOWERBOUND, CES, CEN, CE_Type},        /* complex entity */
   {LR_LOWERBOUND, LRS, LRN, LR_Type},        /* load report */
   {OR_LOWERBOUND, ORS, ORN, OR_Type},        /* ?? */
   {OQ_LOWERBOUND, OQS, OQN, OQ_Type},        /* ?? */
   {US_LOWERBOUND, USES, USEN, US_Type},      /* user set */
   {UE_LOWERBOUND, UES, UEN, UE_Type},        /* user set entry */
   {RN_LOWERBOUND, RNS, RNN, RN_Type},        /* range list */
   {PN_LOWERBOUND, PNS, PNN, PN_Type},        /* path name list */
   {VA_LOWERBOUND, VAS, VAN, VA_Type},        /* variable list */
   {MR_LOWERBOUND, MRS, MRN, MR_Type},        /* mail recipiants list */
   {UM_LOWERBOUND, UMS, UMN, UM_Type},        /* manager list */
   {UO_LOWERBOUND, UOS, UON, UO_Type},        /* operator list */
   {PE_LOWERBOUND, PES, PEN, PE_Type},        /* parallel environment object */
   {QR_LOWERBOUND, QRS, QRN, QR_Type},        /* queue reference used in PE object */
   {JC_LOWERBOUND, JCS, JCN, JC_Type},        /* job couter used in schedd */
   {CONF_LOWERBOUND, CONFS, CONFN, CONF_Type},  /* config */
   {CF_LOWERBOUND, CFS, CFN, CF_Type},        /* config list */
   {ST_LOWERBOUND, STS, STN, ST_Type},        /* string list */
   {STU_LOWERBOUND, STUS, STUN, STU_Type},     /* unique string list */
   {JG_LOWERBOUND, JGS, JGN, JG_Type},        /* jobs sublist of granted destinatin 
                                      * identifiers */
   {SO_LOWERBOUND, SOS, SON, SO_Type},        /* subordinate configuration list */
   {QAJ_LOWERBOUND, QAJS, QAJN, QAJ_Type},     /* list for qacct special purpose */
   {SPA_LOWERBOUND, SPAS, SPAN, SPA_Type},     /* option parse struct */
   {REP_LOWERBOUND, REPS, REPN, REP_Type},     /* report list */
   {UA_LOWERBOUND, UAS, UAN, UA_Type},        /* usage list */
   {PR_LOWERBOUND, PRS, PRN, PR_Type},        /* SGEEE - project */
   {UU_LOWERBOUND, UUS, UUN, UU_Type},        /* SGEEE - user */
   {STN_LOWERBOUND, STNS, STNN, STN_Type},     /* SGEEE - share tree node */
   {SC_LOWERBOUND, SCS, SCN, SC_Type},        /* scheduler config */
   {PET_LOWERBOUND, PETS, PETN, PET_Type},     /* PE Task object */
   {PETR_LOWERBOUND, PETRS, PETRN, PETR_Type},  /* PE Task request object */
   {FPET_LOWERBOUND, FPETS, FPETN, FPET_Type},  /* finished PE Task reference */
   {JR_LOWERBOUND, JRS, JRN, JR_Type},        /* Job report */
   {LIC_LOWERBOUND, LICS, LICN, LIC_Type},     /* structure of license report */

   {JL_LOWERBOUND, JLS, JLN, JL_Type},        /* ptf job list */
   {JP_LOWERBOUND, JPS, JPN, JP_Type},        /* ptf pid list */
   {JO_LOWERBOUND, JOS, JON, JO_Type},        /* ptf O.S. job list */

   {UPU_LOWERBOUND, UPUS, UPUN, UPU_Type},     /* SGEEE - sublist of user/project for
                                      * storing jobs old usage */
   {CK_LOWERBOUND, CKS, CKN, CK_Type},        /* checkpointing object */
   {KRB_LOWERBOUND, KRBS, KRBN, KRB_Type},     /* Kerberos connection list */
   {PA_LOWERBOUND, PAS, PAN, PA_Type},        /* Path alias list */
   {JRE_LOWERBOUND, JRES, JREN, JRE_Type},     /* job reference */
   {ID_LOWERBOUND, IDS, IDN, ID_Type},        /* id struct used for qmod requests */
   {MA_LOWERBOUND, MAS, MAN, MA_Type},        /* ma struct used for multi gdi
                                      * requests */

   {TE_LOWERBOUND, TES, TEN, TE_Type},        /* time event struct used for timer
                                      * in qmaster */
   {CAL_LOWERBOUND, CALS, CALN, CAL_Type},     /* calendar week/year */
   {CA_LOWERBOUND, CAS, CAN, CA_Type},        /* calendar week/year */
   {TMR_LOWERBOUND, TMRS, TMRN, TMR_Type},     /* time range */
   {TM_LOWERBOUND, TMS, TMN, TM_Type},        /* cullified struct tm */

   {RT_LOWERBOUND, RTS, RTN, RT_Type},        /* remote task (qrexec) */
   {UPP_LOWERBOUND, UPPS, UPPN, UPP_Type},     /* SGEEE - sublist of user/project for
                                      * storing project usage */
   {KTGT_LOWERBOUND, KTGTS, KTGTN, KTGT_Type},  /* Kerberos TGT list */
   {SME_LOWERBOUND, SMES, SMEN, SME_Type},     /* scheduler message structure */
   {MES_LOWERBOUND, MESS, MESN, MES_Type},     /* scheduler job info */
   {JAT_LOWERBOUND, JATS, JATN, JAT_Type},     /* JobArray task structure contains
                                      * the dynamic elements of a Job */
   {CT_LOWERBOUND, CTS, CTN, CT_Type},        /* scheduler job category */

   {SGEJ_LOWERBOUND, SGEJS, SGEJN, SGEJ_Type},     /* scheduler sge job sort element */
   {ULNG_LOWERBOUND, ULNGS, ULNGN, ULNG_Type},          /* ???? info-messages ??? */

   {HGRP_LOWERBOUND, HGRPS, HGRPN, HGRP_Type},  /* hostgroup list */
   {HR_LOWERBOUND, HRS, HRN, HR_Type},        /* host/group reference list */
   {PERM_LOWERBOUND, PERMS, PERMN, PERM_Type},  /* permission list */
   {LS_LOWERBOUND, LSS, LSN, LS_Type},        /* load sensor list */

   {RU_LOWERBOUND, RUS, RUN, RU_Type},        /* user unknown list */
   {FES_LOWERBOUND, FESS, FESN, FES_Type},
   
   {SU_LOWERBOUND, SUS, SUN, SU_Type},        /* submit user */

   {SPC_LOWERBOUND, SPCS, SPCN, SPC_Type},     /* Spooling context */
   {SPR_LOWERBOUND, SPRS, SPRN, SPR_Type},     /* Spooling rule */
   {SPT_LOWERBOUND, SPTS, SPTN, SPT_Type},     /* Spooling object type */
   {SPTR_LOWERBOUND, SPTRS, SPTRN, SPTR_Type},  /* Spooling rules for object type */

   {JJ_LOWERBOUND, JJS, JJN, JJ_Type},        /* JAPI job */
   {JJAT_LOWERBOUND, JJATS, JJATN, JJAT_Type},  /* JAPI array task */
   {NSV_LOWERBOUND, NSVS, NSVN, NSV_Type},  /* JAPI ??? */

   {ASTR_LOWERBOUND, ASTRS, ASTRN, ASTR_Type},          /* CQ string sublist */
   {AULNG_LOWERBOUND, AULNGS, AULNGN, AULNG_Type},       /* CQ u_long32 sublist */
   {ABOOL_LOWERBOUND, ABOOLS, ABOOLN, ABOOL_Type},       /* CQ bool sublist */
   {ATIME_LOWERBOUND, ATIMES, ATIMEN, ATIME_Type},       /* CQ time limit sublist */
   {AMEM_LOWERBOUND, AMEMS, AMEMN, AMEM_Type},          /* CQ memory limit sublist */
   {AINTER_LOWERBOUND, AINTERS, AINTERN, AINTER_Type},    /* CQ interval sublist */
   {ASTRING_LOWERBOUND, ASTRINGS, ASTRINGN, ASTRING_Type},
   {ASTRLIST_LOWERBOUND, ASTRLISTS, ASTRLISTN, ASTRLIST_Type}, /* CQ ST_Type-list sublist */
   {AUSRLIST_LOWERBOUND, AUSRLISTS, AUSRLISTN, AUSRLIST_Type}, /* CQ US_Type-list sublist */
   {APRJLIST_LOWERBOUND, APRJLISTS, APRJLISTN, APRJLIST_Type}, /* CQ PR_Type-list sublist */
   {ACELIST_LOWERBOUND, ACELISTS, ACELISTN, ACELIST_Type},    /* CQ CE_Type-list sublist */
   {ASOLIST_LOWERBOUND, ASOLISTS, ASOLISTN, ASOLIST_Type},    /* CQ SO_Type-list sublist */
   {AQTLIST_LOWERBOUND, AQTLISTS, AQTLISTN, AQTLIST_Type},    /* CQ qtype sublist */
   {CQ_LOWERBOUND, CQS, CQN, CQ_Type},                /* Cluster Queue list */
   {QIM_LOWERBOUND, QIMS, QIMN, QIM_Type},                /* Queue Instance Messege list */
   {FCAT_LOWERBOUND, FCATS, FCATN, FCAT_Type},          /* Functional category */
   {CTI_LOWERBOUND, CTIS, CTIN, CTI_Type},             /* ignore host/queue list in a job category */
   {PARA_LOWERBOUND, PARAS, PARAN, PARA_Type},          /* store the configuration "params" parameters in a list */

/* this would generate a cycle in the dependencies between lib cull and lib obj. Therefor
   we ignore the names here and live with the fact, that lWriteList or lWriteElem will
   not print the CULL_names for the PACK structure. */
/*      {PACK_LOWERBOUND, PACKS, PACKN, PACK_Type},   */       /* a cull version of the pack buffer */

   {XMLA_LOWERBOUND, XMLAS, XMLAN, XMLA_Type},          /* XML-Attribute */
   {XMLH_LOWERBOUND, XMLHS, XMLHN, XMLH_Type},          /* XML-Header*/
   {XMLS_LOWERBOUND, XMLSS, XMLSN, XMLS_Type},          /* XML-Stype-Sheet */
   {XMLE_LOWERBOUND, XMLES, XMLEN, XMLE_Type},          /* XML-Element*/

   {RDE_LOWERBOUND, RDES, RDEN, RDE_Type},             /* resource diagram */
   {RUE_LOWERBOUND, RUES, RUEN, RUE_Type},             /* resource utilization */
   {QETI_LOWERBOUND, QETIS, QETIN, QETI_Type},          /* queue end time iterator (scheduler) */

   {LDR_LOWERBOUND, LDRS, LDRN, LDR_Type},             /* queue consumables load alarm structure */
   {QRL_LOWERBOUND, QRL_S, QRL_N, QRL_Type},           /* queue consumables load alarm structure */

   {CCT_LOWERBOUND, CCTS, CCTN, CCT_Type},

   {CQU_LOWERBOUND, CQUS, CQUN, CQU_Type},             /* queue state changes structure */
   
   {SCT_LOWERBOUND, SCTS, SCTN, SCT_Type},             /* scheduler categories */

   {REF_LOWERBOUND, REFS, REFN, REF_Type},             /* a simple ref object */

   {RQS_LOWERBOUND, RQSS, RQSN, RQS_Type},             /* resource quota set */
   {RQR_LOWERBOUND, RQRS, RQRN, RQR_Type},             /* resource quota rule */
   {RQRF_LOWERBOUND, RQRFS, RQRFN, RQRF_Type},          /* resource quota rule filter */
   {RQRL_LOWERBOUND, RQRLS, RQRLN, RQRL_Type},          /* resource quota rule limit */
   {RQL_LOWERBOUND, RQLS, RQLN, RQL_Type},             /* resource quota limit (scheduler) */
  
   {AR_LOWERBOUND, ARS, ARN, AR_Type},                /* advance reservation */ 
   {ARA_LOWERBOUND, ARAS, ARAN, ARA_Type},             /* advance reservation acl*/ 
   
   {ACK_LOWERBOUND, ACKS, ACKN, ACK_Type},             /* acknowledge */

   {EVR_LOWERBOUND, EVRS, EVRN, EVR_Type},             /* event master requests */
   {JSV_LOWERBOUND, JSVS, JSVN, JSV_Type},             /* job submission verifier */
   {RTIC_LOWERBOUND, RTICS, RTICN, RTIC_Type},          /* internal list for reprioritzie tickets to distribute */
   {PRO_LOWERBOUND, PROS, PRON, PRO_Type},             /* list for all running processes under Linux */
   {GR_LOWERBOUND, GRS, GRN, GR_Type},                /* list of all process groups of Linux process */

   {BN_LOWERBOUND, BNS, BNN, BN_Type},                /* list of binding information */

   {0, 0, NULL, NULL}
};

#ifdef  __cplusplus
}
#endif
#else
#ifdef __SGE_GDI_LIBRARY_SUBLIST_FILE__
#else
#ifdef  __cplusplus
extern "C" {
#endif

   extern lNameSpace nmv[];

#ifdef  __cplusplus
}
#endif
#endif                          /* __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__ */
#endif                          /* __SGE_GDI_LIBRARY_SUBLIST_FILE__     */
#endif                          /* __SGE_ALL_LISTSL_H */
