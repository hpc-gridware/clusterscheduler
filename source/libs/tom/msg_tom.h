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
 * @brief Localisable message catalogue of the cull library
 *
 * Message ids are reserved for the cull library. Do not write the message
 * macro's name followed by an opening parenthesis anywhere in this file's
 * comments: the testsuite parses these headers line by line looking for
 * exactly that string, and a prose line carrying it fails
 * `update_macro_messages_list` during test setup.
 */

#include <cinttypes>

// Retired ids, not to be reused: messages without any user left were removed, the ids stay reserved
// so that a catalogue entry never changes its meaning. Retired so far: 41025, 41029 - 41052, 41060,
// 41062, 41069, 41077 - 41081, 41083, 41084, 41086, 41087, 41089 - 41142 (messages of the removed error
// numbers), 41160, 41161, 41163, 41165 - 41167, 41170 - 41172, 41176, 41177, 41180 - 41184 (the authentication
// messages, moved to gdi/msg_gdilib.h with the check they belong to), 41188, 41189.

// clang-format off

#define MSG_TOM_WRITETODISK_NOPREFIXANDNOFILENAME          _MESSAGE(41000, _("write_to_disk(): no prefix and no file name for the spool file"))
#define MSG_TOM_READFROMDISK_NOPREFIXANDNOFILENAME         _MESSAGE(41001, _("read_from_disk(): no prefix and no file name for the spool file"))
#define MSG_TOM_NOTENOUGHMEMORYFORPACKINGXY_SS             _MESSAGE(41002, _("not enough memory for packing " SFN " " SFQ))
#define MSG_TOM_NOTENOUGHMEMORYFORUNPACKINGXY_SS           _MESSAGE(41003, _("not enough memory for unpacking " SFN " " SFQ))
#define MSG_TOM_FORMATERRORWHILEPACKINGXY_SS               _MESSAGE(41004, _("format error while packing " SFN " " SFQ))
#define MSG_TOM_UNEXPECTEDERRORWHILEPACKINGXY_SS           _MESSAGE(41005, _("unexpected error while packing " SFN " " SFQ))
#define MSG_TOM_UNEXPECTEDERRORWHILEUNPACKINGXY_SS         _MESSAGE(41006, _("unexpected error while unpacking " SFN " " SFQ))
#define MSG_TOM_FORMATERRORWHILEUNPACKINGXY_SS             _MESSAGE(41007, _("format error while unpacking " SFN " " SFQ))
#define MSG_TOM_CANTOPENXFORWRITINGOFYZ_SSS                _MESSAGE(41008, _("can't open " SFN " for writing of " SFN ": " SFN))
#define MSG_TOM_CANTWRITEXTOFILEY_SS                       _MESSAGE(41009, _("can't write " SFN " to file " PFNMAX))
#define MSG_TOM_CANTREADXFROMFILEY_SS                      _MESSAGE(41010, _("can't read " SFN " from file " PFNMAX))
#define MSG_TOM_CANTGETFILESTATFORXFILEY_SS                _MESSAGE(41011, _("can't get file stat for " SFN " file " PFQMAX))
#define MSG_TOM_XFILEYHASZEROSIYE_SS                       _MESSAGE(41012, _("" SFN " file " PFQMAX " has zero size"))
#define MSG_TOM_ERRORREADINGXINFILEY_SS                    _MESSAGE(41013, _("error reading " SFN " in file " PFNMAX))
#define MSG_TOM_BADARGUMENTWHILEUNPACKINGXY_SS             _MESSAGE(41014, _("bad argument error while unpacking " SFN " " SFQ))
#define MSG_TOM_ERRORININITPACKBUFFER_S                    _MESSAGE(41015, _("error initializing a pack buffer: " SFN))
#define MSG_TOM_NOTENOUGHMEMORY_D                          _MESSAGE(41016, _("not enough memory to allocate %d bytes for a pack buffer"))

#define MSG_CULL_ADDELEMSTRERRORXRUNTIMETYPE_S           _MESSAGE(41053, _("error: lAddElemStr(" SFN "): run time type error"))
#define MSG_CULL_ADDELEMHOSTERRORXRUNTIMETYPE_S           _MESSAGE(41054, _("error: lAddElemHost(" SFN "): run time type error"))
#define MSG_CULL_ADDELEMULONGERRORXRUNTIMETYPE_S         _MESSAGE(41063, _("error: lAddElemUlong(" SFN "): run time type error"))
#define MSG_CULL_GETELEMULONGERRORXRUNTIMETYPE_S         _MESSAGE(41067, _("error: lGetElemUlong(" SFN "): run time type error"))
#define MSG_CULL_GETELEMCASESTRERRORXRUNTIMETYPE_S       _MESSAGE(41074, _("error: lGetElemCaseStr(" SFN "): run time type error"))
#define MSG_CULL_GETELEMHOSTERRORXRUNTIMETYPE_S          _MESSAGE(41076, _("error: lGetElemHost(" SFN "): run time type error"))


#define MSG_TOM_MALLOCFAILURE                              _MESSAGE(41088, _("malloc failure"))
// 41089 - 41142: retired messages of the removed error numbers; the ids stay unused

#define MSG_TOM_HASHTABLEALREADYEXISTS_S                   _MESSAGE(41151, _("hash table already exists for field " SFQ))

#define MSG_TOM_PACK_WRONG_VERSION_XX                      _MESSAGE(41152, _("wrong packing version, read 0x%08x, but expected actual version 0x%08x"))

#define MSG_TOM_PACK_SUCCESS                               _MESSAGE(41153, _("packing successful"))
#define MSG_TOM_PACK_ENOMEM                                _MESSAGE(41154, _("can't allocate memory"))
#define MSG_TOM_PACK_FORMAT                                _MESSAGE(41155, _("invalid input parameter"))
#define MSG_TOM_PACK_BADARG                                _MESSAGE(41156, _("bad argument"))
#define MSG_TOM_PACK_VERSION                               _MESSAGE(41157, _("wrong packing version"))
#define MSG_TOM_PACK_AUTHINFO                              _MESSAGE(41158, _("incorrect authentication information"))

#define MSG_CULL_XNOTFOUNDINELEMENT_S                      _MESSAGE(41068, _("!!!!!!!!!! " SFN " not found in element !!!!!!!!!!"))

#define MSG_CULL_ADDELEMULONG64ERRORXRUNTIMETYPE_S         _MESSAGE(41173, _("error: lAddElemUlong64(" SFN "): run time type error"))
#define MSG_CULL_GETELEMULONG64ERRORXRUNTIMETYPE_S         _MESSAGE(41175, _("error: lGetElemUlong64(" SFN "): run time type error"))

#define MSG_TOM_DESCR_SOFTCAPREACHED_U                     _MESSAGE(41185, _("descriptor registry of the object model holds " sge_u32 " descriptors, further ones are private copies"))
#define MSG_TOM_BADSLOT_IIUU                               _MESSAGE(41186, _("value access at position %d of the type starting at field %d lies outside its " sge_u32 " fields or " sge_u32 " values"))
#define MSG_TOM_DESCRNOTFULLLENGTH_IU                      _MESSAGE(41187, _("reduced descriptor starting at field %d has " sge_u32 " entries instead of the length of its type"))
#define MSG_TOM_MODELREJECTED_S                            _MESSAGE(41190, _("inconsistent object model rejected: %s"))
// clang-format on
