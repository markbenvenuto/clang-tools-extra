#include "CommentStyle.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Lex/Lexer.h"
#include "clang/AST/Comment.h"

#include <string>
#include <vector>

using namespace clang::ast_matchers;


namespace clang {
namespace tidy {
void CommentStyleCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(recordDecl().bind("id"), this);
  Finder->addMatcher(cxxMethodDecl(hasAncestor(recordDecl())).bind("id"), this);
}


using namespace std;

// returns pair of offset from beginning of str + line str
vector<tuple<int, string>> split(string s) {
    int begin = 0;
    int pos = 0;
    vector<tuple<int, string>> ret;

    while (pos < s.size()) {
        if (s[pos] == '\n' || s[pos] == '\r') {
            if (begin != pos) {
                ret.emplace_back(begin, s.substr(begin, pos - begin));
            }
            if (pos + 1 < s.size() && s[pos + 1] == '\n') {
                ++pos;
            }

            begin = pos + 1;
        }
        ++pos;
    }
    if (begin < pos)
        ret.emplace_back(begin, s.substr(begin, pos - 1));

    return ret;
}


void CommentStyleCheck::CheckCommentString(const comments::FullComment *Comment,
    RawComment* raw,
    SourceLocation loc,
    SourceManager& mgr, bool function) 
{
    // Check comment as follows
    // 1. Check java doc
    // 2. Check indentation
    // 3. Check sentences end with periods if end of comment or paragraph
    auto text = raw->getRawText(mgr);
    loc = raw->getLocStart();
    auto col = mgr.getSpellingColumnNumber(loc);
    
    string s = text.str();

    int offset = s.find("/**");
    if (offset == -1) {
        return;
    }

    /*
    auto loc1 = raw->getLocStart();
    auto col1 = mgr.getSpellingColumnNumber(loc1);
    auto loc2 = raw->getLocEnd();
    auto col2 = mgr.getSpellingColumnNumber(loc2);
    auto loc3 = Comment->getLocation();
    auto col3 = mgr.getSpellingColumnNumber(loc3);
    */

    if (col < 1) {
        // skipp
        return;
    }

    // The source location column is 1 indexed.
    offset = col - 1;

    vector<tuple<int, string>> lines = split(s);

    // NOTE: we assume the compiler has check this comment is terminated correctly
    //
    {
        auto it = lines.begin();
        //printf("line: |%s|\n", (*it).c_str());
        ++it;
        for (; it != lines.end(); ++it)  {
            auto line = *it;
            //printf("line: |%s|\n", line.c_str());
            // TODO: check for space after *
            int line_offset = std::get<0>(line);
            string& linestr = std::get<1>(line);
            int star = linestr.find("*");
            if (star != offset + 1) {

                SourceLocation locError(loc.getLocWithOffset(line_offset + star));
                auto D = diag(locError, "Found bad indentation before * line continuation in /** java doc comment.");

                // Now compute the difference from where the star is to where it should be and fix it.
                if (star == offset) {
                    SourceLocation insLoc(loc.getLocWithOffset(line_offset));
                    D << FixItHint::CreateInsertion(insLoc, " ");
                }

                //printf("Found bad indent\n");
                //return false;
            }

            if (star + 1 == linestr.size())
                continue;

            char c = linestr[star + 1];
            if (' ' != c && '\n' != c && '\r' != c && '/' != c) {
                SourceLocation locError(loc.getLocWithOffset(line_offset + star + 1));
                auto D = diag(locError, "Found no space after * line continuation in /** java doc comment.");

                D << FixItHint::CreateInsertion(locError, " ");
                //printf("Found bad spacing after star\n");
                //return false;
            }
        }
    }

    // Check for paragraphs
    {
        auto it = lines.begin();
        ++it;
        int l = 1;
        bool needsPeriod = false;
        int line_offset ;
        for (; it != lines.end(); ++it)  {
            auto line = *it;
            string& linestr = std::get<1>(line);
            line_offset = std::get<0>(line);
            int star = linestr.find("*");

            if (count_if(linestr.begin() + linestr.find('*') + 1, linestr.end(), [](char x){  return x == ' '; }) > 5 &&
                linestr.rfind(".") != linestr.size() - 1){
                needsPeriod = true;
            }
            else if (linestr.rfind(".") == linestr.size() - 1){
                needsPeriod = false;
            }
            else if (linestr == "" && needsPeriod) {
                SourceLocation locError(loc.getLocWithOffset(line_offset));
                diag(locError, "Sentence needs period.");
                //printf("Sentence needs period at  %d\n", l);
                needsPeriod = false;
                //return false;
            }
            //printf("%d, %d, %d\n", line.rfind('.'), line.size(), needsPeriod);
            ++l;
        }

        if (needsPeriod) {
            SourceLocation locError(loc.getLocWithOffset(line_offset));
            diag(locError, "Sentence needs period2.");
            //printf("Sentence needs period at  %d\n", l);
            //return false;
        }
        
        //return true;
    }
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
                CheckCommentString(Comment, raw, loc, mgr, function);
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
            d2->isImplicit() || d2->isOverloadedOperator())
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

#if 0
#include <string>
#include <vector>
#include <stdlib.h>
#include <algorithm>

using namespace std;

#define ASSERT(x) if(!(x)) { printf("ASSERT FAILED - %s\n", #x ); };


int main()
{
    string shortc =
        "   /**\n"
        "    * A comment\n"
        "    */\n";
    ASSERT(checkComment(shortc) == true);

    string badSpacing =
        "   /**\n"
        "    *Bad spacing comment\n"
        "    */\n";
    ASSERT(checkComment(badSpacing) == false);

    string goodBlank =
        "   /**\n"
        "    *\n"
        "    */\n";
    ASSERT(checkComment(goodBlank) == true);

    string badIndent =
        "   /**\n"
        "   * Bad indent comment\n"
        "   */\n";
    ASSERT(checkComment(badIndent) == false);

    string badMissingPeriod =
        "   /**\n"
        "    * A comment that is missing a period\n"
        "    */\n";
    ASSERT(checkComment(badMissingPeriod) == false);


    string goodPeriod =
        "   /**\n"
        "    * A comment that is missing a period,\n"
        "    * and another sentence but with a period.\n"
        "    */\n";
    ASSERT(checkComment(goodPeriod) == true);

    string badMissingPeriodPara =
        "   /**\n"
        "    * A comment that is missing a period,\n"
        "    * and another sentence but without a period\n"
        "    *\n"
        "    * Another setence\n"
        "    */\n";
    ASSERT(checkComment(badMissingPeriodPara) == false);


}
#endif


} // namespace tidy
} // namespace clang

