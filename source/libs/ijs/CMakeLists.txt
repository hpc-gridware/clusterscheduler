# source/libs/ijs
set(LIBRARY_NAME ijs)
set(LIBRARY_SOURCES sge_ijs_lib.cc)
set(LIBRARY_INCLUDES "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES}
      ${LIBRARY_INCLUDES})
