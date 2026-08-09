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
 *   Copyright: 2003 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Interface shared by the locking tests
 */

/** @name The five hooks each locking test implements
 *
 * `test_uti_lock_main.cc` owns the `main()` that starts the threads, waits for
 * them and reports; each locking test supplies only these, so the harness is
 * written once and the tests differ only in what their threads do.
 * @{
 */

/** @brief How many threads this test wants
 * @return the number of threads to start
 */
int get_thread_demand();

/** @brief The function those threads run
 * @return a pointer to the thread function
 */
void *(*get_thread_func())(void *anArg);

/** @brief What to pass each thread
 * @return the argument handed to every thread
 */
void *get_thread_func_arg();

/** @brief Tell the test how many threads were actually started
 * @param count the number started, which may be fewer than requested
 */
void set_thread_count(int count);

/** @brief Decide whether the test passed
 * @param count the number of threads that ran
 * @return 0 when the test passed
 */
int validate(int count);
/** @} */
