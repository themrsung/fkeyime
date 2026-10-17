# Shared-memory state table, also used by the unit tests.
add_library(fkeyime_state STATIC state_table.cpp)
target_include_directories(fkeyime_state PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(fkeyime_state PUBLIC advapi32)

# Client of the bundled Mozc conversion server, also used by tools/spike.
add_library(fkeyime_mozc_client STATIC mozc_client.cpp)
target_include_directories(fkeyime_mozc_client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(fkeyime_mozc_client PUBLIC fkeyime_core advapi32 bcrypt)

add_library(fkeyime_tip SHARED
    candidate_window.cpp
    display_attribute.cpp
    dllmain.cpp
    edit_session.cpp
    fkeyime_api.cpp
    log.cpp
    registration.cpp
    text_service.cpp
    fkeyime.def)
set_target_properties(fkeyime_tip PROPERTIES OUTPUT_NAME fkeyime)
target_link_libraries(fkeyime_tip PRIVATE fkeyime_core fkeyime_state fkeyime_mozc_client ole32
                                          advapi32 user32 gdi32)
