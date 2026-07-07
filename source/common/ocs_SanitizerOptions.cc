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

/*
 * ASan-runtime hook.
 *
 * libasan resolves __asan_default_options via dlsym(RTLD_DEFAULT, ...) at
 * startup and applies whatever it returns as the process-wide ASan option
 * defaults (still overridable via the ASAN_OPTIONS environment variable).
 *
 * We turn off detect_odr_violation because libsgeobj is a static library
 * and its runtime data (CULL type descriptors: nmv[], JB_Type[], and
 * every other lDescr[] instantiated via LISTDEF in sge_all_listsL.h) is
 * duplicated into every output that links sgeobj — the main binary AND
 * every dlopen'd spool plugin (libspoolc.so, libspool_berkeleydb.so,
 * ...). Linux dlopen at source/libs/spool/dynamic/sge_spooling_dynamic.cc
 * uses RTLD_LOCAL, so the dynamic linker does NOT merge these copies at
 * runtime. The duplication is benign — all copies hold identical bytes
 * and no code compares descriptor-pointer identity across the plugin /
 * main-binary boundary — but strict ODR checking rejects it. Every other
 * ASan check stays active.
 *
 * This file is compiled directly into every binary that dlopens the
 * spooling plugins: sge_qmaster (source/daemons/qmaster/CMakeLists.txt)
 * and spoolinit / spooldefaults / spooledit (source/utilbin/CMakeLists.txt).
 * Add it to any new binary that also dlopens sgeobj-consuming shared
 * libraries.
 */
/* Clang provides __has_feature; GCC doesn't. Define it as a no-op fallback
 * so the #if below is well-formed on GCC. Just guarding with
 * defined(__has_feature) is NOT enough — the preprocessor still parses the
 * whole expression, so __has_feature(address_sanitizer) becomes
 * 0(address_sanitizer) after undefined-identifier substitution and the
 * compile errors out even when ASan is off.
 */
#ifndef __has_feature
#  define __has_feature(x) 0
#endif

#if defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer)
extern "C" const char *
__asan_default_options() {
   return "detect_odr_violation=0";
}
#endif
