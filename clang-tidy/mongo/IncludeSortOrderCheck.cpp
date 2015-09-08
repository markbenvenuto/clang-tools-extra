//===--- IncludeSortOrderCheck.cpp - clang-tidy -------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "IncludeSortOrderCheck.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Preprocessor.h"
#include <string>
#include <algorithm>

namespace clang {
namespace tidy {

namespace {
class IncludeSortOrderPPCallbacks : public PPCallbacks {
public:
  explicit IncludeSortOrderPPCallbacks(ClangTidyCheck &Check, SourceManager &SM)
      : LookForMainModule(true), Check(Check), SM(SM) {}

  void InclusionDirective(SourceLocation HashLoc, const Token &IncludeTok,
                          StringRef FileName, bool IsAngled,
                          CharSourceRange FilenameRange, const FileEntry *File,
                          StringRef SearchPath, StringRef RelativePath,
                          const Module *Imported) override;
  void EndOfMainFile() override;

private:
  struct IncludeDirective {
    SourceLocation Loc;    ///< '#' location in the include directive
    CharSourceRange Range; ///< SourceRange for the file name
    std::string Filename;    ///< Filename as a string
    bool IsAngled;         ///< true if this was an include with angle brackets
    bool IsMainModule;     ///< true if this was the matching h
    const FileEntry* File;    ///FE
  };
  std::vector<IncludeDirective> IncludeDirectives;
  bool LookForMainModule;

  ClangTidyCheck &Check;
  SourceManager &SM;
};
} // namespace

void IncludeSortOrderCheck::registerPPCallbacks(CompilerInstance &Compiler) {
  Compiler.getPreprocessor().addPPCallbacks(
      llvm::make_unique<IncludeSortOrderPPCallbacks>(*this,
                                                 Compiler.getSourceManager()));
}

static int getPriority(StringRef Filename, bool IsAngled, bool IsMainModule) {
    // We leave the main module header at the top.

    /*
        1 : "platform/basic.h header",
        2 : "matching header for current .cpp file",
        3 : "boost header",
        4 : "C++ Standard header",
        5 : "MongoDB header"
        */

    // basic header for windows, optional but first
    if (Filename.endswith("platform/basic.h"))
        return 0;

    // matching header for current .cpp file
    if (IsMainModule)
        return 10;

    // TODO: lint these have <>, not ""
    // boost headers
    if (Filename.startswith("boost/"))
        return 20;


    // TODO: lint this is not a mongo header
    if (IsAngled)
        return 23;

    // mongo headers are sorted last
    return 40;
}

void IncludeSortOrderPPCallbacks::InclusionDirective(
    SourceLocation HashLoc, const Token &IncludeTok, StringRef FileName,
    bool IsAngled, CharSourceRange FilenameRange, const FileEntry *File,
    StringRef SearchPath, StringRef RelativePath, const Module *Imported) {
  // We recognize the first include as a special main module header and want
  // to leave it in the top position.
    auto FID = SM.getFileID(HashLoc);

    IncludeDirective ID = { HashLoc, FilenameRange, FileName, IsAngled, false, SM.getFileEntryForID(FID) };


  if (LookForMainModule && !IsAngled) {
      // TODO - what is FileName here - FileName is the file being included
      // TODO: how do we get the name of the file we are processing?
      // SourceManager??

     const  FileEntry* fe = SM.getFileEntryForID(SM.getMainFileID());
      auto name = fe->getName();
      StringRef srName(name);

      // Remove the .cpp extension
      StringRef srShortCpp = srName.drop_back(3);

      // Remove .h extension
      StringRef srShortH = FileName.drop_back(1);

      // TODO: check it ends with cpp and .h?
      if (srShortCpp.find(srShortH) != -1) {
          ID.IsMainModule = true;
          LookForMainModule = false;
      }
  }
  IncludeDirectives.push_back(std::move(ID));
}

void IncludeSortOrderPPCallbacks::EndOfMainFile() {
  LookForMainModule = true;
  if (IncludeDirectives.empty())
    return;

  // We want a few things
  // 1. Headers are sorted as we wish
  // 2. Headers are in blocks as follows
  // -- 1. "platform/basic.h"
  // -- 2. "main module.h"
  // -- 3. <boost and system>
  // -- 4. "mongo/ headers"

  // TODO: find duplicated includes.

  // Block includes into the file they are a part of
  //

  // Form blocks of includes. We don't want to sort across blocks. This also
  // implicitly makes us never reorder over #defines or #if directives.
  // FIXME: We should be more careful about sorting below comments as we don't
  // know if the comment refers to the next include or the whole block that
  // follows.
  std::vector<unsigned> Blocks(1, 0);
  for (unsigned I = 1, E = IncludeDirectives.size(); I != E; ++I)
    if (SM.getExpansionLineNumber(IncludeDirectives[I].Loc) !=
        SM.getExpansionLineNumber(IncludeDirectives[I - 1].Loc) + 1)
      Blocks.push_back(I);
  Blocks.push_back(IncludeDirectives.size()); // Sentinel value.

  // Get a vector of indices.
  std::vector<unsigned> IncludeIndices;
  for (unsigned I = 0, E = IncludeDirectives.size(); I != E; ++I)
    IncludeIndices.push_back(I);

  // Check blocks are not mixed. We may have many blocks due to ifdefs
  // and such, but just make sure they are not mixed.
  bool mixedBlocks = false;
  for (unsigned BI = 0, BE = Blocks.size() - 1; BI != BE; ++BI) {
    auto begin = Blocks[BI];
    IncludeDirective &LHS = IncludeDirectives[begin];
    int priority = 
          getPriority(LHS.Filename, LHS.IsAngled, LHS.IsMainModule) / 10;
    auto it = begin + 1;
    while(it < Blocks[BI + 1]) {
    IncludeDirective &RHS = IncludeDirectives[it];
      int second_priority =
          getPriority(RHS.Filename, RHS.IsAngled, RHS.IsMainModule) / 10;

      if (priority != second_priority) {
          Check.diag(RHS.Loc, "#includes are not grouped correctly into blocks.");
       mixedBlocks = true; 
       break;
      }
      it++;
    }
  }

  // Don't bother sorting for the user if the blocks are mixed
  /*if( mixedBlocks) {
      IncludeDirectives.clear();
      return;
  }*/

  // Sort the includes. We first sort by priority, then lexicographically.
  // NOTE: We do not sort across blocks, so ifdefs and spaces essentially defeat
  // this code, but it comes close enough
  // On the plus side, it will never have false negatives.
  for (unsigned BI = 0, BE = Blocks.size() - 1; BI != BE; ++BI)
    std::sort(IncludeIndices.begin() + Blocks[BI],
              IncludeIndices.begin() + Blocks[BI + 1],
              [this](unsigned LHSI, unsigned RHSI) {
      IncludeDirective &LHS = IncludeDirectives[LHSI];
      IncludeDirective &RHS = IncludeDirectives[RHSI];

      int PriorityLHS =
          getPriority(LHS.Filename, LHS.IsAngled, LHS.IsMainModule);
      int PriorityRHS =
          getPriority(RHS.Filename, RHS.IsAngled, RHS.IsMainModule);

      return std::tie(PriorityLHS, LHS.Filename) <
             std::tie(PriorityRHS, RHS.Filename);
    });

  bool foundBadSort = false;

  // Emit a warning for each block and fixits for all changes within that block.
  for (unsigned BI = 0, BE = Blocks.size() - 1; BI != BE; ++BI) {
    // Find the first include that's not in the right position.
    unsigned I, E;
    for (I = Blocks[BI], E = Blocks[BI + 1]; I != E; ++I) {
        if (IncludeIndices[I] != I)
            break;
    }

    if (I == E)
      continue;

    foundBadSort = true;

    // Emit a warning.
    auto D = Check.diag(IncludeDirectives[I].Loc,
                        "#includes are not sorted properly");

    // Emit fix-its for all following includes in this block.
    for (; I != E; ++I) {
      if (IncludeIndices[I] == I)
        continue;
      const IncludeDirective &CopyFrom = IncludeDirectives[IncludeIndices[I]];

      SourceLocation FromLoc = CopyFrom.Range.getBegin();
      const char *FromData = SM.getCharacterData(FromLoc);
      unsigned FromLen = std::strcspn(FromData, "\n");

      StringRef FixedName(FromData, FromLen);

      SourceLocation ToLoc = IncludeDirectives[I].Range.getBegin();
      const char *ToData = SM.getCharacterData(ToLoc);
      unsigned ToLen = std::strcspn(ToData, "\n");
      auto ToRange =
          CharSourceRange::getCharRange(ToLoc, ToLoc.getLocWithOffset(ToLen));

      D << FixItHint::CreateReplacement(ToRange, FixedName);
    }
  }

  // If the local sort is good, check the global sort
  //
  // Sort the includes. We first sort by priority, then lexicographically.
  // NOTE: We do not sort across blocks, so ifdefs and spaces essentially defeat
  // this code, but it comes close enough
  // On the plus side, it will never have false negatives.

    std::vector<IncludeDirective> GlobalIncludeDirectives;
    const  FileEntry* fe = SM.getFileEntryForID(SM.getMainFileID());
    std::copy_if(IncludeDirectives.begin(), IncludeDirectives.end(),
        std::back_inserter(GlobalIncludeDirectives), [fe](IncludeDirective x) {
        return x.File == fe; });

// Get a vector of indices.
std::vector<unsigned>  GlobalIncludeIndices;
  for (unsigned I = 0, E = GlobalIncludeDirectives.size(); I != E; ++I)
      GlobalIncludeIndices.push_back(I);


  if (!std::is_sorted(GlobalIncludeIndices.begin(),
      GlobalIncludeIndices.end(),
      [this, &GlobalIncludeDirectives](unsigned LHSI, unsigned RHSI) {
      IncludeDirective &LHS = GlobalIncludeDirectives[LHSI];
      IncludeDirective &RHS = GlobalIncludeDirectives[RHSI];

      int PriorityLHS =
          getPriority(LHS.Filename, LHS.IsAngled, LHS.IsMainModule);
      int PriorityRHS =
          getPriority(RHS.Filename, RHS.IsAngled, RHS.IsMainModule);

      return std::tie(PriorityLHS, LHS.Filename) <
          std::tie(PriorityRHS, RHS.Filename);
  })) {
      // Emit a warning.
      auto D = Check.diag(GlobalIncludeDirectives[0].Loc,
          "#includes are not sorted properly globally.");
  }
  
  IncludeDirectives.clear();
}

} // namespace tidy
} // namespace clang
