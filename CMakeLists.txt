cmake_minimum_required(VERSION 3.18)

project(OpenBlade)

include(Hork-Source/hork_config.cmake)

set(HK_BUILD_SAMPLES OFF)

add_subdirectory(Hork-Source)

include_directories(Hork-Source)
include_directories(${CMAKE_BINARY_DIR}/include/ThirdParty)

setup_msvc_runtime_library()
make_source_list_for_directory(Source SOURCE_FILES)

file(GLOB RESOURCES res/resource.rc res/hork.ico)
source_group("res" FILES ${RESOURCES})

set(SOURCE_FILES ${SOURCE_FILES} ${RESOURCES})

if(WIN32)
add_executable(${PROJECT_NAME} WIN32 ${SOURCE_FILES})
else()
add_executable(${PROJECT_NAME} ${SOURCE_FILES})
endif()

target_link_libraries(${PROJECT_NAME} Runtime)

target_compile_definitions(${PROJECT_NAME} PUBLIC ${HK_COMPILER_DEFINES})
target_compile_options(${PROJECT_NAME} PUBLIC ${HK_COMPILER_FLAGS})

#set_property( TARGET ${PROJECT} PROPERTY POSITION_INDEPENDENT_CODE ON )

set(HK_ASSET_DATA_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Data")
set(HK_ASSET_DATA_INSTALL_PATH "${HK_PROJECT_BUILD_PATH}/Data")

make_link(${HK_ASSET_DATA_PATH} ${HK_ASSET_DATA_INSTALL_PATH})
