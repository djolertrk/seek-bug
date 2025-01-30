//===-------- AICommands.cpp ----------------------------------------------===//
//
// Part of the SeekBug Project, under the Apache License v2.0.
// See LICENSE for details.
//
//===----------------------------------------------------------------------===//

#include "seek-bug/AICommands.h"

#include <lldb/API/SBCommandInterpreter.h>
#include <lldb/API/SBCommandReturnObject.h>
#include <lldb/API/SBDebugger.h>
#include <lldb/API/SBStream.h>

#include <iostream>
#include <sstream>

namespace seekbug {

bool AISuggestCommand::DoExecute(lldb::SBDebugger debugger, char **command,
                                 lldb::SBCommandReturnObject &result) {
  std::ostringstream oss;
  if (command) {
    bool first = true;
    for (int i = 0; command[i] != nullptr; i++) {
      if (!first)
        oss << " ";
      oss << command[i];
      first = false;
    }
  }

  std::string userInput = oss.str();
  if (userInput.empty()) {
    result.SetStatus(lldb::eReturnStatusFailed);
    result.Printf("Usage: ai suggest <your question or context>\n");
    return false;
  }

  // Replace with real AI logic...
  result.SetStatus(lldb::eReturnStatusSuccessFinishResult);
  result.Printf("[AI Suggestion] You asked: %s\n", userInput.c_str());
  result.Printf(
      "[AI Suggestion] Here's a placeholder suggestion from the AI.\n");

  return true;
}

bool RegisterAICommands(lldb::SBCommandInterpreter &interpreter) {
  // Add the main 'ai' multiword command, which groups sub-commands
  lldb::SBCommand aiCmd =
      interpreter.AddMultiwordCommand("ai", "AI-based commands");
  if (!aiCmd.IsValid()) {
    // Something went wrong registering the multiword command
    return false;
  }

  // Now add the sub-command "suggest" to the "ai" group
  static AISuggestCommand *suggestCmdImpl = new AISuggestCommand();
  lldb::SBCommand suggestCmd = aiCmd.AddCommand(
      "suggest", suggestCmdImpl,
      "Ask the AI for suggestions. Usage: ai suggest <question>");

  return suggestCmd.IsValid();
}

} // end namespace seekbug
