//===- PseudoLoweringEmitter.cpp - PseudoLowering Generator -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "pseudo-lowering"
#include "CodeGenInstruction.h"
#include "PseudoLoweringEmitter.h"
#include "llvm/TableGen/Error.h"
#include "llvm/TableGen/Record.h"
#include "llvm/ADT/IndexedMap.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Debug.h"
#include <vector>
using namespace llvm;

// FIXME: This pass currently can only expand a pseudo to a single instruction.
//        The pseudo expansion really should take a list of dags, not just
//        a single dag, so we can do fancier things.

unsigned PseudoLoweringEmitter::
addDagOperandMapping(Record *Rec, DagInit *Dag, CodeGenInstruction &Insn,
                     IndexedMap<OpData> &OperandMap, unsigned BaseIdx) {
  unsigned OpsAdded = 0;
  for (unsigned i = 0, e = Dag->getNumArgs(); i != e; ++i) {
    if (DefInit *DI = dynamic_cast<DefInit*>(Dag->getArg(i))) {
      // Physical register reference. Explicit check for the special case
      // "zero_reg" definition.
      if (DI->getDef()->isSubClassOf("Register") ||
          DI->getDef()->getName() == "zero_reg") {
        OperandMap[BaseIdx + i].Kind = OpData::Reg;
        OperandMap[BaseIdx + i].Data.Reg = DI->getDef();
        ++OpsAdded;
        continue;
      }

      // Normal operands should always have the same type, or we have a
      // problem.
      // FIXME: We probably shouldn't ever get a non-zero BaseIdx here.
      assert(BaseIdx == 0 && "Named subargument in pseudo expansion?!");
      if (DI->getDef() != Insn.Operands[BaseIdx + i].Rec)
        throw TGError(Rec->getLoc(),
                      "Pseudo operand type '" + DI->getDef()->getName() +
                      "' does not match expansion operand type '" +
                      Insn.Operands[BaseIdx + i].Rec->getName() + "'");
      // Source operand maps to destination operand. The Data element
      // will be filled in later, just set the Kind for now. Do it
      // for each corresponding MachineInstr operand, not just the first.
      for (unsigned I = 0, E = Insn.Operands[i].MINumOperands; I != E; ++I)
        OperandMap[BaseIdx + i + I].Kind = OpData::Operand;
      OpsAdded += Insn.Operands[i].MINumOperands;
    } else if (IntInit *II = dynamic_cast<IntInit*>(Dag->getArg(i))) {
      OperandMap[BaseIdx + i].Kind = OpData::Imm;
      OperandMap[BaseIdx + i].Data.Imm = II->getValue();
      ++OpsAdded;
    } else if (DagInit *SubDag = dynamic_cast<DagInit*>(Dag->getArg(i))) {
      // Just add the operands recursively. This is almost certainly
      // a constant value for a complex operand (> 1 MI operand).
      unsigned NewOps =
        addDagOperandMapping(Rec, SubDag, Insn, OperandMap, BaseIdx + i);
      OpsAdded += NewOps;
      // Since we added more than one, we also need to adjust the base.
      BaseIdx += NewOps - 1;
    } else
      assert(0 && "Unhandled pseudo-expansion argument type!");
  }
  return OpsAdded;
}

void PseudoLoweringEmitter::evaluateExpansion(Record *Rec) {
  DEBUG(dbgs() << "Pseudo definition: " << Rec->getName() << "\n");

  // Validate that the result pattern has the corrent number and types
  // of arguments for the instruction it references.
  DagInit *Dag = Rec->getValueAsDag("ResultInst");
  assert(Dag && "Missing result instruction in pseudo expansion!");
  DEBUG(dbgs() << "  Result: " << *Dag << "\n");

  DefInit *OpDef = dynamic_cast<DefInit*>(Dag->getOperator());
  if (!OpDef)
    throw TGError(Rec->getLoc(), Rec->getName() +
                  " has unexpected operator type!");
  Record *Operator = OpDef->getDef();
  if (!Operator->isSubClassOf("Instruction"))
    throw TGError(Rec->getLoc(), "Pseudo result '" + Operator->getName() +
                                 "' is not an instruction!");

  CodeGenInstruction Insn(Operator);

  if (Insn.isCodeGenOnly || Insn.isPseudo)
    throw TGError(Rec->getLoc(), "Pseudo result '" + Operator->getName() +
                                 "' cannot be another pseudo instruction!");

  if (Insn.Operands.size() != Dag->getNumArgs())
    throw TGError(Rec->getLoc(), "Pseudo result '" + Operator->getName() +
                                 "' operand count mismatch");

  IndexedMap<OpData> OperandMap;
  OperandMap.grow(Insn.Operands.size());

  addDagOperandMapping(Rec, Dag, Insn, OperandMap, 0);

  // If there are more operands that weren't in the DAG, they have to
  // be operands that have default values, or we have an error. Currently,
  // PredicateOperand and OptionalDefOperand both have default values.


  // Validate that each result pattern argument has a matching (by name)
  // argument in the source instruction, in either the (outs) or (ins) list.
  // Also check that the type of the arguments match.
  //
  // Record the mapping of the source to result arguments for use by
  // the lowering emitter.
  CodeGenInstruction SourceInsn(Rec);
  StringMap<unsigned> SourceOperands;
  for (unsigned i = 0, e = SourceInsn.Operands.size(); i != e; ++i)
    SourceOperands[SourceInsn.Operands[i].Name] = i;

  DEBUG(dbgs() << "  Operand mapping:\n");
  for (unsigned i = 0, e = Insn.Operands.size(); i != e; ++i) {
    // We've already handled constant values. Just map instruction operands
    // here.
    if (OperandMap[Insn.Operands[i].MIOperandNo].Kind != OpData::Operand)
      continue;
    StringMap<unsigned>::iterator SourceOp =
      SourceOperands.find(Dag->getArgName(i));
    if (SourceOp == SourceOperands.end())
      throw TGError(Rec->getLoc(),
                    "Pseudo output operand '" + Dag->getArgName(i) +
                    "' has no matching source operand.");
    // Map the source operand to the destination operand index for each
    // MachineInstr operand.
    for (unsigned I = 0, E = Insn.Operands[i].MINumOperands; I != E; ++I)
      OperandMap[Insn.Operands[i].MIOperandNo + I].Data.Operand =
        SourceOp->getValue();

    DEBUG(dbgs() << "    " << SourceOp->getValue() << " ==> " << i << "\n");
  }

  Expansions.push_back(PseudoExpansion(SourceInsn, Insn, OperandMap));
}

void PseudoLoweringEmitter::emitLoweringEmitter(raw_ostream &o) {
  // Emit file header.
  EmitSourceFileHeader("Pseudo-instruction MC lowering Source Fragment", o);

  o << "bool " << Target.getName() + "AsmPrinter" << "::\n"
    << "emitPseudoExpansionLowering(MCStreamer &OutStreamer,\n"
    << "                            const MachineInstr *MI) {\n"
    << "  switch (MI->getOpcode()) {\n"
    << "    default: return false;\n";
  for (unsigned i = 0, e = Expansions.size(); i != e; ++i) {
    PseudoExpansion &Expansion = Expansions[i];
    CodeGenInstruction &Source = Expansion.Source;
    CodeGenInstruction &Dest = Expansion.Dest;
    o << "    case " << Source.Namespace << "::"
      << Source.TheDef->getName() << ": {\n"
      << "      MCInst TmpInst;\n"
      << "      MCOperand MCOp;\n"
      << "      TmpInst.setOpcode(" << Dest.Namespace << "::"
      << Dest.TheDef->getName() << ");\n";

    // Copy the operands from the source instruction.
    // FIXME: Instruction operands with defaults values (predicates and cc_out
    //        in ARM, for example shouldn't need explicit values in the
    //        expansion DAG.
    unsigned MIOpNo = 0;
    for (unsigned OpNo = 0, E = Dest.Operands.size(); OpNo != E;
         ++OpNo) {
      o << "      // Operand: " << Dest.Operands[OpNo].Name << "\n";
      for (unsigned i = 0, e = Dest.Operands[OpNo].MINumOperands;
           i != e; ++i) {
        switch (Expansion.OperandMap[MIOpNo + i].Kind) {
        default:
          llvm_unreachable("Unknown operand type?!");
        case OpData::Operand:
          o << "      lowerOperand(MI->getOperand("
            << Source.Operands[Expansion.OperandMap[MIOpNo].Data
                .Operand].MIOperandNo + i
            << "), MCOp);\n"
            << "      TmpInst.addOperand(MCOp);\n";
          break;
        case OpData::Imm:
          o << "      TmpInst.addOperand(MCOperand::CreateImm("
            << Expansion.OperandMap[MIOpNo + i].Data.Imm << "));\n";
          break;
        case OpData::Reg: {
          Record *Reg = Expansion.OperandMap[MIOpNo + i].Data.Reg;
          o << "      TmpInst.addOperand(MCOperand::CreateReg(";
          // "zero_reg" is special.
          if (Reg->getName() == "zero_reg")
            o << "0";
          else
            o << Reg->getValueAsString("Namespace") << "::" << Reg->getName();
          o << "));\n";
          break;
        }
        }
      }
      MIOpNo += Dest.Operands[OpNo].MINumOperands;
    }
    if (Dest.Operands.isVariadic) {
      o << "      // variable_ops\n";
      o << "      for (unsigned i = " << MIOpNo
        << ", e = MI->getNumOperands(); i != e; ++i)\n"
        << "        if (lowerOperand(MI->getOperand(i), MCOp))\n"
        << "          TmpInst.addOperand(MCOp);\n";
    }
    o << "      OutStreamer.EmitInstruction(TmpInst);\n"
      << "      break;\n"
      << "    }\n";
  }
  o << "  }\n  return true;\n}\n\n";
}

void PseudoLoweringEmitter::run(raw_ostream &o) {
  Record *ExpansionClass = Records.getClass("PseudoInstExpansion");
  Record *InstructionClass = Records.getClass("PseudoInstExpansion");
  assert(ExpansionClass && "PseudoInstExpansion class definition missing!");
  assert(InstructionClass && "Instruction class definition missing!");

  std::vector<Record*> Insts;
  for (std::map<std::string, Record*>::const_iterator I =
         Records.getDefs().begin(), E = Records.getDefs().end(); I != E; ++I) {
    if (I->second->isSubClassOf(ExpansionClass) &&
        I->second->isSubClassOf(InstructionClass))
      Insts.push_back(I->second);
  }

  // Process the pseudo expansion definitions, validating them as we do so.
  for (unsigned i = 0, e = Insts.size(); i != e; ++i)
    evaluateExpansion(Insts[i]);

  // Generate expansion code to lower the pseudo to an MCInst of the real
  // instruction.
  emitLoweringEmitter(o);
}

