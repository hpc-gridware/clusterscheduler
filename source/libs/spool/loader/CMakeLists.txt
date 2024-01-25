# source/libs/spooling/loader
set(LIBRARY_NAME spoolloader)
set(LIBRARY_SOURCES sge_spooling_loader.cc)
set(LIBRARY_INCLUDES "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES} ${LIBRARY_INCLUDES})
