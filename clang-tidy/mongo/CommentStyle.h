#include "../ClangTidy.h"

namespace clang {
namespace tidy {

class CommentStyleCheck: public ClangTidyCheck {
public:
    CommentStyleCheck(StringRef Name, ClangTidyContext *Context)
        : ClangTidyCheck(Name, Context) {}
    void registerMatchers(ast_matchers::MatchFinder *Finder) override;
    void check(const ast_matchers::MatchFinder::MatchResult &Result) override;

    void CheckComment(const comments::FullComment *Comment,
        RawComment* raw,
        SourceLocation loc,
        SourceManager& mgr,
        bool function);

};

} // namespace tidy
} // namespace clang
