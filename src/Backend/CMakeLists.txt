add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/InstructionSets)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/LIR)

# Add VariableTypes.cpp
set(BACKEND_UTILS
    ${CMAKE_CURRENT_SOURCE_DIR}/VariableTypes.cpp
)

set(BACKEND ${BACKEND_INSTS} ${BACKEND_LIR} ${BACKEND_UTILS} PARENT_SCOPE)