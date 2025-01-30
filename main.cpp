//===-------- main.cpp - Main driver for the tool -------------------------===//
//
// Part of the SeekBug Project, under the Apache License v2.0.
// See LICENSE for details.
//
//===----------------------------------------------------------------------===//
//
// This file provides the implementation of the main driver.
//
//===----------------------------------------------------------------------===//

#include "seek-bug/AICommands.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"

#include "lldb/API/SBCommandInterpreter.h"
#include "lldb/API/SBCommandReturnObject.h"
#include "lldb/API/SBEvent.h"
#include "lldb/API/SBLaunchInfo.h"
#include "lldb/API/SBListener.h"
#include "lldb/API/SBStream.h"
#include "lldb/API/SBThread.h"

#include <iostream>

using namespace llvm;

namespace {
using namespace cl;

OptionCategory SeekBugCategory("Specific Options");
static opt<bool> Help("h", desc("Alias for -help"), Hidden,
                      cat(SeekBugCategory));
static opt<std::string> InputFilename(Positional, desc("<input file>"),
                                      cat(SeekBugCategory));
} // namespace
/// @}
//===----------------------------------------------------------------------===//

int main(int argc, char **argv) {
  WithColor(llvm::outs(), HighlightColor::String)
      << "=== SeekBug - Modern, Portable and Deep Debugger\n";

  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <program to debug> " << std::endl;
    return 1;
  }

  std::string program = argv[1];

  // Initialize LLDB
  lldb::SBDebugger::Initialize();
  lldb::SBDebugger debugger = lldb::SBDebugger::Create();

  // Set debugger options
  debugger.SetAsync(false);

  // Create a target
  lldb::SBTarget target = debugger.CreateTarget(program.c_str());
  if (!target.IsValid()) {
    llvm::WithColor::error()
        << "Failed to create LLDB target for program: " << program << '\n';
    return 1;
  }

  debugger.SetPrompt("(seek-bug) ");
  // Register custom commands
  lldb::SBCommandInterpreter interpreter = debugger.GetCommandInterpreter();
  // TODO: register comamnd(s) dealing with LLM.
  seekbug::RegisterAICommands(interpreter);

  debugger.RunCommandInterpreter(true, false);

  lldb::SBDebugger::Destroy(debugger);
  lldb::SBDebugger::Terminate();

  WithColor(llvm::outs(), HighlightColor::String)
      << "=== Happy Debugging! Bye!\n";

  return 0;
}
