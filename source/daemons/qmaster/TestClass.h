#pragma once

#include <string>

namespace oge {
   class TestClass {
   private:
   protected:
   public:
      explicit TestClass(const std::string &name);
      static void method(const std::string &arg1);
   };
}