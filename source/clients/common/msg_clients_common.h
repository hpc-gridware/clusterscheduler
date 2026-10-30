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

#include <cinttypes>

// clang-format off

#define MSG_FILE_CANTREADCURRENTWORKINGDIR   _MESSAGE(1002, _("cannot read current working directory"))
#define MSG_SRC_USAGE                        _MESSAGE(1003, _("usage:"))
#define MSG_QDEL_not_available_OPT_USAGE_S   _MESSAGE(1004, _("no usage for " SFQ " available"))
#define MSG_WARNING                          _MESSAGE(1005, _("warning: "))
#define MSG_COMMON_help_OPT_USAGE            _MESSAGE(1006, _("print this help"))
#define MSG_EITHERSCOPEORMASTERX             _MESSAGE(1007, _("do not mix -scope options with -masterq options"))

#define MSG_QSTAT_HELP_WCCQ                  _MESSAGE(1030, _("wildcard expression matching a cluster queue"))
#define MSG_QSTAT_HELP_WCHOST                _MESSAGE(1031, _("wildcard expression matching a host"))
#define MSG_QSTAT_HELP_WCHG                  _MESSAGE(1032, _("wildcard expression matching a hostgroup"))
#define MSG_CQUEUE_NOQMATCHING_S             _MESSAGE(1033, _("No cluster queue or queue instance matches the phrase " SFQ))

#define MSG_PARSE_NOOPTIONARGUMENT           _MESSAGE(1042, _("ERROR! no option argument"))
#define MSG_CONFIG_CANTGETCONFIGURATIONFROMQMASTER _MESSAGE(1044, _("Cannot get configuration from qmaster."))

// clang-format on
