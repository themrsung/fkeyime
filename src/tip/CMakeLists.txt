# Shared-memory state table, also used by the unit tests.
add_library(fkeyime_state STATIC state_table.cpp)
target_include_directories(fkeyime_state PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(fkeyime_state PUBLIC advapi32)

add_library(fkeyime_tip SHARED
    display_attribute.cpp
    dllmain.cpp
    edit_session.cpp
    fkeyime_api.cpp
    log.cpp
    registration.cpp
    text_service.cpp
    fkeyime.def)
set_target_properties(fkeyime_tip PROPERTIES OUTPUT_NAME fkeyime)
target_link_libraries(fkeyime_tip PRIVATE fkeyime_core fkeyime_state ole32 advapi32 user32)
