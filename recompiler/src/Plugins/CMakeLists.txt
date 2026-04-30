set(CLANG_LIBS
    clangAST
    clangBasic
    clangFrontend
    clangRewrite
    clangTooling
)

include_directories(include)
include_directories(${CLANG_INCLUDE_DIRS})

add_library(RecompilerPlugin MODULE src/CorrectBoolType.cpp src/RegExprToInlineAsm.cpp)

target_link_libraries(RecompilerPlugin PRIVATE ${CLANG_LIBS})
