add_executable(fkeyime_tests main.cpp mode_machine_test.cpp)
target_link_libraries(fkeyime_tests PRIVATE fkeyime_core)
add_test(NAME fkeyime_tests COMMAND fkeyime_tests)
