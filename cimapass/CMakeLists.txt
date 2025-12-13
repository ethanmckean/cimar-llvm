# Build base CIMA pass
add_llvm_pass_plugin(CIMAPass
    cimapass.cpp
    PARTIAL_SOURCES_INTENDED
)

# Build nearest valid CIMA pass
add_llvm_pass_plugin(CIMAPassNearestValid
    cimapass_nearest_valid.cpp
    PARTIAL_SOURCES_INTENDED
)

# Build tainted CIMA pass
add_llvm_pass_plugin(CIMAPassTainted
    cimapass_tainted.cpp
    PARTIAL_SOURCES_INTENDED
)

# Build runtime library as object file
add_library(cima_runtime OBJECT
    cima_runtime.cpp
)

set_target_properties(cima_runtime PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_STANDARD 17
)

# Create a custom target to copy runtime object to build directory
add_custom_target(copy_cima_runtime ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/cima_runtime.dir/cima_runtime.cpp.o"
        "${CMAKE_BINARY_DIR}/cimapass/cima_runtime.o"
    COMMENT "Copying cima_runtime.o to build directory"
    DEPENDS cima_runtime
)
