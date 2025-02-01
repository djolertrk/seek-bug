//===-------- AICommands.cpp ----------------------------------------------===//
//
// Part of the SeekBug Project, under the Apache License v2.0.
// See LICENSE for details.
//
//===----------------------------------------------------------------------===//

#include "seek-bug/AICommands.h"
#include "seek-bug/llm.h"

#include <lldb/API/SBAddress.h>
#include <lldb/API/SBBreakpointLocation.h>
#include <lldb/API/SBBreakpointName.h>
#include <lldb/API/SBBroadcaster.h>
#include <lldb/API/SBCommandInterpreter.h>
#include <lldb/API/SBCommandInterpreterRunOptions.h>
#include <lldb/API/SBCommandReturnObject.h>
#include <lldb/API/SBCommunication.h>
#include <lldb/API/SBDebugger.h>
#include <lldb/API/SBDeclaration.h>
#include <lldb/API/SBEnvironment.h>
#include <lldb/API/SBError.h>
#include <lldb/API/SBEvent.h>
#include <lldb/API/SBExecutionContext.h>
#include <lldb/API/SBExpressionOptions.h>
#include <lldb/API/SBFile.h>
#include <lldb/API/SBFileSpec.h>
#include <lldb/API/SBFormat.h>
#include <lldb/API/SBFrame.h>
#include <lldb/API/SBHostOS.h>
#include <lldb/API/SBInstruction.h>
#include <lldb/API/SBListener.h>
#include <lldb/API/SBMemoryRegionInfoList.h>
#include <lldb/API/SBModuleSpec.h>
#include <lldb/API/SBStream.h>
#include <lldb/API/SBTarget.h>
#include <lldb/API/SBThread.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace seekbug {

using namespace lldb;

static std::string StopReasonToString(lldb::SBThread &thread) {
  // We first try to retrieve a short textual description using
  // the (buffer, length) version of GetStopDescription.
  constexpr size_t BUF_SIZE = 512;
  char buffer[BUF_SIZE];
  memset(buffer, 0, BUF_SIZE);

  size_t len = thread.GetStopDescription(buffer, BUF_SIZE);
  // len is the number of characters actually written. If len==0,
  // there's no description, so we can fallback to a switch-case.
  if (len > 0 && len < BUF_SIZE) {
    // We got a valid description
    return std::string(buffer, len);
  }

  // If we don’t have a description, we can do a switch-case based on the enum
  switch (thread.GetStopReason()) {
  case lldb::eStopReasonNone:
    return "None";
  case lldb::eStopReasonTrace:
    return "Trace";
  case lldb::eStopReasonBreakpoint:
    return "Breakpoint";
  case lldb::eStopReasonWatchpoint:
    return "Watchpoint";
  case lldb::eStopReasonSignal:
    return "Signal";
  case lldb::eStopReasonException:
    return "Exception";
  case lldb::eStopReasonPlanComplete:
    return "PlanComplete";
  default:
    return "Unknown";
  }
}

static std::string GetSourceSnippet(const lldb::SBFileSpec &fileSpec,
                                    uint32_t centerLine, int contextLines = 2) {
  // Build the full path from the SBFileSpec
  // (SBFileSpec can separate directory and filename)
  std::string path;
  if (fileSpec.GetDirectory() && *fileSpec.GetDirectory()) {
    path = fileSpec.GetDirectory();
    // Ensure we have a slash
    if (!path.empty() && path.back() != '/' && path.back() != '\\') {
      path.push_back('/');
    }
  }
  if (fileSpec.GetFilename() && *fileSpec.GetFilename()) {
    path += fileSpec.GetFilename();
  }

  // Attempt to open the file
  std::ifstream infile(path);
  if (!infile.is_open()) {
    return ""; // Couldn’t read the file
  }

  // We’ll grab lines in [startLine, endLine], inclusive
  int startLine = std::max(1, (int)centerLine - contextLines);
  int endLine = (int)centerLine + contextLines;

  std::ostringstream snippet;
  std::string lineText;
  int currentLine = 1;

  // Read until we pass endLine or reach EOF
  while (std::getline(infile, lineText)) {
    if (currentLine >= startLine && currentLine <= endLine) {
      // Mark the “current line” with a pointer or arrow, if you like
      if (currentLine == (int)centerLine) {
        snippet << "-> " << currentLine << ": " << lineText << "\n";
      } else {
        snippet << "   " << currentLine << ": " << lineText << "\n";
      }
    }
    if (currentLine > endLine) {
      break;
    }
    currentLine++;
  }

  return snippet.str();
}

std::string createRichPrompt(lldb::SBDebugger &debugger,
                             const std::string &userQuery) {
  std::ostringstream promptStream;

  promptStream << "You are a helpful AI assistant integrated with LLDB. "
               << "You have the following debugging context:\n\n";

  lldb::SBTarget target = debugger.GetSelectedTarget();
  if (target.IsValid()) {
    promptStream << "Target: " << target.GetExecutable().GetFilename() << "\n";

    lldb::SBProcess process = target.GetProcess();
    if (process.IsValid()) {
      promptStream << "Process: ID=" << process.GetProcessID() << ", State="
                   << lldb::SBDebugger::StateAsCString(process.GetState())
                   << "\n";

      lldb::SBThread thread = process.GetSelectedThread();
      if (thread.IsValid()) {
        promptStream << "Thread: ID=" << thread.GetThreadID()
                     << ", Name=" << (thread.GetName() ? thread.GetName() : "")
                     << ", StopReason=" << StopReasonToString(thread) << "\n";

        lldb::SBFrame frame = thread.GetSelectedFrame();
        if (frame.IsValid()) {
          // Retrieve module name
          std::string moduleName;
          lldb::SBModule mod = frame.GetModule();
          if (mod.IsValid()) {
            moduleName = mod.GetFileSpec().GetFilename();
          }

          // Retrieve symbol name (e.g., current function)
          lldb::SBSymbol symbol = frame.GetSymbol();
          std::string symbolName = symbol.IsValid() ? symbol.GetName() : "";

          promptStream << "Frame " << frame.GetFrameID() << " in binary "
                       << moduleName << " from function " << symbolName << "\n";

          // ---[ Get line entry ]-------------------------------------------
          lldb::SBLineEntry lineEntry = frame.GetLineEntry();
          if (lineEntry.IsValid()) {
            lldb::SBFileSpec fileSpec = lineEntry.GetFileSpec();
            uint32_t line = lineEntry.GetLine();
            uint32_t column = lineEntry.GetColumn();

            promptStream << "Source location: " << fileSpec.GetFilename() << ":"
                         << line << ":" << column << "\n";

            // Optionally gather a snippet of the source code around this line
            std::string snippet =
                GetSourceSnippet(fileSpec, line, /* contextLines = */ 2);
            if (!snippet.empty()) {
              promptStream << "Source snippet around line " << line << ":\n";
              promptStream << snippet << "\n";
            }
          }
        }
      }
    }
  }

  promptStream << "\n---\n"
               << "User Question: " << userQuery << "\n"
               << "Answer carefully and concisely. Focus on control flow. You "
                  "can do it! And the answer should not be too large, use a "
                  "few sentences. Do not print </think> and things after it. "
                  "Use up to 5 sentences.\n";

  return promptStream.str();
}

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

  // Hardcode or configure the model path somewhere:
  // You could read from environment variable or a config file:
  // std::string modelPath = "/Users/djtodorovic/projects/SeekBug/"
  //                         "DeepSeek-R1-Distill-Llama-8B-Q8_0.gguf";
  std::string prompt = createRichPrompt(debugger, userInput);
  std::string response = runLLM(prompt, context.DeepSeekLLMPath);

  result.SetStatus(lldb::eReturnStatusSuccessFinishResult);
  // result.Printf("[AI Suggestion] You asked: %s\n", userInput.c_str());
  // result.Printf("[AI Suggestion] prompt: %s\n", prompt.c_str());
  result.Printf("%s\n", response.c_str());

  return true;
}

bool RegisterAICommands(lldb::SBCommandInterpreter &interpreter,
                        SeekBugContext &context) {
  // Add the main 'ai' multiword command, which groups sub-commands.
  lldb::SBCommand aiCmd =
      interpreter.AddMultiwordCommand("ai", "AI-based commands");
  if (!aiCmd.IsValid()) {
    // Something went wrong registering the multiword command
    return false;
  }

  // Add the sub-command "suggest" to the "ai" group.
  static AISuggestCommand *suggestCmdImpl = new AISuggestCommand(context);
  lldb::SBCommand suggestCmd = aiCmd.AddCommand(
      "suggest", suggestCmdImpl,
      "Ask the AI for suggestions. Usage: ai suggest <question>");

  return suggestCmd.IsValid();
}

} // end namespace seekbug
