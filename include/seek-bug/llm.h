#pragma once

#include <string>

// This function will handle prompt creation, model loading, inference, etc.
std::string runLLM(const std::string &prompt, const std::string &modelPath);
