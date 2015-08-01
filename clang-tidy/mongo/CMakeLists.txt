set(LLVM_LINK_COMPONENTS support)

add_clang_library(clangTidyMongoModule
  CommentStyle.cpp
  NamingRules.cpp
  MongoModule.cpp

  LINK_LIBS
  clangAST
  clangASTMatchers
  clangBasic
  clangLex
  clangTidy
  clangTooling
  )
