#include "wheel/llm/llm_factory.h"
#include "wheel/llm/imp/deepseek_engine.h"

namespace lynne {
namespace wheel {

LLMEngine* LLMEngineFactory::create(const LLMConfig& config) const {
    return new DeepSeekEngine(config);
}

} // namespace wheel
} // namespace lynne
