#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025 HPC-Gridware GmbH
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

#include <string>

namespace ocs {
   class GdiSubCommand {
      GdiSubCommand() = default; // prevent instantiation
   public:
      enum SubCommand {
         SGE_GDI_SUB_NONE = 0,

         // used for add-job-reuqests so that the created job with jid is returned
         SGE_GDI_RETURN_NEW_VERSION = (1<<8),

         // delete or modify all jobs
         SGE_GDI_ALL_JOBS  = (1<<9),
         SGE_GDI_ALL_USERS = (1<<10),

         // for queues and hosts to define how to handle sublists
         SGE_GDI_SET     = 0,        //< overwrite the sublist with given values
         SGE_GDI_CHANGE  = (1<<11),  //< change the given elements
         SGE_GDI_APPEND  = (1<<12),  //< add some elements into a sublist
         SGE_GDI_REMOVE  = (1<<13),  //< remove some elements from a sublist
         SGE_GDI_SET_ALL = (1<<14),  //< overwrite the sublist with given values and erase all
                                     // domain/host specific values not given with the current request

         SGE_GDI_EXECD_RESTART = (1<<15)
      };

      static std::string toString(SubCommand mode);
   };
}