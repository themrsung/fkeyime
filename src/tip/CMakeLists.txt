add_library(fkeyime_tip SHARED
    dllmain.cpp
    log.cpp
    registration.cpp
    text_service.cpp
    fkeyime.def)
set_target_properties(fkeyime_tip PROPERTIES OUTPUT_NAME fkeyime)
target_link_libraries(fkeyime_tip PRIVATE fkeyime_core ole32 advapi32 user32)
