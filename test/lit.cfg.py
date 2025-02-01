# -*- Python -*-

import os
import platform
import re
import subprocess
import tempfile

import lit.formats
import lit.util

from lit.llvm import llvm_config
from lit.llvm.subst import ToolSubst
from lit.llvm.subst import FindTool

# Configuration file for the 'lit' test runner.

# name: The name of this test suite.
config.name = "SeekBug"

config.test_format = lit.formats.ShTest(not llvm_config.use_lit_shell)

# suffixes: A list of file extensions to treat as test files.
config.suffixes = [".c", ".test"]

# test_source_root: The root path where tests are located.
config.test_source_root = os.path.dirname(__file__)

# test_exec_root: The root path where tests should be run.
config.test_exec_root = os.path.join(config.seekbug_obj_root, "test")

config.substitutions.append(("%PATH%", config.environment["PATH"]))

llvm_config.with_system_environment(["HOME", "INCLUDE", "LIB", "TMP", "TEMP"])

# excludes: A list of directories to exclude from the testsuite. The 'Inputs'
# subdirectories contain auxiliary inputs for various tests in their parent
# directories.
config.excludes = ["Inputs", "Examples", "CMakeLists.txt", "README.txt", "LICENSE.txt", "Artefacts", "test-artefacts"]

# test_exec_root: The root path where tests should be run.
config.test_exec_root = os.path.join(config.seekbug_obj_root, "test")

# Get the paths from the site configuration
filecheck_path = getattr(config, 'filecheck_path', 'FileCheck')
not_path = getattr(config, 'not_path', 'not')

# Add substitutions
config.substitutions.append(('%FileCheck', filecheck_path))
config.substitutions.append(('%not', not_path))
config.substitutions.append(('%seek-bug', config.seekbug_bin_path))
config.substitutions.append(("%seekbug_testdir", config.seekbug_obj_root))
