//===-------- Plugin.cpp ----------------------------------------------===//
//
// Part of the SeekBug Project, under the Apache License v2.0.
// See LICENSE for details.
//
// This file implements the LLDB plugin entry point that registers
// the SeekBug AI commands (which use DeepSeek LLM and LLDB).
//
//===----------------------------------------------------------------------===//

#include "seek-bug/AICommands.h"
#include "seek-bug/SeekBugContext.h"

#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

namespace lldb {

// The required LLDB plugin initialization function.
bool PluginInitialize(SBDebugger debugger) {
  // Retrieve the command interpreter from the debugger.
  SBCommandInterpreter interpreter = debugger.GetCommandInterpreter();
  debugger.SetPrompt("(seek-bug) ");

  // Create our configuration context.
  SeekBugContext context;

  // Get the path to the DeepSeek LLM model from an environment variable.
  const char *env_llm = std::getenv("DEEP_SEEK_LLM_PATH");
  if (env_llm)
    context.DeepSeekLLMPath = std::string(env_llm);
  else {
    llvm::WithColor::error()
        << "DEEP_SEEK_LLM_PATH environment variable is not set.\n";
    return false;
  }

  if (!fs::exists(context.DeepSeekLLMPath)) {
    llvm::WithColor::error()
        << "Model file " << context.DeepSeekLLMPath << " does not exist!\n";
    return false;
  }

  fs::path modelPath(context.DeepSeekLLMPath);
  if (modelPath.filename() != "DeepSeek-R1-Distill-Llama-8B-Q8_0.gguf") {
    llvm::WithColor::error()
        << "Unexpected model file: " << modelPath.filename() << "\n";
    return false;
  }

  // Register our AI commands (for example, "ai suggest") using our existing
  // API.
  if (!seekbug::RegisterAICommands(interpreter, context)) {
    llvm::WithColor::error() << "Failed to register SeekBug commands.\n";
    return false;
  }

  llvm::WithColor(llvm::outs(), llvm::HighlightColor::String)
      << "SeekBug is using " << context.DeepSeekLLMPath << "\n";
  llvm::WithColor(llvm::outs(), llvm::HighlightColor::String)
      << "SeekBug plugin loaded successfully.\n";

  return true;
}

} // namespace lldb
