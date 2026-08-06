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
 * @brief Implementation of the per-layer debug level bitmask
 */

#include "uti/sge_rmon_monitoring_level.h"

int
/**
 * @brief Is debugging switched off for every layer?
 *
 * @param m the level to test
 * @return non-zero when no message class is enabled in any layer
 */
rmon_mliszero(const monitoring_level *m) {
   for (int j = 0; j < N_LAYER; j++) {
       if (m->ml[j] != 0) {
           return 0;
       }
   }
   return 1;
}

void
/**
 * @brief Switch debugging off for every layer
 *
 * @param d the level to clear
 */
rmon_mlclr(monitoring_level *d) {
   for (int j = 0; j < N_LAYER; j++) {
       d->ml[j] = 0;
   }
}

u_long
/**
 * @brief The message classes switched on for one layer
 *
 * @param s the level to read
 * @param i the layer, one of the `*_LAYER` constants
 * @return the bitmask, or 0 when @p i is out of range
 */
rmon_mlgetl(const monitoring_level *s, const int i) {
   return s->ml[i];
}

void
/**
 * @brief Set the message classes switched on for one layer
 *
 * Out of range layers are ignored.
 *
 * @param s the level to modify
 * @param i the layer, one of the `*_LAYER` constants
 * @param mask the message classes to enable
 */
rmon_mlputl(monitoring_level *s, const int i, const u_long mask) {
   s->ml[i] = mask;
}

