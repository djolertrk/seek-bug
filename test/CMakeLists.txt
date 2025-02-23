# Find the paths to FileCheck and not
find_program(FILECHECK_EXE NAMES FileCheck FileCheck-18 FileCheck-19)
find_program(NOT_EXE NAMES not not-18 not-19)

if(NOT FILECHECK_EXE)
    message(FATAL_ERROR "FileCheck executable not found!")
endif()
if(NOT NOT_EXE)
    message(FATAL_ERROR "'not' executable not found!")
endif()

# Pass the paths to lit via configure_file
set(FILECHECK_PATH "${FILECHECK_EXE}")
set(NOT_PATH "${NOT_EXE}")

configure_lit_site_cfg(
        ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.py.in
        ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg.py
        MAIN_CONFIG
        ${CMAKE_CURRENT_SOURCE_DIR}/lit.cfg.py
)

include(TableGen)
include(AddLLVM)

# Find llvm-lit
find_program(LLVM_LIT NAMES llvm-lit lit)

if(NOT LLVM_LIT)
    message(FATAL_ERROR "llvm-lit not found. Please install it or specify its location.")
else()
    message(STATUS "Found llvm-lit: ${LLVM_LIT}")
endif()

# Check and set LLVM_EXTERNAL_LIT
if(DEFINED LLVM_EXTERNAL_LIT AND EXISTS ${LLVM_EXTERNAL_LIT})
    message(STATUS "Using LLVM_EXTERNAL_LIT: ${LLVM_EXTERNAL_LIT}")
else()
    message(WARNING "LLVM_EXTERNAL_LIT not set or invalid, using LLVM_LIT instead")
    set(LLVM_EXTERNAL_LIT ${LLVM_LIT} CACHE FILEPATH "Path to llvm-lit" FORCE)
endif()

# Define the custom target if CROSS_TESTS is ON
add_lit_testsuite(check-seek-bug "Running the SeekBug regression tests"
    ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${SEEKBUG_TEST_DEPENDS}
    # ARGS --threads=1
)

set_target_properties(check-seek-bug PROPERTIES FOLDER "Tests")

# Add the main lit testsuites
add_lit_testsuites(SeekBug ${CMAKE_CURRENT_SOURCE_DIR} DEPENDS ${SEEKBUG_TEST_DEPENDS})
