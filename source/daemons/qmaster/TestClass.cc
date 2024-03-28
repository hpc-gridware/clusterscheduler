/*___INFO__MARK_BEGIN_NEW__*/
/*___INFO__MARK_END_NEW__*/

#include <iostream>

#include "TestClass.h"

oge::TestClass::TestClass(const std::string &name) {
   TestClass::method(name);
}

void
oge::TestClass::method(const std::string &arg1) {
   //std::cout << "method from gridengine repo was called" << std::endl;
   ;
}
