add_executable(kbd ${CMAKE_CURRENT_SOURCE_DIR}/kbd.cpp)
target_link_libraries(kbd termbox11)
