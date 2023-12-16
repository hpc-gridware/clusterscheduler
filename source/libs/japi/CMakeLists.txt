# source/libs/japi
set(LIBRARY_NAME japi)
set(LIBRARY_SOURCES drmaa.c japi.c)
set(LIBRARY_INCLUDES "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES}
                                                  ${LIBRARY_INCLUDES})

install(FILES drmaa.h DESTINATION include)

install(
  FILES example.c
        howto/howto1_1.c
        howto/howto1.c
        howto/howto2_1.c
        howto/howto2.c
        howto/howto3_1.c
        howto/howto3_2.c
        howto/howto3.c
        howto/howto4.c
        howto/howto5.c
        howto/howto6.c
  DESTINATION examples/drmaa)
