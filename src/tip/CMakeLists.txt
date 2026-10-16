add_library(fkeyide_tip SHARED dllmain.cpp fkeyide.def)
set_target_properties(fkeyide_tip PROPERTIES OUTPUT_NAME fkeyide)
target_link_libraries(fkeyide_tip PRIVATE fkeyide_core)
