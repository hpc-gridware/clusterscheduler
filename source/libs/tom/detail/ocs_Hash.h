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
 * @brief Hash tables over list fields
 */

#include <cstdint>

#include "uti/sge_dstring.h"
#include "uti/sge_htable.h"

#include "tom/ocs_Types.h"
#include "tom/detail/ocs_HashP.h"

namespace ocs::tom::detail {
   tom_htable hash_create(const ocs::tom::FieldDescr *dp, int pos, int size);

   tom_htable hash_build(const ocs::tom::ListData *lp, int pos);

   tom_htable hash_lookup_table(const ocs::tom::ListData *lp, int pos);

   void hash_set_min_elems(uint32_t n);

   void hash_set_min_elems_host(uint32_t n);

   void hash_set_min_elems_string(uint32_t n);

   int hash_new(ocs::tom::ListData *lp, int name, bool unique);

   int hash_new_check(ocs::tom::ListData *lp, int nm, bool unique);

   void hash_insert(const ocs::tom::ElementData *ep, void *key, tom_htable ht, bool unique);

   void hash_insert_changed(const ocs::tom::ElementData *ep, int pos);

   void hash_remove(const ocs::tom::ElementData *ep, int pos);

   void hash_elem(const ocs::tom::ElementData *ep);

   void hash_remove_elem(const ocs::tom::ElementData *ep);

   ocs::tom::ElementData *hash_first(tom_htable ht, const void *key, bool unique, const void **iterator);

   ocs::tom::ElementData *hash_next(tom_htable ht, const void **iterator);

   bool hash_init_slots(ocs::tom::ListData *lp, bool hashable);

   void hash_free_tables(ocs::tom::ListData *lp);

   void hash_free_slots(ocs::tom::ListData *lp);

   void *hash_key(const ocs::tom::ElementData *ep, int pos, char *host_key);

   const char *hash_statistics(tom_htable ht, dstring *buffer);

   void hash_recreate_after_sort(ocs::tom::ListData *lp);
} // namespace ocs::tom::detail
