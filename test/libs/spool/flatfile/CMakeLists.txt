# test/libs/spool/flatfile

add_executable(test_spool_flatfile test_spool_flatfile.cc)
target_include_directories(test_spool_flatfile PRIVATE "./")
target_link_libraries(test_spool_flatfile PRIVATE spool spoolc gdi sgeobj uti commlists ${SGE_LIBS})
add_test(NAME test_spool_flatfile COMMAND test_spool_flatfile)

add_executable(test_spool_flatfile2 test_spool_flatfile2.cc)
target_include_directories(test_spool_flatfile2 PRIVATE "./")
target_link_libraries(test_spool_flatfile2 PRIVATE spool spoolc gdi sgeobj uti commlists ${SGE_LIBS})
add_test(NAME test_spool_flatfile2 COMMAND test_spool_flatfile2)

if (INSTALL_SGE_TEST)
   install(TARGETS test_spool_flatfile DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_spool_flatfile2 DESTINATION testbin/${SGE_ARCH})
endif ()
