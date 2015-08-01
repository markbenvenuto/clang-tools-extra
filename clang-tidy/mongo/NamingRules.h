#include "../ClangTidy.h"

namespace clang {
namespace tidy {

class NamingRulesCheck: public ClangTidyCheck {
public:
    NamingRulesCheck(StringRef Name, ClangTidyContext *Context)
        : ClangTidyCheck(Name, Context) {}
    void registerMatchers(ast_matchers::MatchFinder *Finder) override;
    void check(const ast_matchers::MatchFinder::MatchResult &Result) override;

};

} // namespace tidy
} // namespace clang
