#pragma once

#include "wheel/browser/browser_manager.h"

namespace lynne {
namespace wheel {

class BrowserFactory {
public:
    BrowserManager* create(const BrowserConfig& config = {}) const;
};

} // namespace wheel
} // namespace lynne
