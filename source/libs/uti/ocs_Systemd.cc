/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025-2026 HPC-Gridware GmbH
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

#if defined(OCS_WITH_SYSTEMD)
#include <filesystem>
#include <fstream>
#include <dlfcn.h>

#include "sge_bootstrap_env.h"
#include "sge_log.h"
#include "sge_rmon_macros.h"
#include "sge_time.h"

#include "msg_utilib.h"
#include "ocs_Systemd.h"

namespace ocs::uti {
   static const int NUM_SD_BUS_RETRIES = 5; // Number of retries for sd_bus_* methods after EINTR

   //================================================================================
   // static members
   void *ocs::uti::Systemd::lib_handle = nullptr;
   sd_bus_open_system_func_t Systemd::sd_bus_open_system_func = nullptr;
   sd_bus_unref_func_t Systemd::sd_bus_unref_func = nullptr;
   sd_bus_call_method_func_t Systemd::sd_bus_call_method_func = nullptr;
   sd_bus_message_read_func_t Systemd::sd_bus_message_read_func = nullptr;
   sd_bus_message_new_method_call_func_t Systemd::sd_bus_message_new_method_call_func = nullptr;
   sd_bus_message_unref_func_t Systemd::sd_bus_message_unref_func = nullptr;
   sd_bus_message_append_func_t Systemd::sd_bus_message_append_func = nullptr;
   sd_bus_message_append_array_func_t Systemd::sd_bus_message_append_array_func = nullptr;
   sd_bus_message_open_container_func_t Systemd::sd_bus_message_open_container_func = nullptr;
   sd_bus_message_close_container_func_t Systemd::sd_bus_message_close_container_func = nullptr;
   sd_bus_call_func_t Systemd::sd_bus_call_func = nullptr;
   sd_bus_add_match_func_t Systemd::sd_bus_add_match_func = nullptr;
   sd_bus_match_signal_func_t Systemd::sd_bus_match_signal_func = nullptr;
   sd_bus_slot_unref_func_t Systemd::sd_bus_slot_unref_func = nullptr;
   sd_bus_process_func_t Systemd::sd_bus_process_func = nullptr;
   sd_bus_wait_func_t Systemd::sd_bus_wait_func = nullptr;
   sd_bus_message_get_member_func_t Systemd::sd_bus_message_get_member_func = nullptr;
   sd_bus_message_get_sender_func_t Systemd::sd_bus_message_get_sender_func = nullptr;
   sd_bus_path_encode_func_t Systemd::sd_bus_path_encode_func = nullptr;
   sd_bus_get_property_func_t Systemd::sd_bus_get_property_func = nullptr;
   sd_bus_error_free_func_t Systemd::sd_bus_error_free_func = nullptr;

   std::string Systemd::slice_name{};
   std::string Systemd::service_name{};
   bool Systemd::running_as_service{false};
   int Systemd::cgroup_version{};
   int Systemd::systemd_version{};

   const std::string Systemd::execd_service_name{"execd.service"};
   const std::string Systemd::shepherd_scope_name{"shepherds.scope"};
   std::map<std::string, bool> Systemd::unclear_properties;

   // @todo move somewhere else
   static std::string
   get_slice_file_name() {
      std::string ret = bootstrap_get_sge_root();
      ret += "/";
      ret += bootstrap_get_sge_cell();
      ret += "/common/slice_name";
      return ret;
   }

   // @todo move somewhere else
   // @todo add a dstring for reporting errors?
   static bool
   read_one_line_file(const std::string &file_name, std::string &line) {
      bool ret = false;

      if (std::filesystem::exists(file_name)) {
         std::ifstream file(file_name);
         if (file.is_open()) {
            if (std::getline(file, line)) {
               ret = true;
            }
            file.close();
         }
      }

      return ret;
   }

   // ================================================================================
   // @brief Initialize the Systemd class
   //
   // This function loads the systemd shared library and retrieves the function pointers
   // for the sd-bus API functions. It checks for errors during loading and function
   // retrieval, and returns a boolean indicating success or failure.
   //
   // @param error_dstr A pointer to a dstring where error messages will be stored
   // @return true if initialization was successful, false otherwise
   // @note This function should be called before using any other methods of the Systemd class.
   // @note Must be root when calling this function (in our daemons before switching to admin user).
   bool
   Systemd::initialize(const std::string &service_name_in, dstring *error_dstr) {
      DENTER(TOP_LAYER);
      bool ret = true;

      if (std::filesystem::exists("/sys/fs/cgroup/systemd")) {
         // cgroup v1 is available
         cgroup_version = 1;
      } else if (std::filesystem::exists("/sys/fs/cgroup/system.slice")) {
         // cgroup v2 is available
         cgroup_version = 2;
      } else {
         sge_dstring_sprintf(error_dstr, SFNMAX, MSG_SYSTEMD_CANNOT_DETECT_CGROUP_VERSION);
         ret = false;
      }
      DPRINTF("==> cgroup version: %d\n", cgroup_version);

      // To use systemd, we need to be root (for writing operations)
      if (ret && getuid() != 0) {
         sge_dstring_sprintf(error_dstr, SFNMAX, MSG_SYSTEMD_NOT_ROOT);
         ret = false;
      }

      // initialize only once
      if (ret && lib_handle != nullptr) {
         sge_dstring_sprintf(error_dstr, SFNMAX, MSG_SYSTEMD_ALREADY_INITIALIZED);
         ret = false;
      }

      // Load the shared library and the required functions
      if (ret) {
         const char *libsystemd = "libsystemd.so.0";
         lib_handle = dlopen(libsystemd, RTLD_LAZY);
         if (lib_handle == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_OPEN_LIB_SS, libsystemd, dlerror());
            ret = false;
         }
      }

      // load the functions
      const char *func;
      if (ret) {
         func = "sd_bus_open_system";
         sd_bus_open_system_func = reinterpret_cast<sd_bus_open_system_func_t>(dlsym(lib_handle, func));
         if (sd_bus_open_system_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         // func = "sd_bus_unref";
         // safer is sd_bus_flush_close_unref()
         func = "sd_bus_flush_close_unref";
         sd_bus_unref_func = reinterpret_cast<sd_bus_unref_func_t>(dlsym(lib_handle, func));
         if (sd_bus_unref_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_call_method";
         sd_bus_call_method_func = reinterpret_cast<sd_bus_call_method_func_t>(dlsym(lib_handle, func));
         if (sd_bus_unref_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_message_read";
         sd_bus_message_read_func = reinterpret_cast<sd_bus_message_read_func_t>(dlsym(lib_handle, func));
         if (sd_bus_unref_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }

      if (ret) {
         func = "sd_bus_message_new_method_call";
         sd_bus_message_new_method_call_func = reinterpret_cast<sd_bus_message_new_method_call_func_t>(dlsym(lib_handle, func));
         if (sd_bus_message_new_method_call_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_message_unref";
         sd_bus_message_unref_func = reinterpret_cast<sd_bus_message_unref_func_t>(dlsym(lib_handle, func));
         if (sd_bus_message_unref_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_message_append";
         sd_bus_message_append_func = reinterpret_cast<sd_bus_message_append_func_t>(dlsym(lib_handle, func));
         if (sd_bus_message_append_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_message_append_array";
         sd_bus_message_append_array_func = reinterpret_cast<sd_bus_message_append_array_func_t>(dlsym(lib_handle, func));
         if (sd_bus_message_append_array_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_message_open_container";
         sd_bus_message_open_container_func = reinterpret_cast<sd_bus_message_open_container_func_t>(dlsym(lib_handle, func));
         if (sd_bus_message_open_container_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_message_close_container";
         sd_bus_message_close_container_func = reinterpret_cast<sd_bus_message_close_container_func_t>(dlsym(lib_handle, func));
         if (sd_bus_message_close_container_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_call";
         sd_bus_call_func = reinterpret_cast<sd_bus_call_func_t>(dlsym(lib_handle, func));
         if (sd_bus_call_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_add_match";
         sd_bus_add_match_func = reinterpret_cast<sd_bus_add_match_func_t>(dlsym(lib_handle, func));
         if (sd_bus_add_match_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_match_signal";
         sd_bus_match_signal_func = reinterpret_cast<sd_bus_match_signal_func_t>(dlsym(lib_handle, func));
         if (sd_bus_match_signal_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_slot_unref";
         sd_bus_slot_unref_func = reinterpret_cast<sd_bus_slot_unref_func_t>(dlsym(lib_handle, func));
         if (sd_bus_slot_unref_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_process";
         sd_bus_process_func = reinterpret_cast<sd_bus_process_func_t>(dlsym(lib_handle, func));
         if (sd_bus_process_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_wait";
         sd_bus_wait_func = reinterpret_cast<sd_bus_wait_func_t>(dlsym(lib_handle, func));
         if (sd_bus_wait_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_message_get_sender";
         sd_bus_message_get_sender_func = reinterpret_cast<sd_bus_message_get_sender_func_t>(dlsym(lib_handle, func));
         if (sd_bus_message_get_sender_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_message_get_member";
         sd_bus_message_get_member_func = reinterpret_cast<sd_bus_message_get_member_func_t>(dlsym(lib_handle, func));
         if (sd_bus_message_get_member_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_path_encode";
         sd_bus_path_encode_func = reinterpret_cast<sd_bus_path_encode_func_t>(dlsym(lib_handle, func));
         if (sd_bus_path_encode_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_get_property";
         sd_bus_get_property_func = reinterpret_cast<sd_bus_get_property_func_t>(dlsym(lib_handle, func));
         if (sd_bus_get_property_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_error_free";
         sd_bus_error_free_func = reinterpret_cast<sd_bus_error_free_func_t>(dlsym(lib_handle, func));
         if (sd_bus_error_free_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }

      if (ret) {
         // We will need the slice name for the service name, read it once.
         // If we have a slice name, we can figure out if we are running as a service.
         service_name = service_name_in;
         std::string slice_file_name = get_slice_file_name();
         ret = read_one_line_file(slice_file_name, slice_name);
         if (ret) {
            // build service name, e.g. "ocs6444-execd.service"
            std::string full_service_name = slice_name + "-" + service_name;
            // get unit path of service
            Systemd systemd;
            if (systemd.connect(error_dstr)) {
               // get systemd version
               std::string systemd_version_str;
               systemd.sd_bus_get_property("Manager", "", "Version", systemd_version_str, error_dstr);
               systemd_version = std::stoi(systemd_version_str);
               DPRINTF("systemd version: %d", systemd_version);

               std::string service_unit_path;
               bool have_service_unit = systemd.sd_bus_method_s_o("GetUnit", full_service_name, service_unit_path, error_dstr);
               if (have_service_unit) {
                  DPRINTF("have_service_unit: %s", service_unit_path.c_str());
                  // get the unit path of this process
                  std::string pid_unit_path;
                  bool have_pid_unit = systemd.sd_bus_method_u_o("GetUnitByPID", getpid(), pid_unit_path, error_dstr);
                  // compare both unit paths, if they are equal then we are running as service
                  if (have_pid_unit) {
                     DPRINTF("have_pid_unit: %s", pid_unit_path.c_str());
                     if (service_unit_path.compare(pid_unit_path) == 0) {
                        running_as_service = true;
                        DPRINTF("we are running as systemd service: %s", service_unit_path.c_str());
                     }
                  } else {
                     DPRINTF("could not get unit path for pid %d: %s", getpid(), sge_dstring_get_string(error_dstr));
                     // this is OK, we might not be running as a service
                  }
               } else {
                  DPRINTF("could not get unit path for service %s: %s", full_service_name.c_str(), sge_dstring_get_string(error_dstr));
                  // this is OK, we might not be running as a service
               }
            } else {
               DPRINTF("cannot connect to systemd: %s", sge_dstring_get_string(error_dstr));
               ret = false;
            }
         } else {
            DPRINTF("slice name file " SFQ "does not exist or is not readable", slice_file_name.c_str());
         }
      }

      // if we could not load the library or the functions,
      // or we cannot connect to the systemd,
      // then close the library
      if (!ret && lib_handle != nullptr) {
         dlclose(lib_handle);
         lib_handle = nullptr;
      }

      DPRINTF("Systemd::initialize() returning %d\n", ret);
      return ret;
   }

   /*!
    * @brief Check if systemd is available
    *
    * This function checks if the systemd library is loaded and available for use.
    * It returns true if the library is loaded, otherwise false.
    *
    * @return true if systemd is available, false otherwise
    */
   bool
   Systemd::is_systemd_available() {
      // Check if systemd is available
      return lib_handle != nullptr;
   }

   /*!
    * @brief Check if we are running as a systemd service
    *
    * This function checks if the current process is running as a systemd service.
    * It returns true if the process is running as a service, otherwise false.
    *
    * @return true if running as a service, false otherwise
    */
   bool
   Systemd::is_running_as_service() {
      return running_as_service;
   }

   /*!
    * @brief Check if a property is available for a given scope
    *
    * This function checks if a specific property is available for a given scope.
    * It caches the result to avoid repeated checks for the same property.
    * We test on a job scope (of the first job running after sge_execd starts). So far all jobs
    * have the same accounting settings, so we can cache the result.
    *
    * @param property_name The name of the property to check
    * @param scope_name The name of the job scope to check the property against
    * @return true if the property is available, false otherwise
    */
   bool
   Systemd::has_property(const std::string &property_name, const std::string &scope_name) {
      DENTER(TOP_LAYER);

      // Check if the property is already cached
      bool ret = false;
      if (unclear_properties.contains(property_name)) {
         ret = unclear_properties[property_name];
      } else {
         // we do not yet know if this property is available
         uint64_t value;
         DSTRING_STATIC(error_dstr, MAX_STRING_SIZE);
         bool not_exists = false;
         if (sd_bus_get_property("Scope", scope_name, property_name, value, &error_dstr, &not_exists)) {
            DPRINTF("property %s is reported by systemd", property_name.c_str());
            unclear_properties[property_name] = true;
            ret = true;
         } else if (not_exists) {
            DPRINTF("property %s is not reported by systemd", property_name.c_str());
            unclear_properties[property_name] = false;
         } else {
            DPRINTF("retrieving property %s for scope %s failed: %s",
                    property_name.c_str(), scope_name.c_str(), sge_dstring_get_string(&error_dstr));
            DPRINTF("we do not know if it would be available or not, so we assume it is unclear");
         }
      }

      DRETURN(ret);
   }

   //================================================================================
   // instance methods

   /*!
    * @brief Constructor for the Systemd class
    *
    * This constructor initializes the Systemd object and sets the bus pointer to nullptr.
    */
   Systemd::Systemd()
      : bus(nullptr) {
   }

   /*!
    * @brief Destructor for the Systemd class
    *
    * This destructor cleans up the Systemd object by unreferencing the bus pointer
    * if it is not nullptr.
    */
   Systemd::~Systemd() {
      // Destructor implementation
      if (bus != nullptr) {
         sd_bus_unref_func(bus);
         bus = nullptr;
      }
   }

   /*!
    * @brief Connect to the systemd bus
    *
    * This function attempts to connect to the systemd bus using the sd_bus_open_system_func.
    * If the connection is successful, it returns true. If there is an error, it sets the
    * error_dstr with an appropriate error message and returns false.
    *
    * @param error_dstr A pointer to a dstring where error messages will be stored
    * @return true if the connection was successful, false otherwise
    */
   bool
   Systemd::connect(dstring *error_dstr) {
      bool ret = true;

      int r = sd_bus_open_system_func(&bus);
      if (r < 0) {
         sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CONNECT_IS, r, strerror(-r));
         ret = false;
      }

      return ret;
   }

   /*!
    * @brief Check if the systemd bus is connected
    *
    * This function checks if the bus pointer is not nullptr, indicating that a connection
    * to the systemd bus has been established.
    *
    * @return true if connected to the systemd bus, false otherwise
    */
   bool
   Systemd::connected() const {
      return bus != nullptr;
   }

   /*!
    * @brief Call a systemd method with a string input and get an object output
    *
    * This function calls a systemd method with a string input and retrieves an object output.
    * An object is represented as a string in this context, which is the path to the object.
    * It handles retries in case of EINTR errors and returns true if the call was successful,
    * otherwise it sets the error_dstr with an appropriate error message.
    *
    * @param method The name of the systemd method to call
    * @param input The input string to pass to the method
    * @param output A reference to a string where the output will be stored
    * @param error_dstr A pointer to a dstring where error messages will be stored
    * @return true if the method call was successful, false otherwise
    */
   bool
   Systemd::sd_bus_method_s_o(const std::string &method, std::string &input, std::string &output, dstring *error_dstr) const {
      bool ret = true;

      bool retry_on_interrupt = true;        // retry on EINTR
      int retries = 0;
      while (ret && retry_on_interrupt) {
         retry_on_interrupt = false;
         sd_bus_error error = SD_BUS_ERROR_NULL;
         sd_bus_message *m = nullptr;
         int r = sd_bus_call_method_func(bus, "org.freedesktop.systemd1",  // service to contact
                                  "/org/freedesktop/systemd1",          // object path
                                  "org.freedesktop.systemd1.Manager",   // interface name
                                  method.c_str(),                       // method name
                                  &error,                               // object to return error in
                                  &m,                                   // return message on success
                                  "s",                                  // input signature
                                  input.c_str());                       // first argument
         if (r < 0) {
            if (-r == EINTR && retries++ < NUM_SD_BUS_RETRIES) {
               retry_on_interrupt = true;
            } else {
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CALL_SSIS, method.c_str(), input.c_str(), r, error.message);
               if (retries > 0) {
                  sge_dstring_sprintf_append(error_dstr, MSG_SYSTEMD_AFTER_RETRIES_I, retries);
               }
               ret = false;
            }
         } else {
            const char *result = nullptr;
            r = sd_bus_message_read_func(m, "o", &result);
            if (r < 0) {
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_READ_RESULT_SISS, method.c_str(), r, error.message);
               ret = false;
            } else {
               if (result == nullptr) {
                  sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_EMPTY_RESULT_S, method.c_str());
                  ret = false;
               }
               output = result;
            }
         }
         sd_bus_error_free_func(&error);
         sd_bus_message_unref_func(m);
      }

      return ret;
   }

   /*!
    * @brief Call a systemd method with a uint32_t input and get an object output
    *
    * This function calls a systemd method with a uint32_t input and retrieves an object output.
    * An object is represented as a string in this context, which is the path to the object.
    * It handles retries in case of EINTR errors and returns true if the call was successful,
    * otherwise it sets the error_dstr with an appropriate error message.
    *
    * @param method The name of the systemd method to call
    * @param input The input uint32_t to pass to the method
    * @param output A reference to a string where the output will be stored
    * @param error_dstr A pointer to a dstring where error messages will be stored
    * @return true if the method call was successful, false otherwise
    */
   bool
   Systemd::sd_bus_method_u_o(const std::string &method, uint32_t input, std::string &output, dstring *error_dstr) const {
      bool ret = true;

      bool retry_on_interrupt = true;        // retry on EINTR
      int retries = 0;
      while (ret && retry_on_interrupt) {
         retry_on_interrupt = false;
         sd_bus_error error = SD_BUS_ERROR_NULL;
         sd_bus_message *m = nullptr;
         int r = sd_bus_call_method_func(bus, "org.freedesktop.systemd1",  // service to contact
                                  "/org/freedesktop/systemd1",          // object path
                                  "org.freedesktop.systemd1.Manager",   // interface name
                                  method.c_str(),                       // method name
                                  &error,                               // object to return error in
                                  &m,                                   // return message on success
                                  "u",                                  // input signature
                                 input);                                // first argument
         if (r < 0) {
            if (-r == EINTR && retries++ < NUM_SD_BUS_RETRIES) {
               retry_on_interrupt = true;
            } else {
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CALL_SSIS, method.c_str(), std::to_string(input), r, error.message);
               if (retries > 0) {
                  sge_dstring_sprintf_append(error_dstr, MSG_SYSTEMD_AFTER_RETRIES_I, retries);
               }
               ret = false;
            }
         } else {
            const char *result = nullptr;
            r = sd_bus_message_read_func(m, "o", &result);
            if (r < 0) {
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_READ_RESULT_SISS, method.c_str(), r, error.message);
               ret = false;
            } else {
               if (result == nullptr) {
                  sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_EMPTY_RESULT_S, method.c_str());
                  ret = false;
               }
               output = result;
            }
         }
         sd_bus_error_free_func(&error);
         sd_bus_message_unref_func(m);
      }

      return ret;
   }

   /*!
    * @brief Move a shepherd process to a systemd scope
    *
    * This function moves a shepherd process identified by its PID to a systemd scope.
    * It first checks if the scope exists, and if not, it creates it. If the scope already
    * exists, it attaches the shepherd process to it. It handles retries in case of EINTR errors
    * and returns true if the operation was successful, otherwise it sets the error_dstr with an
    * appropriate error message.
    *
    * @param pid The PID of the shepherd process to move
    * @param error_dstr A pointer to a dstring where error messages will be stored
    * @return true if the operation was successful, false otherwise
    */
   bool
   Systemd::move_shepherd_to_scope(pid_t pid, dstring *error_dstr) const {
      DENTER(TOP_LAYER);

      bool ret = true;
      bool create = false;
      std::string full_scope_name = slice_name + "-" + "shepherds.scope";
      std::string full_slice_name = slice_name + ".slice";

      bool retry_on_interrupt = true;        // retry on EINTR
      int retries = 0;
      while (ret && retry_on_interrupt) {
         retry_on_interrupt = false;
         DPRINTF("Systemd::move_shepherd_to_scope: Calling GetUnit\n");
         sd_bus_message *m = nullptr;
         sd_bus_error error = SD_BUS_ERROR_NULL;
         // search the unit (full_scope_name)
         // if it does not exist, then call StartTransientUnit
         // if it exists, then call AttachProcessesToUnit
         // if AttachProcessesToUnit fails with -2 (ENOENT), we might have run into a race condition:
         //    the unit was removed just between GetUnit and AttachProcessesToUnit.
         //    then call StartTransientUnit which will re-create the unit
         int r = sd_bus_call_method_func(bus, "org.freedesktop.systemd1",  // service to contact
                                     "/org/freedesktop/systemd1",          // object path
                                     "org.freedesktop.systemd1.Manager",   // interface name
                                     "GetUnit",                            // method name
                                     &error,                               // object to return error in
                                     &m,                                   // return message on success
                                     "s",                                  // input signature
                                     full_scope_name.c_str());             // input argument
         if (r < 0) {
            // ENOENT (-2): scope does not exist
            if (-r == ENOENT) {
               DPRINTF("Systemd::move_shepherd_to_scope: scope does not exist, create it\n");
               create = true;
            } else if (-r == EINTR && retries < NUM_SD_BUS_RETRIES) {
               retry_on_interrupt = true;
            } else {
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CALL_SSIS, "GetUnit", full_scope_name.c_str(), r, error.message);
               if (retries > 0) {
                  sge_dstring_sprintf_append(error_dstr, MSG_SYSTEMD_AFTER_RETRIES_I, retries);
               }
               ret = false;
            }
         }

         sd_bus_error_free_func(&error);
         sd_bus_message_unref_func(m);
      }

#if 0
      // Enable for debugging the situation, that the scope did not exist, according to GetUnit,
      // but when we try to create it, it suddenly exists.
      // This can happen if sge_execd forks multiple shepherds in short succession,
      // they all see that the unit does not exist, but only one of them can create it, the others fail.
      // Submit an array job to the test host, and you should see the DPRINTF output below.
      srand(pid);
      int microsecs = rand() % 1000000;
      DPRINTF("===> sleeping for %d µsec, then %s shepherd scope %s", microsecs, create ? "creating" : "attaching to", full_scope_name.c_str());
      usleep(microsecs);
#endif

      // the scope exists, we can attach the shepherd to it
      if (ret && !create) {
         DPRINTF("Systemd::move_shepherd_to_scope: calling AttachProcessesToUnit\n");
         bool scope_not_exists = false;
         ret = attach_pid_to_scope(full_scope_name, pid, scope_not_exists, error_dstr);
         if (!ret && scope_not_exists) {
            // The scope vanished between GetUnit and AttachProcessesToUnit,
            // we have to create it again.

            // Clear dstring as we handle the error.
            sge_dstring_clear(error_dstr);
            DPRINTF("Systemd::move_shepherd_to_scope: scope no longer exists, calling StartTransientUnit\n");
            create = true;
         }
      }

      if (ret && create) {
         DPRINTF("Systemd::move_shepherd_to_scope: calling create_scope_with_pid\n");
         SystemdProperties_t properties;
         bool scope_already_exists = false;
         ret = create_scope_with_pid(full_scope_name, full_slice_name, properties, pid, scope_already_exists, error_dstr);
         if (!ret) {
            if (scope_already_exists) {
               // The scope already exists, someone (another shepherd) created it in the meantime.
               // We can try to attach the pid to the existing scope.
               DPRINTF("===> Scope already exists, while GetUnit called earlier said it didn't exist.");

               // Clear dstring as we handle the error.
               sge_dstring_clear(error_dstr);

               bool scope_not_exists = false;
               ret = attach_pid_to_scope(full_scope_name, pid, scope_not_exists, error_dstr);
               // if it fails here again, then there was really a problem
            }
         }
      }

      DRETURN(ret);
   }

   /*!
    * @brief Attach a PID to a systemd scope
    *
    * This function attaches a given PID to a specified systemd scope.
    * If the operation fails as the scope does not exist, it sets the scope_not_exists flag to true and
    * returns false. It handles retries in case of EINTR errors and returns true
    * if the operation was successful, otherwise it sets the error_dstr with an
    * appropriate error message.
    *
    * Background for the scope_not_exists flag: We can run into race conditions:
    * - We call GetUnit to check if the scope exists.
    * - If it exists, we call AttachProcessesToUnit to attach the PID to the scope.
    * - But between GetUnit and AttachProcessesToUnit, the scope might have been removed.
    *
    * @param scope The name of the systemd scope to attach the PID to
    * @param pid The PID to attach to the scope
    * @param scope_not_exists A reference to a boolean that will be set to true if the scope does not exist
    * @param error_dstr A pointer to a dstring where error messages will be stored
    * @return true if the operation was successful, false otherwise
    */
   bool
   Systemd::attach_pid_to_scope(const std::string &scope, pid_t pid, bool &scope_not_exists, dstring *error_dstr) const {
      DENTER(TOP_LAYER);

      bool ret = true;
      scope_not_exists = false;

      DPRINTF("Systemd::attach_pid_to_scope: calling AttachProcessesToUnit\n");
      sd_bus_message *m = nullptr;
      sd_bus_error error = SD_BUS_ERROR_NULL;
      bool retry_on_interrupt = true;        // retry on EINTR
      int retries = 0;
      while (ret && retry_on_interrupt) {
         retry_on_interrupt = false;
         int r = sd_bus_call_method_func(bus, "org.freedesktop.systemd1",  // service to contact
                                  "/org/freedesktop/systemd1",          // object path
                                  "org.freedesktop.systemd1.Manager",   // interface name
                                  "AttachProcessesToUnit",              // method name
                                  &error,                               // object to return error in
                                  &m,                                   // return message on success
                                  "ssau",                               // input signature
                                  scope.c_str(),                        //    -> scope
                                  "",                                   //    -> subcgroup
                                  1, pid);                              // array containing one pid
         if (r < 0) {
            if (-r == ENOENT) {
               scope_not_exists = true;
               DPRINTF("Systemd::attach_pid_to_scope: scope does not exist\n");
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CALL_SSIS, "AttachProcessesToUnit", scope.c_str(), r, error.message);
               ret = false;
            } else if (-r == EINTR && retries++ < NUM_SD_BUS_RETRIES) {
               retry_on_interrupt = true;
            } else {
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CALL_SSIS, "AttachProcessesToUnit", scope.c_str(), r, error.message);
               if (retries > 0) {
                  sge_dstring_sprintf_append(error_dstr, MSG_SYSTEMD_AFTER_RETRIES_I, retries);
               }
               ret = false;
            }
         }

         sd_bus_message_unref_func(m);
         sd_bus_error_free_func(&error);
      }

      DRETURN(ret);
   }

   /*!
    * @brief Create a systemd scope with a given PID
    *
    * This function creates a systemd scope with the specified name and slice,
    * and attaches the given PID to it. It also sets properties for the scope.
    * If the scope already exists, it sets the scope_already_exists flag to true
    * and returns false.
    * It handles retries in case of EINTR errors and returns true if the operation
    * was successful, otherwise it sets the error_dstr with an appropriate error message.
    *
    * Background for the scope_already_exists flag: We can run into race conditions:
    * - We call GetUnit to check if the scope exists.
    * - If it does not exist, we call this function to create it.
    * - But between GetUnit and StartTransientUnit, the scope might have been created by another process,
    *   e.g., a second execd child (sge_shepherd) having been forked and calling move_shepherd_to_scope().
    *
    * @param scope The name of the systemd scope to create
    * @param slice The name of the systemd slice to use
    * @param properties A map of properties to set for the scope
    * @param pid The PID to attach to the scope
    * @param scope_already_exists A reference to a boolean that will be set to true if the scope already exists
    * @param error_dstr A pointer to a dstring where error messages will be stored
    * @return true if the operation was successful, false otherwise
    */
   bool
   Systemd::create_scope_with_pid(const std::string &scope, const std::string &slice,
                                  const SystemdProperties_t &properties, pid_t pid, bool &scope_already_exists,
                                  dstring *error_dstr) const {
      DENTER(TOP_LAYER);

      bool ret = true;

      DPRINTF("Systemd::move_shepherd_to_scope: calling StartTransientUnit\n");

      // StartTransientUnit will start a systemd job, we have to wait for it to finish
      // @todo AI claims that this is only needed with systemd version >= 239, but is this correct?
      //          from man page: sd_bus_wait() was added in version 240.
      //          but older shared libs have it already - so with which version shall we start to use it?
      sd_bus_slot *slot = nullptr;
      if (systemd_version >= 240) {
         slot = sd_bus_wait_for_job_subscribe("JobRemoved", error_dstr);
         if (slot == nullptr) {
            ret = false;
         }
      }

      sd_bus_message *m = nullptr;
      int r;
      if (ret) {
         // build the method step by step as we add arrays
         r = sd_bus_message_new_method_call_func(bus, &m,
                 "org.freedesktop.systemd1",          // service to contact
                 "/org/freedesktop/systemd1",         // object path
                 "org.freedesktop.systemd1.Manager",  // interface name
                 "StartTransientUnit");       // method name
         if (r < 0) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CREATE_MESSAGE_CALL_SIS, "StartTransientUnit", r, strerror(-r));
            ret = false;
         }
      }

      if (ret) {
         r = sd_bus_message_append_func(m, "ss", scope.c_str(), "fail");
         if (r < 0) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_APPEND_TO_MESSAGE_SSIS, "name and mode", "StartTransientUnit", r, strerror(-r));
            ret = false;
         }
      }

      // we add an array of properties (which are of type struct containing a string and a variant)
      if (ret) {
         r = sd_bus_message_open_container_func(m, SD_BUS_TYPE_ARRAY, "(sv)");
         if (r < 0) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_OPEN_CONTAINER_SSIS, "properties", "StartTransientUnit", r, strerror(-r));
            ret = false;
         }
      }

      if (ret) {
         r = sd_bus_message_append_func(m, "(sv)", "Delegate", "b", 1);
         if (r < 0) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_APPEND_PROPERTY_SSIS, "Delegate", "StartTransientUnit", r, strerror(-r));
            ret = false;
         }
      }
      if (ret) {
         r = sd_bus_message_append_func(m, "(sv)", "Slice", "s", slice.c_str());
         if (r < 0) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_APPEND_PROPERTY_SSIS, "Slice", "StartTransientUnit", r, strerror(-r));
            ret = false;
         }
      }
      if (ret) {
         r = sd_bus_message_append_func(m, "(sv)", "PIDs", "au", 1, pid);
         if (r < 0) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_APPEND_PROPERTY_SSIS, "PIDs", "StartTransientUnit", r, strerror(-r));
            ret = false;
         }
      }

      // add properties, e.g. "MemoryMax" or "IOReadBandwidthMax
      // @todo need to catch bad_variant_access exception? Not really needed here, as we know the types, but to be on the safe side?
      if (ret && properties.size() > 0) {
         for (auto const& [key, value] : properties) {
            if (ret) {
               // open the struct (sv)
               r = sd_bus_message_open_container_func(m, SD_BUS_TYPE_STRUCT, "sv");
               if (r < 0) {
                  sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_OPEN_CONTAINER_SSIS, "one property struct", "StartTransientUnit", r, strerror(-r));
                  ret = false;
               }
               if (ret) {
                  r = sd_bus_message_append_func(m, "s", key.c_str());
                  if (r < 0) {
                     sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_APPEND_PROPERTY_SSIS, "one property key", "StartTransientUnit", r, strerror(-r));
                     ret = false;
                  }
               }
               if (ret) {
                  switch (value.index()) {
                     case 0: // std::string
                        r = sd_bus_message_open_container_func(m, SD_BUS_TYPE_VARIANT, "s");
                        break;
                     case 1: // uint64_t
                        r = sd_bus_message_open_container_func(m, SD_BUS_TYPE_VARIANT, "t");
                        break;
                     case 2: // bool
                        r = sd_bus_message_open_container_func(m, SD_BUS_TYPE_VARIANT, "b");
                        break;
                     case 3: // std::vector<uint8_t>
                        r = sd_bus_message_open_container_func(m, SD_BUS_TYPE_VARIANT, "ay");
                        break;
                     case 4: // SystemdDevice_t (struct of two strings, device and mode)
                        r = sd_bus_message_open_container_func(m, SD_BUS_TYPE_VARIANT, "a(ss)");
                        break;
                     default:
                        r = -EINVAL; // invalid type
                        break;
                  }
                  if (r < 0) {
                     sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_OPEN_CONTAINER_SSIS, "one property variant", "StartTransientUnit", r, strerror(-r));
                     ret = false;
                  }
               }
               if (ret) {
                  switch (value.index()) {
                     case 0: // std::string
                        r = sd_bus_message_append_func(m, "s", std::get<std::string>(value).c_str());
                        break;
                     case 1: // uint64_t
                        r = sd_bus_message_append_func(m, "t", std::get<uint64_t>(value));
                        break;
                     case 2: // bool
                        r = sd_bus_message_append_func(m, "b", std::get<bool>(value) ? 1 : 0);
                        break;
                     case 3: // std::vector<uint8_t>
                        {
                           // adding the vector<uint8_t> as an array
                           std::vector bits = std::get<std::vector<uint8_t>>(value);
                           r = sd_bus_message_append_array_func(m, 'y', bits.data(), bits.size());
                        }
                        break;
                     case 4:
                        {
                           std::vector<SystemdDevice_t> devices = std::get<std::vector<SystemdDevice_t>>(value);
                           // adding the vector<SystemdDevice_t> as an array of structs (ss)
                           r = sd_bus_message_open_container_func(m, SD_BUS_TYPE_ARRAY, "(ss)");
                           if (r >= 0) {
                              for (auto const& device : devices) {
                                 // append each device as a struct (ss)
                                 r = sd_bus_message_append_func(m, "(ss)", device.first.c_str(), device.second.c_str());
                                 if (r < 0) {
                                    break;
                                 }
                              }
                           }
                           if (r >= 0) {
                              // close the array of structs
                              r = sd_bus_message_close_container_func(m);
                           }
                        }
                        break;
                     default:
                        // cannot really happen
                        r = -EINVAL; // invalid type
                        break;
                  }
                  if (r < 0) {
                     sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_APPEND_PROPERTY_SSIS, key.c_str(), "StartTransientUnit", r, strerror(-r));
                     ret = false;
                  }
               }

               if (ret) {
                  r = sd_bus_message_close_container_func(m);
                  if (r < 0) {
                     sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CLOSE_CONTAINER_SSIS, "one property variant", "StartTransientUnit", r, strerror(-r));
                     ret = false;
                  }
               }
               if (ret) {
                  r = sd_bus_message_close_container_func(m);
                  if (r < 0) {
                     sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CLOSE_CONTAINER_SSIS, "one property struct", "StartTransientUnit", r, strerror(-r));
                     ret = false;
                  }
               }
            }
         } // loop over properties
      }

      if (ret) {
         r = sd_bus_message_close_container_func(m);
         if (r < 0) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CLOSE_CONTAINER_SSIS, "properties", "StartTransientUnit", r, strerror(-r));
            ret = false;
         }
      }
      if (ret) {
         r = sd_bus_message_open_container_func(m, SD_BUS_TYPE_ARRAY, "(sa(sv))");
         if (r < 0) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_OPEN_CONTAINER_SSIS, "aux", "StartTransientUnit", r, strerror(-r));
            ret = false;
         }
      }
      if (ret) {
         r = sd_bus_message_close_container_func(m);
         if (r < 0) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CLOSE_CONTAINER_SSIS, "aux", "StartTransientUnit", r, strerror(-r));
            ret = false;
         }
      }
      bool retry_on_interrupt = true;        // retry on EINTR
      int retries = 0;
      while (ret && retry_on_interrupt) {
         retry_on_interrupt = false;
         sd_bus_message *reply = nullptr;
         sd_bus_error error = SD_BUS_ERROR_NULL;
         r = sd_bus_call_func(bus, m, 0, &error, &reply);
         DPRINTF("===> StartTransientUnit returned %d", r);
         if (r < 0) {
            if (-r == EEXIST) {
               scope_already_exists = true;
               DPRINTF("Systemd::attach_pid_to_scope: scope does not exist\n");
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CALL_SSIS, "StartTransientUnit", scope.c_str(), r, error.message);
               ret = false;
            } else if (-r == EINTR && retries++ < NUM_SD_BUS_RETRIES) {
               retry_on_interrupt = true;
            } else {
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CALL_SSIS, "StartTransientUnit", scope.c_str(), r, error.message);
               if (retries > 0) {
                  sge_dstring_sprintf_append(error_dstr, MSG_SYSTEMD_AFTER_RETRIES_I, retries);
               }
               ret = false;
            }
         } else {
            const char *job = nullptr;
            r = sd_bus_message_read_func(reply, "o", &job);
            if (r < 0) {
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_READ_RESULT_SISS, "StartTransientUnit", r, strerror(-r));
               ret = false;
            } else {
               if (job == nullptr) {
                  sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_EMPTY_RESULT_S, "StartTransientUnit");
                  ret = false;
               } else {
                  // wait for the job to finish
                  if (slot != nullptr) {
                     ret = sd_bus_wait_for_job_completion(job, error_dstr);
                     sd_bus_wait_for_job_unsubscribe(&slot);
                  }
               }
            }
         }
         sd_bus_message_unref_func(reply);
         sd_bus_error_free_func(&error);
      }

      sd_bus_message_unref_func(m);

      DRETURN(ret);
   }

   /*!
    * @brief Wait for a systemd job signal
    *
    * This function subscribes to a systemd job signal, allowing the caller to wait for
    * specific job events, such as JobRemoved. It returns a pointer to the sd_bus_slot
    * that can be used to receive the signals.
    *
    * @param signal The name of the signal to subscribe to (e.g., "JobRemoved")
    * @param error_dstr A pointer to a dstring where error messages will be stored
    * @return A pointer to the sd_bus_slot for the subscribed signal, or nullptr on failure
    */
   sd_bus_slot *
   Systemd::sd_bus_wait_for_job_subscribe(const std::string &signal, dstring *error_dstr) const {
      DENTER(TOP_LAYER);

      // add match for JobRemoved signal
      sd_bus_slot *slot = nullptr;
      std::string match_rule = "type='signal',interface='org.freedesktop.systemd1.Manager',member='" + signal + "'";
      int r = sd_bus_add_match_func(bus, &slot, match_rule.c_str(), nullptr, nullptr);
      // @todo should also work but doesn't
      // int r = sd_bus_match_signal_func(bus, &slot, nullptr, nullptr, "JobRemoved", nullptr, nullptr);
      if (r < 0) {
         sge_dstring_sprintf(error_dstr, SFN ": adding match func " SFQ " failed: error %d: " SFN, __func__, match_rule.c_str(), r, strerror(-r));
      }

      DRETURN(slot);
   }

   /*!
    * @brief Unsubscribe from a systemd job signal
    *
    * This function unsubscribes from a systemd job signal by releasing the match pattern
    * associated with the sd_bus_slot. It sets the slot pointer to nullptr after unreferencing it.
    *
    * @param slot A pointer to the sd_bus_slot to unsubscribe from
    */
   void
   Systemd::sd_bus_wait_for_job_unsubscribe(sd_bus_slot **slot) const {
      // release the match pattern
      sd_bus_slot_unref_func(*slot);
      slot = nullptr;
   }

   /*!
    * @brief Wait for a systemd job to complete
    *
    * This function waits for a specific systemd job to complete by processing bus messages
    * and checking for the JobRemoved signal. It returns true if the job was completed successfully,
    * otherwise it sets the error_dstr with an appropriate error message.
    * It waits with a 5s timeout for the job to complete, if it times out it returns an error.
    * @note we might want to adjust the timeout based on experience, or make it configurable.
    *
    * @param job_path The path of the job to wait for completion
    * @param error_dstr A pointer to a dstring where error messages will be stored
    * @return true if the job was completed successfully, false otherwise
    */
   bool
   Systemd::sd_bus_wait_for_job_completion(const std::string &job_path, dstring *error_dstr) const {
      DENTER(TOP_LAYER);
      DPRINTF("==> sd_bus_wait_for_job_completion(%s)", job_path.c_str());

      bool ret = true;

      // Wait for the job to finish.
      // @todo What is an appropriate timeout?
      //       1 second was not enough in some cases, so we use 5 seconds.
      //       Make it configurable?
      u_long64 timeout = sge_get_gmt64() + 5000000; // 5 second timeout
      while (ret == true) {
         if (sge_get_gmt64() > timeout) {
            sge_dstring_sprintf(error_dstr, SFN ": timeout waiting for completion of job " SFN, __func__, job_path.c_str());
            ret = false;
            break;
         }

         // wait for the next signal
         sd_bus_message *m = nullptr;
         int r = sd_bus_process_func(bus, &m);
         DPRINTF("sd_bus_process_func(bus, &m) returned %d", r);
         if (r < 0) {
            if (-r != EINTR) {
               // We ignore EINTR, as we might get it, e.g., when sge_execd gets a SIGCHILD from an exiting sge_shepherd.
               sge_dstring_sprintf(error_dstr, SFN ": processing bus failed: error %d: " SFN, __func__, r, strerror(-r));
               ret = false;
            }
         } else {
            // 0 means we need to wait before calling sd_bus_process again
            if (r == 0) {
               // we wait with timeout
               // sd_bus_wait() returns 0 on timeout, not an error, final timeout handled above
               // does it actually make sense to use a timeout < our final timeout? We use 100ms for now.
               r = sd_bus_wait_func(bus, 100000);
               DPRINTF("sd_bus_wait_func(bus, nullptr) returned %d", r);
               if (r < 0) {
                  sge_dstring_sprintf(error_dstr, SFN ": waiting for bus failed: error %d: " SFN, __func__, r, strerror(-r));
                  ret = false;
               }
               // do the next sd_bus_process call
               sd_bus_message_unref_func(m);
               continue;
            }
         }

         // sd_bus_process read signal
         if (ret && m != nullptr) {
            if (strcmp(sd_bus_message_get_member_func(m), "JobRemoved") == 0) {
               DPRINTF("got JobRemoved signal");
               const char *completed_job_path = nullptr;
               // message contains for a signal:
               // `u`: job id, e.g. `1234`
               // `o`: job path, e.g. `/org/freedesktop/systemd1/job/1234`
               // `s`: status, e.g. `"done"`
               r = sd_bus_message_read_func(m, "uos", nullptr, &completed_job_path, nullptr);
               DPRINTF("sd_bus_message_read_func(m, \"uos\", &completed_job_path) returned %d", r);
               if (r < 0) {
                  sge_dstring_sprintf(error_dstr, SFN ": reading job path failed: error %d: " SFN, __func__, r, strerror(-r));
                  ret = false;
               } else {
                  DPRINTF("completed_job_path = %s", completed_job_path);
                  if (job_path.compare(completed_job_path) == 0) {
                     sd_bus_message_unref_func(m);
                     break; // job done
                  }
               }
            }
         }

         sd_bus_message_unref_func(m);
      }

      DRETURN(ret);
   }

   /*!
    * @brief Get a string property from a systemd unit
    *
    * This function retrieves a property from a specified systemd unit.
    * It handles retries in case of EINTR errors and returns true if the operation was successful,
    * otherwise it sets the error_dstr with an appropriate error message.
    *
    * @param interface The interface of the systemd unit (e.g., "Unit" or "Scope")
    * @param unit The name of the systemd unit to query
    * @param property The name of the property to retrieve
    * @param value A reference to a string where the property value will be stored
    * @param error_dstr A pointer to a dstring where error messages will be stored
    * @return true if the operation was successful, false otherwise
    */
   bool
   Systemd::sd_bus_get_property(const std::string &interface, const std::string &unit, const std::string &property,
                                std::string &value, dstring *error_dstr, bool *not_exists) const {
      DENTER(TOP_LAYER);

      bool ret = true;

      // encode the object path
      char *path = nullptr;
      if (unit.empty()) {
         path = strdup("/org/freedesktop/systemd1");
      } else {
         int r = sd_bus_path_encode_func("/org/freedesktop/systemd1/unit", unit.c_str(), &path);
         if (r < 0) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_ENCODE_PATH_SIS, unit.c_str(), r, strerror(-r));
            ret = false;
         }
      }

      int retries = 0;
      bool retry_on_interrupt = true;        // retry on EINTR
      while (ret && retry_on_interrupt) {
         retry_on_interrupt = false;
         sd_bus_message *m = nullptr;
         sd_bus_error error = SD_BUS_ERROR_NULL;
         std::string full_interface = "org.freedesktop.systemd1." + interface; // e.g. "org.freedesktop.systemd1.Unit"
         int r = sd_bus_get_property_func(bus, "org.freedesktop.systemd1",     // service to contact
                                      path,                                    // object path
                                      full_interface.c_str(),                  // interface name
                                      property.c_str(),                        // property name
                                      &error,                                  // object to return error in
                                      &m,                                      // return message on success
                                      "s");                               // type signature
         if (r < 0) {
            DPRINTF("sd_bus_get_property(%s, %s, %s) returned %d: %s", interface.c_str(), unit.c_str(), property.c_str(), r, error.message);
            if (-r == EINTR && retries++ < NUM_SD_BUS_RETRIES) {
               retry_on_interrupt = true;
            } else {
               if (-r == ENOENT && not_exists != nullptr) {
                  *not_exists = true; // property does not exist
               }
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CALL_SSIS, "sd_bus_get_property", property.c_str(), r, error.message);
               if (retries > 0) {
                  sge_dstring_sprintf_append(error_dstr, MSG_SYSTEMD_AFTER_RETRIES_I, retries);
               }
               ret = false;
            }
         } else {
            char *result = nullptr;
            r = sd_bus_message_read_func(m, "s", &result);
            if (r < 0) {
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_READ_PROPERTY_RESULT_SIS, property.c_str(), r, strerror(-r));
               value.clear();
               ret = false;
            } else {
               value = result;
            }
         }
         sd_bus_message_unref_func(m);
         sd_bus_error_free_func(&error);
      }

      free(path); // free the encoded path

      DRETURN(ret);
   }

   /*!
    * @brief Get an integer property from a systemd unit
    *
    * This function retrieves a property from a specified systemd unit.
    * It handles retries in case of EINTR errors and returns true if the operation was successful,
    * otherwise it sets the error_dstr with an appropriate error message.
    *
    * @param interface The interface of the systemd unit (e.g., "Unit" or "Scope")
    * @param unit The name of the systemd unit to query
    * @param property The name of the property to retrieve
    * @param value A reference to a uint64_t where the property value will be stored
    * @param error_dstr A pointer to a dstring where error messages will be stored
    * @return true if the operation was successful, false otherwise
    */
   bool
   Systemd::sd_bus_get_property(const std::string &interface, const std::string &unit, const std::string &property,
                                uint64_t &value, dstring *error_dstr, bool *not_exists) const {
      DENTER(TOP_LAYER);

      bool ret = true;

      // encode the object path
      char *path = nullptr;
      int r = sd_bus_path_encode_func("/org/freedesktop/systemd1/unit", unit.c_str(), &path);
      if (r < 0) {
         sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_ENCODE_PATH_SIS, unit.c_str(), r, strerror(-r));
         ret = false;
      }

      bool retry_on_interrupt = true;        // retry on EINTR
      int retries = 0;
      while (ret && retry_on_interrupt) {
         retry_on_interrupt = false;
         sd_bus_message *m = nullptr;
         sd_bus_error error = SD_BUS_ERROR_NULL;
         std::string full_interface = "org.freedesktop.systemd1." + interface;   // e.g. "org.freedesktop.systemd1.Unit"
         r = sd_bus_get_property_func(bus, "org.freedesktop.systemd1",           // service to contact
                                      path,                                      // object path
                                      full_interface.c_str(),                    // interface name
                                      property.c_str(),                          // property name
                                      &error,                                    // object to return error in
                                      &m,                                        // return message on success
                                      "t");                                 // type signature
         if (r < 0) {
            DPRINTF("sd_bus_get_property(%s, %s, %s) returned %d: %s", interface.c_str(), unit.c_str(), property.c_str(), r, error.message);
            if (-r == EINTR && retries++ < NUM_SD_BUS_RETRIES) {
               retry_on_interrupt = true;
            } else {
               if (-r == ENOENT && not_exists != nullptr) {
                  *not_exists = true; // property does not exist
               }
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CALL_SSIS, "sd_bus_get_property", property.c_str(), r, error.message);
               if (retries > 0) {
                  sge_dstring_sprintf_append(error_dstr, MSG_SYSTEMD_AFTER_RETRIES_I, retries);
               }
               ret = false;
            }
         } else {
            uint64_t result{};
            r = sd_bus_message_read_func(m, "t", &result);
            if (r < 0) {
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_READ_PROPERTY_RESULT_SIS, property.c_str(), r, strerror(-r));
               value = 0;
               ret = false;
            } else {
               value = result;
            }
         }
         sd_bus_message_unref_func(m);
         sd_bus_error_free_func(&error);
      }

      free(path); // free the encoded path

      DRETURN(ret);
   }

   /*!
    * @brief Stop a systemd unit
    *
    * This function stops a specified systemd unit by calling the StopUnit method.
    * It handles retries in case of EINTR errors and returns true if the operation was successful,
    * otherwise it sets the error_dstr with an appropriate error message.
    *
    * @param unit The name of the systemd unit to stop
    * @param error_dstr A pointer to a dstring where error messages will be stored
    * @return true if the operation was successful, false otherwise
    */
   bool
   Systemd::stop_unit(const std::string &unit, dstring *error_dstr) const {
      bool ret = true;

      bool retry_on_interrupt = true;        // retry on EINTR
      int retries = 0;
      while (ret && retry_on_interrupt) {
         retry_on_interrupt = false;
         sd_bus_error error = SD_BUS_ERROR_NULL;
         int r = sd_bus_call_method_func(bus, "org.freedesktop.systemd1",   // service to contact
                                      "/org/freedesktop/systemd1",          // object path
                                      "org.freedesktop.systemd1.Manager",   // interface name
                                      "StopUnit",                           // method name
                                      &error,                               // object to return error in
                                      nullptr,                              // return message on success (not needed)
                                                                            // @todo Really? Would give us the job we could wait on.
                                      "ss",                                 // input signature
                                      unit.c_str(),                         // first argument (unit name)
                                      "replace");                           // second argument (mode)
         // If the unit does not exist, we get -2 (ENOENT) which is OK
         if (r < 0 && -r != ENOENT) {
            if (-r == EINTR && retries++ < NUM_SD_BUS_RETRIES) {
               retry_on_interrupt = true;
            } else {
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CALL_SSIS, "StopUnit", unit.c_str(), r, error.message);
               if (retries > 0) {
                  sge_dstring_sprintf_append(error_dstr, MSG_SYSTEMD_AFTER_RETRIES_I, retries);
               }
               ret = false;
            }
         }
         sd_bus_error_free_func(&error);
      }

      return ret;
   }

   /*!
    * @brief Freeze (suspend) a systemd unit
    *
    * This function stops a specified systemd unit by calling the StopUnit method.
    * It handles retries in case of EINTR errors and returns true if the operation was successful,
    * otherwise it sets the error_dstr with an appropriate error message.
    *
    * @param unit The name of the systemd unit to stop
    * @param error_dstr A pointer to a dstring where error messages will be stored
    * @return true if the operation was successful, false otherwise
    */
   bool
   Systemd::freeze_unit(const std::string &unit, dstring *error_dstr) const {
      bool ret = true;

      bool retry_on_interrupt = true;        // retry on EINTR
      int retries = 0;
      while (ret && retry_on_interrupt) {
         retry_on_interrupt = false;
         sd_bus_error error = SD_BUS_ERROR_NULL;
         int r = sd_bus_call_method_func(bus, "org.freedesktop.systemd1",   // service to contact
                                      "/org/freedesktop/systemd1",          // object path
                                      "org.freedesktop.systemd1.Manager",   // interface name
                                      "FreezeUnit",                         // method name
                                      &error,                               // object to return error in
                                      nullptr,                              // return message on success (not needed)
                                      "s",                                  // input signature
                                      unit.c_str());                        // first argument (unit name)
         if (r < 0) {
            if (-r == EINTR && retries++ < NUM_SD_BUS_RETRIES) {
               retry_on_interrupt = true;
            } else {
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CALL_SSIS, "FreezeUnit", unit.c_str(), r, error.message);
               if (retries > 0) {
                  sge_dstring_sprintf_append(error_dstr, MSG_SYSTEMD_AFTER_RETRIES_I, retries);
               }
               ret = false;
            }
         }

         sd_bus_error_free_func(&error);
      }

      return ret;
   }

   /*!
    * @brief Thaw (unsuspend) a systemd unit
    *
    * This function thaws (unsuspends) a specified systemd unit by calling the ThawUnit method.
    * It handles retries in case of EINTR errors and returns true if the operation was successful,
    * otherwise it sets the error_dstr with an appropriate error message.
    *
    * @param unit The name of the systemd unit to thaw
    * @param error_dstr A pointer to a dstring where error messages will be stored
    * @return true if the operation was successful, false otherwise
    */
   bool
   Systemd::thaw_unit(const std::string &unit, dstring *error_dstr) const {
      bool ret = true;

      bool retry_on_interrupt = true;        // retry on EINTR
      int retries = 0;
      while (ret && retry_on_interrupt) {
         retry_on_interrupt = false;
         sd_bus_error error = SD_BUS_ERROR_NULL;
         int r = sd_bus_call_method_func(bus, "org.freedesktop.systemd1",   // service to contact
                                      "/org/freedesktop/systemd1",          // object path
                                      "org.freedesktop.systemd1.Manager",   // interface name
                                      "ThawUnit",                           // method name
                                      &error,                               // object to return error in
                                      nullptr,                              // return message on success (not needed)
                                      "s",                                  // input signature
                                      unit.c_str());                        // first argument (unit name)
         if (r < 0) {
            if (-r == EINTR && retries++ < NUM_SD_BUS_RETRIES) {
               retry_on_interrupt = true;
            } else {
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CALL_SSIS, "ThawUnit", unit.c_str(), r, error.message);
               if (retries > 0) {
                  sge_dstring_sprintf_append(error_dstr, MSG_SYSTEMD_AFTER_RETRIES_I, retries);
               }
               ret = false;
            }
         }

         sd_bus_error_free_func(&error);
      }

      return ret;
   }

   /*!
    * @brief Send a signal to a systemd unit
    *
    * This function sends a signal to a specified systemd unit by calling the KillUnit method.
    * It handles retries in case of EINTR errors and returns true if the operation was successful,
    * otherwise it sets the error_dstr with an appropriate error message.
    *
    * It uses the KillUnit method of the systemd Manager interface to send a signal to the unit.
    * Depending on the `only_main` parameter, it can signal either the main process of the unit
    * or all processes associated with the unit.
    *
    * @param unit The name of the systemd unit to signal
    * @param signal The signal number to send (e.g., SIGTERM)
    * @param only_main If true, only the main process of the unit will be signaled
    * @param error_dstr A pointer to a dstring where error messages will be stored
    * @return true if the operation was successful, false otherwise
    */
   bool
   Systemd::signal_unit(const std::string &unit, int signal, bool only_main, dstring *error_dstr) const {
      bool ret = true;

      bool retry_on_interrupt = true;        // retry on EINTR
      int retries = 0;
      while (ret && retry_on_interrupt) {
         retry_on_interrupt = false;
         sd_bus_error error = SD_BUS_ERROR_NULL;
         int r = sd_bus_call_method_func(bus, "org.freedesktop.systemd1",   // service to contact
                                      "/org/freedesktop/systemd1",          // object path
                                      "org.freedesktop.systemd1.Manager",   // interface name
                                      "KillUnit",                           // method name
                                      &error,                               // object to return error in
                                      nullptr,                              // return message on success (not needed)
                                      "ssi",                                // input signature
                                      unit.c_str(),                         // first argument (unit name)
                                      only_main ? "main" : "all",           // second argument (mode)
                                      signal);                              // third argument (signal number)
         if (r < 0) {
            if (-r == EINTR && retries++ < NUM_SD_BUS_RETRIES) {
               retry_on_interrupt = true;
            } else {
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CALL_SSIS, "SignalUnit", unit.c_str(), r, error.message);
               if (retries > 0) {
                  sge_dstring_sprintf_append(error_dstr, MSG_SYSTEMD_AFTER_RETRIES_I, retries);
               }
               ret = false;
            }
         }
         sd_bus_error_free_func(&error);
      }

      return ret;
   }

} // namespace
#endif
