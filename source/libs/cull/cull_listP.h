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
 * @brief Internal layout of a cull list and its elements
 */

#include "cull/cull_list.h"

/**
 * @defgroup cull_elem_status Element status
 * @brief Possible values of _lListElem::status
 *
 * The status records who owns an element, which decides whether it may be
 * appended to a list and who has to free it.
 * @{
 */


/* values of _lListElem::status */
#define FREE_ELEM             (1<<0) ///< not part of a list and not a sub-object; the holder must free it
#define BOUND_ELEM            (1<<1) ///< contained in a list, which owns it
#define TRANS_BOUND_ELEM      (1<<2) ///< temporary status while unpacking; bound elements and sub-objects carry it afterwards so that functions such as `lAppendElem()`, which reject bound objects, do not fail
#define OBJECT_ELEM           (1<<3) ///< a sub-object of another element's field
/** @} */

/**
 * @brief One element of a cull list
 *
 * An element carries its own descriptor, so it can exist outside any list —
 * `lCreateElem()` produces exactly that. Its field values live in a separate
 * array indexed the same way as the descriptor.
 */
struct _lListElem {
   lListElem *next;   ///< next element, or nullptr at the end of the list
   lListElem *prev;   ///< previous element, or nullptr at the head
   lUlong status;     ///< whether the element is in a list or free standing
   lDescr *descr;     ///< the object type, as an array of field descriptors
   lMultiType *cont;  ///< the field values, indexed the same way as descr
};

/**
 * @brief A cull list: a doubly linked list of elements of one object type
 *
 * Every element shares the list's descriptor, so a list is homogeneous. The
 * element count is kept rather than walked, and both ends are held so
 * appending is O(1).
 */
struct _lList {
   uint32_t nelem;    ///< number of elements currently in the list
   char *listname;    ///< name of the list, used in dumps and error messages
   lDescr *descr;     ///< the object type every element has
   lListElem *first;  ///< first element, or nullptr when the list is empty
   lListElem *last;   ///< last element, or nullptr when the list is empty
};
