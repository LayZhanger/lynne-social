#include "wheel/llm/llm_factory.h"
#include "wheel/llm/imp/deepseek_engine.h"
#include <stdexcept>

namespace lynne {
namespace wheel {

LLMEngine* LLMEngineFactory::create(const LLMConfig& config) const {
    if (config.api_key.empty()) {
        throw std::runtime_error("LLMConfig.api_key is required");
    }
    return new DeepSeekEngine(config);
}

} // namespace wheel
} // namespace lynne
