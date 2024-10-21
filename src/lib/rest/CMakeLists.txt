cmake_minimum_required (VERSION 3.0.2)
project (tor_rest)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (WIN32)
	add_compile_definitions(_WIN32)
endif(WIN32)

add_library(pugixml OBJECT 
	${CMAKE_CURRENT_SOURCE_DIR}/pugixml/pugixml.cpp
)

add_library(libmicrohttpd OBJECT
	${CMAKE_CURRENT_SOURCE_DIR}/libmicrohttpd/connection.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/libmicrohttpd/daemon.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/libmicrohttpd/internal.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/libmicrohttpd/memorypool.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/libmicrohttpd/postprocessor.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/libmicrohttpd/reason_phrase.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/libmicrohttpd/response.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/libmicrohttpd/w32functions.cpp
)

add_library(rest_server OBJECT
	${CMAKE_CURRENT_SOURCE_DIR}/rest_server/json_builder.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/rest_server/json_response_generator.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/rest_server/rest_server.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/rest_server/version.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/rest_server/xml_builder.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/rest_server/xml_response_generator.cpp
)

add_library(tor_rest OBJECT
	${CMAKE_CURRENT_SOURCE_DIR}/tor_rest/rest_service.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/tor_rest/rest_lib.cpp
)

add_library(tor_plus_server 
	$<TARGET_OBJECTS:pugixml>
	$<TARGET_OBJECTS:libmicrohttpd>
	$<TARGET_OBJECTS:rest_server>
	$<TARGET_OBJECTS:tor_rest>
)

add_executable (test_app ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
add_executable (test_app_c ${CMAKE_CURRENT_SOURCE_DIR}/main.c)

target_include_directories(test_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/libmicrohttpd )
target_include_directories(tor_rest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/libmicrohttpd )
target_include_directories(test_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} )
target_include_directories(tor_rest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} )

target_link_libraries (test_app tor_plus_server pthread)
target_link_libraries (test_app_c tor_plus_server pthread)

if (WIN32)
	target_link_libraries(test_app Ws2_32)
	target_link_libraries(test_app_c Ws2_32)
endif (WIN32)
