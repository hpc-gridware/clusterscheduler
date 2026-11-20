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
 *  Portions of this software are Copyright (c) 2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The public part of the list library: the data types only
 *
 * Since CS-3103 (M6) product code reaches objects through the typed object model
 * and the raw API (tom/ocs_Element.h, tom/ocs_List.h, tom/ocs_Raw.h, ...). Of the
 * old library this header keeps only the data types, under their old names until
 * they move into the typed object model:
 * - lDescr, lList, lListElem, lCondition, lEnumeration, ... and the object type
 *   definition macros LISTDEF/SGE_* (tom/cull_types.h),
 * - the packbuffer sge_pack_buffer and the PACK_* result codes (tom/ocs_PackBuffer.h).
 *
 * The old function interface, list walking and the pack buffer primitives
 * included, is in cull/cull_old_api.h, which only libs/cull, libs/tom,
 * libs/sgeobj/tom and the tests (through cull/cull_test_api.h) may include. The
 * old functions are not declared in product code. The TOM headers still need
 * internal headers of the list library for their inline code, so only the
 * internals (hash tables, element ownership, descriptor registry, the raw value
 * slot) remain reachable through them until a later CS-3103 step moves them
 * behind TOM's own detail headers; product code must not call them. The build
 * checks both, the includes and the calls of the internals, see
 * test/libs/cull/check_cull_api.cmake.
 */

#include <cstdio>

#include "uti/sge_dstring.h"

#include "tom/cull_types.h"
#include "tom/ocs_PackBuffer.h"
