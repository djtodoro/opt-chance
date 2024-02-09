//
// A tool for binary analysis.
//
// Author: djtodoro
//

#include "llvm/DebugInfo/DWARF/DWARFContext.h"
#include "llvm/Object/MachOUniversal.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/DebugInfo/DIContext.h"
#include "llvm/DebugInfo/DWARF/DWARFAcceleratorTable.h"
#include "llvm/DebugInfo/DWARF/DWARFCompileUnit.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Object/Archive.h"
#include "llvm/Object/MachOUniversal.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Regex.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Triple.h"
#include <cstdlib>
#include <map>

#define DEBUG_TYPE "opt-chance"

using namespace llvm;
using namespace object;

/// Command line options.
/// @{

namespace {
using namespace cl;

OptionCategory OptChanceCategory("Specific Options");
static opt<bool> Help("h", desc("Alias for -help"), Hidden,
                      cat(OptChanceCategory));
static opt<std::string> InputFilename1(Positional, desc("<input object file1>"),
                                       cat(OptChanceCategory));
static opt<std::string> InputFilename2(Positional, desc("<input object file2>"),
                                       cat(OptChanceCategory));
static opt<std::string>
    OutputFilename("out-file", cl::init("-"),
                   cl::desc("Redirect output to the specified file."),
                   cl::value_desc("filename"), cat(OptChanceCategory));
static alias OutputFilenameAlias("o", desc("Alias for -out-file."),
                                 aliasopt(OutputFilename),
                                 cat(OptChanceCategory));
static opt<bool> Verbose("verbose", desc("Print verbose info."),
                         cat(OptChanceCategory));

} // namespace
/// @}

using HandlerFn = std::function<void(ObjectFile &, DWARFContext &DICtx, Twine,
                                     raw_ostream &)>;

using FuncMap = std::map<std::string, uint64_t>;

std::map<std::string, FuncMap> StatPerFile;

static void error(StringRef Prefix, std::error_code EC) {
  if (!EC)
    return;
  WithColor::error() << Prefix << ": " << EC.message() << "\n";
  exit(1);
}

static void collectStatsRecursive(DWARFDie Die, DWARFContext &DICtx,
                                  FuncMap &FuncSizes) {
  const dwarf::Tag Tag = Die.getTag();
  const bool IsFunction = Tag == dwarf::DW_TAG_subprogram;
  if (IsFunction) {
    auto FunctionName = Die.getName(DINameKind::ShortName);
    if (not FunctionName)
      return;

    // Ignore forward declarations.
    if (Die.find(dwarf::DW_AT_declaration)) {
      LLVM_DEBUG(llvm::dbgs() << "  -declaration ignored\n");
      return;
    }

    // Ignore inlined subprograms.
    if (Die.find(dwarf::DW_AT_inline)) {
      LLVM_DEBUG(llvm::dbgs() << "  -inlined subprogram ignored\n");
      return;
    }

    // PC Ranges.
    auto RangesOrError = Die.getAddressRanges();
    if (!RangesOrError) {
      llvm::consumeError(RangesOrError.takeError());
      return;
    }

    auto Ranges = RangesOrError.get();
    uint64_t BytesInThisScope = 0;
    for (auto Range : Ranges)
      BytesInThisScope += Range.HighPC - Range.LowPC;

    if (Verbose)
      llvm::outs() << " function " << FunctionName << ": " << BytesInThisScope
                   << " bytes\n";

    FuncSizes[FunctionName] = BytesInThisScope;
  }

  // Traverse children.
  DWARFDie Child = Die.getFirstChild();
  while (Child) {
    collectStatsRecursive(Child, DICtx, FuncSizes);
    Child = Child.getSibling();
  }
}

static void collectInfo(ObjectFile &Obj, DWARFContext &DICtx, Twine Filename,
                        raw_ostream &OS) {
  FuncMap FuncSizes;

  for (const auto &CU : static_cast<DWARFContext *>(&DICtx)->compile_units())
    if (DWARFDie CUDie = CU->getNonSkeletonUnitDIE(false))
      collectStatsRecursive(CUDie, DICtx, FuncSizes);

  StatPerFile[Filename.str()] = FuncSizes;
}

static void handleBuffer(StringRef Filename, MemoryBufferRef Buffer,
                         HandlerFn HandleObj, raw_ostream &OS) {
  Expected<std::unique_ptr<Binary>> BinOrErr = object::createBinary(Buffer);
  error(Filename, errorToErrorCode(BinOrErr.takeError()));

  if (auto *Obj = dyn_cast<ObjectFile>(BinOrErr->get())) {
    std::unique_ptr<DWARFContext> DICtx = DWARFContext::create(*Obj);
    HandleObj(*Obj, *DICtx, Filename, OS);
  }
}

static void handleFile(StringRef Filename, HandlerFn HandleObj,
                       raw_ostream &OS) {
  ErrorOr<std::unique_ptr<MemoryBuffer>> BuffOrErr =
      MemoryBuffer::getFileOrSTDIN(Filename);
  error(Filename, BuffOrErr.getError());
  std::unique_ptr<MemoryBuffer> Buffer = std::move(BuffOrErr.get());
  handleBuffer(Filename, *Buffer, HandleObj, OS);
}

static void findAndOutputCandidates(std::string F1, std::string F2) {
  auto &FnStatForFile1 = StatPerFile[F1];
  auto &FnStatForFile2 = StatPerFile[F2];

  // map func name to size diff.
  std::map<std::string, int64_t> Diff;

  for (auto &func : FnStatForFile1) {
    auto &name = func.first;

    if (!FnStatForFile2.count(name))
      continue;

    auto &funcFromFile2Size = FnStatForFile2[name];

    int64_t sizeDiff = funcFromFile2Size - func.second;
    Diff[name] = sizeDiff;
  }

  std::vector<std::pair<std::string, int64_t>> SortedData{Diff.begin(),
                                                          Diff.end()};
  std::sort(
      SortedData.begin(), SortedData.end(),
      [](std::pair<std::string, int64_t> l, std::pair<std::string, int64_t> r) {
        return l.second >= r.second;
      });

  for (auto &e : SortedData) {
    if (e.second)
      llvm::outs() << "for function \'" << e.first << "\' the diff is "
                   << e.second << '\n';
  }
}

int main(int argc, char const *argv[]) {
  llvm::outs() << "=== opt-chance Tool ===\n";
  llvm::outs() << "Analysis...\n";

  InitLLVM X(argc, argv);

  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargetMCs();

  HideUnrelatedOptions({&OptChanceCategory});
  cl::ParseCommandLineOptions(argc, argv, "find spots for optimizations\n");

  if (Help) {
    PrintHelpMessage(false, true);
    return 0;
  }

  if (InputFilename1 == "" or InputFilename2 == "") {
    WithColor::error(errs()) << "Expected two input files\n";
    exit(1);
  }

  std::error_code EC;
  ToolOutputFile OutputFile(OutputFilename, EC, sys::fs::OF_TextWithCRLF);
  error("Unable to open output file" + OutputFilename, EC);
  // Don't remove output file if we exit with an error.
  OutputFile.keep();

  // File1:
  if (Verbose)
    llvm::outs() << "Analysis of file: " << InputFilename1 << '\n';
  handleFile(InputFilename1, collectInfo, OutputFile.os());

  if (Verbose)
    llvm::outs() << '\n';

  if (Verbose)
    llvm::outs() << "Analysis of file: " << InputFilename2 << '\n';
  handleFile(InputFilename2, collectInfo, OutputFile.os());

  llvm::outs() << "\nCandidates to optimize:\n";
  findAndOutputCandidates(InputFilename1, InputFilename2);
  llvm::outs() << '\n';

  llvm::outs() << "Analysis done.\n";

  return 0;
}
