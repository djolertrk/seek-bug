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
    promptStream << "Program name is: " << target.GetExecutable().GetFilename()
                 << "\n";
    promptStream << "User created a breakpoint and execution stopped at this "
                    "program point:\n";

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

  promptStream
      << "\n---\n"
      << "User Question: " << userQuery << "\n"
      << "Answer carefully and concisely. Focus on control flow. You "
         "can do it! And the answer should not be too large, use a "
         "few sentences. Do not print </think> and things after it. "
         "Use up to 5 sentences.\n If the question is not related to "
         "this program or program point, just say that you are here "
         "to answer questions about this program, and that you cannot "
         "think about something else. Do not print duplicated answers. Take a "
         "breath, relax and do it!\n\n"
         "Print answer only, DO NOT print thinking process. Print answer only! "
         "Do not print </think> and all things before it!\n\n";

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

bool AICrashElaborateCommand::DoExecute(lldb::SBDebugger debugger,
                                        char **command,
                                        lldb::SBCommandReturnObject &result) {
  int argCount = 0;
  for (int i = 0; command[i] != nullptr; ++i) {
    argCount++;
  }
  if (argCount != 1) {
    result.Printf("Usage: ai crash-elaborate <path to corefile>\n");
    result.SetStatus(lldb::eReturnStatusFailed);
    return false;
  }

  std::string coreFilePath = command[0];
  // Retrieve the current target.
  lldb::SBTarget target = debugger.GetSelectedTarget();
  if (!target.IsValid()) {
    result.Printf("No valid target selected.\n");
    result.SetStatus(lldb::eReturnStatusFailed);
    return false;
  }

  // Load the core file.
  lldb::SBProcess process = target.LoadCore(coreFilePath.c_str());
  if (!process.IsValid()) {
    result.Printf("Failed to load core file: %s\n", coreFilePath.c_str());
    result.SetStatus(lldb::eReturnStatusFailed);
    return false;
  }

  // Get the selected thread from the loaded process.
  lldb::SBThread thread = process.GetSelectedThread();
  if (!thread.IsValid()) {
    result.Printf("No valid thread in the loaded core file.\n");
    result.SetStatus(lldb::eReturnStatusFailed);
    return false;
  }

  // TODO: This takes more tokens in prompt!
  // Gather register information from the first frame.
  // std::ostringstream registersStream;
  // lldb::SBFrame frame = thread.GetFrameAtIndex(0);
  // if (frame.IsValid()) {
  //   lldb::SBValueList regGroups = frame.GetRegisters();
  //   for (uint32_t i = 0; i < regGroups.GetSize(); i++) {
  //     lldb::SBValue regSet = regGroups.GetValueAtIndex(i);
  //     registersStream << "Register set: " << regSet.GetName() << "\n";
  //     for (uint32_t j = 0; j < regSet.GetNumChildren(); j++) {
  //       lldb::SBValue reg = regSet.GetChildAtIndex(j);
  //       registersStream << "  " << reg.GetName() << " = "
  //                       << (reg.GetValue() ? reg.GetValue() : "N/A") <<
  //                       "\n";
  //     }
  //     break;
  //   }
  // } else {
  //   registersStream << "No valid frame available to retrieve registers.\n";
  // }

  // Gather the call stack information and corresponding source snippets.
  std::ostringstream callStackStream;
  int numFrames = thread.GetNumFrames();
  for (int i = 0; i < numFrames; i++) {
    lldb::SBFrame currFrame = thread.GetFrameAtIndex(i);
    if (currFrame.IsValid()) {
      lldb::SBLineEntry lineEntry = currFrame.GetLineEntry();
      lldb::SBFileSpec fileSpec = lineEntry.GetFileSpec();
      uint32_t line = lineEntry.GetLine();

      callStackStream << "#" << i << " " << currFrame.GetFunctionName()
                      << " at " << fileSpec.GetFilename() << ":" << line
                      << "\n";

      // Retrieve a snippet of the source around the current line.
      std::string snippet =
          GetSourceSnippet(fileSpec, line, /* contextLines = */ 2);
      if (!snippet.empty()) {
        callStackStream << "Source snippet:\n" << snippet << "\n";
      }
    }
  }

  // Build the prompt for the LLM.
  std::ostringstream promptStream;
  promptStream << "You are a helpful AI assistant integrated with LLDB for "
                  "crash analysis. You are expert for C/C++ as well.\n";
  // promptStream << "Registers:\n" << registersStream.str() << "\n";
  promptStream << "Program crashed with Call Stack:\n"
               << callStackStream.str() << "\n";
  promptStream << "---\n\n\n";
  promptStream << "Analyze the crash in detail and provide potential causes "
                  "and debugging suggestions.\n";

  std::string prompt = promptStream.str();
  std::string response = runLLM(prompt, context.DeepSeekLLMPath);

  result.SetStatus(lldb::eReturnStatusSuccessFinishResult);
  result.Printf("%s\n", response.c_str());
  return true;
}

//----------------------------------------------------------------------------//
// AIExplainCommand: Explains the current code snippet.
//----------------------------------------------------------------------------//

AIExplainCommand::AIExplainCommand(SeekBugContext &ctx) : context(ctx) {}

bool AIExplainCommand::DoExecute(lldb::SBDebugger debugger, char **command,
                                 lldb::SBCommandReturnObject &result) {
  // Retrieve the current frame.
  lldb::SBTarget target = debugger.GetSelectedTarget();
  if (!target.IsValid()) {
    result.Printf("No valid target selected.\n");
    result.SetStatus(lldb::eReturnStatusFailed);
    return false;
  }
  lldb::SBProcess process = target.GetProcess();
  if (!process.IsValid()) {
    result.Printf("No valid process.\n");
    result.SetStatus(lldb::eReturnStatusFailed);
    return false;
  }
  lldb::SBThread thread = process.GetSelectedThread();
  if (!thread.IsValid()) {
    result.Printf("No valid thread.\n");
    result.SetStatus(lldb::eReturnStatusFailed);
    return false;
  }
  lldb::SBFrame frame = thread.GetSelectedFrame();
  if (!frame.IsValid()) {
    result.Printf("No valid frame.\n");
    result.SetStatus(lldb::eReturnStatusFailed);
    return false;
  }
  lldb::SBLineEntry lineEntry = frame.GetLineEntry();
  lldb::SBFileSpec fileSpec = lineEntry.GetFileSpec();
  uint32_t line = lineEntry.GetLine();

  // Get a source snippet with 5 lines of context.
  std::string snippet =
      GetSourceSnippet(fileSpec, line, /* contextLines = */ 5);

  // Build prompt for explanation.
  std::ostringstream promptStream;
  promptStream << "You are an expert C/C++ code analyst. Please explain what "
                  "the following code does, "
               << "and highlight any potential issues:\n";
  promptStream << "File: " << fileSpec.GetFilename() << " at line " << line
               << "\n";
  promptStream << snippet << "\n";

  promptStream
      << "\n---\nAnswer carefully and concisely. You can do it! And the answer "
         "should not be too large, use a few sentences. Do not print </think> "
         "and things after it. Use up to 5 sentences.\n";

  std::string prompt = promptStream.str();

  // Run the LLM on the prompt.
  std::string response = runLLM(prompt, context.DeepSeekLLMPath);

  result.SetStatus(lldb::eReturnStatusSuccessFinishResult);
  result.Printf("%s\n", response.c_str());
  return true;
}

//----------------------------------------------------------------------------//
// AIStackSummaryCommand: Provides a summary of the current call stack.
//----------------------------------------------------------------------------//

AIStackSummaryCommand::AIStackSummaryCommand(SeekBugContext &ctx)
    : context(ctx) {}

bool AIStackSummaryCommand::DoExecute(lldb::SBDebugger debugger, char **command,
                                      lldb::SBCommandReturnObject &result) {
  // Retrieve the current target, process, and thread.
  lldb::SBTarget target = debugger.GetSelectedTarget();
  if (!target.IsValid()) {
    result.Printf("No valid target selected.\n");
    result.SetStatus(lldb::eReturnStatusFailed);
    return false;
  }
  lldb::SBProcess process = target.GetProcess();
  if (!process.IsValid()) {
    result.Printf("No valid process.\n");
    result.SetStatus(lldb::eReturnStatusFailed);
    return false;
  }
  lldb::SBThread thread = process.GetSelectedThread();
  if (!thread.IsValid()) {
    result.Printf("No valid thread.\n");
    result.SetStatus(lldb::eReturnStatusFailed);
    return false;
  }

  // Build a call stack string with source snippets.
  std::ostringstream callStackStream;
  int numFrames = thread.GetNumFrames();
  for (int i = 0; i < numFrames; i++) {
    lldb::SBFrame currFrame = thread.GetFrameAtIndex(i);
    if (currFrame.IsValid()) {
      lldb::SBLineEntry lineEntry = currFrame.GetLineEntry();
      lldb::SBFileSpec fileSpec = lineEntry.GetFileSpec();
      uint32_t line = lineEntry.GetLine();
      callStackStream << "#" << i << " " << currFrame.GetFunctionName()
                      << " at " << fileSpec.GetFilename() << ":" << line
                      << "\n";

      // Retrieve a snippet with 2 lines of context.
      std::string snippet = GetSourceSnippet(fileSpec, line, 2);
      if (!snippet.empty()) {
        callStackStream << "Snippet:\n" << snippet << "\n";
      }
    }
  }

  // Build prompt for stack summary.
  std::ostringstream promptStream;
  promptStream << "You are an expert debugger assistant. You are C/C++ expert "
                  "as well. Provide a summary of the following call stack, "
               << "including potential causes for errors and suggestions for "
                  "further investigation:\n\n";
  promptStream << callStackStream.str() << "\n";
  promptStream
      << "\n---\nAnswer carefully and concisely. You can do it! And the answer "
         "should not be too large, use a few sentences. Do not print </think> "
         "and things after it. Use up to 5 sentences.\n";

  std::string prompt = promptStream.str();

  // Run the LLM on the prompt.
  std::string response = runLLM(prompt, context.DeepSeekLLMPath);

  result.SetStatus(lldb::eReturnStatusSuccessFinishResult);
  result.Printf("%s\n", response.c_str());
  return true;
}

//----------------------------------------------------------------------------//
// AIFixCommand: Suggests a fix for the current code snippet.
//----------------------------------------------------------------------------//

AIFixCommand::AIFixCommand(SeekBugContext &ctx) : context(ctx) {}

bool AIFixCommand::DoExecute(lldb::SBDebugger debugger, char **command,
                             lldb::SBCommandReturnObject &result) {
  // Retrieve the current frame.
  lldb::SBTarget target = debugger.GetSelectedTarget();
  if (!target.IsValid()) {
    result.Printf("No valid target selected.\n");
    result.SetStatus(lldb::eReturnStatusFailed);
    return false;
  }
  lldb::SBProcess process = target.GetProcess();
  if (!process.IsValid()) {
    result.Printf("No valid process.\n");
    result.SetStatus(lldb::eReturnStatusFailed);
    return false;
  }
  lldb::SBThread thread = process.GetSelectedThread();
  if (!thread.IsValid()) {
    result.Printf("No valid thread.\n");
    result.SetStatus(lldb::eReturnStatusFailed);
    return false;
  }
  lldb::SBFrame frame = thread.GetSelectedFrame();
  if (!frame.IsValid()) {
    result.Printf("No valid frame.\n");
    result.SetStatus(lldb::eReturnStatusFailed);
    return false;
  }
  lldb::SBLineEntry lineEntry = frame.GetLineEntry();
  lldb::SBFileSpec fileSpec = lineEntry.GetFileSpec();
  uint32_t line = lineEntry.GetLine();

  // Retrieve a snippet with 5 lines of context.
  std::string snippet = GetSourceSnippet(fileSpec, line, 5);

  // Build prompt asking for a suggested fix.
  std::ostringstream promptStream;
  promptStream << "You are an expert C/C++ engineer. The following code "
                  "snippet may contain a bug. "
               << "Please suggest a fix along with an explanation:\n";
  promptStream << "File: " << fileSpec.GetFilename() << " at line " << line
               << "\n";
  promptStream << snippet << "\n";
  promptStream
      << "\n---\nAnswer carefully and concisely. You can do it! And the answer "
         "should not be too large, use a few sentences. Do not print </think> "
         "and things after it. Use up to 5 sentences.\n";

  std::string prompt = promptStream.str();

  // Run the LLM on the prompt.
  std::string response = runLLM(prompt, context.DeepSeekLLMPath);

  result.SetStatus(lldb::eReturnStatusSuccessFinishResult);
  result.Printf("%s\n", response.c_str());
  return true;
}

bool RegisterAICommands(lldb::SBCommandInterpreter &interpreter,
                        SeekBugContext &context) {
  // Add the main 'ai' multiword command, which groups sub-commands.
  lldb::SBCommand aiCmd =
      interpreter.AddMultiwordCommand("ai", "AI-based commands");
  if (!aiCmd.IsValid()) {
    return false;
  }

  // Add the "suggest" sub-command.
  static AISuggestCommand *suggestCmdImpl = new AISuggestCommand(context);
  lldb::SBCommand suggestCmd =
      aiCmd.AddCommand("suggest", suggestCmdImpl,
                       "Ask the AI for suggestions about your program. Usage: "
                       "ai suggest <question>");
  if (!suggestCmd.IsValid()) {
    return false;
  }

  // Add the "crash-elaborate" sub-command.
  static AICrashElaborateCommand *crashElaborateCmd =
      new AICrashElaborateCommand(context);
  lldb::SBCommand crashElabCmd =
      aiCmd.AddCommand("crash-elaborate", crashElaborateCmd,
                       "Analyze a crash by loading a core file. Usage: ai "
                       "crash-elaborate <path to corefile>");
  if (!crashElabCmd.IsValid()) {
    return false;
  }

  // Add the "explain" sub-command.
  static AIExplainCommand *explainCmd = new AIExplainCommand(context);
  lldb::SBCommand explainSB =
      aiCmd.AddCommand("explain", explainCmd,
                       "Explain the current code snippet. Usage: ai explain");
  if (!explainSB.IsValid()) {
    return false;
  }

  // Add the "stack-summary" sub-command.
  static AIStackSummaryCommand *stackSummaryCmd =
      new AIStackSummaryCommand(context);
  lldb::SBCommand stackSummarySB = aiCmd.AddCommand(
      "stack-summary", stackSummaryCmd,
      "Summarize the current call stack. Usage: ai stack-summary");
  if (!stackSummarySB.IsValid()) {
    return false;
  }

  // Add the "fix" sub-command.
  static AIFixCommand *fixCmd = new AIFixCommand(context);
  lldb::SBCommand fixSB = aiCmd.AddCommand(
      "fix", fixCmd,
      "Suggest a fix for the current code snippet. Usage: ai fix");
  if (!fixSB.IsValid()) {
    return false;
  }

  return true;
}

} // end namespace seekbug
