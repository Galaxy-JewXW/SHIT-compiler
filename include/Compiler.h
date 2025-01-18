#ifndef COMPILER_H
#define COMPILER_H

#include <string>
#include <vector>
#include <iostream>
#include "Frontend/Lexer.h"
#include "Frontend/Parser.h"
#include "Mir/Value.h"
#include "Utils/Log.h"

extern bool options[28];

int main(int, char *[]);

#endif
