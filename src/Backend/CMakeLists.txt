add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/InstructionSets)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/MIR)

# Add VariableTypes.cpp
set(BACKEND_UTILS
    ${CMAKE_CURRENT_SOURCE_DIR}/VariableTypes.cpp
)

set(BACKEND ${BACKEND_INSTS} ${BACKEND_MIR} ${BACKEND_UTILS} PARENT_SCOPE)