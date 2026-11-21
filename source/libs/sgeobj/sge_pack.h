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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Declarations for packing CULL conditions for transport
 *
 * @see sge_pack.cc
 */

#include "sgeobj/tom/ocs_tom_PACK.h"
#include "tom/ocs_Element.h"

/// What a packed CULL condition object is
enum {
   SGE_WHAT=1, ///< a field selection, as `lWhat` builds it
   SGE_WHERE   ///< a filter condition, as `lWhere` builds it
};

ocs::tom::ElementPtr<ocs::tom::PACK> ocs_where_to_elem(const lCondition *where);

lCondition *ocs_where_from_elem(ocs::tom::ConstElement<ocs::tom::PACK> where);

ocs::tom::ElementPtr<ocs::tom::PACK> ocs_what_to_elem(const lEnumeration *what);

lEnumeration *ocs_what_from_elem(ocs::tom::ConstElement<ocs::tom::PACK> what);
