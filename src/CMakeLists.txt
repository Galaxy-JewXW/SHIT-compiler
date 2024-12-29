include_directories (../include)
include_directories (Frontend)
include_directories (Utils)

aux_source_directory(Frontend FRONTEND)
aux_source_directory(Utils UTILS)
add_executable(compiler Compiler.cpp ${FRONTEND} ${UTILS})

set (EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)