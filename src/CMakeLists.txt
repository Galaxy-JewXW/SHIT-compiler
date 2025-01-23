include_directories(../include)
include_directories(Frontend)
include_directories(Utils)

aux_source_directory(Frontend FRONTEND)
aux_source_directory(Utils UTILS)
aux_source_directory(Mir MIR)
add_executable(compiler Compiler.cpp ${UTILS} ${FRONTEND} ${MIR}
        ../include/Mir/Symbol.h
        Mir/Symbol.cpp
        ../include/Mir/IRType.h
        Mir/IRType.cpp
        ../include/Mir/IRStructure.h
        ../include/Mir/IRInstruction.h
        Mir/IRBuilder.cpp
        ../include/Mir/IRBuilder.h
)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)