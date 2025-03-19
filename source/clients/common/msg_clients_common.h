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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "basis_types.h"

// clang-format off

#define MSG_HEADER_HOSTNAME         "HOSTNAME"
#define MSG_HEADER_ARCH             "ARCH"
#define MSG_HEADER_NPROC            "NCPU"
#define MSG_HEADER_NSOC             "NSOC"
#define MSG_HEADER_NCOR             "NCOR"
#define MSG_HEADER_NTHR             "NTHR"
#define MSG_HEADER_LOAD             "LOAD"
#define MSG_HEADER_MEMTOT           "MEMTOT"
#define MSG_HEADER_MEMUSE           "MEMUSE"
#define MSG_HEADER_SWAPTO           "SWAPTO"
#define MSG_HEADER_SWAPUS           "SWAPUS"

#define MSG_QSTAT_PRT_QUEUENAME     "queuename"
#define MSG_QSTAT_PRT_QTYPE         "qtype"
#define MSG_QSTAT_PRT_RESVUSEDTOT   "resv/used/tot."
#define MSG_QSTAT_PRT_STATES        "states"

#define MSG_HEADER_RULE             "resource quota rule"
#define MSG_HEADER_LIMIT            "limit"
#define MSG_HEADER_FILTER           "filter"

#define MSG_JOB_UNASSIGNED                   _MESSAGE(1001, _("unassigned"))
#define MSG_FILE_CANTREADCURRENTWORKINGDIR   _MESSAGE(1002, _("cannot read current working directory"))
#define MSG_SRC_USAGE                        _MESSAGE(1003, _("usage:"))
#define MSG_QDEL_not_available_OPT_USAGE_S   _MESSAGE(1004, _("no usage for " SFQ " available"))
#define MSG_WARNING                          _MESSAGE(1005, _("warning: "))
#define MSG_COMMON_help_OPT_USAGE            _MESSAGE(1006, _("print this help"))
#define MSG_COMMON_xml_OPT_USAGE             _MESSAGE(1007, _("display the information in XML format"))
#define MSG_EITHERSCOPEORMASTERX             _MESSAGE(1008, _("do not mix -scope options with -masterq options"))

#define MSG_QSTAT_HELP_WCCQ                  _MESSAGE(1030, _("wildcard expression matching a cluster queue"))
#define MSG_QSTAT_HELP_WCHOST                _MESSAGE(1031, _("wildcard expression matching a host"))
#define MSG_QSTAT_HELP_WCHG                  _MESSAGE(1032, _("wildcard expression matching a hostgroup"))
#define MSG_CQUEUE_NOQMATCHING_S             _MESSAGE(1033, _("No cluster queue or queue instance matches the phrase " SFQ))
#define MSG_PE_NOSUCHPARALLELENVIRONMENT     _MESSAGE(1034, _("error: no such parallel environment"))

#define MSG_QSTAT_PRT_FINISHEDJOBS           _MESSAGE(1040, _(" --  FINISHED JOBS  -  FINISHED JOBS  -  FINISHED JOBS  -  FINISHED JOBS  --  "))
#define MSG_QSTAT_PRT_PEDINGJOBS             _MESSAGE(1041, _(" - PENDING JOBS - PENDING JOBS - PENDING JOBS - PENDING JOBS - PENDING JOBS"))
#define MSG_PARSE_NOOPTIONARGUMENT           _MESSAGE(1042, _("ERROR! no option argument"))
#define MSG_NORQSFOUND                       _MESSAGE(1043, _("No resource quota set found"))
#define MSG_CONFIG_CANTGETCONFIGURATIONFROMQMASTER _MESSAGE(1044, _("\nCannot get configuration from qmaster."))

// clang-format on
