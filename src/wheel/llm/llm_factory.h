#pragma once

#include "wheel/llm/llm_engine.h"
#include "wheel/llm/llm_models.h"

namespace lynne {
namespace wheel {

class LLMEngineFactory {
public:
    LLMEngine* create(const LLMConfig& config) const;
};

} // namespace wheel
} // namespace lynne
