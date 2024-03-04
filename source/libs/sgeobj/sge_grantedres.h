#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/*___INFO__MARK_END_NEW__*/

#include <string>

#include "sgeobj/cull/sge_grantedres_GRU_L.h"

const char *
granted_res_get_id_string(std::string &buffer, const lList *resource_map_list);

lListElem *
gru_list_search(lList *granted_resources_list, const char *name, const char *host_name);
