include_directories(../include)
include_directories(Frontend)
include_directories(Utils)

aux_source_directory(Frontend FRONTEND)
aux_source_directory(Utils UTILS)
aux_source_directory(Mir MIR)
add_executable(compiler Compiler.cpp ${UTILS} ${FRONTEND} ${MIR})

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)