#pragma once

//===-------- AICommands.h ------------------------------------------------===//
//
// Part of the SeekBug Project, under the Apache License v2.0.
// See LICENSE for details.
//
//===----------------------------------------------------------------------===//

#include "seek-bug/SeekBugContext.h"

#include <lldb/API/SBCommandInterpreter.h>

namespace seekbug {

/// A simple command that uses an "AI" to suggest something to the user.
/// TODO: Replace with real logic for your LLM.
class AISuggestCommand : public lldb::SBCommandPluginInterface {
  SeekBugContext context;

public:
  AISuggestCommand(SeekBugContext &context) : context(context) {}

  /// The main entry point for your custom command.
  /// `command` is the string typed after `ai suggest`.
  /// Use `result` to output data to the LLDB console.
  bool DoExecute(lldb::SBDebugger debugger, char **command,
                 lldb::SBCommandReturnObject &result) override;
};

/// Register all AI-related commands in the LLDB interpreter.
/// E.g., an 'ai' multiword command and a 'suggest' sub-command.
bool RegisterAICommands(lldb::SBCommandInterpreter &interpreter,
                        SeekBugContext &context);

} // end namespace seekbug
