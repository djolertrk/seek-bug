cmake_minimum_required(VERSION 3.10)

# Set the project name
project(SeekBug)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find required packages
find_package(PkgConfig REQUIRED)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
include(AddLLVM)

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# Check for LLDB
link_directories(${LLVM_BUILD_ROOT}/)
find_library(LLDB_LIBRARY REQUIRED
    NAMES lldb LLDB
    HINTS "${LLVM_BUILD_ROOT}/lib"
    PATHS "${LLVM_BUILD_ROOT}/lib" /usr/lib/llvm-19/lib/
)

find_library(LLAMA_CPP REQUIRED
  NAMES libllama.so libllama.dylib llama
  PATHS /usr/local/lib/
  NO_DEFAULT_PATH
)

include_directories(${LLVM_INCLUDE_DIRS} ${LLVM_PROJECT_SOURCE}/lldb/include/)
include_directories(${LLVM_PROJECT_SOURCE}/llvm/include)
include_directories(/usr/lib/llvm-19/include/)

llvm_map_components_to_libnames(llvm_libs
  Support
)

set(SEEKBUG_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(lib)
add_subdirectory(test)

add_executable(seek-bug main.cpp)

# TODO: Fix this.
# Link required libraries
if(APPLE)
  target_link_libraries(seek-bug ${llvm_libs} ${LLDB_LIBRARY} AICommands ${LLAMA_CPP})
else()
  # On Linux or other Unix-like systems
  target_link_libraries(seek-bug ${llvm_libs} ${LLDB_LIBRARY} AICommands ${LLAMA_CPP})
endif()

target_include_directories(seek-bug
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(seek-bug
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

install(TARGETS seek-bug
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "seek-bug")
set(CPACK_PACKAGE_VERSION "0.0.1")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Djordje Todorovic <djolertrk@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "liblldb-19-dev, libllama")
include(CPack)
