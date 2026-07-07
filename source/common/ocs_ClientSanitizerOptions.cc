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
 * LSan-runtime hook.
 *
 * libasan (which embeds LSan) resolves __lsan_default_options via
 * dlsym(RTLD_DEFAULT, ...) at startup. We turn off detect_leaks in client
 * tools because those binaries run once, do their work, and exit without
 * unwinding the CULL master lists, packed buffers, GDI contexts, and
 * every other object that persists for the tool's lifetime. Every one of
 * those looks like a leak to LSan even though the OS reclaims all of it
 * immediately on exit — noise that drowns out real bugs.
 *
 * This is a "we chose not to clean up before exit" concession, NOT "leaks
 * don't matter." Keep this file OUT of the daemons (sge_qmaster,
 * sge_execd, sge_shepherd, sge_shadowd) — shutdown-time leaks in those
 * ARE real bugs (see e.g. the pgdb / bdb pgdb_destroy / bdb_destroy
 * fixes that added missing spool_info cleanup on qmaster shutdown).
 *
 * Every other ASan / UBSan check stays active in the client tools; only
 * the atexit leak sweep is skipped.
 *
 * The guard mirrors ocs_SanitizerOptions.cc — see that file for the
 * explanation of why `defined(__has_feature) && __has_feature(...)`
 * alone is not sufficient on GCC.
 */
#ifndef __has_feature
#  define __has_feature(x) 0
#endif

#if defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer)
extern "C" const char *
__lsan_default_options() {
   return "detect_leaks=0";
}
#endif
