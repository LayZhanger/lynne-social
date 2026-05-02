#pragma once

#include <string>

namespace lynne {
namespace common {

class Module {
public:
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool health_check() = 0;
    virtual std::string name() const = 0;
    virtual ~Module() = default;
};

} // namespace common
} // namespace lynne
