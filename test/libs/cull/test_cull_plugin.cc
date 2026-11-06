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
 * @brief Module test: one descriptor registry across the plugin boundary (CS-2997, SHD-05 at unit level)
 *
 * The spooling plugins link CULL statically and are loaded with dlopen(RTLD_LOCAL).
 * Their code must use the registry of the loading process, else descriptors are
 * built twice and released into the wrong registry (N-A-22). The loader of the
 * spooling methods checks that with ocs_cull_descr_registry_shared() and refuses
 * a plugin that fails. This test loads two modules built like a plugin, one bound
 * to the CULL of this executable and one bound to its own copy, and runs the same
 * check on both.
 */

#include <dlfcn.h>
#include <string>
#include <unistd.h>

#include "cull/cull.h"
#include "cull/ocs_cull_descr.h"

#include "ocs_module_test.h"

/// a reduced descriptor both sides intern
static const lDescr RED[] = {{1, lUlongT | CULL_IS_REDUCED}, {NoName, lEndT}};

/**
 * @brief The path of a test module, next to this executable
 * @param name the file name of the module
 * @return the absolute path, or name alone if the executable cannot be found
 */
static std::string module_path(const char *name) {
   char exe[4096];
   const ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
   if (n <= 0) {
      return name;
   }
   std::string dir(exe, n);
   return dir.substr(0, dir.rfind('/') + 1) + name;
}

/** @brief What one test module showed */
struct module_result {
   bool loaded{false};       ///< dlopen() succeeded
   bool shared{false};       ///< ocs_cull_descr_registry_shared() returned true
   std::string own;          ///< the object holding the registry of this executable
   std::string seen;         ///< the object holding the registry the module uses
   bool same_descr{false};   ///< the module interned the same descriptor as this executable
   size_t growth{0};         ///< entries the module added to the registry of this executable
};

/**
 * @brief Load a test module, check its registry and intern through it
 * @param name the file name of the module
 * @return what the module showed
 */
static module_result probe_module(const char *name) {
   module_result r;
   // as the loader of the spooling methods does; RTLD_NODELETE also keeps the registry of the split module reachable
   void *handle = dlopen(module_path(name).c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);
   if (handle == nullptr) {
      printf("dlopen %s: %s\n", name, dlerror());
      return r;
   }
   r.loaded = true;
   r.shared = ocs_cull_descr_registry_shared(handle, &r.own, &r.seen);
   printf("%s: own registry in %s, module uses the one in %s\n", name, r.own.c_str(), r.seen.c_str());

   using intern_func = const lDescr *(*)(const lDescr *);
   using release_func = void (*)(const lDescr **);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
   const auto intern = reinterpret_cast<intern_func>(dlsym(handle, "test_cull_plugin_intern"));
   const auto release = reinterpret_cast<release_func>(dlsym(handle, "test_cull_plugin_release"));
#pragma GCC diagnostic pop
   if (intern != nullptr && release != nullptr) {
      const size_t base = ocs_cull_descr_registry_size();
      const lDescr *mine = ocs_cull_descr_intern(RED);
      const lDescr *theirs = intern(RED);
      r.same_descr = mine != nullptr && mine == theirs;
      r.growth = ocs_cull_descr_registry_size() - base;
      release(&theirs);
      ocs_cull_descr_release(&mine);
   }
   dlclose(handle);
   return r;
}

/** @brief PLUGIN-01: a module built like a spooling plugin uses the registry of the process */
static void test_shared() {
   const module_result r = probe_module("test_cull_plugin_shared.so");
   CHECK(1, "the module is loaded", r.loaded);
   CHECK(2, "the loader check passes and names one object for both", r.shared && r.own == r.seen);
   CHECK(3, "the module and the executable intern one descriptor into one registry", r.same_descr && r.growth == 1);
}

/** @brief PLUGIN-02: a module bound to its own CULL is caught */
static void test_split() {
   const module_result r = probe_module("test_cull_plugin_split.so");
   CHECK(4, "the module is loaded", r.loaded);
   CHECK(5, "the loader check fails and names the module as the holder of its registry",
         !r.shared && r.seen.find("test_cull_plugin_split.so") != std::string::npos);
   CHECK(6, "the module builds its own descriptor in its own registry", !r.same_descr && r.growth == 1);
}

int main(int argc, char *argv[]) {
   return ocs::module_test::run(argc, argv, {
      {"PLUGIN-01", "one registry with a plugin",
       "a library that links CULL statically and is loaded with RTLD_LOCAL uses the registry of the process",
       {"load a module built like a spooling plugin", "run the loader check on it",
        "intern one descriptor through the module and through the executable"},
       "the check passes and both get the same descriptor from one registry",
       {{"SHD-05", "unit level"}}, {1, 2, 3}, test_shared},
      {"PLUGIN-02", "a plugin with its own registry",
       "the loader check detects a library whose code is bound to its own copy of CULL",
       {"load a module linked with -Bsymbolic", "run the loader check on it",
        "intern one descriptor through the module and through the executable"},
       "the check fails and names the module, and the module builds its own descriptor",
       {{"SHD-05", "unit level"}}, {4, 5, 6}, test_split},
   });
}
