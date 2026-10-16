add_library(fkeyide_tip SHARED
    dllmain.cpp
    log.cpp
    registration.cpp
    text_service.cpp
    fkeyide.def)
set_target_properties(fkeyide_tip PROPERTIES OUTPUT_NAME fkeyide)
target_link_libraries(fkeyide_tip PRIVATE fkeyide_core ole32 advapi32 user32)
