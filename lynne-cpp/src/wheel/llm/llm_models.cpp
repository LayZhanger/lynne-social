#include "wheel/llm/llm_models.h"

namespace lynne {
namespace wheel {

void from_json(const nlohmann::json& j, LLMConfig& c) {
    c.provider = j.value("provider", "deepseek");
    c.api_key = j.value("api_key", "");
    c.base_url = j.value("base_url", "");
    c.model = j.value("model", "deepseek-chat");
    c.temperature = j.value("temperature", 0.7);
    c.max_tokens = j.value("max_tokens", 4096);
    c.timeout = j.value("timeout", 60);
    c.ca_cert_path = j.value("ca_cert_path", "");
}

} // namespace wheel
} // namespace lynne
