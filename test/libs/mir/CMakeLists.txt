# test/libs/mir

add_executable(test_mir_basic test_mir_basic.cc  ../../../source/common/sig_handlers.cc)
target_include_directories(test_mir_basic PRIVATE "./")
target_link_libraries(test_mir_basic PRIVATE mir evc gdi sgeobj cull comm uti commlists ${SGE_LIBS})
add_test(NAME test_mir_basic COMMAND test_mir_basic)

if (INSTALL_SGE_TEST)
   install(TARGETS test_mir_basic DESTINATION testbin/${SGE_ARCH})
endif ()
