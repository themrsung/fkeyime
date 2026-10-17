# M0 spike tools (tools/spike.ps1). Not shipped.
add_executable(fkeyime_probe probe.cpp)
target_link_libraries(fkeyime_probe PRIVATE comctl32 user32)
target_link_libraries(fkeyime_probe PRIVATE ole32 advapi32)
target_include_directories(fkeyime_probe PRIVATE ${PROJECT_SOURCE_DIR}/src/tip)

add_executable(fkeyime_mozc_cli mozc_cli.cpp)
target_link_libraries(fkeyime_mozc_cli PRIVATE fkeyime_mozc_client)
