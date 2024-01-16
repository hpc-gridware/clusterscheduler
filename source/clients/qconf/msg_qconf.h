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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "basis_types.h"

#define MSG_QCONF_NEEDAHOSTNAMEORALL                  _MESSAGE(5000, _("Need a hostname or the keyword \"all\""))
#define MSG_QCONF_NOOPTIONARGPROVIDEDTOX_S            _MESSAGE(5001, _("no option argument provided to "SFQ""))
#define MSG_QCONF_BAD_ATTR_ARGS_SS                    _MESSAGE(5002, _("The attribute name ("SFQ") and/or value ("SFQ") is invalid"))
#define MSG_QCONF_CANT_MODIFY_NONE                    _MESSAGE(5003, _("\"NONE\" is not a valid list attribute value for -mattr.  Try using -rattr instead."))
#define MSG_QCONF_ATTR_ARGS_NOT_FOUND                 _MESSAGE(5004, _("Attribute name ("SFQ") and/or value ("SFQ") not found"))
#define MSG_QCONF_MODIFICATIONOFOBJECTNOTSUPPORTED_S  _MESSAGE(5005, _("Modification of object "SFQ" not supported"))
#define MSG_QCONF_NOATTRIBUTEGIVEN                    _MESSAGE(5006, _("No attribute given"))
#define MSG_QCONF_GIVENOBJECTINSTANCEINCOMPLETE_S     _MESSAGE(5007, _("Given object_instance "SFQ" is incomplete"))
#define MSG_QCONF_MODIFICATIONOFHOSTNOTSUPPORTED_S    _MESSAGE(5008, _("Modification of host "SFQ" not supported"))
#define MSG_QCONF_POSITIVE_SHARE_VALUE                _MESSAGE(5008, _("share value must be positive"))

#define MSG_ACL_USERINACL_SS                          _MESSAGE(5050, _(SFQ" is already in access list "SFQ))
#define MSG_ACL_CANTADDTOACL_SS                       _MESSAGE(5051, _("can't add "SFQ" to access list "SFQ))
#define MSG_ACL_ADDTOACL_SS                           _MESSAGE(5052, _("added "SFQ" to access list "SFQ))
#define MSG_ACL_ACLDOESNOTEXIST_S                     _MESSAGE(5053, _("access list "SFQ" doesn't exist"))
#define MSG_ACL_USERNOTINACL_SS                       _MESSAGE(5054, _("user "SFQ" is not in access list "SFQ))
#define MSG_ACL_CANTDELFROMACL_SS                     _MESSAGE(5055, _("can't delete user "SFQ" from access list "SFQ))
#define MSG_ACL_DELFROMACL_SS                         _MESSAGE(5056, _("deleted user "SFQ" from access list "SFQ))

#define MSG_CENTRY_NOTCHANGED                         _MESSAGE(5100, _("Complex attribute configuration has not been changed"))
#define MSG_CENTRY_DOESNOTEXIST_S                     _MESSAGE(5101, _("Complex attribute "SFQ" does not exist"))
#define MSG_CENTRY_FILENOTCORRECT_S                   _MESSAGE(5102, _("Complex attribute file "SFQ" is not correct"))
#define MSG_CENTRY_NULL_URGENCY                       _MESSAGE(5103, _("Complex urgency definition is missing"))
#define MSG_CENTRY_NULL_NAME                          _MESSAGE(5104, _("Invalid complex attribute definition"))
#define MSG_CENTRY_NULL_SHORTCUT_S                    _MESSAGE(5105, _("Complex attribute "SFQ" has no shortcut defined"))

#define MSG_CQUEUE_DOESNOTEXIST_S                     _MESSAGE(5150, _("Cluster queue entry "SFQ" does not exist"))
#define MSG_CQUEUE_FILENOTCORRECT_S                   _MESSAGE(5151, _("Cluster queue file "SFQ" is not correct"))
#define MSG_CQUEUE_NAMENOTCORRECT_SS                  _MESSAGE(5152, _("The queue name "SFQ" is not correct.  Queue names may not begin with @.  Perhaps you mean \"*"SFN"\"?"))

#define MSG_HGROUP_NOTEXIST_S                         _MESSAGE(5200, _("Host group "SFQ" does not exist"))
#define MSG_HGROUP_FILEINCORRECT_S                    _MESSAGE(5201, _("Host group file "SFQ" is not correct"))

#define MSG_RQS_NOTFOUNDINFILE_SS                     _MESSAGE(5251, _("resource quota set "SFQ" not found in file "SFQ))



