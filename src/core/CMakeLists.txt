add_library(fkeyime_core STATIC mode_machine.cpp jis_layout.cpp hangul.cpp kana.cpp mozc_protocol.cpp)
target_include_directories(fkeyime_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
