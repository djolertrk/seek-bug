#include "seek-bug/llm.h"

#include "llama.h"

#include <llvm/Support/WithColor.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Example: discard all llama.cpp logs
static void llama_null_log_callback(enum ggml_log_level level, const char *text,
                                    void *user_data) {
  (void)level;
  (void)text;
  (void)user_data;
}

std::string runLLM(const std::string &prompt, const std::string &modelPath) {
  if (const char* debugEnv = std::getenv("DEBUG_SEEKBUG")) {
    if (std::string(debugEnv) == "1") {
      llvm::WithColor(llvm::outs(), llvm::HighlightColor::String)
          << "The prompt is: " << prompt << "\n";
    }
  }

  llvm::WithColor(llvm::outs(), llvm::HighlightColor::String)
      << "DeepSeek is thinking...\n";

  // Load the model
  llama_model_params model_params = llama_model_default_params();

  // Optionally disable logs:
  llama_log_set(llama_null_log_callback, nullptr);

  llama_model *model =
      llama_load_model_from_file(modelPath.c_str(), model_params);
  if (!model) {
    return "[Error] Could not load model from " + modelPath;
  }

  // Create a context from the model
  llama_context_params ctx_params = llama_context_default_params();
  llama_context *ctx = llama_init_from_model(model, ctx_params);
  if (!ctx) {
    llama_free_model(model);
    return "[Error] Could not create llama context from model.";
  }

  // 3) We need the vocab to tokenize
  const struct llama_vocab *vocab = llama_model_get_vocab(model);
  if (!vocab) {
    llama_free(ctx);
    llama_free_model(model);
    return "[Error] Could not retrieve vocab from model.";
  }

  // Tokenize the prompt.
  // This older API typically has the signature:
  //   llama_tokenize(vocab, text, text_len, tokens, n_tokens_max,
  //                  bool add_special, bool parse_special);
  std::vector<llama_token> prompt_tokens(2048);
  int n_prompt =
      llama_tokenize(vocab, prompt.c_str(), (int32_t)prompt.size(),
                     prompt_tokens.data(), (int32_t)prompt_tokens.size(),
                     /* add_special  */ true,
                     /* parse_special */ true);
  if (n_prompt < 0) {
    llama_free(ctx);
    llama_free_model(model);
    return "[Error] Failed to tokenize prompt.";
  }
  prompt_tokens.resize(n_prompt);

  // Evaluate the prompt tokens (decode them) so the model sees the prompt
  // context
  //    This older fork calls 'llama_decode(...)' with a 'llama_batch'.
  llama_batch batch = llama_batch_get_one(prompt_tokens.data(), n_prompt);
  if (llama_decode(ctx, batch) != 0) {
    llama_free(ctx);
    llama_free_model(model);
    return "[Error] Failed to decode prompt tokens.";
  }

  // Create a sampler chain (again, older API in some forks).
  auto sparams = llama_sampler_chain_default_params();
  llama_sampler *smpl = llama_sampler_chain_init(sparams);

  // For demonstration, a greedy sampler:
  llama_sampler_chain_add(smpl, llama_sampler_init_greedy());

  std::ostringstream ss;
  const int max_new_tokens = 256;
  for (int i = 0; i < max_new_tokens; i++) {
    // Sample next token
    llama_token token_id = llama_sampler_sample(smpl, ctx, -1);

    // Check if we got the end-of-generation signal
    // The older code uses 'llama_token_is_eog(vocab, token_id)'
    if (llama_token_is_eog(vocab, token_id)) {
      // End of generation
      break;
    }

    // Convert token to string
    char buf[256];
    int n = llama_token_to_piece(vocab, token_id, buf, sizeof(buf), 0, true);
    if (n < 0) {
      ss << "[Error: failed to convert token to piece]\n";
      break;
    }
    // Append token text
    std::string token_str(buf, n);
    ss << token_str;

    // Feed the newly generated token back into the decoder
    llama_batch next_batch = llama_batch_get_one(&token_id, 1);
    if (llama_decode(ctx, next_batch) != 0) {
      ss << "[Error: decode failure in generation loop]\n";
      break;
    }
  }

  // Cleanup
  // llama_sampler_chain_free(smpl);
  llama_free(ctx);
  llama_free_model(model);

  // 9) Return the final generated text
  return ss.str();
}
