cmake_minimum_required (VERSION 3.10)
project (tor_rest)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (WIN32)
	add_compile_definitions(_WIN32)
endif(WIN32)

set(MHTTPD_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../../external/microrestd/src)

add_library(libmicrohttpd OBJECT
	${MHTTPD_PATH}/libmicrohttpd/connection.cpp
	${MHTTPD_PATH}/libmicrohttpd/daemon.cpp
	${MHTTPD_PATH}/libmicrohttpd/internal.cpp
	${MHTTPD_PATH}/libmicrohttpd/memorypool.cpp
	${MHTTPD_PATH}/libmicrohttpd/postprocessor.cpp
	${MHTTPD_PATH}/libmicrohttpd/reason_phrase.cpp
	${MHTTPD_PATH}/libmicrohttpd/response.cpp
	${MHTTPD_PATH}/libmicrohttpd/w32functions.cpp
)

add_library(rest_server OBJECT
	${MHTTPD_PATH}/rest_server/json_builder.cpp
	${MHTTPD_PATH}/rest_server/json_response_generator.cpp
	${MHTTPD_PATH}/rest_server/rest_server.cpp
	${MHTTPD_PATH}/rest_server/version.cpp
	${MHTTPD_PATH}/rest_server/xml_builder.cpp
	${MHTTPD_PATH}/rest_server/xml_response_generator.cpp
)

add_library(tor_rest OBJECT
	${CMAKE_CURRENT_SOURCE_DIR}/tor_rest/rest_service.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/tor_rest/rest_lib.cpp
)

add_library(tor_plus_server 
	$<TARGET_OBJECTS:libmicrohttpd>
	$<TARGET_OBJECTS:rest_server>
	$<TARGET_OBJECTS:tor_rest>
)

add_executable (test_app ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
add_executable (test_app_c ${CMAKE_CURRENT_SOURCE_DIR}/main.c)

target_include_directories(test_app PRIVATE ${MHTTPD_PATH}/libmicrohttpd )
target_include_directories(tor_rest PRIVATE ${MHTTPD_PATH}/libmicrohttpd )
target_include_directories(test_app PRIVATE ${MHTTPD_PATH} )
target_include_directories(tor_rest PRIVATE ${MHTTPD_PATH} )

target_link_libraries (test_app tor_plus_server pthread)
target_link_libraries (test_app_c tor_plus_server pthread)

if (APPLE)
	target_link_options(test_app PRIVATE LINKER:-sectcreate,__TEXT,__info_plist,${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
	target_link_options(test_app_c PRIVATE LINKER:-sectcreate,__TEXT,__info_plist,${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
	target_link_options(tor_rest PRIVATE LINKER:-sectcreate,__TEXT,__info_plist,${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
    add_link_options("-Wl–sectcreate,__TEXT,__info_plist,${Carabiner_SOURCE_DIR}/CMake/Info.plist.in")
	set_target_properties(test_app PROPERTIES
		MACOSX_BUNDLE ON
		MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
		)
	set_target_properties(test_app_c PROPERTIES
		MACOSX_BUNDLE ON
		MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
		)
endif (APPLE)

if (WIN32)
	target_link_libraries(test_app Ws2_32)
	target_link_libraries(test_app_c Ws2_32)
endif (WIN32)
