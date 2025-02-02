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
message("LLVM_BUILD_ROOT ${LLVM_BUILD_ROOT}")
link_directories(${LLVM_BUILD_ROOT}/)
find_library(LLDB_LIBRARY REQUIRED
    NAMES lldb LLDB
    HINTS "${LLVM_BUILD_ROOT}/lib"
    PATHS "${LLVM_BUILD_ROOT}/lib"
)

message("Using LLDB library from ${LLDB_LIBRARY}")

# Find llama.cpp.
if (NOT LLAMA_CPP_DIR)
  message(FATAL_ERROR "LLAMA_CPP_DIR must be specified in order to find llama.cpp")
endif()

include_directories(${LLAMA_CPP_DIR}/include)

include_directories(${LLVM_INCLUDE_DIRS} ${LLVM_PROJECT_SOURCE}/lldb/include/)
include_directories(${LLVM_PROJECT_SOURCE}/llvm/include)

llvm_map_components_to_libnames(llvm_libs
  Support
)

set(SEEKBUG_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(lib)
add_subdirectory(test)

add_executable(seek-bug main.cpp)

# Link required libraries
target_link_libraries(seek-bug ${llvm_libs} ${LLDB_LIBRARY} AICommands ${LLAMA_CPP_DIR}/lib/libllama.dylib)

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
