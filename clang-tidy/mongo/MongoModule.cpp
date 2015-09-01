
#include "../ClangTidy.h"
#include "../ClangTidyModule.h"
#include "../ClangTidyModuleRegistry.h"
#include "CommentStyle.h"
#include "NamingRules.h"


namespace clang {
namespace tidy {

class MongoModule: public ClangTidyModule {
 public:
  void addCheckFactories(ClangTidyCheckFactories &CheckFactories) override {
    CheckFactories.registerCheck<CommentStyleCheck>(
        "mongo-comment-style");
    CheckFactories.registerCheck<NamingRulesCheck>(
        "mongo-naming-rules");
        CheckFactories.registerCheck<IncludeSortOrderCheck>("mongo-include-order");

  }
};

static ClangTidyModuleRegistry::Add<MongoModule> X("mongo-module",
                                                "Adds my lint checks.");

// This anchor is used to force the linker to link in the generated object file
// and thus register the MyModule.
volatile int MongoModuleAnchorSource = 0;

}
}
