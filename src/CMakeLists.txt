include_directories(../include)

aux_source_directory(Frontend FRONTEND)
aux_source_directory(Utils UTILS)
aux_source_directory(Mir MIR)
aux_source_directory(Pass/Analysis PASS_ANALYSIS)
aux_source_directory(Pass/Transform PASS_TRANSFORM)
aux_source_directory(Pass/Utility PASS_UTILITY)
set(PASS ${PASS_ANALYSIS} ${PASS_TRANSFORM} ${PASS_UTILITY})

add_executable(compiler Compiler.cpp ${UTILS} ${FRONTEND} ${MIR} ${PASS})

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)