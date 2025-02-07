add_library(AICommands STATIC
    AICommands.cpp
    llm.cpp
)

target_include_directories(AICommands
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        /usr/lib/llvm-19/include/
)

# TODO: Fix this.
if(APPLE)
  target_link_libraries(AICommands
      PRIVATE
          ${LLDB_LIBRARY}
          ${LLAMA_CPP_DIR}/lib/libllama.dylib
          ${llvm_libs}
  )
else()
  # On Linux or other Unix-like systems
  target_link_libraries(AICommands
      PRIVATE
          ${LLDB_LIBRARY}
          ${LLAMA_CPP_DIR}/lib/libllama.so
          ${llvm_libs}
  )
endif()

# This target builds as a shared library (LLDB plugin) and links
# against the static library AICommands.
add_library(SeekBugPlugin SHARED
    Plugin.cpp
    # If your plugin consists solely of Plugin.cpp, list it here.
    # If there are other plugin-specific files, add them here.
)

target_include_directories(SeekBugPlugin
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

# Link the plugin against the static library (and indirectly, LLDB/llama)
target_link_libraries(SeekBugPlugin
    PRIVATE
        AICommands
)

set_target_properties(SeekBugPlugin PROPERTIES
    INSTALL_RPATH "@loader_path"
    MACOSX_RPATH ON
    LINK_FLAGS "-Wl,-export_dynamic"
)

set_target_properties(SeekBugPlugin
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
