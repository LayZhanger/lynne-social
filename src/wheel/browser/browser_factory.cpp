#include "wheel/browser/browser_factory.h"
#include "wheel/browser/imp/cdp_browser_manager.h"

namespace lynne {
namespace wheel {

BrowserManager* BrowserFactory::create(const BrowserConfig& config) const {
    return new CdpBrowserManager(config);
}

} // namespace wheel
} // namespace lynne
