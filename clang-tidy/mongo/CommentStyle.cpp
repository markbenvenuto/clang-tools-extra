#include "CommentStyle.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Lex/Lexer.h"
#include "clang/AST/Comment.h"

using namespace clang::ast_matchers;


namespace clang {
namespace tidy {
void CommentStyleCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(recordDecl().bind("id"), this);
  Finder->addMatcher(methodDecl(hasAncestor(recordDecl())).bind("id"), this);
}

void CommentStyleCheck::CheckComment(const comments::FullComment *Comment,
    RawComment* raw,
    SourceLocation loc,
    SourceManager& mgr, bool function) {

    if (Comment != nullptr && raw != nullptr) {
        if (raw->getKind() == RawComment::CommentKind::RCK_JavaDoc) {
            auto text = raw->getRawText(mgr);
            if (text.startswith("/**\n") || text.startswith("/**\r")) {
                // Check it ends in a sentence
                return;
            }
        }
    }

    // Check decl has comment
    if (function) {
        diag(loc, "Functions should have /** java-doc style comments");
    } else {
        diag(loc, "Class declarations should have /** java-doc style comments");
    }
}

void CommentStyleCheck::check(const MatchFinder::MatchResult &Result) {
    const CXXRecordDecl *decl =
        Result.Nodes.getNodeAs<CXXRecordDecl>("id");
    if (!decl) {
        const CXXMethodDecl *d2 =
            Result.Nodes.getNodeAs<CXXMethodDecl>("id");

        SourceLocation Loc = d2->getLocation();
        if (!Loc.isValid()) {
            return;
        }


        if( d2->getKind() == Decl::Kind::CXXConstructor ||  d2->getKind() == Decl::Kind::CXXDestructor) {
            return;
        }

        if (d2->isCopyAssignmentOperator() || d2->isMoveAssignmentOperator() ||
            d2->isImplicit())
            return;

        const comments::FullComment *Comment = d2->getASTContext().getLocalCommentForDeclUncached(d2);

        RawComment* raw = d2->getASTContext().getRawCommentForDeclNoCache(d2);

        CheckComment(Comment, raw, Loc, d2->getASTContext().getSourceManager(), true);
    } else {
        SourceLocation Loc = decl->getLocation();
        if (!Loc.isValid()) {
            return;
        }
        
        if (decl->isImplicit() || !decl->isCompleteDefinition()
            || decl->isStruct()
            )
            return;

        const comments::FullComment *Comment = decl->getASTContext().getLocalCommentForDeclUncached(decl);

        RawComment* raw = decl->getASTContext().getRawCommentForDeclNoCache(decl);


        CheckComment(Comment, raw, Loc, decl->getASTContext().getSourceManager(), false);
    }


}
} // namespace tidy
} // namespace clang

