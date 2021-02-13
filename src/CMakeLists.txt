add_library(termbox11 	${CMAKE_CURRENT_SOURCE_DIR}/termbox.cpp
			${CMAKE_CURRENT_SOURCE_DIR}/utf8.cpp)
target_include_directories(termbox11 PUBLIC
            		$<INSTALL_INTERFACE:include>
            		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
