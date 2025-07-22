aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/RISC-V RV_INSTS)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/RISC-V/RegisterAllocator RV_RA)
set(BACKEND_INSTS ${RV_INSTS} ${RV_RA} PARENT_SCOPE)