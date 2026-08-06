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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The per-layer debug level bitmask used by the rmon macros
 */

#include <sys/types.h>

/// Number of layers debugging can be switched on for, and the size of monitoring_level::ml
constexpr int N_LAYER = 8;

/* different layers for monitoring; the letter is the one the user types in a
 * debug level string, e.g. "t" turns on the top layer */
#define TOP_LAYER        0 ///< general code, selected by `t`
#define CULL_LAYER       1 ///< the cull list library, selected by `c`
#define BASIS_LAYER      2 ///< low level helpers, selected by `b`
#define GUI_LAYER        3 ///< the qmon GUI, selected by `g`
#define UNUSED0_LAYER    4 ///< unused; kept so the layer numbering stays stable
#define COMMD_LAYER      5 ///< the communication library, selected by `h`
#define GDI_LAYER        6 ///< the GDI request layer, selected by `a`
#define PACK_LAYER       7 ///< the packing library, selected by `p`

/* different classes of monitoring messages */
#define TRACE            1 ///< function entry and exit tracing, selected by `t`
#define INFOPRINT        2 ///< explicit debug messages, selected by `i`

/// Which message classes are switched on, per layer
struct monitoring_level {
   u_long ml[N_LAYER]; ///< one bitmask of message classes per layer, indexed by the `*_LAYER` constants
};

int rmon_mliszero(const monitoring_level *);

void rmon_mlclr(monitoring_level *);

u_long rmon_mlgetl(const monitoring_level *, int);

void rmon_mlputl(monitoring_level *, int, u_long);
