//===-- SparcSubtarget.h - Define Subtarget for the SPARC -------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file declares the SPARC specific subclass of TargetSubtargetInfo.
//
//===----------------------------------------------------------------------===//

#ifndef SPARC_SUBTARGET_H
#define SPARC_SUBTARGET_H

#include "llvm/Target/TargetSubtargetInfo.h"
#include <string>

#define GET_SUBTARGETINFO_HEADER
#include "SparcGenSubtargetInfo.inc"

namespace llvm {
class StringRef;

class SparcSubtarget : public SparcGenSubtargetInfo {
  virtual void anchor();
  bool IsV9;
  bool V8DeprecatedInsts;
  bool IsVIS;
  bool Is64Bit;
  
public:
  SparcSubtarget(const std::string &TT, const std::string &CPU,
                 const std::string &FS, bool is64bit);

  bool isV9() const { return IsV9; }
  bool isVIS() const { return IsVIS; }
  bool useDeprecatedV8Instructions() const { return V8DeprecatedInsts; }
  
  /// ParseSubtargetFeatures - Parses features string setting specified 
  /// subtarget options.  Definition of function is auto generated by tblgen.
  void ParseSubtargetFeatures(StringRef CPU, StringRef FS);
  
  bool is64Bit() const { return Is64Bit; }
  std::string getDataLayout() const {
    const char *p;
    if (is64Bit()) {
      p = "E-p:64:64:64-i64:64:64-f64:64:64-f128:128:128-n32:64";
    } else {
      p = "E-p:32:32:32-i64:64:64-f64:64:64-f128:64:64-n32";
    }
    return std::string(p);
  }
};

} // end namespace llvm

#endif
