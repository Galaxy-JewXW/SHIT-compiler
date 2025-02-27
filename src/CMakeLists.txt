include_directories(../include)

aux_source_directory(Frontend FRONTEND)
aux_source_directory(Utils UTILS)
aux_source_directory(Mir MIR)

add_subdirectory(Pass)

add_executable(compiler Compiler.cpp ${UTILS} ${FRONTEND} ${MIR} ${PASS}
        Pass/Analysis/LoopAnalysis.cpp
        Pass/Transform/LoopSimplyForm.cpp
)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)