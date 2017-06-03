//===- Writer.cpp ---------------------------------------------------------===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Writer.h"
#include "Config.h"
#include "Filesystem.h"
#include "LinkerScript.h"
#include "MapFile.h"
#include "Memory.h"
#include "OutputSections.h"
#include "Relocations.h"
#include "Strings.h"
#include "SymbolTable.h"
#include "SyntheticSections.h"
#include "Target.h"
#include "Threads.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/FileOutputBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include <climits>

using namespace llvm;
using namespace llvm::ELF;
using namespace llvm::object;
using namespace llvm::support;
using namespace llvm::support::endian;

using namespace lld;
using namespace lld::elf;

namespace {
// The writer writes a SymbolTable result to a file.
template <class ELFT> class Writer {
public:
  typedef typename ELFT::Shdr Elf_Shdr;
  typedef typename ELFT::Ehdr Elf_Ehdr;
  typedef typename ELFT::Phdr Elf_Phdr;

  void run();

private:
  void clearOutputSections();
  void createSyntheticSections();
  void copyLocalSymbols();
  void addSectionSymbols();
  void addReservedSymbols();
  void createSections();
  void forEachRelSec(std::function<void(InputSectionBase &)> Fn);
  void sortSections();
  void finalizeSections();
  void addPredefinedSections();

  std::vector<PhdrEntry> createPhdrs();
  void removeEmptyPTLoad();
  void addPtArmExid(std::vector<PhdrEntry> &Phdrs);
  void assignFileOffsets();
  void assignFileOffsetsBinary();
  void setPhdrs();
  void fixSectionAlignments();
  void fixPredefinedSymbols();
  void openFile();
  void writeHeader();
  void writeSections();
  void writeSectionsBinary();
  void writeBuildId();

  std::unique_ptr<FileOutputBuffer> Buffer;

  std::vector<OutputSection *> OutputSections;
  std::vector<OutputSectionCommand *> OutputSectionCommands;
  OutputSectionFactory Factory{OutputSections};

  void addRelIpltSymbols();
  void addStartEndSymbols();
  void addStartStopSymbols(OutputSection *Sec);
  uint64_t getEntryAddr();
  OutputSection *findSection(StringRef Name);
  OutputSection *findSectionInScript(StringRef Name);
  OutputSectionCommand *findSectionCommand(StringRef Name);

  std::vector<PhdrEntry> Phdrs;

  uint64_t FileSize;
  uint64_t SectionHeaderOff;
};
} // anonymous namespace

StringRef elf::getOutputSectionName(StringRef Name) {
  if (Config->Relocatable)
    return Name;

  // If -emit-relocs is given (which is rare), we need to copy
  // relocation sections to the output. If input section .foo is
  // output as .bar, we want to rename .rel.foo .rel.bar as well.
  if (Config->EmitRelocs) {
    for (StringRef V : {".rel.", ".rela."}) {
      if (Name.startswith(V)) {
        StringRef Inner = getOutputSectionName(Name.substr(V.size() - 1));
        return Saver.save(V.drop_back() + Inner);
      }
    }
  }

  for (StringRef V :
       {".text.", ".rodata.", ".data.rel.ro.", ".data.", ".bss.rel.ro.",
        ".bss.", ".init_array.", ".fini_array.", ".ctors.", ".dtors.", ".tbss.",
        ".gcc_except_table.", ".tdata.", ".ARM.exidx."}) {
    StringRef Prefix = V.drop_back();
    if (Name.startswith(V) || Name == Prefix)
      return Prefix;
  }

  // CommonSection is identified as "COMMON" in linker scripts.
  // By default, it should go to .bss section.
  if (Name == "COMMON")
    return ".bss";

  // ".zdebug_" is a prefix for ZLIB-compressed sections.
  // Because we decompressed input sections, we want to remove 'z'.
  if (Name.startswith(".zdebug_"))
    return Saver.save("." + Name.substr(2));
  return Name;
}

template <class ELFT> static bool needsInterpSection() {
  return !Symtab<ELFT>::X->getSharedFiles().empty() &&
         !Config->DynamicLinker.empty() && !Script->ignoreInterpSection();
}

template <class ELFT> void elf::writeResult() { Writer<ELFT>().run(); }

template <class ELFT> void Writer<ELFT>::removeEmptyPTLoad() {
  auto I = std::remove_if(Phdrs.begin(), Phdrs.end(), [&](const PhdrEntry &P) {
    if (P.p_type != PT_LOAD)
      return false;
    if (!P.First)
      return true;
    uint64_t Size = P.Last->Addr + P.Last->Size - P.First->Addr;
    return Size == 0;
  });
  Phdrs.erase(I, Phdrs.end());
}

// This function scans over the input sections and creates mergeable
// synthetic sections. It removes MergeInputSections from array and
// adds new synthetic ones. Each synthetic section is added to the
// location of the first input section it replaces.
static void combineMergableSections() {
  std::vector<MergeSyntheticSection *> MergeSections;
  for (InputSectionBase *&S : InputSections) {
    MergeInputSection *MS = dyn_cast<MergeInputSection>(S);
    if (!MS)
      continue;

    // We do not want to handle sections that are not alive, so just remove
    // them instead of trying to merge.
    if (!MS->Live)
      continue;

    StringRef OutsecName = getOutputSectionName(MS->Name);
    uint64_t Flags = MS->Flags & ~(uint64_t)SHF_GROUP;
    uint32_t Alignment = std::max<uint32_t>(MS->Alignment, MS->Entsize);

    auto I = llvm::find_if(MergeSections, [=](MergeSyntheticSection *Sec) {
      return Sec->Name == OutsecName && Sec->Flags == Flags &&
             Sec->Alignment == Alignment;
    });
    if (I == MergeSections.end()) {
      MergeSyntheticSection *Syn =
          make<MergeSyntheticSection>(OutsecName, MS->Type, Flags, Alignment);
      MergeSections.push_back(Syn);
      I = std::prev(MergeSections.end());
      S = Syn;
    } else {
      S = nullptr;
    }
    (*I)->addSection(MS);
  }

  std::vector<InputSectionBase *> &V = InputSections;
  V.erase(std::remove(V.begin(), V.end(), nullptr), V.end());
}

template <class ELFT> static void combineEhFrameSections() {
  for (InputSectionBase *&S : InputSections) {
    EhInputSection *ES = dyn_cast<EhInputSection>(S);
    if (!ES || !ES->Live)
      continue;

    In<ELFT>::EhFrame->addSection(ES);
    S = nullptr;
  }

  std::vector<InputSectionBase *> &V = InputSections;
  V.erase(std::remove(V.begin(), V.end(), nullptr), V.end());
}

template <class ELFT> void Writer<ELFT>::clearOutputSections() {
  // Clear the OutputSections to make sure it is not used anymore. Any
  // code from this point on should be using the linker script
  // commands.
  for (OutputSection *Sec : OutputSections)
    Sec->Sections.clear();
  OutputSections.clear();
}

// The main function of the writer.
template <class ELFT> void Writer<ELFT>::run() {
  // Create linker-synthesized sections such as .got or .plt.
  // Such sections are of type input section.
  createSyntheticSections();
  combineMergableSections();

  if (!Config->Relocatable)
    combineEhFrameSections<ELFT>();

  // We need to create some reserved symbols such as _end. Create them.
  if (!Config->Relocatable)
    addReservedSymbols();

  // Create output sections.
  Script->OutputSections = &OutputSections;
  if (Script->Opt.HasSections) {
    // If linker script contains SECTIONS commands, let it create sections.
    Script->processCommands(Factory);

    // Linker scripts may have left some input sections unassigned.
    // Assign such sections using the default rule.
    Script->addOrphanSections(Factory);
  } else {
    // If linker script does not contain SECTIONS commands, create
    // output sections by default rules. We still need to give the
    // linker script a chance to run, because it might contain
    // non-SECTIONS commands such as ASSERT.
    createSections();
    Script->processCommands(Factory);
  }

  if (Config->Discard != DiscardPolicy::All)
    copyLocalSymbols();

  if (Config->CopyRelocs)
    addSectionSymbols();

  // Now that we have a complete set of output sections. This function
  // completes section contents. For example, we need to add strings
  // to the string table, and add entries to .got and .plt.
  // finalizeSections does that.
  finalizeSections();
  if (ErrorCount)
    return;

  if (!Script->Opt.HasSections)
    Script->fabricateDefaultCommands();
  else
    Script->synchronize();

  for (BaseCommand *Base : Script->Opt.Commands)
    if (auto *Cmd = dyn_cast<OutputSectionCommand>(Base))
      OutputSectionCommands.push_back(Cmd);

  clearOutputSections();

  if (!Script->Opt.HasSections &&!Config->Relocatable)
    fixSectionAlignments();

  // If -compressed-debug-sections is specified, we need to compress
  // .debug_* sections. Do it right now because it changes the size of
  // output sections.
  parallelForEach(
      OutputSectionCommands.begin(), OutputSectionCommands.end(),
      [](OutputSectionCommand *Cmd) { Cmd->maybeCompress<ELFT>(); });

  if (Config->Relocatable) {
    assignFileOffsets();
  } else {
    Script->assignAddresses(Phdrs, OutputSectionCommands);

    // Remove empty PT_LOAD to avoid causing the dynamic linker to try to mmap a
    // 0 sized region. This has to be done late since only after assignAddresses
    // we know the size of the sections.
    removeEmptyPTLoad();

    if (!Config->OFormatBinary)
      assignFileOffsets();
    else
      assignFileOffsetsBinary();

    setPhdrs();
    fixPredefinedSymbols();
  }

  // It does not make sense try to open the file if we have error already.
  if (ErrorCount)
    return;
  // Write the result down to a file.
  openFile();
  if (ErrorCount)
    return;

  if (!Config->OFormatBinary) {
    writeHeader();
    writeSections();
  } else {
    writeSectionsBinary();
  }

  // Backfill .note.gnu.build-id section content. This is done at last
  // because the content is usually a hash value of the entire output file.
  writeBuildId();
  if (ErrorCount)
    return;


  // Handle -Map option.
  writeMapFile<ELFT>(OutputSectionCommands);
  if (ErrorCount)
    return;

  if (auto EC = Buffer->commit())
    error("failed to write to the output file: " + EC.message());

  // Flush the output streams and exit immediately. A full shutdown
  // is a good test that we are keeping track of all allocated memory,
  // but actually freeing it is a waste of time in a regular linker run.
  if (Config->ExitEarly)
    exitLld(0);
}

// Initialize Out members.
template <class ELFT> void Writer<ELFT>::createSyntheticSections() {
  // Initialize all pointers with NULL. This is needed because
  // you can call lld::elf::main more than once as a library.
  memset(&Out::First, 0, sizeof(Out));

  auto Add = [](InputSectionBase *Sec) { InputSections.push_back(Sec); };

  InX::DynStrTab = make<StringTableSection>(".dynstr", true);
  InX::Dynamic = make<DynamicSection<ELFT>>();
  In<ELFT>::RelaDyn = make<RelocationSection<ELFT>>(
      Config->IsRela ? ".rela.dyn" : ".rel.dyn", Config->ZCombreloc);
  InX::ShStrTab = make<StringTableSection>(".shstrtab", false);

  Out::ElfHeader = make<OutputSection>("", 0, SHF_ALLOC);
  Out::ElfHeader->Size = sizeof(Elf_Ehdr);
  Out::ProgramHeaders = make<OutputSection>("", 0, SHF_ALLOC);
  Out::ProgramHeaders->updateAlignment(Config->Wordsize);

  if (needsInterpSection<ELFT>()) {
    InX::Interp = createInterpSection();
    Add(InX::Interp);
  } else {
    InX::Interp = nullptr;
  }

  if (!Config->Relocatable)
    Add(createCommentSection<ELFT>());

  if (Config->Strip != StripPolicy::All) {
    InX::StrTab = make<StringTableSection>(".strtab", false);
    InX::SymTab = make<SymbolTableSection<ELFT>>(*InX::StrTab);
  }

  if (Config->BuildId != BuildIdKind::None) {
    InX::BuildId = make<BuildIdSection>();
    Add(InX::BuildId);
  }

  InX::Common = createCommonSection<ELFT>();
  if (InX::Common)
    Add(InX::Common);

  InX::Bss = make<BssSection>(".bss");
  Add(InX::Bss);
  InX::BssRelRo = make<BssSection>(".bss.rel.ro");
  Add(InX::BssRelRo);

  // Add MIPS-specific sections.
  bool HasDynSymTab = !Symtab<ELFT>::X->getSharedFiles().empty() ||
                      Config->Pic || Config->ExportDynamic;
  if (Config->EMachine == EM_MIPS) {
    if (!Config->Shared && HasDynSymTab) {
      InX::MipsRldMap = make<MipsRldMapSection>();
      Add(InX::MipsRldMap);
    }
    if (auto *Sec = MipsAbiFlagsSection<ELFT>::create())
      Add(Sec);
    if (auto *Sec = MipsOptionsSection<ELFT>::create())
      Add(Sec);
    if (auto *Sec = MipsReginfoSection<ELFT>::create())
      Add(Sec);
  }

  if (HasDynSymTab) {
    InX::DynSymTab = make<SymbolTableSection<ELFT>>(*InX::DynStrTab);
    Add(InX::DynSymTab);

    In<ELFT>::VerSym = make<VersionTableSection<ELFT>>();
    Add(In<ELFT>::VerSym);

    if (!Config->VersionDefinitions.empty()) {
      In<ELFT>::VerDef = make<VersionDefinitionSection<ELFT>>();
      Add(In<ELFT>::VerDef);
    }

    In<ELFT>::VerNeed = make<VersionNeedSection<ELFT>>();
    Add(In<ELFT>::VerNeed);

    if (Config->GnuHash) {
      InX::GnuHashTab = make<GnuHashTableSection>();
      Add(InX::GnuHashTab);
    }

    if (Config->SysvHash) {
      In<ELFT>::HashTab = make<HashTableSection<ELFT>>();
      Add(In<ELFT>::HashTab);
    }

    Add(InX::Dynamic);
    Add(InX::DynStrTab);
    Add(In<ELFT>::RelaDyn);
  }

  // Add .got. MIPS' .got is so different from the other archs,
  // it has its own class.
  if (Config->EMachine == EM_MIPS) {
    InX::MipsGot = make<MipsGotSection>();
    Add(InX::MipsGot);
  } else {
    InX::Got = make<GotSection>();
    Add(InX::Got);
  }

  InX::GotPlt = make<GotPltSection>();
  Add(InX::GotPlt);
  InX::IgotPlt = make<IgotPltSection>();
  Add(InX::IgotPlt);

  if (Config->GdbIndex) {
    InX::GdbIndex = make<GdbIndexSection>();
    Add(InX::GdbIndex);
  }

  // We always need to add rel[a].plt to output if it has entries.
  // Even for static linking it can contain R_[*]_IRELATIVE relocations.
  In<ELFT>::RelaPlt = make<RelocationSection<ELFT>>(
      Config->IsRela ? ".rela.plt" : ".rel.plt", false /*Sort*/);
  Add(In<ELFT>::RelaPlt);

  // The RelaIplt immediately follows .rel.plt (.rel.dyn for ARM) to ensure
  // that the IRelative relocations are processed last by the dynamic loader
  In<ELFT>::RelaIplt = make<RelocationSection<ELFT>>(
      (Config->EMachine == EM_ARM) ? ".rel.dyn" : In<ELFT>::RelaPlt->Name,
      false /*Sort*/);
  Add(In<ELFT>::RelaIplt);

  InX::Plt = make<PltSection>(Target->PltHeaderSize);
  Add(InX::Plt);
  InX::Iplt = make<PltSection>(0);
  Add(InX::Iplt);

  if (!Config->Relocatable) {
    if (Config->EhFrameHdr) {
      In<ELFT>::EhFrameHdr = make<EhFrameHeader<ELFT>>();
      Add(In<ELFT>::EhFrameHdr);
    }
    In<ELFT>::EhFrame = make<EhFrameSection<ELFT>>();
    Add(In<ELFT>::EhFrame);
  }

  if (InX::SymTab)
    Add(InX::SymTab);
  Add(InX::ShStrTab);
  if (InX::StrTab)
    Add(InX::StrTab);
}

static bool shouldKeepInSymtab(SectionBase *Sec, StringRef SymName,
                               const SymbolBody &B) {
  if (B.isFile() || B.isSection())
    return false;

  // If sym references a section in a discarded group, don't keep it.
  if (Sec == &InputSection::Discarded)
    return false;

  if (Config->Discard == DiscardPolicy::None)
    return true;

  // In ELF assembly .L symbols are normally discarded by the assembler.
  // If the assembler fails to do so, the linker discards them if
  // * --discard-locals is used.
  // * The symbol is in a SHF_MERGE section, which is normally the reason for
  //   the assembler keeping the .L symbol.
  if (!SymName.startswith(".L") && !SymName.empty())
    return true;

  if (Config->Discard == DiscardPolicy::Locals)
    return false;

  return !Sec || !(Sec->Flags & SHF_MERGE);
}

static bool includeInSymtab(const SymbolBody &B) {
  if (!B.isLocal() && !B.symbol()->IsUsedInRegularObj)
    return false;

  if (auto *D = dyn_cast<DefinedRegular>(&B)) {
    // Always include absolute symbols.
    SectionBase *Sec = D->Section;
    if (!Sec)
      return true;
    if (auto *IS = dyn_cast<InputSectionBase>(Sec)) {
      Sec = IS->Repl;
      IS = cast<InputSectionBase>(Sec);
      // Exclude symbols pointing to garbage-collected sections.
      if (!IS->Live)
        return false;
    }
    if (auto *S = dyn_cast<MergeInputSection>(Sec))
      if (!S->getSectionPiece(D->Value)->Live)
        return false;
  }
  return true;
}

// Local symbols are not in the linker's symbol table. This function scans
// each object file's symbol table to copy local symbols to the output.
template <class ELFT> void Writer<ELFT>::copyLocalSymbols() {
  if (!InX::SymTab)
    return;
  for (elf::ObjectFile<ELFT> *F : Symtab<ELFT>::X->getObjectFiles()) {
    for (SymbolBody *B : F->getLocalSymbols()) {
      if (!B->IsLocal)
        fatal(toString(F) +
              ": broken object: getLocalSymbols returns a non-local symbol");
      auto *DR = dyn_cast<DefinedRegular>(B);

      // No reason to keep local undefined symbol in symtab.
      if (!DR)
        continue;
      if (!includeInSymtab(*B))
        continue;

      SectionBase *Sec = DR->Section;
      if (!shouldKeepInSymtab(Sec, B->getName(), *B))
        continue;
      InX::SymTab->addSymbol(B);
    }
  }
}

template <class ELFT> void Writer<ELFT>::addSectionSymbols() {
  // Create one STT_SECTION symbol for each output section we might
  // have a relocation with.
  for (OutputSection *Sec : OutputSections) {
    if (Sec->Sections.empty())
      continue;

    InputSection *IS = Sec->Sections[0];
    if (isa<SyntheticSection>(IS) || IS->Type == SHT_REL ||
        IS->Type == SHT_RELA)
      continue;

    auto *Sym =
        make<DefinedRegular>("", /*IsLocal=*/true, /*StOther=*/0, STT_SECTION,
                             /*Value=*/0, /*Size=*/0, IS, nullptr);
    InX::SymTab->addSymbol(Sym);
  }
}

// Today's loaders have a feature to make segments read-only after
// processing dynamic relocations to enhance security. PT_GNU_RELRO
// is defined for that.
//
// This function returns true if a section needs to be put into a
// PT_GNU_RELRO segment.
bool elf::isRelroSection(const OutputSection *Sec) {
  if (!Config->ZRelro)
    return false;

  uint64_t Flags = Sec->Flags;

  // Non-allocatable or non-writable sections don't need RELRO because
  // they are not writable or not even mapped to memory in the first place.
  // RELRO is for sections that are essentially read-only but need to
  // be writable only at process startup to allow dynamic linker to
  // apply relocations.
  if (!(Flags & SHF_ALLOC) || !(Flags & SHF_WRITE))
    return false;

  // Once initialized, TLS data segments are used as data templates
  // for a thread-local storage. For each new thread, runtime
  // allocates memory for a TLS and copy templates there. No thread
  // are supposed to use templates directly. Thus, it can be in RELRO.
  if (Flags & SHF_TLS)
    return true;

  // .init_array, .preinit_array and .fini_array contain pointers to
  // functions that are executed on process startup or exit. These
  // pointers are set by the static linker, and they are not expected
  // to change at runtime. But if you are an attacker, you could do
  // interesting things by manipulating pointers in .fini_array, for
  // example. So they are put into RELRO.
  uint32_t Type = Sec->Type;
  if (Type == SHT_INIT_ARRAY || Type == SHT_FINI_ARRAY ||
      Type == SHT_PREINIT_ARRAY)
    return true;

  // .got contains pointers to external symbols. They are resolved by
  // the dynamic linker when a module is loaded into memory, and after
  // that they are not expected to change. So, it can be in RELRO.
  if (InX::Got && Sec == InX::Got->getParent())
    return true;

  // .got.plt contains pointers to external function symbols. They are
  // by default resolved lazily, so we usually cannot put it into RELRO.
  // However, if "-z now" is given, the lazy symbol resolution is
  // disabled, which enables us to put it into RELRO.
  if (Sec == InX::GotPlt->getParent())
    return Config->ZNow;

  // .dynamic section contains data for the dynamic linker, and
  // there's no need to write to it at runtime, so it's better to put
  // it into RELRO.
  if (Sec == InX::Dynamic->getParent())
    return true;

  // .bss.rel.ro is used for copy relocations for read-only symbols.
  // Since the dynamic linker needs to process copy relocations, the
  // section cannot be read-only, but once initialized, they shouldn't
  // change.
  if (Sec == InX::BssRelRo->getParent())
    return true;

  // Sections with some special names are put into RELRO. This is a
  // bit unfortunate because section names shouldn't be significant in
  // ELF in spirit. But in reality many linker features depend on
  // magic section names.
  StringRef S = Sec->Name;
  return S == ".data.rel.ro" || S == ".ctors" || S == ".dtors" || S == ".jcr" ||
         S == ".eh_frame" || S == ".openbsd.randomdata";
}

// We compute a rank for each section. The rank indicates where the
// section should be placed in the file.  Instead of using simple
// numbers (0,1,2...), we use a series of flags. One for each decision
// point when placing the section.
// Using flags has two key properties:
// * It is easy to check if a give branch was taken.
// * It is easy two see how similar two ranks are (see getRankProximity).
enum RankFlags {
  RF_NOT_ADDR_SET = 1 << 16,
  RF_NOT_INTERP = 1 << 15,
  RF_NOT_ALLOC = 1 << 14,
  RF_WRITE = 1 << 13,
  RF_EXEC_WRITE = 1 << 12,
  RF_EXEC = 1 << 11,
  RF_NON_TLS_BSS = 1 << 10,
  RF_NON_TLS_BSS_RO = 1 << 9,
  RF_NOT_TLS = 1 << 8,
  RF_BSS = 1 << 7,
  RF_PPC_NOT_TOCBSS = 1 << 6,
  RF_PPC_OPD = 1 << 5,
  RF_PPC_TOCL = 1 << 4,
  RF_PPC_TOC = 1 << 3,
  RF_PPC_BRANCH_LT = 1 << 2,
  RF_MIPS_GPREL = 1 << 1,
  RF_MIPS_NOT_GOT = 1 << 0
};

static unsigned getSectionRank(const OutputSection *Sec) {
  unsigned Rank = 0;

  // We want to put section specified by -T option first, so we
  // can start assigning VA starting from them later.
  if (Config->SectionStartMap.count(Sec->Name))
    return Rank;
  Rank |= RF_NOT_ADDR_SET;

  // Put .interp first because some loaders want to see that section
  // on the first page of the executable file when loaded into memory.
  if (Sec->Name == ".interp")
    return Rank;
  Rank |= RF_NOT_INTERP;

  // Allocatable sections go first to reduce the total PT_LOAD size and
  // so debug info doesn't change addresses in actual code.
  if (!(Sec->Flags & SHF_ALLOC))
    return Rank | RF_NOT_ALLOC;

  // Sort sections based on their access permission in the following
  // order: R, RX, RWX, RW.  This order is based on the following
  // considerations:
  // * Read-only sections come first such that they go in the
  //   PT_LOAD covering the program headers at the start of the file.
  // * Read-only, executable sections come next, unless the
  //   -no-rosegment option is used.
  // * Writable, executable sections follow such that .plt on
  //   architectures where it needs to be writable will be placed
  //   between .text and .data.
  // * Writable sections come last, such that .bss lands at the very
  //   end of the last PT_LOAD.
  bool IsExec = Sec->Flags & SHF_EXECINSTR;
  bool IsWrite = Sec->Flags & SHF_WRITE;

  if (IsExec) {
    if (IsWrite)
      Rank |= RF_EXEC_WRITE;
    else if (!Config->SingleRoRx)
      Rank |= RF_EXEC;
  } else {
    if (IsWrite)
      Rank |= RF_WRITE;
  }

  // If we got here we know that both A and B are in the same PT_LOAD.

  bool IsTls = Sec->Flags & SHF_TLS;
  bool IsNoBits = Sec->Type == SHT_NOBITS;

  // The first requirement we have is to put (non-TLS) nobits sections last. The
  // reason is that the only thing the dynamic linker will see about them is a
  // p_memsz that is larger than p_filesz. Seeing that it zeros the end of the
  // PT_LOAD, so that has to correspond to the nobits sections.
  bool IsNonTlsNoBits = IsNoBits && !IsTls;
  if (IsNonTlsNoBits)
    Rank |= RF_NON_TLS_BSS;

  // We place nobits RelRo sections before plain r/w ones, and non-nobits RelRo
  // sections after r/w ones, so that the RelRo sections are contiguous.
  bool IsRelRo = isRelroSection(Sec);
  if (IsNonTlsNoBits && !IsRelRo)
    Rank |= RF_NON_TLS_BSS_RO;
  if (!IsNonTlsNoBits && IsRelRo)
    Rank |= RF_NON_TLS_BSS_RO;

  // The TLS initialization block needs to be a single contiguous block in a R/W
  // PT_LOAD, so stick TLS sections directly before the other RelRo R/W
  // sections. The TLS NOBITS sections are placed here as they don't take up
  // virtual address space in the PT_LOAD.
  if (!IsTls)
    Rank |= RF_NOT_TLS;

  // Within the TLS initialization block, the non-nobits sections need to appear
  // first.
  if (IsNoBits)
    Rank |= RF_BSS;

  // // Some architectures have additional ordering restrictions for sections
  // // within the same PT_LOAD.
  if (Config->EMachine == EM_PPC64) {
    // PPC64 has a number of special SHT_PROGBITS+SHF_ALLOC+SHF_WRITE sections
    // that we would like to make sure appear is a specific order to maximize
    // their coverage by a single signed 16-bit offset from the TOC base
    // pointer. Conversely, the special .tocbss section should be first among
    // all SHT_NOBITS sections. This will put it next to the loaded special
    // PPC64 sections (and, thus, within reach of the TOC base pointer).
    StringRef Name = Sec->Name;
    if (Name != ".tocbss")
      Rank |= RF_PPC_NOT_TOCBSS;

    if (Name == ".opd")
      Rank |= RF_PPC_OPD;

    if (Name == ".toc1")
      Rank |= RF_PPC_TOCL;

    if (Name == ".toc")
      Rank |= RF_PPC_TOC;

    if (Name == ".branch_lt")
      Rank |= RF_PPC_BRANCH_LT;
  }
  if (Config->EMachine == EM_MIPS) {
    // All sections with SHF_MIPS_GPREL flag should be grouped together
    // because data in these sections is addressable with a gp relative address.
    if (Sec->Flags & SHF_MIPS_GPREL)
      Rank |= RF_MIPS_GPREL;

    if (Sec->Name != ".got")
      Rank |= RF_MIPS_NOT_GOT;
  }

  return Rank;
}

static bool compareSectionsNonScript(const OutputSection *A,
                                     const OutputSection *B) {
  if (A->SortRank != B->SortRank)
    return A->SortRank < B->SortRank;
  if (!(A->SortRank & RF_NOT_ADDR_SET))
    return Config->SectionStartMap.lookup(A->Name) <
           Config->SectionStartMap.lookup(B->Name);
  return false;
}

// Output section ordering is determined by this function.
static bool compareSections(const OutputSection *A, const OutputSection *B) {
  // For now, put sections mentioned in a linker script
  // first. Sections not on linker script will have a SectionIndex of
  // INT_MAX.
  int AIndex = A->SectionIndex;
  int BIndex = B->SectionIndex;
  if (AIndex != BIndex)
    return AIndex < BIndex;

  return compareSectionsNonScript(A, B);
}

void PhdrEntry::add(OutputSection *Sec) {
  Last = Sec;
  if (!First)
    First = Sec;
  p_align = std::max(p_align, Sec->Alignment);
  if (p_type == PT_LOAD)
    Sec->FirstInPtLoad = First;
}

template <class ELFT>
static Symbol *addRegular(StringRef Name, SectionBase *Sec, uint64_t Value,
                          uint8_t StOther = STV_HIDDEN,
                          uint8_t Binding = STB_WEAK) {
  // The linker generated symbols are added as STB_WEAK to allow user defined
  // ones to override them.
  return Symtab<ELFT>::X->addRegular(Name, StOther, STT_NOTYPE, Value,
                                     /*Size=*/0, Binding, Sec,
                                     /*File=*/nullptr);
}

template <class ELFT>
static DefinedRegular *
addOptionalRegular(StringRef Name, SectionBase *Sec, uint64_t Val,
                   uint8_t StOther = STV_HIDDEN, uint8_t Binding = STB_GLOBAL) {
  SymbolBody *S = Symtab<ELFT>::X->find(Name);
  if (!S)
    return nullptr;
  if (S->isInCurrentDSO())
    return nullptr;
  return cast<DefinedRegular>(
      addRegular<ELFT>(Name, Sec, Val, StOther, Binding)->body());
}

// The beginning and the ending of .rel[a].plt section are marked
// with __rel[a]_iplt_{start,end} symbols if it is a statically linked
// executable. The runtime needs these symbols in order to resolve
// all IRELATIVE relocs on startup. For dynamic executables, we don't
// need these symbols, since IRELATIVE relocs are resolved through GOT
// and PLT. For details, see http://www.airs.com/blog/archives/403.
template <class ELFT> void Writer<ELFT>::addRelIpltSymbols() {
  if (InX::DynSymTab)
    return;
  StringRef S = Config->IsRela ? "__rela_iplt_start" : "__rel_iplt_start";
  addOptionalRegular<ELFT>(S, In<ELFT>::RelaIplt, 0, STV_HIDDEN, STB_WEAK);

  S = Config->IsRela ? "__rela_iplt_end" : "__rel_iplt_end";
  addOptionalRegular<ELFT>(S, In<ELFT>::RelaIplt, -1, STV_HIDDEN, STB_WEAK);
}

// The linker is expected to define some symbols depending on
// the linking result. This function defines such symbols.
template <class ELFT> void Writer<ELFT>::addReservedSymbols() {
  if (Config->EMachine == EM_MIPS) {
    // Define _gp for MIPS. st_value of _gp symbol will be updated by Writer
    // so that it points to an absolute address which by default is relative
    // to GOT. Default offset is 0x7ff0.
    // See "Global Data Symbols" in Chapter 6 in the following document:
    // ftp://www.linux-mips.org/pub/linux/mips/doc/ABI/mipsabi.pdf
    ElfSym::MipsGp = Symtab<ELFT>::X->addAbsolute("_gp", STV_HIDDEN, STB_LOCAL);

    // On MIPS O32 ABI, _gp_disp is a magic symbol designates offset between
    // start of function and 'gp' pointer into GOT.
    if (Symtab<ELFT>::X->find("_gp_disp"))
      ElfSym::MipsGpDisp =
          Symtab<ELFT>::X->addAbsolute("_gp_disp", STV_HIDDEN, STB_LOCAL);

    // The __gnu_local_gp is a magic symbol equal to the current value of 'gp'
    // pointer. This symbol is used in the code generated by .cpload pseudo-op
    // in case of using -mno-shared option.
    // https://sourceware.org/ml/binutils/2004-12/msg00094.html
    if (Symtab<ELFT>::X->find("__gnu_local_gp"))
      ElfSym::MipsLocalGp =
          Symtab<ELFT>::X->addAbsolute("__gnu_local_gp", STV_HIDDEN, STB_LOCAL);
  }

  // In the assembly for 32 bit x86 the _GLOBAL_OFFSET_TABLE_ symbol
  // is magical and is used to produce a R_386_GOTPC relocation.
  // The R_386_GOTPC relocation value doesn't actually depend on the
  // symbol value, so it could use an index of STN_UNDEF which, according
  // to the spec, means the symbol value is 0.
  // Unfortunately both gas and MC keep the _GLOBAL_OFFSET_TABLE_ symbol in
  // the object file.
  // The situation is even stranger on x86_64 where the assembly doesn't
  // need the magical symbol, but gas still puts _GLOBAL_OFFSET_TABLE_ as
  // an undefined symbol in the .o files.
  // Given that the symbol is effectively unused, we just create a dummy
  // hidden one to avoid the undefined symbol error.
  Symtab<ELFT>::X->addIgnored("_GLOBAL_OFFSET_TABLE_");

  // __tls_get_addr is defined by the dynamic linker for dynamic ELFs. For
  // static linking the linker is required to optimize away any references to
  // __tls_get_addr, so it's not defined anywhere. Create a hidden definition
  // to avoid the undefined symbol error.
  if (!InX::DynSymTab)
    Symtab<ELFT>::X->addIgnored("__tls_get_addr");

  // __ehdr_start is the location of ELF file headers. Note that we define
  // this symbol unconditionally even when using a linker script, which
  // differs from the behavior implemented by GNU linker which only define
  // this symbol if ELF headers are in the memory mapped segment.
  addOptionalRegular<ELFT>("__ehdr_start", Out::ElfHeader, 0, STV_HIDDEN);

  // If linker script do layout we do not need to create any standart symbols.
  if (Script->Opt.HasSections)
    return;

  auto Add = [](StringRef S) {
    return addOptionalRegular<ELFT>(S, Out::ElfHeader, 0, STV_DEFAULT);
  };

  ElfSym::Bss = Add("__bss_start");
  ElfSym::End1 = Add("end");
  ElfSym::End2 = Add("_end");
  ElfSym::Etext1 = Add("etext");
  ElfSym::Etext2 = Add("_etext");
  ElfSym::Edata1 = Add("edata");
  ElfSym::Edata2 = Add("_edata");
}

// Sort input sections by section name suffixes for
// __attribute__((init_priority(N))).
static void sortInitFini(OutputSection *S) {
  if (S)
    reinterpret_cast<OutputSection *>(S)->sortInitFini();
}

// Sort input sections by the special rule for .ctors and .dtors.
static void sortCtorsDtors(OutputSection *S) {
  if (S)
    reinterpret_cast<OutputSection *>(S)->sortCtorsDtors();
}

// Sort input sections using the list provided by --symbol-ordering-file.
template <class ELFT>
static void sortBySymbolsOrder(ArrayRef<OutputSection *> OutputSections) {
  if (Config->SymbolOrderingFile.empty())
    return;

  // Build a map from symbols to their priorities. Symbols that didn't
  // appear in the symbol ordering file have the lowest priority 0.
  // All explicitly mentioned symbols have negative (higher) priorities.
  DenseMap<StringRef, int> SymbolOrder;
  int Priority = -Config->SymbolOrderingFile.size();
  for (StringRef S : Config->SymbolOrderingFile)
    SymbolOrder.insert({S, Priority++});

  // Build a map from sections to their priorities.
  DenseMap<SectionBase *, int> SectionOrder;
  for (elf::ObjectFile<ELFT> *File : Symtab<ELFT>::X->getObjectFiles()) {
    for (SymbolBody *Body : File->getSymbols()) {
      auto *D = dyn_cast<DefinedRegular>(Body);
      if (!D || !D->Section)
        continue;
      int &Priority = SectionOrder[D->Section];
      Priority = std::min(Priority, SymbolOrder.lookup(D->getName()));
    }
  }

  // Sort sections by priority.
  for (OutputSection *Base : OutputSections)
    if (auto *Sec = dyn_cast<OutputSection>(Base))
      Sec->sort([&](InputSectionBase *S) { return SectionOrder.lookup(S); });
}

template <class ELFT>
void Writer<ELFT>::forEachRelSec(std::function<void(InputSectionBase &)> Fn) {
  for (InputSectionBase *IS : InputSections) {
    if (!IS->Live)
      continue;
    // Scan all relocations. Each relocation goes through a series
    // of tests to determine if it needs special treatment, such as
    // creating GOT, PLT, copy relocations, etc.
    // Note that relocations for non-alloc sections are directly
    // processed by InputSection::relocateNonAlloc.
    if (!(IS->Flags & SHF_ALLOC))
      continue;
    if (isa<InputSection>(IS) || isa<EhInputSection>(IS))
      Fn(*IS);
  }

  if (!Config->Relocatable) {
    for (EhInputSection *ES : In<ELFT>::EhFrame->Sections)
      Fn(*ES);
  }
}

template <class ELFT> void Writer<ELFT>::createSections() {
  for (InputSectionBase *IS : InputSections)
    if (IS)
      Factory.addInputSec(IS, getOutputSectionName(IS->Name));

  sortBySymbolsOrder<ELFT>(OutputSections);
  sortInitFini(findSection(".init_array"));
  sortInitFini(findSection(".fini_array"));
  sortCtorsDtors(findSection(".ctors"));
  sortCtorsDtors(findSection(".dtors"));

  for (OutputSection *Sec : OutputSections)
    Sec->assignOffsets();
}

// We want to find how similar two ranks are.
// The more branches in getSectionRank that match, the more similar they are.
// Since each branch corresponds to a bit flag, we can just use
// countLeadingZeros.
static unsigned getRankProximity(OutputSection *A, OutputSection *B) {
  return countLeadingZeros(A->SortRank ^ B->SortRank);
}

// We want to place orphan sections so that they share as much
// characteristics with their neighbors as possible. For example, if
// both are rw, or both are tls.
template <typename ELFT>
static std::vector<OutputSection *>::iterator
findOrphanPos(std::vector<OutputSection *>::iterator B,
              std::vector<OutputSection *>::iterator E) {
  OutputSection *Sec = *E;

  // Find the first element that has as close a rank as possible.
  auto I = std::max_element(B, E, [=](OutputSection *A, OutputSection *B) {
    return getRankProximity(Sec, A) < getRankProximity(Sec, B);
  });
  if (I == E)
    return E;

  // Consider all existing sections with the same proximity.
  unsigned Proximity = getRankProximity(Sec, *I);
  while (I != E && getRankProximity(Sec, *I) == Proximity &&
         Sec->SortRank >= (*I)->SortRank)
    ++I;
  return I;
}

template <class ELFT> void Writer<ELFT>::sortSections() {
  // Don't sort if using -r. It is not necessary and we want to preserve the
  // relative order for SHF_LINK_ORDER sections.
  if (Config->Relocatable)
    return;

  if (Script->Opt.HasSections)
    Script->adjustSectionsBeforeSorting();

  for (OutputSection *Sec : OutputSections)
    Sec->SortRank = getSectionRank(Sec);

  if (!Script->Opt.HasSections) {
    std::stable_sort(OutputSections.begin(), OutputSections.end(),
                     compareSectionsNonScript);
    return;
  }

  // The order of the sections in the script is arbitrary and may not agree with
  // compareSectionsNonScript. This means that we cannot easily define a
  // strict weak ordering. To see why, consider a comparison of a section in the
  // script and one not in the script. We have a two simple options:
  // * Make them equivalent (a is not less than b, and b is not less than a).
  //   The problem is then that equivalence has to be transitive and we can
  //   have sections a, b and c with only b in a script and a less than c
  //   which breaks this property.
  // * Use compareSectionsNonScript. Given that the script order doesn't have
  //   to match, we can end up with sections a, b, c, d where b and c are in the
  //   script and c is compareSectionsNonScript less than b. In which case d
  //   can be equivalent to c, a to b and d < a. As a concrete example:
  //   .a (rx) # not in script
  //   .b (rx) # in script
  //   .c (ro) # in script
  //   .d (ro) # not in script
  //
  // The way we define an order then is:
  // *  First put script sections at the start and sort the script sections.
  // *  Move each non-script section to its preferred position. We try
  //    to put each section in the last position where it it can share
  //    a PT_LOAD.

  std::stable_sort(OutputSections.begin(), OutputSections.end(),
                   compareSections);

  auto I = OutputSections.begin();
  auto E = OutputSections.end();
  auto NonScriptI =
      std::find_if(OutputSections.begin(), E,
                   [](OutputSection *S) { return S->SectionIndex == INT_MAX; });
  while (NonScriptI != E) {
    auto Pos = findOrphanPos<ELFT>(I, NonScriptI);

    // As an optimization, find all sections with the same sort rank
    // and insert them with one rotate.
    unsigned Rank = (*NonScriptI)->SortRank;
    auto End = std::find_if(NonScriptI + 1, E, [=](OutputSection *Sec) {
      return Sec->SortRank != Rank;
    });
    std::rotate(Pos, NonScriptI, End);
    NonScriptI = End;
  }

  Script->adjustSectionsAfterSorting();
}

static void applySynthetic(const std::vector<SyntheticSection *> &Sections,
                           std::function<void(SyntheticSection *)> Fn) {
  for (SyntheticSection *SS : Sections)
    if (SS && SS->getParent() && !SS->empty()) {
      Fn(SS);
      SS->getParent()->assignOffsets();
    }
}

// We need to add input synthetic sections early in createSyntheticSections()
// to make them visible from linkescript side. But not all sections are always
// required to be in output. For example we don't need dynamic section content
// sometimes. This function filters out such unused sections from the output.
static void removeUnusedSyntheticSections(std::vector<OutputSection *> &V) {
  // All input synthetic sections that can be empty are placed after
  // all regular ones. We iterate over them all and exit at first
  // non-synthetic.
  for (InputSectionBase *S : llvm::reverse(InputSections)) {
    SyntheticSection *SS = dyn_cast<SyntheticSection>(S);
    if (!SS)
      return;
    OutputSection *OS = SS->getParent();
    if (!SS->empty() || !OS)
      continue;
    OS->Sections.erase(std::find(OS->Sections.begin(), OS->Sections.end(), SS));
    SS->Live = false;
    // If there are no other sections in the output section, remove it from the
    // output.
    if (OS->Sections.empty())
      V.erase(std::find(V.begin(), V.end(), OS));
  }
}

// Create output section objects and add them to OutputSections.
template <class ELFT> void Writer<ELFT>::finalizeSections() {
  Out::DebugInfo = findSection(".debug_info");
  Out::PreinitArray = findSection(".preinit_array");
  Out::InitArray = findSection(".init_array");
  Out::FiniArray = findSection(".fini_array");

  // The linker needs to define SECNAME_start, SECNAME_end and SECNAME_stop
  // symbols for sections, so that the runtime can get the start and end
  // addresses of each section by section name. Add such symbols.
  if (!Config->Relocatable) {
    addStartEndSymbols();
    for (OutputSection *Sec : OutputSections)
      addStartStopSymbols(Sec);
  }

  // Add _DYNAMIC symbol. Unlike GNU gold, our _DYNAMIC symbol has no type.
  // It should be okay as no one seems to care about the type.
  // Even the author of gold doesn't remember why gold behaves that way.
  // https://sourceware.org/ml/binutils/2002-03/msg00360.html
  if (InX::DynSymTab)
    addRegular<ELFT>("_DYNAMIC", InX::Dynamic, 0);

  // Define __rel[a]_iplt_{start,end} symbols if needed.
  addRelIpltSymbols();

  // This responsible for splitting up .eh_frame section into
  // pieces. The relocation scan uses those pieces, so this has to be
  // earlier.
  applySynthetic({In<ELFT>::EhFrame},
                 [](SyntheticSection *SS) { SS->finalizeContents(); });

  // Scan relocations. This must be done after every symbol is declared so that
  // we can correctly decide if a dynamic relocation is needed.
  forEachRelSec(scanRelocations<ELFT>);

  if (InX::Plt && !InX::Plt->empty())
    InX::Plt->addSymbols();
  if (InX::Iplt && !InX::Iplt->empty())
    InX::Iplt->addSymbols();

  // Now that we have defined all possible global symbols including linker-
  // synthesized ones. Visit all symbols to give the finishing touches.
  for (Symbol *S : Symtab<ELFT>::X->getSymbols()) {
    SymbolBody *Body = S->body();

    if (!includeInSymtab(*Body))
      continue;
    if (InX::SymTab)
      InX::SymTab->addSymbol(Body);

    if (InX::DynSymTab && S->includeInDynsym()) {
      InX::DynSymTab->addSymbol(Body);
      if (auto *SS = dyn_cast<SharedSymbol>(Body))
        if (cast<SharedFile<ELFT>>(SS->File)->isNeeded())
          In<ELFT>::VerNeed->addSymbol(SS);
    }
  }

  // Do not proceed if there was an undefined symbol.
  if (ErrorCount)
    return;

  addPredefinedSections();
  removeUnusedSyntheticSections(OutputSections);

  sortSections();

  // This is a bit of a hack. A value of 0 means undef, so we set it
  // to 1 t make __ehdr_start defined. The section number is not
  // particularly relevant.
  Out::ElfHeader->SectionIndex = 1;

  unsigned I = 1;
  for (OutputSection *Sec : OutputSections) {
    Sec->SectionIndex = I++;
    Sec->ShName = InX::ShStrTab->addString(Sec->Name);
  }

  // Binary and relocatable output does not have PHDRS.
  // The headers have to be created before finalize as that can influence the
  // image base and the dynamic section on mips includes the image base.
  if (!Config->Relocatable && !Config->OFormatBinary) {
    Phdrs = Script->hasPhdrsCommands() ? Script->createPhdrs() : createPhdrs();
    addPtArmExid(Phdrs);
    Out::ProgramHeaders->Size = sizeof(Elf_Phdr) * Phdrs.size();
  }

  // Dynamic section must be the last one in this list and dynamic
  // symbol table section (DynSymTab) must be the first one.
  applySynthetic({InX::DynSymTab,    InX::Bss,           InX::BssRelRo,
                  InX::GnuHashTab,   In<ELFT>::HashTab,  InX::SymTab,
                  InX::ShStrTab,     InX::StrTab,        In<ELFT>::VerDef,
                  InX::DynStrTab,    InX::GdbIndex,      InX::Got,
                  InX::MipsGot,      InX::IgotPlt,       InX::GotPlt,
                  In<ELFT>::RelaDyn, In<ELFT>::RelaIplt, In<ELFT>::RelaPlt,
                  InX::Plt,          InX::Iplt,          In<ELFT>::EhFrameHdr,
                  In<ELFT>::VerSym,  In<ELFT>::VerNeed,  InX::Dynamic},
                 [](SyntheticSection *SS) { SS->finalizeContents(); });

  // Some architectures use small displacements for jump instructions.
  // It is linker's responsibility to create thunks containing long
  // jump instructions if jump targets are too far. Create thunks.
  if (Target->NeedsThunks) {
    // FIXME: only ARM Interworking and Mips LA25 Thunks are implemented,
    // these
    // do not require address information. To support range extension Thunks
    // we need to assign addresses so that we can tell if jump instructions
    // are out of range. This will need to turn into a loop that converges
    // when no more Thunks are added
    ThunkCreator TC;
    if (TC.createThunks(OutputSections))
      applySynthetic({InX::MipsGot},
                     [](SyntheticSection *SS) { SS->updateAllocSize(); });
  }
  // Fill other section headers. The dynamic table is finalized
  // at the end because some tags like RELSZ depend on result
  // of finalizing other sections.
  for (OutputSection *Sec : OutputSections)
    Sec->finalize<ELFT>();

  // createThunks may have added local symbols to the static symbol table
  applySynthetic({InX::SymTab, InX::ShStrTab, InX::StrTab},
                 [](SyntheticSection *SS) { SS->postThunkContents(); });
}

template <class ELFT> void Writer<ELFT>::addPredefinedSections() {
  // ARM ABI requires .ARM.exidx to be terminated by some piece of data.
  // We have the terminater synthetic section class. Add that at the end.
  auto *OS = dyn_cast_or_null<OutputSection>(findSection(".ARM.exidx"));
  if (!OS || OS->Sections.empty() || Config->Relocatable)
    return;

  auto *Sentinel = make<ARMExidxSentinelSection>();
  OS->addSection(Sentinel);
  // If there are linker script commands existing at this point then add the
  // sentinel to the last of these too.
  if (OutputSectionCommand *C = Script->getCmd(OS)) {
    auto ISD = std::find_if(C->Commands.rbegin(), C->Commands.rend(),
                            [](const BaseCommand *Base) {
                              return isa<InputSectionDescription>(Base);
                            });
    cast<InputSectionDescription>(*ISD)->Sections.push_back(Sentinel);
  }
}

// The linker is expected to define SECNAME_start and SECNAME_end
// symbols for a few sections. This function defines them.
template <class ELFT> void Writer<ELFT>::addStartEndSymbols() {
  auto Define = [&](StringRef Start, StringRef End, OutputSection *OS) {
    // These symbols resolve to the image base if the section does not exist.
    // A special value -1 indicates end of the section.
    if (OS) {
      addOptionalRegular<ELFT>(Start, OS, 0);
      addOptionalRegular<ELFT>(End, OS, -1);
    } else {
      if (Config->Pic)
        OS = Out::ElfHeader;
      addOptionalRegular<ELFT>(Start, OS, 0);
      addOptionalRegular<ELFT>(End, OS, 0);
    }
  };

  Define("__preinit_array_start", "__preinit_array_end", Out::PreinitArray);
  Define("__init_array_start", "__init_array_end", Out::InitArray);
  Define("__fini_array_start", "__fini_array_end", Out::FiniArray);

  if (OutputSection *Sec = findSection(".ARM.exidx"))
    Define("__exidx_start", "__exidx_end", Sec);
}

// If a section name is valid as a C identifier (which is rare because of
// the leading '.'), linkers are expected to define __start_<secname> and
// __stop_<secname> symbols. They are at beginning and end of the section,
// respectively. This is not requested by the ELF standard, but GNU ld and
// gold provide the feature, and used by many programs.
template <class ELFT>
void Writer<ELFT>::addStartStopSymbols(OutputSection *Sec) {
  StringRef S = Sec->Name;
  if (!isValidCIdentifier(S))
    return;
  addOptionalRegular<ELFT>(Saver.save("__start_" + S), Sec, 0, STV_DEFAULT);
  addOptionalRegular<ELFT>(Saver.save("__stop_" + S), Sec, -1, STV_DEFAULT);
}

template <class ELFT>
OutputSectionCommand *Writer<ELFT>::findSectionCommand(StringRef Name) {
  for (OutputSectionCommand *Cmd : OutputSectionCommands)
    if (Cmd->Name == Name)
      return Cmd;
  return nullptr;
}

template <class ELFT> OutputSection *Writer<ELFT>::findSectionInScript(StringRef Name) {
  if (OutputSectionCommand *Cmd = findSectionCommand(Name))
    return Cmd->Sec;
  return nullptr;
}

template <class ELFT> OutputSection *Writer<ELFT>::findSection(StringRef Name) {
  for (OutputSection *Sec : OutputSections)
    if (Sec->Name == Name)
      return Sec;
  return nullptr;
}

static bool needsPtLoad(OutputSection *Sec) {
  if (!(Sec->Flags & SHF_ALLOC))
    return false;

  // Don't allocate VA space for TLS NOBITS sections. The PT_TLS PHDR is
  // responsible for allocating space for them, not the PT_LOAD that
  // contains the TLS initialization image.
  if (Sec->Flags & SHF_TLS && Sec->Type == SHT_NOBITS)
    return false;
  return true;
}

// Linker scripts are responsible for aligning addresses. Unfortunately, most
// linker scripts are designed for creating two PT_LOADs only, one RX and one
// RW. This means that there is no alignment in the RO to RX transition and we
// cannot create a PT_LOAD there.
static uint64_t computeFlags(uint64_t Flags) {
  if (Config->Omagic)
    return PF_R | PF_W | PF_X;
  if (Config->SingleRoRx && !(Flags & PF_W))
    return Flags | PF_X;
  return Flags;
}

// Decide which program headers to create and which sections to include in each
// one.
template <class ELFT> std::vector<PhdrEntry> Writer<ELFT>::createPhdrs() {
  std::vector<PhdrEntry> Ret;
  auto AddHdr = [&](unsigned Type, unsigned Flags) -> PhdrEntry * {
    Ret.emplace_back(Type, Flags);
    return &Ret.back();
  };

  // The first phdr entry is PT_PHDR which describes the program header itself.
  AddHdr(PT_PHDR, PF_R)->add(Out::ProgramHeaders);

  // PT_INTERP must be the second entry if exists.
  if (OutputSection *Sec = findSection(".interp"))
    AddHdr(PT_INTERP, Sec->getPhdrFlags())->add(Sec);

  // Add the first PT_LOAD segment for regular output sections.
  uint64_t Flags = computeFlags(PF_R);
  PhdrEntry *Load = AddHdr(PT_LOAD, Flags);

  // Add the headers. We will remove them if they don't fit.
  Load->add(Out::ElfHeader);
  Load->add(Out::ProgramHeaders);

  for (OutputSection *Sec : OutputSections) {
    if (!(Sec->Flags & SHF_ALLOC))
      break;
    if (!needsPtLoad(Sec))
      continue;

    // Segments are contiguous memory regions that has the same attributes
    // (e.g. executable or writable). There is one phdr for each segment.
    // Therefore, we need to create a new phdr when the next section has
    // different flags or is loaded at a discontiguous address using AT linker
    // script command.
    uint64_t NewFlags = computeFlags(Sec->getPhdrFlags());
    if (Script->hasLMA(Sec) || Flags != NewFlags) {
      Load = AddHdr(PT_LOAD, NewFlags);
      Flags = NewFlags;
    }

    Load->add(Sec);
  }

  // Add a TLS segment if any.
  PhdrEntry TlsHdr(PT_TLS, PF_R);
  for (OutputSection *Sec : OutputSections)
    if (Sec->Flags & SHF_TLS)
      TlsHdr.add(Sec);
  if (TlsHdr.First)
    Ret.push_back(std::move(TlsHdr));

  // Add an entry for .dynamic.
  if (InX::DynSymTab)
    AddHdr(PT_DYNAMIC, InX::Dynamic->getParent()->getPhdrFlags())
        ->add(InX::Dynamic->getParent());

  // PT_GNU_RELRO includes all sections that should be marked as
  // read-only by dynamic linker after proccessing relocations.
  PhdrEntry RelRo(PT_GNU_RELRO, PF_R);
  for (OutputSection *Sec : OutputSections)
    if (needsPtLoad(Sec) && isRelroSection(Sec))
      RelRo.add(Sec);
  if (RelRo.First)
    Ret.push_back(std::move(RelRo));

  // PT_GNU_EH_FRAME is a special section pointing on .eh_frame_hdr.
  if (!In<ELFT>::EhFrame->empty() && In<ELFT>::EhFrameHdr &&
      In<ELFT>::EhFrame->getParent() && In<ELFT>::EhFrameHdr->getParent())
    AddHdr(PT_GNU_EH_FRAME, In<ELFT>::EhFrameHdr->getParent()->getPhdrFlags())
        ->add(In<ELFT>::EhFrameHdr->getParent());

  // PT_OPENBSD_RANDOMIZE is an OpenBSD-specific feature. That makes
  // the dynamic linker fill the segment with random data.
  if (OutputSection *Sec = findSection(".openbsd.randomdata"))
    AddHdr(PT_OPENBSD_RANDOMIZE, Sec->getPhdrFlags())->add(Sec);

  // PT_GNU_STACK is a special section to tell the loader to make the
  // pages for the stack non-executable. If you really want an executable
  // stack, you can pass -z execstack, but that's not recommended for
  // security reasons.
  unsigned Perm;
  if (Config->ZExecstack)
    Perm = PF_R | PF_W | PF_X;
  else
    Perm = PF_R | PF_W;
  AddHdr(PT_GNU_STACK, Perm)->p_memsz = Config->ZStackSize;

  // PT_OPENBSD_WXNEEDED is a OpenBSD-specific header to mark the executable
  // is expected to perform W^X violations, such as calling mprotect(2) or
  // mmap(2) with PROT_WRITE | PROT_EXEC, which is prohibited by default on
  // OpenBSD.
  if (Config->ZWxneeded)
    AddHdr(PT_OPENBSD_WXNEEDED, PF_X);

  // Create one PT_NOTE per a group of contiguous .note sections.
  PhdrEntry *Note = nullptr;
  for (OutputSection *Sec : OutputSections) {
    if (Sec->Type == SHT_NOTE) {
      if (!Note || Script->hasLMA(Sec))
        Note = AddHdr(PT_NOTE, PF_R);
      Note->add(Sec);
    } else {
      Note = nullptr;
    }
  }
  return Ret;
}

template <class ELFT>
void Writer<ELFT>::addPtArmExid(std::vector<PhdrEntry> &Phdrs) {
  if (Config->EMachine != EM_ARM)
    return;
  auto I = std::find_if(
      OutputSections.begin(), OutputSections.end(),
      [](OutputSection *Sec) { return Sec->Type == SHT_ARM_EXIDX; });
  if (I == OutputSections.end())
    return;

  // PT_ARM_EXIDX is the ARM EHABI equivalent of PT_GNU_EH_FRAME
  PhdrEntry ARMExidx(PT_ARM_EXIDX, PF_R);
  ARMExidx.add(*I);
  Phdrs.push_back(ARMExidx);
}

// The first section of each PT_LOAD, the first section in PT_GNU_RELRO and the
// first section after PT_GNU_RELRO have to be page aligned so that the dynamic
// linker can set the permissions.
template <class ELFT> void Writer<ELFT>::fixSectionAlignments() {
  auto PageAlign = [](OutputSection *Sec) {
    OutputSectionCommand *Cmd = Script->getCmd(Sec);
    if (Cmd && !Cmd->AddrExpr)
      Cmd->AddrExpr = [=] {
        return alignTo(Script->getDot(), Config->MaxPageSize);
      };
  };

  for (const PhdrEntry &P : Phdrs)
    if (P.p_type == PT_LOAD && P.First)
      PageAlign(P.First);

  for (const PhdrEntry &P : Phdrs) {
    if (P.p_type != PT_GNU_RELRO)
      continue;
    if (P.First)
      PageAlign(P.First);
    // Find the first section after PT_GNU_RELRO. If it is in a PT_LOAD we
    // have to align it to a page.
    auto End = OutputSectionCommands.end();
    auto I =
        std::find(OutputSectionCommands.begin(), End, Script->getCmd(P.Last));
    if (I == End || (I + 1) == End)
      continue;
    OutputSection *Sec = (*(I + 1))->Sec;
    if (needsPtLoad(Sec))
      PageAlign(Sec);
  }
}

// Adjusts the file alignment for a given output section and returns
// its new file offset. The file offset must be the same with its
// virtual address (modulo the page size) so that the loader can load
// executables without any address adjustment.
static uint64_t getFileAlignment(uint64_t Off, OutputSection *Sec) {
  OutputSection *First = Sec->FirstInPtLoad;
  // If the section is not in a PT_LOAD, we just have to align it.
  if (!First)
    return alignTo(Off, Sec->Alignment);

  // The first section in a PT_LOAD has to have congruent offset and address
  // module the page size.
  if (Sec == First)
    return alignTo(Off, Config->MaxPageSize, Sec->Addr);

  // If two sections share the same PT_LOAD the file offset is calculated
  // using this formula: Off2 = Off1 + (VA2 - VA1).
  return First->Offset + Sec->Addr - First->Addr;
}

static uint64_t setOffset(OutputSection *Sec, uint64_t Off) {
  if (Sec->Type == SHT_NOBITS) {
    Sec->Offset = Off;
    return Off;
  }

  Off = getFileAlignment(Off, Sec);
  Sec->Offset = Off;
  return Off + Sec->Size;
}

template <class ELFT> void Writer<ELFT>::assignFileOffsetsBinary() {
  uint64_t Off = 0;
  for (OutputSectionCommand *Cmd : OutputSectionCommands) {
    OutputSection *Sec = Cmd->Sec;
    if (Sec->Flags & SHF_ALLOC)
      Off = setOffset(Sec, Off);
  }
  FileSize = alignTo(Off, Config->Wordsize);
}

// Assign file offsets to output sections.
template <class ELFT> void Writer<ELFT>::assignFileOffsets() {
  uint64_t Off = 0;
  Off = setOffset(Out::ElfHeader, Off);
  Off = setOffset(Out::ProgramHeaders, Off);

  for (OutputSectionCommand *Cmd : OutputSectionCommands)
    Off = setOffset(Cmd->Sec, Off);

  SectionHeaderOff = alignTo(Off, Config->Wordsize);
  FileSize =
      SectionHeaderOff + (OutputSectionCommands.size() + 1) * sizeof(Elf_Shdr);
}

// Finalize the program headers. We call this function after we assign
// file offsets and VAs to all sections.
template <class ELFT> void Writer<ELFT>::setPhdrs() {
  for (PhdrEntry &P : Phdrs) {
    OutputSection *First = P.First;
    OutputSection *Last = P.Last;
    if (First) {
      P.p_filesz = Last->Offset - First->Offset;
      if (Last->Type != SHT_NOBITS)
        P.p_filesz += Last->Size;
      P.p_memsz = Last->Addr + Last->Size - First->Addr;
      P.p_offset = First->Offset;
      P.p_vaddr = First->Addr;
      if (!P.HasLMA)
        P.p_paddr = First->getLMA();
    }
    if (P.p_type == PT_LOAD)
      P.p_align = Config->MaxPageSize;
    else if (P.p_type == PT_GNU_RELRO)
      P.p_align = 1;

    // The TLS pointer goes after PT_TLS. At least glibc will align it,
    // so round up the size to make sure the offsets are correct.
    if (P.p_type == PT_TLS) {
      Out::TlsPhdr = &P;
      if (P.p_memsz)
        P.p_memsz = alignTo(P.p_memsz, P.p_align);
    }
  }
}

// The entry point address is chosen in the following ways.
//
// 1. the '-e' entry command-line option;
// 2. the ENTRY(symbol) command in a linker control script;
// 3. the value of the symbol start, if present;
// 4. the address of the first byte of the .text section, if present;
// 5. the address 0.
template <class ELFT> uint64_t Writer<ELFT>::getEntryAddr() {
  // Case 1, 2 or 3. As a special case, if the symbol is actually
  // a number, we'll use that number as an address.
  if (SymbolBody *B = Symtab<ELFT>::X->find(Config->Entry))
    return B->getVA();
  uint64_t Addr;
  if (to_integer(Config->Entry, Addr))
    return Addr;

  // Case 4
  if (OutputSection *Sec = findSectionInScript(".text")) {
    if (Config->WarnMissingEntry)
      warn("cannot find entry symbol " + Config->Entry + "; defaulting to 0x" +
           utohexstr(Sec->Addr));
    return Sec->Addr;
  }

  // Case 5
  if (Config->WarnMissingEntry)
    warn("cannot find entry symbol " + Config->Entry +
         "; not setting start address");
  return 0;
}

static uint16_t getELFType() {
  if (Config->Pic)
    return ET_DYN;
  if (Config->Relocatable)
    return ET_REL;
  return ET_EXEC;
}

// This function is called after we have assigned address and size
// to each section. This function fixes some predefined
// symbol values that depend on section address and size.
template <class ELFT> void Writer<ELFT>::fixPredefinedSymbols() {
  // _etext is the first location after the last read-only loadable segment.
  // _edata is the first location after the last read-write loadable segment.
  // _end is the first location after the uninitialized data region.
  PhdrEntry *Last = nullptr;
  PhdrEntry *LastRO = nullptr;
  PhdrEntry *LastRW = nullptr;
  for (PhdrEntry &P : Phdrs) {
    if (P.p_type != PT_LOAD)
      continue;
    Last = &P;
    if (P.p_flags & PF_W)
      LastRW = &P;
    else
      LastRO = &P;
  }

  auto Set = [](DefinedRegular *S, OutputSection *Sec, uint64_t Value) {
    if (S) {
      S->Section = Sec;
      S->Value = Value;
    }
  };

  if (Last) {
    Set(ElfSym::End1, Last->First, Last->p_memsz);
    Set(ElfSym::End2, Last->First, Last->p_memsz);
  }
  if (LastRO) {
    Set(ElfSym::Etext1, LastRO->First, LastRO->p_filesz);
    Set(ElfSym::Etext2, LastRO->First, LastRO->p_filesz);
  }
  if (LastRW) {
    Set(ElfSym::Edata1, LastRW->First, LastRW->p_filesz);
    Set(ElfSym::Edata2, LastRW->First, LastRW->p_filesz);
  }

  if (ElfSym::Bss)
    ElfSym::Bss->Section = findSectionInScript(".bss");

  // Setup MIPS _gp_disp/__gnu_local_gp symbols which should
  // be equal to the _gp symbol's value.
  if (Config->EMachine == EM_MIPS && !ElfSym::MipsGp->Value) {
    // Find GP-relative section with the lowest address
    // and use this address to calculate default _gp value.
    for (const OutputSectionCommand *Cmd : OutputSectionCommands) {
      OutputSection *OS = Cmd->Sec;
      if (OS->Flags & SHF_MIPS_GPREL) {
        ElfSym::MipsGp->Value = OS->Addr + 0x7ff0;
        break;
      }
    }
  }
}

template <class ELFT> void Writer<ELFT>::writeHeader() {
  uint8_t *Buf = Buffer->getBufferStart();
  memcpy(Buf, "\177ELF", 4);

  // Write the ELF header.
  auto *EHdr = reinterpret_cast<Elf_Ehdr *>(Buf);
  EHdr->e_ident[EI_CLASS] = Config->Is64 ? ELFCLASS64 : ELFCLASS32;
  EHdr->e_ident[EI_DATA] = Config->IsLE ? ELFDATA2LSB : ELFDATA2MSB;
  EHdr->e_ident[EI_VERSION] = EV_CURRENT;
  EHdr->e_ident[EI_OSABI] = Config->OSABI;
  EHdr->e_type = getELFType();
  EHdr->e_machine = Config->EMachine;
  EHdr->e_version = EV_CURRENT;
  EHdr->e_entry = getEntryAddr();
  EHdr->e_shoff = SectionHeaderOff;
  EHdr->e_ehsize = sizeof(Elf_Ehdr);
  EHdr->e_phnum = Phdrs.size();
  EHdr->e_shentsize = sizeof(Elf_Shdr);
  EHdr->e_shnum = OutputSectionCommands.size() + 1;
  EHdr->e_shstrndx = InX::ShStrTab->getParent()->SectionIndex;

  if (Config->EMachine == EM_ARM)
    // We don't currently use any features incompatible with EF_ARM_EABI_VER5,
    // but we don't have any firm guarantees of conformance. Linux AArch64
    // kernels (as of 2016) require an EABI version to be set.
    EHdr->e_flags = EF_ARM_EABI_VER5;
  else if (Config->EMachine == EM_MIPS)
    EHdr->e_flags = getMipsEFlags<ELFT>();

  if (!Config->Relocatable) {
    EHdr->e_phoff = sizeof(Elf_Ehdr);
    EHdr->e_phentsize = sizeof(Elf_Phdr);
  }

  // Write the program header table.
  auto *HBuf = reinterpret_cast<Elf_Phdr *>(Buf + EHdr->e_phoff);
  for (PhdrEntry &P : Phdrs) {
    HBuf->p_type = P.p_type;
    HBuf->p_flags = P.p_flags;
    HBuf->p_offset = P.p_offset;
    HBuf->p_vaddr = P.p_vaddr;
    HBuf->p_paddr = P.p_paddr;
    HBuf->p_filesz = P.p_filesz;
    HBuf->p_memsz = P.p_memsz;
    HBuf->p_align = P.p_align;
    ++HBuf;
  }

  // Write the section header table. Note that the first table entry is null.
  auto *SHdrs = reinterpret_cast<Elf_Shdr *>(Buf + EHdr->e_shoff);
  for (OutputSectionCommand *Cmd : OutputSectionCommands)
    Cmd->Sec->writeHeaderTo<ELFT>(++SHdrs);
}

// Open a result file.
template <class ELFT> void Writer<ELFT>::openFile() {
  if (!Config->Is64 && FileSize > UINT32_MAX) {
    error("output file too large: " + Twine(FileSize) + " bytes");
    return;
  }

  unlinkAsync(Config->OutputFile);
  ErrorOr<std::unique_ptr<FileOutputBuffer>> BufferOrErr =
      FileOutputBuffer::create(Config->OutputFile, FileSize,
                               FileOutputBuffer::F_executable);

  if (auto EC = BufferOrErr.getError())
    error("failed to open " + Config->OutputFile + ": " + EC.message());
  else
    Buffer = std::move(*BufferOrErr);
}

template <class ELFT> void Writer<ELFT>::writeSectionsBinary() {
  uint8_t *Buf = Buffer->getBufferStart();
  for (OutputSectionCommand *Cmd : OutputSectionCommands) {
    OutputSection *Sec = Cmd->Sec;
    if (Sec->Flags & SHF_ALLOC)
      Cmd->writeTo<ELFT>(Buf + Sec->Offset);
  }
}

// Write section contents to a mmap'ed file.
template <class ELFT> void Writer<ELFT>::writeSections() {
  uint8_t *Buf = Buffer->getBufferStart();

  // PPC64 needs to process relocations in the .opd section
  // before processing relocations in code-containing sections.
  if (auto *OpdCmd = findSectionCommand(".opd")) {
    Out::Opd = OpdCmd->Sec;
    Out::OpdBuf = Buf + Out::Opd->Offset;
    OpdCmd->template writeTo<ELFT>(Buf + Out::Opd->Offset);
  }

  OutputSection *EhFrameHdr =
      (In<ELFT>::EhFrameHdr && !In<ELFT>::EhFrameHdr->empty())
          ? In<ELFT>::EhFrameHdr->getParent()
          : nullptr;

  // In -r or -emit-relocs mode, write the relocation sections first as in
  // ELf_Rel targets we might find out that we need to modify the relocated
  // section while doing it.
  for (OutputSectionCommand *Cmd : OutputSectionCommands) {
    OutputSection *Sec = Cmd->Sec;
    if (Sec->Type == SHT_REL || Sec->Type == SHT_RELA)
      Cmd->writeTo<ELFT>(Buf + Sec->Offset);
  }

  for (OutputSectionCommand *Cmd : OutputSectionCommands) {
    OutputSection *Sec = Cmd->Sec;
    if (Sec != Out::Opd && Sec != EhFrameHdr && Sec->Type != SHT_REL &&
        Sec->Type != SHT_RELA)
      Cmd->writeTo<ELFT>(Buf + Sec->Offset);
  }

  // The .eh_frame_hdr depends on .eh_frame section contents, therefore
  // it should be written after .eh_frame is written.
  if (EhFrameHdr) {
    OutputSectionCommand *Cmd = Script->getCmd(EhFrameHdr);
    Cmd->writeTo<ELFT>(Buf + EhFrameHdr->Offset);
  }
}

template <class ELFT> void Writer<ELFT>::writeBuildId() {
  if (!InX::BuildId || !InX::BuildId->getParent())
    return;

  // Compute a hash of all sections of the output file.
  uint8_t *Start = Buffer->getBufferStart();
  uint8_t *End = Start + FileSize;
  InX::BuildId->writeBuildId({Start, End});
}

template void elf::writeResult<ELF32LE>();
template void elf::writeResult<ELF32BE>();
template void elf::writeResult<ELF64LE>();
template void elf::writeResult<ELF64BE>();
