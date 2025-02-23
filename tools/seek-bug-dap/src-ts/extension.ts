import * as vscode from "vscode";
import { LLDBDapOptions } from "./types";
import { DisposableContext } from "./disposable-context";
import { LLDBDapDescriptorFactory } from "./debug-adapter-factory";

/**
 * This creates the configurations for this project if used as a standalone
 * extension.
 */
function createDefaultLLDBDapOptions(): LLDBDapOptions {
  return {
    debuggerType: "seek-bug-dap",
    async createDapExecutableCommand(
      session: vscode.DebugSession,
      packageJSONExecutable: vscode.DebugAdapterExecutable | undefined,
    ): Promise<vscode.DebugAdapterExecutable | undefined> {
      const path = vscode.workspace
        .getConfiguration("seek-bug-dap", session.workspaceFolder)
        .get<string>("executable-path");
      if (path) {
        return new vscode.DebugAdapterExecutable(path, []);
      }
      return packageJSONExecutable;
    },
  };
}

/**
 * This class represents the extension and manages its life cycle. Other extensions
 * using it as as library should use this class as the main entry point.
 */
export class LLDBDapExtension extends DisposableContext {
  private lldbDapOptions: LLDBDapOptions;

  constructor(lldbDapOptions: LLDBDapOptions) {
    super();
    this.lldbDapOptions = lldbDapOptions;

    this.pushSubscription(
      vscode.debug.registerDebugAdapterDescriptorFactory(
        this.lldbDapOptions.debuggerType,
        new LLDBDapDescriptorFactory(this.lldbDapOptions),
      ),
    );
  }
}

/**
 * This is the entry point when initialized by VS Code.
 */
export function activate(context: vscode.ExtensionContext) {
  context.subscriptions.push(
    new LLDBDapExtension(createDefaultLLDBDapOptions())
  );

  const workspaceFolder = vscode.workspace.workspaceFolders ? vscode.workspace.workspaceFolders[0].uri : undefined;
  const deepSeekLlmPath = vscode.workspace
    .getConfiguration("seek-bug-dap", workspaceFolder)
    .get<string>("deep-seek-llm-path");

  // Register AI Suggest command.
  context.subscriptions.push(
    vscode.commands.registerCommand("seek-bug.aiSuggest", async () => {
      const question = await vscode.window.showInputBox({
        prompt: "Enter your AI suggestion question:"
      });
      if (!question) { return; }
      const session = vscode.debug.activeDebugSession;
      if (session) {
        try {
          const response = await session.customRequest("aiRequest", {
            command: `suggest ${question}`,
            deepSeekLlmPath: deepSeekLlmPath
          });
          vscode.window.showInformationMessage(`AI Suggestion: ${response.body.output}`);
        } catch (err) {
          vscode.window.showErrorMessage(`Error: ${err}`);
        }
      } else {
        vscode.window.showErrorMessage("No active debug session.");
      }
    })
  );
  
  // Register additional commands similarly: aiExplain, aiFix, etc.
  // (Ensure you use the exact command IDs as defined in package.json.)
}
