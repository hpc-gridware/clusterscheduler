#___INFO__MARK_BEGIN_CLOSED__
###########################################################################
#
#  Copyright 2025 HPC-Gridware GmbH
#
###########################################################################
#___INFO__MARK_END_CLOSED__

# source/libs/juti
set(LIBRARY_NAME juti)
set(LIBRARY_SOURCES juti.c com_sun_grid_util_SGEUtil.c)
set(LIBRARY_INCLUDES ".")

if (WITH_JNI)
   set(JNI_INCLUDE $ENV{JAVA_HOME}/include $ENV{JAVA_HOME}/include/${JNI_ARCH})
endif()

add_library(${LIBRARY_NAME} SHARED ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES} ${JNI_INCLUDE}
      ${LIBRARY_INCLUDES})

if (INSTALL_SGE_BIN)
   install(TARGETS ${LIBRARY_NAME} DESTINATION lib/${SGE_ARCH})
endif ()

