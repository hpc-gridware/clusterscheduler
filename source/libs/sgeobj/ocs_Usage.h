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

#include "cull/cull.h"

#include "cull/sge_usage_UA_L.h"

// usage interval in seconds (as double to avoid integer division)
constexpr double sge_usage_interval = 60.0;

namespace ocs {
   class Usage {
   public:
      static void
      calculate_default_decay_constant(int halftime);

      static void
      calculate_decay_constant(double halftime, double *decay_rate, double *decay_constant);

      static void
      decay_usage(const lList *usage_list, const lList *decay_list, double interval);

      static lList *
      create_decay_list();

      static lListElem *
      get_by_name(const lList *usage_list, const char *name);

      static lListElem *
      create_with_name(const char *name);

      static lListElem *
      get_or_create_by_name(lList *usage_list, const char *name);
   };
}
