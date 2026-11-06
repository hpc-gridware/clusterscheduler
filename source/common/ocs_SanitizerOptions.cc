/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

/** @file
 * @brief Default ASan/LSan options, linked into the daemons when built with a sanitizer
 */

/*
 * ASan-runtime hook.
 *
 * libasan carries a weak default definition of __asan_default_options
 * (`nm -D libasan.so` shows it as W). The strong definition below, in the
 * executable, takes its place through ordinary ELF symbol interposition -
 * the executable comes first in the global lookup scope - and libasan
 * applies what it returns at startup as the process-wide ASan option
 * defaults (still overridable via the ASAN_OPTIONS environment variable).
 * That is why the definition has to be in the executable: one in a
 * plugin opened with RTLD_LOCAL is never seen.
 *
 * We turn off detect_odr_violation because libsgeobj is a static library
 * and its runtime data (CULL type descriptors: nmv[], JB_Type[], and
 * every other lDescr[] instantiated via LISTDEF in sge_all_listsL.h) is
 * duplicated into every output that links sgeobj — the main binary AND
 * every dlopen'd spool plugin (libspoolc.so, libspoolb.so, ...). Each
 * object keeps its own copy, which strict ODR checking rejects. The copies
 * of the plugin are not used, though: the executables are linked -rdynamic,
 * so the references of the plugin, opened with RTLD_LOCAL at
 * source/libs/spool/dynamic/sge_spooling_dynamic.cc, bind to the copies of
 * the executable. The loader checks that for the CULL descriptor registry
 * and refuses a plugin bound to its own (CS-2997, N-A-22), because the
 * identity of shared descriptors matters across the boundary. Every other
 * ASan check stays active.
 *
 * This file is compiled directly into every binary that dlopens the
 * spooling plugins: sge_qmaster (source/daemons/qmaster/CMakeLists.txt),
 * spoolinit / spooldefaults / spooledit (source/utilbin/CMakeLists.txt) and
 * test_cull_plugin (test/libs/cull/CMakeLists.txt). Add it to any new binary
 * that also dlopens sgeobj-consuming shared libraries.
 */
/* Clang provides __has_feature; GCC doesn't. Define it as a no-op fallback
 * so the #if below is well-formed on GCC. Just guarding with
 * defined(__has_feature) is NOT enough — the preprocessor still parses the
 * whole expression, so __has_feature(address_sanitizer) becomes
 * 0(address_sanitizer) after undefined-identifier substitution and the
 * compile errors out even when ASan is off.
 */
#ifndef __has_feature
/** @brief Stand-in for the clang feature test on compilers that lack it
 * @param x the feature being asked about; the answer is always "no"
 */
#  define __has_feature(x) 0
#endif

#if defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer)
extern "C" const char *
__asan_default_options() {
   return "detect_odr_violation=0";
}
#endif
