add_library(AICommands STATIC
    AICommands.cpp
)

target_include_directories(AICommands
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(AICommands
    PRIVATE
        ${LLDB_LIBRARY}
)
