#include "Mir/IRType.h"

namespace IRTYPE {

    size_t IRArrayType::getSize() const {
        return size;
    };

    std::shared_ptr<IRType> IRArrayType::getElementType() const {
        return elementType;
    }

    std::shared_ptr<IRType> IRPointerType::getContainType() const {
        return containType;
    }

}
