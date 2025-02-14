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

class AICrashElaborateCommand : public lldb::SBCommandPluginInterface {
  SeekBugContext context;

public:
  AICrashElaborateCommand(SeekBugContext &context) : context(context) {}
  bool DoExecute(lldb::SBDebugger debugger, char **command,
                 lldb::SBCommandReturnObject &result) override;
};

/// Command that explains the current code snippet.
class AIExplainCommand : public lldb::SBCommandPluginInterface {
  SeekBugContext context;

public:
  AIExplainCommand(SeekBugContext &context);
  bool DoExecute(lldb::SBDebugger debugger, char **command,
                 lldb::SBCommandReturnObject &result) override;
};

/// Command that provides a summary of the current call stack.
class AIStackSummaryCommand : public lldb::SBCommandPluginInterface {
  SeekBugContext context;

public:
  AIStackSummaryCommand(SeekBugContext &context);
  bool DoExecute(lldb::SBDebugger debugger, char **command,
                 lldb::SBCommandReturnObject &result) override;
};

/// Command that suggests a fix for the current code snippet.
class AIFixCommand : public lldb::SBCommandPluginInterface {
  SeekBugContext context;

public:
  AIFixCommand(SeekBugContext &context);
  bool DoExecute(lldb::SBDebugger debugger, char **command,
                 lldb::SBCommandReturnObject &result) override;
};

/// Register all AI-related commands in the LLDB interpreter.
/// E.g., an 'ai' multiword command and a 'suggest' sub-command.
bool RegisterAICommands(lldb::SBCommandInterpreter &interpreter,
                        SeekBugContext &context);

} // end namespace seekbug
