separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

include_directories(include)
add_library(Recompiler MODULE src/FixCallingConv.cpp src/ForceDSOLocal.cpp src/StackLayout.cpp src/LocalVariableToArg.cpp src/ParseCmdOpts.cpp src/InsertInlineAsm.cpp src/StackCanary.cpp src/IndirectBrachTracking.cpp src/UpdatedPrologEpilogInserter.cpp)
