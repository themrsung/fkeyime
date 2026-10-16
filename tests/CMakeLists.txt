add_executable(fkeyide_tests main.cpp mode_machine_test.cpp)
target_link_libraries(fkeyide_tests PRIVATE fkeyide_core)
add_test(NAME fkeyide_tests COMMAND fkeyide_tests)
