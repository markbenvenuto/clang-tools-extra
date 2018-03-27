#include "NamingRules.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Lex/Lexer.h"
#include "clang/AST/Comment.h"

using namespace clang::ast_matchers;


namespace clang {
namespace tidy {
void NamingRulesCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(recordDecl().bind("id"), this);
  Finder->addMatcher(cxxMethodDecl(hasAncestor(recordDecl())).bind("id"), this);
  Finder->addMatcher(fieldDecl(hasAncestor(recordDecl())).bind("id"), this);
}

void NamingRulesCheck::check(const MatchFinder::MatchResult &Result) {
    const CXXRecordDecl *decl =
        Result.Nodes.getNodeAs<CXXRecordDecl>("id");
    if (!decl) {
        const CXXMethodDecl *d2 =
            Result.Nodes.getNodeAs<CXXMethodDecl>("id");

        if (!d2) {
            const FieldDecl *d3 =
                Result.Nodes.getNodeAs<FieldDecl>("id");

            SourceLocation Loc = d3->getLocation();
            if (!Loc.isValid()) {
                return;
            }

            if (!d3->getDeclName()) {
                return;
            }

            std::string str = d3->getNameAsString();
            char c1 = str[0];
            char c2 = str.size() > 1 ? str[1] : 'a';
            if (c1 != '_' || tolower(c2) != c2) {
                diag(Loc, "Fields should start with '_' (underscore) followed by lower case letters.");
            }

            return;
        } else {
            SourceLocation Loc = d2->getLocation();
            if (!Loc.isValid()) {
                return;
            }

            if (!d2->getDeclName()) {
                return;
            }
 
            if (d2->getKind() == Decl::Kind::CXXConstructor)
                return;
            
            std::string str = d2->getNameAsString();
            char c = str[0];
            if (tolower(c) != c) {
                diag(Loc, "Methods should start with lower case letters.");
            }

            return;
        }

    } else {
        SourceLocation Loc = decl->getLocation();
        if (!Loc.isValid()) {
            return;
        }
        if (decl->isImplicit())
            return;
        if (!decl->getDeclName()) {
            return;
        }
        std::string str = decl->getNameAsString();
        char c = str[0];
        if (tolower(c) == c) {
            diag(Loc, "Classes should start with capital case letters.");
        }

        return;
    }
}
} // namespace tidy
} // namespace clang

