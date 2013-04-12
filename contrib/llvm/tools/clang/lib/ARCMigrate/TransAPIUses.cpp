//===--- TransAPIUses.cpp - Transformations to ARC mode -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// checkAPIUses:
//
// Emits error/fix with some API uses that are obsolete or not safe in ARC mode:
//
// - NSInvocation's [get/set]ReturnValue and [get/set]Argument are only safe
//   with __unsafe_unretained objects.
// - Calling -zone gets replaced with 'nil'.
//
//===----------------------------------------------------------------------===//

#include "Transforms.h"
#include "Internals.h"
#include "clang/AST/ASTContext.h"
#include "clang/Sema/SemaDiagnostic.h"

using namespace clang;
using namespace arcmt;
using namespace trans;

namespace {

class APIChecker : public RecursiveASTVisitor<APIChecker> {
  MigrationPass &Pass;

  Selector getReturnValueSel, setReturnValueSel;
  Selector getArgumentSel, setArgumentSel;

  Selector zoneSel;
public:
  APIChecker(MigrationPass &pass) : Pass(pass) {
    SelectorTable &sels = Pass.Ctx.Selectors;
    IdentifierTable &ids = Pass.Ctx.Idents;
    getReturnValueSel = sels.getUnarySelector(&ids.get("getReturnValue"));
    setReturnValueSel = sels.getUnarySelector(&ids.get("setReturnValue"));

    IdentifierInfo *selIds[2];
    selIds[0] = &ids.get("getArgument");
    selIds[1] = &ids.get("atIndex");
    getArgumentSel = sels.getSelector(2, selIds);
    selIds[0] = &ids.get("setArgument");
    setArgumentSel = sels.getSelector(2, selIds);

    zoneSel = sels.getNullarySelector(&ids.get("zone"));
  }

  bool VisitObjCMessageExpr(ObjCMessageExpr *E) {
    // NSInvocation.
    if (E->isInstanceMessage() &&
        E->getReceiverInterface() &&
        E->getReceiverInterface()->getName() == "NSInvocation") {
      StringRef selName;
      if (E->getSelector() == getReturnValueSel)
        selName = "getReturnValue";
      else if (E->getSelector() == setReturnValueSel)
        selName = "setReturnValue";
      else if (E->getSelector() == getArgumentSel)
        selName = "getArgument";
      else if (E->getSelector() == setArgumentSel)
        selName = "setArgument";

      if (selName.empty())
        return true;

      Expr *parm = E->getArg(0)->IgnoreParenCasts();
      QualType pointee = parm->getType()->getPointeeType();
      if (pointee.isNull())
        return true;

      if (pointee.getObjCLifetime() > Qualifiers::OCL_ExplicitNone) {
        std::string err = "NSInvocation's ";
        err += selName;
        err += " is not safe to be used with an object with ownership other "
            "than __unsafe_unretained";
        Pass.TA.reportError(err, parm->getLocStart(), parm->getSourceRange());
      }
      return true;
    }

    // -zone.
    if (E->isInstanceMessage() &&
        E->getInstanceReceiver() &&
        E->getSelector() == zoneSel &&
        Pass.TA.hasDiagnostic(diag::err_unavailable,
                              diag::err_unavailable_message,
                              E->getInstanceReceiver()->getExprLoc())) {
      // Calling -zone is meaningless in ARC, change it to nil.
      Transaction Trans(Pass.TA);
      Pass.TA.clearDiagnostic(diag::err_unavailable,
                              diag::err_unavailable_message,
                              E->getInstanceReceiver()->getExprLoc());
      Pass.TA.replace(E->getSourceRange(), getNilString(Pass.Ctx));
    }
    return true;
  }
};

} // anonymous namespace

void trans::checkAPIUses(MigrationPass &pass) {
  APIChecker(pass).TraverseDecl(pass.Ctx.getTranslationUnitDecl());
}
