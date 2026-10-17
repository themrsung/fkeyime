# FKeyIME settings window (fkeyime-settings.exe): Report an issue, Copy system details.
add_executable(fkeyime_settings WIN32 main.cpp system_details.cpp)
set_target_properties(fkeyime_settings PROPERTIES OUTPUT_NAME fkeyime-settings)
target_include_directories(fkeyime_settings PRIVATE ${PROJECT_SOURCE_DIR}/src/tip
                                                    ${PROJECT_BINARY_DIR}/generated)
target_link_libraries(fkeyime_settings PRIVATE user32 gdi32 shell32 advapi32 ole32 version)
