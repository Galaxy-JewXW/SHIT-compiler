#include "Mir/Interpreter.h"

using namespace Mir;

void Interpreter::interpret_abort() {
    throw std::runtime_error("Interpreter abort");
}
