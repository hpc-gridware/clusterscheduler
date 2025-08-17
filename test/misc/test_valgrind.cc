/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2025 HPC-Gridware GmbH
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

#include <iostream>
#include <cstring>

#if 0
// some compilers don't like the following pragma - disabling for now
#pragma GCC diagnostic ignored "-Wuse-after-free"
void free_errors() {
   std::cout << "accessing freed memory and freeing it twice" << std::endl;
   const char *str = "hello";
   const char *dup = strdup(str);
   std::cout << dup << std::endl;
   free((void *)dup);
   std::cout << dup << std::endl;
   free((void *)dup);
}
#endif

void memory_access_error() {
   std::cout << "invalid access before and after allocated memory" << std::endl;
   int array[10]{};
   // @todo valgrind doesn't catch these errors
   array[10] = 42;
   array[-1] = 3;
   std::cout << array[0] << std::endl;
}

void leak_scope() {
   std::cout << "allocating memory which will leak" << std::endl;
   int *leak = new int[10];
   leak[0] = 42;
   std::cout << leak[0] << std::endl;
}

int main() {
   leak_scope();
   memory_access_error();
#if 0
   free_errors();
#endif
   return 0;
}
