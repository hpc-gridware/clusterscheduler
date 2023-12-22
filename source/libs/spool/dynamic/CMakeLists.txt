# source/libs/spool/dynamic
if(${WITH_SPOOL_DYNAMIC})
   set(LIBRARY_NAME spoold)
   set(LIBRARY_SOURCES sge_spooling_dynamic.c)
   set(LIBRARY_INCLUDES "./")

   add_library(${LIBRARY_NAME}_objlib OBJECT ${LIBRARY_SOURCES})
   target_include_directories(${LIBRARY_NAME}_objlib PUBLIC ${SGE_INCLUDES} ${LIBRARY_INCLUDES})
   set_property(TARGET ${LIBRARY_NAME}_objlib PROPERTY POSITION_INDEPENDENT_CODE 1)

   add_library(${LIBRARY_NAME}_static STATIC $<TARGET_OBJECTS:${LIBRARY_NAME}_objlib>)

   add_library(${LIBRARY_NAME} SHARED $<TARGET_OBJECTS:${LIBRARY_NAME}_objlib>)
   target_link_libraries(
      ${LIBRARY_NAME}
   PRIVATE
      sgeobj
      gdi
      cull
      comm
      uti
      commlists)

   #set(CMAKE_ENABLE_EXPORTS ON)
   #set(CMAKE_C_FLAGS "-g -fno-common -dynamic")
   #set(CMAKE_EXE_LINKER_FLAGS "-g -dynamiclib -flat_namespace")

   #add_compile_options(-fno-common -dynamic)
   #add_link_options(-dynamiclib -flat_namespace)

   if(INSTALL_SGE_BIN)
      install(TARGETS spoold DESTINATION lib/${SGE_ARCH})
   endif()
endif()
