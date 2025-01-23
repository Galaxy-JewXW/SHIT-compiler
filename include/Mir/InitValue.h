#ifndef INITVALUE_H
#define INITVALUE_H

#include <string>

namespace Mir {
class InitValue {
public:
    virtual ~InitValue() = default;

    [[nodiscard]] virtual std::string to_string() const = 0;
};
}

#endif
