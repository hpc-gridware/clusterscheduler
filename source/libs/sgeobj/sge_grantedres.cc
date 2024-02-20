/*___INFO__MARK_BEGIN_NEW__*/
/*___INFO__MARK_END_NEW__*/

#include "sgeobj/sge_host.h"

#include "uti/sge_string.h"
#include "uti/sge_hostname.h"

#include "sge_grantedres.h"

/**
 * @brief print resource map ids to a string
 * @param buffer - c++ std::string buffer
 * @param resource_map_list - resource map list, e.g. from CE_Type or GRU_Type
 * @return - pointer to c string
 */
const char *
granted_res_get_id_string(std::string &buffer, const lList *resource_map_list) {
   const lListElem *resl;
   for_each_ep (resl, resource_map_list) {
      const char *value = lGetString(resl, RESL_value);
      u_long32 amount = lGetUlong(resl, RESL_amount);
      for (u_long32 i = 0; i < amount; ++i) {
         if (!buffer.empty()) {
            buffer += ' ';
         }
         buffer += value;
      }
   }

   return buffer.c_str();
}

/**
 * @brief search a GRU_Type element in a list
 *
 * A GRU_Type Object is uniquely defined by resource name and host name.
 * We search the element with these two keys.
 *
 * @param granted_resources_list
 * @param name
 * @param host_name
 * @return pointer a GRU_Type lListElem if found, else nullptr
 * @todo we have no primary key and no hashes,
 *       but with big PE jobs could have a significant number of elements
 */
lListElem *
gru_list_search(lList *granted_resources_list, const char *name, const char *host_name) {
   lListElem *gru;
   for_each_rw(gru, granted_resources_list) {
      if (sge_strnullcmp(lGetString(gru, GRU_name), name) == 0 &&
          sge_hostcmp(lGetHost(gru, GRU_host), host_name) == 0) {
         return gru;
      }
   }
   return nullptr;
}

