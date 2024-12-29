include_directories(../include)
include_directories(Frontend)
include_directories(Utils)

aux_source_directory(Frontend FRONTEND)
add_executable(compiler Compiler.cpp ${FRONTEND})

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)