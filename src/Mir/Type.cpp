#include "Mir/Type.h"

namespace Mir::Type {
const std::shared_ptr<Integer> Integer::i1 = std::make_shared<Integer>(1);
const std::shared_ptr<Integer> Integer::i32 = std::make_shared<Integer>(32);
const std::shared_ptr<Float> Float::f32 = std::make_shared<Float>();
const std::shared_ptr<Void> Void::void_ = std::make_shared<Void>();
const std::shared_ptr<Label> Label::label = std::make_shared<Label>();

std::shared_ptr<Type> Array::get_atomic_type() const {
    std::shared_ptr<Type> current = element_type;
    while (current->is_array()) {
        current = std::static_pointer_cast<Array>(current)->get_element_type();
    }
    return current;
}

size_t Array::get_flattened_size() const {
    size_t result = size;
    std::shared_ptr<Type> current = element_type;
    while (current->is_array()) {
        result *= std::static_pointer_cast<Array>(current)->get_size();
        current = std::static_pointer_cast<Array>(current)->get_element_type();
    }
    return result;
}
}
