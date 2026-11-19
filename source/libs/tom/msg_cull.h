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
// so that a catalogue entry never changes its meaning. Retired so far: 41033, 41036, 41046, 41049,
// 41051, 41052, 41062, 41077, 41078, 41087, 41089 - 41142 (lerrno messages), 41160, 41172, 41176.

// clang-format off

#define MSG_CULL_NOPREFIXANDNOFILENAMEINWRITEELMTODISK   _MESSAGE(41000, _("no prefix and no filename in lWriteElemToDisk"))
#define MSG_CULL_NOPREFIXANDNOFILENAMEINREADELEMFROMDISK _MESSAGE(41001, _("no prefix and no filename in lReadElemFromDisk"))
#define MSG_CULL_NOTENOUGHMEMORYFORPACKINGXY_SS          _MESSAGE(41002, _("not enough memory for packing " SFN " " SFQ))
#define MSG_CULL_NOTENOUGHMEMORYFORUNPACKINGXY_SS        _MESSAGE(41003, _("not enough memory for unpacking " SFN " " SFQ))
#define MSG_CULL_FORMATERRORWHILEPACKINGXY_SS            _MESSAGE(41004, _("format error while packing " SFN " " SFQ))
#define MSG_CULL_UNEXPECTEDERRORWHILEPACKINGXY_SS        _MESSAGE(41005, _("unexpected error while packing " SFN " " SFQ))
#define MSG_CULL_UNEXPECTEDERRORWHILEUNPACKINGXY_SS      _MESSAGE(41006, _("unexpected error while unpacking " SFN " " SFQ))
#define MSG_CULL_FORMATERRORWHILEUNPACKINGXY_SS          _MESSAGE(41007, _("format error while unpacking " SFN " " SFQ))
#define MSG_CULL_CANTOPENXFORWRITINGOFYZ_SSS             _MESSAGE(41008, _("can't open " SFN " for writing of " SFN ": " SFN))
#define MSG_CULL_CANTWRITEXTOFILEY_SS                    _MESSAGE(41009, _("can't write " SFN " to file " PFNMAX))
#define MSG_CULL_CANTREADXFROMFILEY_SS                   _MESSAGE(41010, _("can't read " SFN " from file " PFNMAX))
#define MSG_CULL_CANTGETFILESTATFORXFILEY_SS             _MESSAGE(41011, _("can't get file stat for " SFN " file " PFQMAX))
#define MSG_CULL_XFILEYHASZEROSIYE_SS                    _MESSAGE(41012, _("" SFN " file " PFQMAX " has zero size"))
#define MSG_CULL_ERRORREADINGXINFILEY_SS                 _MESSAGE(41013, _("error reading " SFN " in file " PFNMAX))
#define MSG_CULL_BADARGUMENTWHILEUNPACKINGXY_SS          _MESSAGE(41014, _("bad argument error while unpacking " SFN " " SFQ))
#define MSG_CULL_ERRORININITPACKBUFFER_S                 _MESSAGE(41015, _("error in init_packbuffer: " SFN))
#define MSG_CULL_NOTENOUGHMEMORY_D                       _MESSAGE(41016, _("not enough memory to allocate %d bytes in init_packbuffer"))

#define MSG_CULL_GETINT_WRONGTYPEFORFIELDXY_SS           _MESSAGE(41025, _("lGetInt: wrong type for field " SFN " (" SFN ")"))
#define MSG_CULL_GETULONG_WRONGTYPEFORFIELDXY_SS         _MESSAGE(41029, _("lGetUlong: wrong type for field " SFN " (" SFN ")"))
#define MSG_CULL_GETSTRING_WRONGTYPEFORFILEDXY_SS        _MESSAGE(41030, _("lGetString: wrong type for field " SFN " (" SFN ")"))
#define MSG_CULL_GETHOST_WRONGTYPEFORFILEDXY_SS          _MESSAGE(41031, _("lGetHost: wrong type for field " SFN " (" SFN ")"))
#define MSG_CULL_GETLIST_WRONGTYPEFORFIELDXY_SS          _MESSAGE(41032, _("lGetList: wrong type for field " SFN " (" SFN ")"))
#define MSG_CULL_GETDOUBLE_WRONGTYPEFORFIELDXY_SS        _MESSAGE(41034, _("lGetDouble: wrong type for field " SFN " (" SFN ")"))
#define MSG_CULL_GETLONG_WRONGTYPEFORFIELDXY_SS          _MESSAGE(41035, _("lGetLong: wrong type for field " SFN " (" SFN ")"))
#define MSG_CULL_GETREF_WRONGTYPEFORFIELDXY_SS           _MESSAGE(41037, _("lGetRef: wrong type for field " SFN " (" SFN ")"))
#define MSG_CULL_SETINT_WRONGTYPEFORFIELDXY_SS           _MESSAGE(41038, _("lSetInt: wrong type for field " SFN " (" SFN ")"))
#define MSG_CULL_SETULONG_WRONGTYPEFORFIELDXY_SS         _MESSAGE(41039, _("lSetUlong: wrong type for field " SFN " (" SFN ")"))
#define MSG_CULL_SETSTRING_NOSUCHNAMEXYINDESCRIPTOR_IS   _MESSAGE(41040, _("lSetString: no such name (%d, " SFN ") in descriptor"))
#define MSG_CULL_SETHOST_NOSUCHNAMEXYINDESCRIPTOR_IS     _MESSAGE(41041, _("lSetHost: no such name (%d, " SFN ") in descriptor"))
#define MSG_CULL_SETSTRING_WRONGTYPEFORFIELDXY_SS        _MESSAGE(41042, _("lSetString: wrong type for field " SFN " (" SFN ")"))
#define MSG_CULL_SETHOST_WRONGTYPEFORFIELDXY_SS          _MESSAGE(41043, _("lSetHost: wrong type for field " SFN " (" SFN ")"))
#define MSG_CULL_XCHGLIST_WRONGTYPEFORFIELDXY_SS         _MESSAGE(41044, _("lXchgList: wrong type for field " SFN " (" SFN ")"))
#define MSG_CULL_SETLIST_WRONGTYPEFORFIELDXY_SS          _MESSAGE(41045, _("lSetList: wrong type for field " SFN " (" SFN ")"))
#define MSG_CULL_SETDOUBLE_WRONGTYPEFORFIELDXY_SS        _MESSAGE(41047, _("lSetDouble: wrong type for field " SFN " (" SFN ")"))
#define MSG_CULL_SETLONG_WRONGTYPEFORFIELDXY_SS          _MESSAGE(41048, _("lSetLong: wrong type for field " SFN " (" SFN ")"))
#define MSG_CULL_SETREF_WRONGTYPEFORFIELDXY_SS           _MESSAGE(41050, _("lSetRef: wrong type for field " SFN " (" SFN ")" ))
#define MSG_CULL_ADDELEMSTRERRORXRUNTIMETYPE_S           _MESSAGE(41053, _("error: lAddElemStr(" SFN "): run time type error"))
#define MSG_CULL_ADDELEMHOSTERRORXRUNTIMETYPE_S           _MESSAGE(41054, _("error: lAddElemHost(" SFN "): run time type error"))
#define MSG_CULL_GETELEMSTRERRORXRUNTIMETYPE_S           _MESSAGE(41060, _("error: lGetElemStr(" SFN "): run time type error"))
#define MSG_CULL_ADDELEMULONGERRORXRUNTIMETYPE_S         _MESSAGE(41063, _("error: lAddElemUlong(" SFN "): run time type error"))
#define MSG_CULL_GETELEMULONGERRORXRUNTIMETYPE_S         _MESSAGE(41067, _("error: lGetElemUlong(" SFN "): run time type error"))
#define MSG_CULL_GETELEMCASESTRERRORXRUNTIMETYPE_S       _MESSAGE(41074, _("error: lGetElemCaseStr(" SFN "): run time type error"))
#define MSG_CULL_GETELEMHOSTERRORXRUNTIMETYPE_S          _MESSAGE(41076, _("error: lGetElemHost(" SFN "): run time type error"))

#define MSG_CULL_WHERE_SHOULDBEINTT                      _MESSAGE(41079, _("lWhere (should be a lIntT)"))
#define MSG_CULL_WHERE_SHOULDBESTRINGT                   _MESSAGE(41080, _("lWhere (should be a lStringT or lHostT)"))
#define MSG_CULL_WHERE_SHOULDBEULONGT                    _MESSAGE(41081, _("lWhere (should be a lUlongT)"))
#define MSG_CULL_WHERE_SHOULDBEDOUBLET                   _MESSAGE(41083, _("lWhere (should be a lDoubleT)"))
#define MSG_CULL_WHERE_SHOULDBELONGT                     _MESSAGE(41084, _("lWhere (should be a lLongT)"))
#define MSG_CULL_WHERE_SHOULDBEREFT                      _MESSAGE(41086, _("lWhere (should be a lRefT)"))

#define MSG_CULL_LEMALLOC                _MESSAGE(41088, _("malloc failure"))
// 41089 - 41142: retired lerrno messages; the ids stay unused

#define MSG_CULL_HASHTABLEALREADYEXISTS_S    _MESSAGE(41151, _("hash table already exists for field " SFQ))

#define MSG_CULL_PACK_WRONG_VERSION_XX    _MESSAGE(41152, _("wrong cull version, read 0x%08x, but expected actual version 0x%08x"))

#define MSG_CULL_PACK_SUCCESS             _MESSAGE(41153, _("packing successful"))
#define MSG_CULL_PACK_ENOMEM              _MESSAGE(41154, _("can't allocate memory"))
#define MSG_CULL_PACK_FORMAT              _MESSAGE(41155, _("invalid input parameter"))
#define MSG_CULL_PACK_BADARG              _MESSAGE(41156, _("bad argument"))
#define MSG_CULL_PACK_VERSION             _MESSAGE(41157, _("wrong cull version"))
#define MSG_CULL_PACK_AUTHINFO            _MESSAGE(41158, _("incorrect authentication information"))

#define MSG_CULL_GETOBJECT_WRONGTYPEFORFIELDXY_SS          _MESSAGE(41161, _("lGetObject: wrong type for field " SFN " (" SFN ")"))
#define MSG_CULL_DECHAINOBJECT_WRONGTYPEFORFIELDXY_S       _MESSAGE(41163, _("lDechainObject: wrong type for field " SFN " (" SFN ")"))
#define MSG_CULL_GETBOOL_WRONGTYPEFORFIELDXY_SS            _MESSAGE(41165, _("lGetBool: wrong type for field " SFN " (" SFN ")"))
#define MSG_CULL_SETBOOL_WRONGTYPEFORFIELDXY_SS            _MESSAGE(41166, _("lSetBool: wrong type for field " SFN " (" SFN ")"))
#define MSG_CULL_WHERE_SHOULDBEBOOL                        _MESSAGE(41167, _("lWhere (should be a lBoolT)"))
#define MSG_CULL_XNOTFOUNDINELEMENT_S                      _MESSAGE(41068, _("!!!!!!!!!! " SFN " not found in element !!!!!!!!!!"))
#define MSG_CULL_POINTER_NULLELEMENTFORX_S                 _MESSAGE(41069, _("!!!!!!!!!! got nullptr element for " SFN " !!!!!!!!!!"))

#define MSG_CULL_GETULONG64_WRONGTYPEFORFIELDXY_SS         _MESSAGE(41170, _("lGetUlong64: wrong type for field " SFN " (" SFN ")"))
#define MSG_CULL_SETULONG64_WRONGTYPEFORFIELDXY_SS         _MESSAGE(41171, _("lSetUlong64: wrong type for field " SFN " (" SFN ")"))
#define MSG_CULL_ADDELEMULONG64ERRORXRUNTIMETYPE_S         _MESSAGE(41173, _("error: lAddElemUlong64(" SFN "): run time type error"))
#define MSG_CULL_GETELEMULONG64ERRORXRUNTIMETYPE_S         _MESSAGE(41175, _("error: lGetElemUlong64(" SFN "): run time type error"))
#define MSG_CULL_WHERE_SHOULDBEULONG64T                    _MESSAGE(41177, _("lWhere (should be a lUlong64T)"))

#define MSG_CULL_AUTHINFO_UIDGIDMISMATCH_UUUU              _MESSAGE(41180, _("uid and gid information from message ( " uid_t_fmt ":" gid_t_fmt ") does not match local user and group information ( " uid_t_fmt ":" gid_t_fmt ")"))
#define MSG_CULL_AUTHINFO_COULDNOTRESOLVEUSER_U            _MESSAGE(41181, _("could not resolve user name for uid " uid_t_fmt))
#define MSG_CULL_AUTHINFO_COULDNOTRESOLVEGROUP_U           _MESSAGE(41182, _("could not resolve group name for gid " gid_t_fmt))
#define MSG_CULL_AUTHINFO_CORRECTINGUSERNAME_SS            _MESSAGE(41183, _("correcting user name from authinfo: " SFQ " to re-resolved name " SFQ))
#define MSG_CULL_AUTHINFO_CORRECTINGGROUPNAME_SS           _MESSAGE(41184, _("correcting group name from authinfo: " SFQ " to re-resolved name " SFQ))
#define MSG_CULL_DESCR_SOFTCAPREACHED_U                    _MESSAGE(41185, _("CULL descriptor registry holds " sge_u32 " descriptors, further ones are private copies"))
#define MSG_CULL_BADSLOT_IIUU                              _MESSAGE(41186, _("CULL value access at position %d of the type starting at field %d lies outside its " sge_u32 " fields or " sge_u32 " values"))
#define MSG_CULL_DESCRNOTFULLLENGTH_IU                     _MESSAGE(41187, _("CULL reduced descriptor starting at field %d has " sge_u32 " entries instead of the length of its type"))
#define MSG_CULL_INVALIDPOSITION_SI                        _MESSAGE(41188, _("%-.100s() got the invalid position %d"))
#define MSG_CULL_NULLELEMENTATPOS_SI                       _MESSAGE(41189, _("%-.100s() got a null element for position %d"))
// clang-format on
