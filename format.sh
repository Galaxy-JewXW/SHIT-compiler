#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
cd "$SCRIPT_DIR"

echo "Formatting C/C++ code..."
git ls-files '*.cpp' '*.hpp' '*.c' '*.h' | xargs clang-format -i
echo "Formatting complete."
