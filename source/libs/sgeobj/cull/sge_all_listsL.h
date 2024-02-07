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

#include "sgeobj/cull/sge_job_JB_L.h"
#include "sgeobj/cull/sge_qinstance_QU_L.h"
#include "sgeobj/cull/sge_host_EH_L.h"
#include "sgeobj/cull/sge_host_AH_L.h"
#include "sgeobj/cull/sge_host_SH_L.h"
#include "sgeobj/cull/sge_answer_AN_L.h"
#include "sgeobj/cull/sge_host_HL_L.h"
#include "sgeobj/cull/sge_host_HS_L.h"
#include "sgeobj/cull/sge_event_ET_L.h"
#include "sgeobj/cull/sge_event_EV_L.h"
#include "sgeobj/cull/sge_event_EVS_L.h"
#include "sgeobj/cull/sge_centry_CE_L.h"
#include "sgeobj/cull/sge_report_LR_L.h"
#include "sgeobj/cull/sge_order_OR_L.h"
#include "sgeobj/cull/sge_order_OQ_L.h"
#include "sgeobj/cull/sge_userset_US_L.h"
#include "sgeobj/cull/sge_userset_UE_L.h"
#include "sgeobj/cull/sge_range_RN_L.h"
#include "sgeobj/cull/sge_job_PN_L.h"
#include "sgeobj/cull/sge_var_VA_L.h"
#include "sgeobj/cull/sge_mailrec_MR_L.h"
#include "sgeobj/cull/sge_manop_UM_L.h"
#include "sgeobj/cull/sge_manop_UO_L.h"
#include "sgeobj/cull/sge_pe_PE_L.h"
#include "sgeobj/cull/sge_qref_QR_L.h"
#include "sgeobj/cull/sge_userset_JC_L.h"
#include "sgeobj/cull/sge_conf_CONF_L.h"
#include "sgeobj/cull/sge_conf_CF_L.h"
#include "sgeobj/cull/sge_str_ST_L.h"
#include "sgeobj/cull/sge_str_STU_L.h"
#include "sgeobj/cull/sge_job_JG_L.h"
#include "sgeobj/cull/sge_subordinate_SO_L.h"
#include "sgeobj/cull/sge_helper_QAJ_L.h"
#include "sgeobj/cull/sge_parse_SPA_L.h"
#include "sgeobj/cull/sge_report_REP_L.h"
#include "sgeobj/cull/sge_usage_UA_L.h"
#include "sgeobj/cull/sge_userprj_PR_L.h"
#include "sgeobj/cull/sge_userprj_UU_L.h"
#include "sgeobj/cull/sge_grantedres_GRU_L.h"
#include "sgeobj/cull/sge_host_RESL_L.h"
#include "sgeobj/cull/sge_sharetree_STN_L.h"
#include "sgeobj/cull/sge_schedd_conf_SC_L.h"
#include "sgeobj/cull/sge_pe_task_PET_L.h"
#include "sgeobj/cull/sge_pe_task_PETR_L.h"
#include "sgeobj/cull/sge_pe_task_FPET_L.h"
#include "sgeobj/cull/sge_report_JR_L.h"
#include "sgeobj/cull/sge_report_LIC_L.h"
#include "sgeobj/cull/sge_ptf_JL_L.h"
#include "sgeobj/cull/sge_ptf_JP_L.h"
#include "sgeobj/cull/sge_ptf_JO_L.h"
#include "sgeobj/cull/sge_userprj_UPU_L.h"
#include "sgeobj/cull/sge_ckpt_CK_L.h"
#include "sgeobj/cull/sge_krb_KRB_L.h"
#include "sgeobj/cull/sge_path_alias_PA_L.h"
#include "sgeobj/cull/sge_job_ref_JRE_L.h"
#include "sgeobj/cull/sge_id_ID_L.h"
#include "sgeobj/cull/sge_multi_MA_L.h"
#include "sgeobj/cull/sge_time_event_TE_L.h"
#include "sgeobj/cull/sge_calendar_CAL_L.h"
#include "sgeobj/cull/sge_calendar_CA_L.h"
#include "sgeobj/cull/sge_calendar_TMR_L.h"
#include "sgeobj/cull/sge_calendar_TM_L.h"
#include "sgeobj/cull/sge_qexec_RT_L.h"
#include "sgeobj/cull/sge_userprj_UPP_L.h"
#include "sgeobj/cull/sge_krb_KTGT_L.h"
#include "sgeobj/cull/sge_message_SME_L.h"
#include "sgeobj/cull/sge_message_MES_L.h"
#include "sgeobj/cull/sge_ja_task_JAT_L.h"
#include "sgeobj/cull/sge_ct_CT_L.h"
#include "sgeobj/cull/sge_eejob_SGEJ_L.h"
#include "sgeobj/cull/sge_ulong_ULNG_L.h"
#include "sgeobj/cull/sge_hgroup_HGRP_L.h"
#include "sgeobj/cull/sge_href_HR_L.h"
#include "sgeobj/cull/sge_permission_PERM_L.h"
#include "sgeobj/cull/sge_loadsensor_LS_L.h"
#include "sgeobj/cull/sge_host_RU_L.h"
#include "sgeobj/cull/sge_feature_FES_L.h"
#include "sgeobj/cull/sge_suser_SU_L.h"
#include "sgeobj/cull/sge_spooling_SPC_L.h"
#include "sgeobj/cull/sge_spooling_SPR_L.h"
#include "sgeobj/cull/sge_spooling_SPT_L.h"
#include "sgeobj/cull/sge_spooling_SPTR_L.h"
#include "sgeobj/cull/sge_japi_JJ_L.h"
#include "sgeobj/cull/sge_japi_JJAT_L.h"
#include "sgeobj/cull/sge_japi_NSV_L.h"
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
#include "sgeobj/cull/sge_cqueue_CQ_L.h"
#include "sgeobj/cull/sge_mesobj_QIM_L.h"
#include "sgeobj/cull/sge_eejob_FCAT_L.h"
#include "sgeobj/cull/sge_ct_CTI_L.h"
#include "sgeobj/cull/sge_schedd_conf_PARA_L.h"
#include "sgeobj/cull/sge_cull_xml_XMLA_L.h"
#include "sgeobj/cull/sge_cull_xml_XMLH_L.h"
#include "sgeobj/cull/sge_cull_xml_XMLS_L.h"
#include "sgeobj/cull/sge_cull_xml_XMLE_L.h"
#include "sgeobj/cull/sge_resource_utilization_RDE_L.h"
#include "sgeobj/cull/sge_resource_utilization_RUE_L.h"
#include "sgeobj/cull/sge_qeti_QETI_L.h"
#include "sgeobj/cull/sge_select_queue_LDR_L.h"
#include "sgeobj/cull/sge_select_queue_QRL_L.h"
#include "sgeobj/cull/sge_ct_CCT_L.h"
#include "sgeobj/cull/sge_calendar_CQU_L.h"
#include "sgeobj/cull/sge_ct_SCT_L.h"
#include "sgeobj/cull/sge_ct_REF_L.h"
#include "sgeobj/cull/sge_resource_quota_RQS_L.h"
#include "sgeobj/cull/sge_resource_quota_RQR_L.h"
#include "sgeobj/cull/sge_resource_quota_RQRF_L.h"
#include "sgeobj/cull/sge_resource_quota_RQRL_L.h"
#include "sgeobj/cull/sge_resource_quota_RQL_L.h"
#include "sgeobj/cull/sge_advance_reservation_AR_L.h"
#include "sgeobj/cull/sge_advance_reservation_ARA_L.h"
#include "sgeobj/cull/sge_ack_ACK_L.h"
#include "sgeobj/cull/sge_event_request_EVR_L.h"
#include "sgeobj/cull/sge_jsv_JSV_L.h"
#include "sgeobj/cull/sge_order_RTIC_L.h"
#include "sgeobj/cull/sge_proc_PRO_L.h"
#include "sgeobj/cull/sge_proc_GR_L.h"
#include "sgeobj/cull/sge_binding_BN_L.h"
#include "sgeobj/cull/sge_pack_PACK_L.h"
#if defined(__SGE_GDI_LIBRARY_HOME_OBJECT_FILE__)

lNameSpace nmv[] = {

/*
   1. unique keq of the first element in the descriptor
   2. number of elements in the descriptor
   3. array with names describing the fields of the descriptor
   4. pointer to the descriptor
      1.              2.   3.   4.
*/

   {JB_LOWERBOUND, JB_SIZE, JBN, JB_Type},
   {QU_LOWERBOUND, QU_SIZE, QUN, QU_Type},
   {EH_LOWERBOUND, EH_SIZE, EHN, EH_Type},
   {AH_LOWERBOUND, AH_SIZE, AHN, AH_Type},
   {SH_LOWERBOUND, SH_SIZE, SHN, SH_Type},
   {AN_LOWERBOUND, AN_SIZE, ANN, AN_Type},
   {HL_LOWERBOUND, HL_SIZE, HLN, HL_Type},
   {HS_LOWERBOUND, HS_SIZE, HSN, HS_Type},
   {ET_LOWERBOUND, ET_SIZE, ETN, ET_Type},
   {EV_LOWERBOUND, EV_SIZE, EVN, EV_Type},
   {EVS_LOWERBOUND, EVS_SIZE, EVSN, EVS_Type},
   {CE_LOWERBOUND, CE_SIZE, CEN, CE_Type},
   {LR_LOWERBOUND, LR_SIZE, LRN, LR_Type},
   {OR_LOWERBOUND, OR_SIZE, ORN, OR_Type},
   {OQ_LOWERBOUND, OQ_SIZE, OQN, OQ_Type},
   {US_LOWERBOUND, US_SIZE, USN, US_Type},
   {UE_LOWERBOUND, UE_SIZE, UEN, UE_Type},
   {RN_LOWERBOUND, RN_SIZE, RNN, RN_Type},
   {PN_LOWERBOUND, PN_SIZE, PNN, PN_Type},
   {VA_LOWERBOUND, VA_SIZE, VAN, VA_Type},
   {MR_LOWERBOUND, MR_SIZE, MRN, MR_Type},
   {UM_LOWERBOUND, UM_SIZE, UMN, UM_Type},
   {UO_LOWERBOUND, UO_SIZE, UON, UO_Type},
   {PE_LOWERBOUND, PE_SIZE, PEN, PE_Type},
   {QR_LOWERBOUND, QR_SIZE, QRN, QR_Type},
   {JC_LOWERBOUND, JC_SIZE, JCN, JC_Type},
   {CONF_LOWERBOUND, CONF_SIZE, CONFN, CONF_Type},
   {CF_LOWERBOUND, CF_SIZE, CFN, CF_Type},
   {ST_LOWERBOUND, ST_SIZE, STN, ST_Type},
   {STU_LOWERBOUND, STU_SIZE, STUN, STU_Type},
   {JG_LOWERBOUND, JG_SIZE, JGN, JG_Type},
   {SO_LOWERBOUND, SO_SIZE, SON, SO_Type},
   {QAJ_LOWERBOUND, QAJ_SIZE, QAJN, QAJ_Type},
   {SPA_LOWERBOUND, SPA_SIZE, SPAN, SPA_Type},
   {REP_LOWERBOUND, REP_SIZE, REPN, REP_Type},
   {UA_LOWERBOUND, UA_SIZE, UAN, UA_Type},
   {PR_LOWERBOUND, PR_SIZE, PRN, PR_Type},
   {UU_LOWERBOUND, UU_SIZE, UUN, UU_Type},
   {GRU_LOWERBOUND, GRU_SIZE, GRUN, GRU_Type},
   {RESL_LOWERBOUND, RESL_SIZE, RESLN, RESL_Type},
   {STN_LOWERBOUND, STN_SIZE, STNN, STN_Type},
   {SC_LOWERBOUND, SC_SIZE, SCN, SC_Type},
   {PET_LOWERBOUND, PET_SIZE, PETN, PET_Type},
   {PETR_LOWERBOUND, PETR_SIZE, PETRN, PETR_Type},
   {FPET_LOWERBOUND, FPET_SIZE, FPETN, FPET_Type},
   {JR_LOWERBOUND, JR_SIZE, JRN, JR_Type},
   {LIC_LOWERBOUND, LIC_SIZE, LICN, LIC_Type},
   {JL_LOWERBOUND, JL_SIZE, JLN, JL_Type},
   {JP_LOWERBOUND, JP_SIZE, JPN, JP_Type},
   {JO_LOWERBOUND, JO_SIZE, JON, JO_Type},
   {UPU_LOWERBOUND, UPU_SIZE, UPUN, UPU_Type},
   {CK_LOWERBOUND, CK_SIZE, CKN, CK_Type},
   {KRB_LOWERBOUND, KRB_SIZE, KRBN, KRB_Type},
   {PA_LOWERBOUND, PA_SIZE, PAN, PA_Type},
   {JRE_LOWERBOUND, JRE_SIZE, JREN, JRE_Type},
   {ID_LOWERBOUND, ID_SIZE, IDN, ID_Type},
   {MA_LOWERBOUND, MA_SIZE, MAN, MA_Type},
   {TE_LOWERBOUND, TE_SIZE, TEN, TE_Type},
   {CAL_LOWERBOUND, CAL_SIZE, CALN, CAL_Type},
   {CA_LOWERBOUND, CA_SIZE, CAN, CA_Type},
   {TMR_LOWERBOUND, TMR_SIZE, TMRN, TMR_Type},
   {TM_LOWERBOUND, TM_SIZE, TMN, TM_Type},
   {RT_LOWERBOUND, RT_SIZE, RTN, RT_Type},
   {UPP_LOWERBOUND, UPP_SIZE, UPPN, UPP_Type},
   {KTGT_LOWERBOUND, KTGT_SIZE, KTGTN, KTGT_Type},
   {SME_LOWERBOUND, SME_SIZE, SMEN, SME_Type},
   {MES_LOWERBOUND, MES_SIZE, MESN, MES_Type},
   {JAT_LOWERBOUND, JAT_SIZE, JATN, JAT_Type},
   {CT_LOWERBOUND, CT_SIZE, CTN, CT_Type},
   {SGEJ_LOWERBOUND, SGEJ_SIZE, SGEJN, SGEJ_Type},
   {ULNG_LOWERBOUND, ULNG_SIZE, ULNGN, ULNG_Type},
   {HGRP_LOWERBOUND, HGRP_SIZE, HGRPN, HGRP_Type},
   {HR_LOWERBOUND, HR_SIZE, HRN, HR_Type},
   {PERM_LOWERBOUND, PERM_SIZE, PERMN, PERM_Type},
   {LS_LOWERBOUND, LS_SIZE, LSN, LS_Type},
   {RU_LOWERBOUND, RU_SIZE, RUN, RU_Type},
   {FES_LOWERBOUND, FES_SIZE, FESN, FES_Type},
   {SU_LOWERBOUND, SU_SIZE, SUN, SU_Type},
   {SPC_LOWERBOUND, SPC_SIZE, SPCN, SPC_Type},
   {SPR_LOWERBOUND, SPR_SIZE, SPRN, SPR_Type},
   {SPT_LOWERBOUND, SPT_SIZE, SPTN, SPT_Type},
   {SPTR_LOWERBOUND, SPTR_SIZE, SPTRN, SPTR_Type},
   {JJ_LOWERBOUND, JJ_SIZE, JJN, JJ_Type},
   {JJAT_LOWERBOUND, JJAT_SIZE, JJATN, JJAT_Type},
   {NSV_LOWERBOUND, NSV_SIZE, NSVN, NSV_Type},
   {ASTR_LOWERBOUND, ASTR_SIZE, ASTRN, ASTR_Type},
   {AULNG_LOWERBOUND, AULNG_SIZE, AULNGN, AULNG_Type},
   {ABOOL_LOWERBOUND, ABOOL_SIZE, ABOOLN, ABOOL_Type},
   {ATIME_LOWERBOUND, ATIME_SIZE, ATIMEN, ATIME_Type},
   {AMEM_LOWERBOUND, AMEM_SIZE, AMEMN, AMEM_Type},
   {AINTER_LOWERBOUND, AINTER_SIZE, AINTERN, AINTER_Type},
   {ASTRING_LOWERBOUND, ASTRING_SIZE, ASTRINGN, ASTRING_Type},
   {ASTRLIST_LOWERBOUND, ASTRLIST_SIZE, ASTRLISTN, ASTRLIST_Type},
   {AUSRLIST_LOWERBOUND, AUSRLIST_SIZE, AUSRLISTN, AUSRLIST_Type},
   {APRJLIST_LOWERBOUND, APRJLIST_SIZE, APRJLISTN, APRJLIST_Type},
   {ACELIST_LOWERBOUND, ACELIST_SIZE, ACELISTN, ACELIST_Type},
   {ASOLIST_LOWERBOUND, ASOLIST_SIZE, ASOLISTN, ASOLIST_Type},
   {AQTLIST_LOWERBOUND, AQTLIST_SIZE, AQTLISTN, AQTLIST_Type},
   {CQ_LOWERBOUND, CQ_SIZE, CQN, CQ_Type},
   {QIM_LOWERBOUND, QIM_SIZE, QIMN, QIM_Type},
   {FCAT_LOWERBOUND, FCAT_SIZE, FCATN, FCAT_Type},
   {CTI_LOWERBOUND, CTI_SIZE, CTIN, CTI_Type},
   {PARA_LOWERBOUND, PARA_SIZE, PARAN, PARA_Type},
   {XMLA_LOWERBOUND, XMLA_SIZE, XMLAN, XMLA_Type},
   {XMLH_LOWERBOUND, XMLH_SIZE, XMLHN, XMLH_Type},
   {XMLS_LOWERBOUND, XMLS_SIZE, XMLSN, XMLS_Type},
   {XMLE_LOWERBOUND, XMLE_SIZE, XMLEN, XMLE_Type},
   {RDE_LOWERBOUND, RDE_SIZE, RDEN, RDE_Type},
   {RUE_LOWERBOUND, RUE_SIZE, RUEN, RUE_Type},
   {QETI_LOWERBOUND, QETI_SIZE, QETIN, QETI_Type},
   {LDR_LOWERBOUND, LDR_SIZE, LDRN, LDR_Type},
   {QRL_LOWERBOUND, QRL_SIZE, QRLN, QRL_Type},
   {CCT_LOWERBOUND, CCT_SIZE, CCTN, CCT_Type},
   {CQU_LOWERBOUND, CQU_SIZE, CQUN, CQU_Type},
   {SCT_LOWERBOUND, SCT_SIZE, SCTN, SCT_Type},
   {REF_LOWERBOUND, REF_SIZE, REFN, REF_Type},
   {RQS_LOWERBOUND, RQS_SIZE, RQSN, RQS_Type},
   {RQR_LOWERBOUND, RQR_SIZE, RQRN, RQR_Type},
   {RQRF_LOWERBOUND, RQRF_SIZE, RQRFN, RQRF_Type},
   {RQRL_LOWERBOUND, RQRL_SIZE, RQRLN, RQRL_Type},
   {RQL_LOWERBOUND, RQL_SIZE, RQLN, RQL_Type},
   {AR_LOWERBOUND, AR_SIZE, ARN, AR_Type},
   {ARA_LOWERBOUND, ARA_SIZE, ARAN, ARA_Type},
   {ACK_LOWERBOUND, ACK_SIZE, ACKN, ACK_Type},
   {EVR_LOWERBOUND, EVR_SIZE, EVRN, EVR_Type},
   {JSV_LOWERBOUND, JSV_SIZE, JSVN, JSV_Type},
   {RTIC_LOWERBOUND, RTIC_SIZE, RTICN, RTIC_Type},
   {PRO_LOWERBOUND, PRO_SIZE, PRON, PRO_Type},
   {GR_LOWERBOUND, GR_SIZE, GRN, GR_Type},
   {BN_LOWERBOUND, BN_SIZE, BNN, BN_Type},
   {0, 0, nullptr, nullptr}
};

#else

extern lNameSpace nmv[];

#endif /* __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__ */

#endif /* __SGE_ALL_LISTSL_H */
