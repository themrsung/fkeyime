# M0 spike tools (tools/spike.ps1). Not shipped.
add_executable(fkeyide_probe probe.cpp)
target_link_libraries(fkeyide_probe PRIVATE comctl32 user32)
target_link_libraries(fkeyide_probe PRIVATE ole32)
target_include_directories(fkeyide_probe PRIVATE ${PROJECT_SOURCE_DIR}/src/tip)
