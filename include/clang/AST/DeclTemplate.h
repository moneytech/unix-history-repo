//===-- DeclTemplate.h - Classes for representing C++ templates -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the C++ template declaration subclasses.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_DECLTEMPLATE_H
#define LLVM_CLANG_AST_DECLTEMPLATE_H

#include "clang/AST/DeclCXX.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/FoldingSet.h"
#include "llvm/ADT/PointerUnion.h"

namespace clang {

class TemplateParameterList;
class TemplateDecl;
class FunctionTemplateDecl;
class ClassTemplateDecl;
class ClassTemplatePartialSpecializationDecl;
class TemplateTypeParmDecl;
class NonTypeTemplateParmDecl;
class TemplateTemplateParmDecl;

/// \brief Stores a template parameter of any kind.
typedef llvm::PointerUnion3<TemplateTypeParmDecl*, NonTypeTemplateParmDecl*,
                            TemplateTemplateParmDecl*> TemplateParameter;

/// TemplateParameterList - Stores a list of template parameters for a
/// TemplateDecl and its derived classes.
class TemplateParameterList {
  /// The location of the 'template' keyword.
  SourceLocation TemplateLoc;

  /// The locations of the '<' and '>' angle brackets.
  SourceLocation LAngleLoc, RAngleLoc;

  /// The number of template parameters in this template
  /// parameter list.
  unsigned NumParams;

  TemplateParameterList(SourceLocation TemplateLoc, SourceLocation LAngleLoc,
                        Decl **Params, unsigned NumParams,
                        SourceLocation RAngleLoc);

public:
  static TemplateParameterList *Create(ASTContext &C, 
                                       SourceLocation TemplateLoc,
                                       SourceLocation LAngleLoc,
                                       Decl **Params,
                                       unsigned NumParams,
                                       SourceLocation RAngleLoc);

  /// iterator - Iterates through the template parameters in this list.
  typedef Decl** iterator;

  /// const_iterator - Iterates through the template parameters in this list.
  typedef Decl* const* const_iterator;

  iterator begin() { return reinterpret_cast<Decl **>(this + 1); }
  const_iterator begin() const {
    return reinterpret_cast<Decl * const *>(this + 1);
  }
  iterator end() { return begin() + NumParams; }
  const_iterator end() const { return begin() + NumParams; }

  unsigned size() const { return NumParams; }

  Decl* getParam(unsigned Idx) {
    assert(Idx < size() && "Template parameter index out-of-range");
    return begin()[Idx];
  }

  const Decl* getParam(unsigned Idx) const {
    assert(Idx < size() && "Template parameter index out-of-range");
    return begin()[Idx];
  }

  /// \btief Returns the minimum number of arguments needed to form a
  /// template specialization. This may be fewer than the number of
  /// template parameters, if some of the parameters have default
  /// arguments or if there is a parameter pack.
  unsigned getMinRequiredArguments() const;

  SourceLocation getTemplateLoc() const { return TemplateLoc; }
  SourceLocation getLAngleLoc() const { return LAngleLoc; }
  SourceLocation getRAngleLoc() const { return RAngleLoc; }

  SourceRange getSourceRange() const {
    return SourceRange(TemplateLoc, RAngleLoc);
  }
};

//===----------------------------------------------------------------------===//
// Kinds of Templates
//===----------------------------------------------------------------------===//

/// TemplateDecl - The base class of all kinds of template declarations (e.g.,
/// class, function, etc.). The TemplateDecl class stores the list of template
/// parameters and a reference to the templated scoped declaration: the
/// underlying AST node.
class TemplateDecl : public NamedDecl {
protected:
  // This is probably never used.
  TemplateDecl(Kind DK, DeclContext *DC, SourceLocation L,
               DeclarationName Name)
    : NamedDecl(DK, DC, L, Name), TemplatedDecl(0), TemplateParams(0)
  { }

  // Construct a template decl with the given name and parameters.
  // Used when there is not templated element (tt-params, alias?).
  TemplateDecl(Kind DK, DeclContext *DC, SourceLocation L,
               DeclarationName Name, TemplateParameterList *Params)
    : NamedDecl(DK, DC, L, Name), TemplatedDecl(0), TemplateParams(Params)
  { }

  // Construct a template decl with name, parameters, and templated element.
  TemplateDecl(Kind DK, DeclContext *DC, SourceLocation L,
               DeclarationName Name, TemplateParameterList *Params,
               NamedDecl *Decl)
    : NamedDecl(DK, DC, L, Name), TemplatedDecl(Decl),
      TemplateParams(Params) { }
public:
  ~TemplateDecl();

  /// Get the list of template parameters
  TemplateParameterList *getTemplateParameters() const {
    return TemplateParams;
  }

  /// Get the underlying, templated declaration.
  NamedDecl *getTemplatedDecl() const { return TemplatedDecl; }

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Decl *D) {
      return D->getKind() >= TemplateFirst && D->getKind() <= TemplateLast;
  }
  static bool classof(const TemplateDecl *D) { return true; }
  static bool classof(const FunctionTemplateDecl *D) { return true; }
  static bool classof(const ClassTemplateDecl *D) { return true; }
  static bool classof(const TemplateTemplateParmDecl *D) { return true; }

protected:
  NamedDecl *TemplatedDecl;
  TemplateParameterList* TemplateParams;
};

/// Declaration of a template function.
class FunctionTemplateDecl : public TemplateDecl {
protected:
  FunctionTemplateDecl(DeclContext *DC, SourceLocation L, DeclarationName Name,
                       TemplateParameterList *Params, NamedDecl *Decl)
    : TemplateDecl(FunctionTemplate, DC, L, Name, Params, Decl) { }
public:
  /// Get the underling function declaration of the template.
  FunctionDecl *getTemplatedDecl() const {
    return static_cast<FunctionDecl*>(TemplatedDecl);
  }

  /// Create a template function node.
  static FunctionTemplateDecl *Create(ASTContext &C, DeclContext *DC,
                                      SourceLocation L,
                                      DeclarationName Name,
                                      TemplateParameterList *Params,
                                      NamedDecl *Decl);

  // Implement isa/cast/dyncast support
  static bool classof(const Decl *D)
  { return D->getKind() == FunctionTemplate; }
  static bool classof(const FunctionTemplateDecl *D)
  { return true; }
};

//===----------------------------------------------------------------------===//
// Kinds of Template Parameters
//===----------------------------------------------------------------------===//

/// The TemplateParmPosition class defines the position of a template parameter
/// within a template parameter list. Because template parameter can be listed
/// sequentially for out-of-line template members, each template parameter is
/// given a Depth - the nesting of template parameter scopes - and a Position -
/// the occurrence within the parameter list.
/// This class is inheritedly privately by different kinds of template
/// parameters and is not part of the Decl hierarchy. Just a facility.
class TemplateParmPosition
{
protected:
  // FIXME: This should probably never be called, but it's here as
  TemplateParmPosition()
    : Depth(0), Position(0)
  { /* assert(0 && "Cannot create positionless template parameter"); */ }

  TemplateParmPosition(unsigned D, unsigned P)
    : Depth(D), Position(P)
  { }

  // FIXME: These probably don't need to be ints. int:5 for depth, int:8 for
  // position? Maybe?
  unsigned Depth;
  unsigned Position;

public:
  /// Get the nesting depth of the template parameter.
  unsigned getDepth() const { return Depth; }

  /// Get the position of the template parameter within its parameter list.
  unsigned getPosition() const { return Position; }
  
  /// Get the index of the template parameter within its parameter list.
  unsigned getIndex() const { return Position; }
};

/// TemplateTypeParmDecl - Declaration of a template type parameter,
/// e.g., "T" in
/// @code
/// template<typename T> class vector;
/// @endcode
class TemplateTypeParmDecl : public TypeDecl {
  /// \brief Whether this template type parameter was declaration with
  /// the 'typename' keyword. If false, it was declared with the
  /// 'class' keyword.
  bool Typename : 1;

  /// \brief Whether this template type parameter inherited its
  /// default argument.
  bool InheritedDefault : 1;

  /// \brief Whether this is a parameter pack.
  bool ParameterPack : 1;

  /// \brief The location of the default argument, if any.
  SourceLocation DefaultArgumentLoc;

  /// \brief The default template argument, if any.
  QualType DefaultArgument;

  TemplateTypeParmDecl(DeclContext *DC, SourceLocation L, IdentifierInfo *Id, 
                       bool Typename, QualType Type, bool ParameterPack)
    : TypeDecl(TemplateTypeParm, DC, L, Id), Typename(Typename),
      InheritedDefault(false), ParameterPack(ParameterPack), DefaultArgument() { 
    TypeForDecl = Type.getTypePtr();
  }

public:
  static TemplateTypeParmDecl *Create(ASTContext &C, DeclContext *DC,
                                      SourceLocation L, unsigned D, unsigned P,
                                      IdentifierInfo *Id, bool Typename,
                                      bool ParameterPack);

  /// \brief Whether this template type parameter was declared with
  /// the 'typename' keyword. If not, it was declared with the 'class'
  /// keyword.
  bool wasDeclaredWithTypename() const { return Typename; }

  /// \brief Determine whether this template parameter has a default
  /// argument.
  bool hasDefaultArgument() const { return !DefaultArgument.isNull(); }

  /// \brief Retrieve the default argument, if any.
  QualType getDefaultArgument() const { return DefaultArgument; }

  /// \brief Retrieve the location of the default argument, if any.
  SourceLocation getDefaultArgumentLoc() const { return DefaultArgumentLoc; }

  /// \brief Determines whether the default argument was inherited
  /// from a previous declaration of this template.
  bool defaultArgumentWasInherited() const { return InheritedDefault; }

  /// \brief Set the default argument for this template parameter, and
  /// whether that default argument was inherited from another
  /// declaration.
  void setDefaultArgument(QualType DefArg, SourceLocation DefArgLoc,
                          bool Inherited) {
    DefaultArgument = DefArg;
    DefaultArgumentLoc = DefArgLoc;
    InheritedDefault = Inherited;
  }

  /// \brief Returns whether this is a parameter pack.
  bool isParameterPack() const { return ParameterPack; }

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Decl *D) {
    return D->getKind() == TemplateTypeParm;
  }
  static bool classof(const TemplateTypeParmDecl *D) { return true; }
};

/// NonTypeTemplateParmDecl - Declares a non-type template parameter,
/// e.g., "Size" in
/// @code
/// template<int Size> class array { };
/// @endcode
class NonTypeTemplateParmDecl
  : public VarDecl, protected TemplateParmPosition {
  /// \brief The default template argument, if any.
  Expr *DefaultArgument;

  NonTypeTemplateParmDecl(DeclContext *DC, SourceLocation L, unsigned D,
                          unsigned P, IdentifierInfo *Id, QualType T,
                          SourceLocation TSSL = SourceLocation())
    : VarDecl(NonTypeTemplateParm, DC, L, Id, T, VarDecl::None, TSSL),
      TemplateParmPosition(D, P), DefaultArgument(0) 
  { }

public:
  static NonTypeTemplateParmDecl *
  Create(ASTContext &C, DeclContext *DC, SourceLocation L, unsigned D,
         unsigned P, IdentifierInfo *Id, QualType T,
         SourceLocation TypeSpecStartLoc = SourceLocation());

  using TemplateParmPosition::getDepth;
  using TemplateParmPosition::getPosition;
  using TemplateParmPosition::getIndex;
    
  /// \brief Determine whether this template parameter has a default
  /// argument.
  bool hasDefaultArgument() const { return DefaultArgument; }

  /// \brief Retrieve the default argument, if any.
  Expr *getDefaultArgument() const { return DefaultArgument; }

  /// \brief Retrieve the location of the default argument, if any.
  SourceLocation getDefaultArgumentLoc() const;

  /// \brief Set the default argument for this template parameter.
  void setDefaultArgument(Expr *DefArg) {
    DefaultArgument = DefArg;
  }

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Decl *D) {
    return D->getKind() == NonTypeTemplateParm;
  }
  static bool classof(const NonTypeTemplateParmDecl *D) { return true; }
};

/// TemplateTemplateParmDecl - Declares a template template parameter,
/// e.g., "T" in
/// @code
/// template <template <typename> class T> class container { };
/// @endcode
/// A template template parameter is a TemplateDecl because it defines the
/// name of a template and the template parameters allowable for substitution.
class TemplateTemplateParmDecl
  : public TemplateDecl, protected TemplateParmPosition {

  /// \brief The default template argument, if any.
  Expr *DefaultArgument;

  TemplateTemplateParmDecl(DeclContext *DC, SourceLocation L,
                           unsigned D, unsigned P,
                           IdentifierInfo *Id, TemplateParameterList *Params)
    : TemplateDecl(TemplateTemplateParm, DC, L, Id, Params),
      TemplateParmPosition(D, P), DefaultArgument(0)
    { }

public:
  static TemplateTemplateParmDecl *Create(ASTContext &C, DeclContext *DC,
                                          SourceLocation L, unsigned D,
                                          unsigned P, IdentifierInfo *Id,
                                          TemplateParameterList *Params);

  using TemplateParmPosition::getDepth;
  using TemplateParmPosition::getPosition;
  using TemplateParmPosition::getIndex;
    
  /// \brief Determine whether this template parameter has a default
  /// argument.
  bool hasDefaultArgument() const { return DefaultArgument; }

  /// \brief Retrieve the default argument, if any.
  Expr *getDefaultArgument() const { return DefaultArgument; }

  /// \brief Retrieve the location of the default argument, if any.
  SourceLocation getDefaultArgumentLoc() const;

  /// \brief Set the default argument for this template parameter.
  void setDefaultArgument(Expr *DefArg) {
    DefaultArgument = DefArg;
  }

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Decl *D) {
    return D->getKind() == TemplateTemplateParm;
  }
  static bool classof(const TemplateTemplateParmDecl *D) { return true; }
};

/// \brief Represents a template argument within a class template
/// specialization.
class TemplateArgument {
  union {
    uintptr_t TypeOrValue;
    struct {
      char Value[sizeof(llvm::APSInt)];
      void *Type;
    } Integer;
  };

  /// \brief Location of the beginning of this template argument.
  SourceLocation StartLoc;

public:
  /// \brief The type of template argument we're storing.
  enum ArgKind {
    Null = 0,
    /// The template argument is a type. It's value is stored in the
    /// TypeOrValue field.
    Type = 1,
    /// The template argument is a declaration
    Declaration = 2,
    /// The template argument is an integral value stored in an llvm::APSInt.
    Integral = 3,
    /// The template argument is a value- or type-dependent expression
    /// stored in an Expr*.
    Expression = 4
  } Kind;

  /// \brief Construct an empty, invalid template argument.
  TemplateArgument() : TypeOrValue(0), StartLoc(), Kind(Null) { }

  /// \brief Construct a template type argument.
  TemplateArgument(SourceLocation Loc, QualType T) : Kind(Type) {
    TypeOrValue = reinterpret_cast<uintptr_t>(T.getAsOpaquePtr());
    StartLoc = Loc;
  }

  /// \brief Construct a template argument that refers to a
  /// declaration, which is either an external declaration or a
  /// template declaration.
  TemplateArgument(SourceLocation Loc, Decl *D) : Kind(Declaration) {
    // FIXME: Need to be sure we have the "canonical" declaration!
    TypeOrValue = reinterpret_cast<uintptr_t>(D);
    StartLoc = Loc;
  }

  /// \brief Construct an integral constant template argument.
  TemplateArgument(SourceLocation Loc, const llvm::APSInt &Value,
                   QualType Type)
    : Kind(Integral) {
    new (Integer.Value) llvm::APSInt(Value);
    Integer.Type = Type.getAsOpaquePtr();
    StartLoc = Loc;
  }

  /// \brief Construct a template argument that is an expression. 
  ///
  /// This form of template argument only occurs in template argument
  /// lists used for dependent types and for expression; it will not
  /// occur in a non-dependent, canonical template argument list.
  TemplateArgument(Expr *E);

  /// \brief Copy constructor for a template argument.
  TemplateArgument(const TemplateArgument &Other) : Kind(Other.Kind) {
    if (Kind == Integral) {
      new (Integer.Value) llvm::APSInt(*Other.getAsIntegral());
      Integer.Type = Other.Integer.Type;
    }
    else
      TypeOrValue = Other.TypeOrValue;
    StartLoc = Other.StartLoc;
  }

  TemplateArgument& operator=(const TemplateArgument& Other) {
    // FIXME: Does not provide the strong guarantee for exception
    // safety.
    using llvm::APSInt;

    if (Kind == Other.Kind && Kind == Integral) {
      // Copy integral values.
      *this->getAsIntegral() = *Other.getAsIntegral();
      Integer.Type = Other.Integer.Type; 
    } else {
      // Destroy the current integral value, if that's what we're holding.
      if (Kind == Integral)
        getAsIntegral()->~APSInt();
      
      Kind = Other.Kind;
      
      if (Other.Kind == Integral) {
        new (Integer.Value) llvm::APSInt(*Other.getAsIntegral());
        Integer.Type = Other.Integer.Type;
      } else
        TypeOrValue = Other.TypeOrValue;
    }
    StartLoc = Other.StartLoc;

    return *this;
  }

  ~TemplateArgument() {
    using llvm::APSInt;

    if (Kind == Integral)
      getAsIntegral()->~APSInt();
  }

  /// \brief Return the kind of stored template argument.
  ArgKind getKind() const { return Kind; }

  /// \brief Determine whether this template argument has no value.
  bool isNull() const { return Kind == Null; }
  
  /// \brief Retrieve the template argument as a type.
  QualType getAsType() const {
    if (Kind != Type)
      return QualType();

    return QualType::getFromOpaquePtr(
                                 reinterpret_cast<void*>(TypeOrValue));
  }

  /// \brief Retrieve the template argument as a declaration.
  Decl *getAsDecl() const {
    if (Kind != Declaration)
      return 0;
    return reinterpret_cast<Decl *>(TypeOrValue);
  }

  /// \brief Retrieve the template argument as an integral value.
  llvm::APSInt *getAsIntegral() {
    if (Kind != Integral)
      return 0;
    return reinterpret_cast<llvm::APSInt*>(&Integer.Value[0]);
  }

  const llvm::APSInt *getAsIntegral() const {
    return const_cast<TemplateArgument*>(this)->getAsIntegral();
  }

  /// \brief Retrieve the type of the integral value.
  QualType getIntegralType() const {
    if (Kind != Integral)
      return QualType();

    return QualType::getFromOpaquePtr(Integer.Type);
  }

  void setIntegralType(QualType T) {
    assert(Kind == Integral && 
           "Cannot set the integral type of a non-integral template argument");
    Integer.Type = T.getAsOpaquePtr();
  };

  /// \brief Retrieve the template argument as an expression.
  Expr *getAsExpr() const {
    if (Kind != Expression)
      return 0;

    return reinterpret_cast<Expr *>(TypeOrValue);
  }

  /// \brief Retrieve the location where the template argument starts.
  SourceLocation getLocation() const { return StartLoc; }

  /// \brief Used to insert TemplateArguments into FoldingSets.
  void Profile(llvm::FoldingSetNodeID &ID) const {
    ID.AddInteger(Kind);
    switch (Kind) {
    case Null:
      break;
        
    case Type:
      getAsType().Profile(ID);
      break;

    case Declaration:
      ID.AddPointer(getAsDecl()); // FIXME: Must be canonical!
      break;

    case Integral:
      getAsIntegral()->Profile(ID);
      getIntegralType().Profile(ID);
      break;

    case Expression:
      // FIXME: We need a canonical representation of expressions.
      ID.AddPointer(getAsExpr());
      break;
    }
  }
};

/// \brief A helper class for making template argument lists.
class TemplateArgumentListBuilder {
  /// Args - contains the template arguments.
  llvm::SmallVector<TemplateArgument, 16> Args;
  
  llvm::SmallVector<unsigned, 32> Indices;

  ASTContext &Context;
  
  /// isAddingFromParameterPack - Returns whether we're adding arguments from
  /// a parameter pack.
  bool isAddingFromParameterPack() const { return Indices.size() % 2; }
  
public:
  TemplateArgumentListBuilder(ASTContext &Context) : Context(Context) { }
  
  size_t size() const { 
    assert(!isAddingFromParameterPack() && 
           "Size is not valid when adding from a parameter pack");
    
    return Indices.size() / 2;
  }
  
  size_t flatSize() const { return Args.size(); }

  void push_back(const TemplateArgument& Arg);
  
  /// BeginParameterPack - Start adding arguments from a parameter pack.
  void BeginParameterPack();
  
  /// EndParameterPack - Finish adding arguments from a parameter pack.
  void EndParameterPack();
  
  const TemplateArgument *getFlatArgumentList() const { return Args.data(); }
  TemplateArgument *getFlatArgumentList() { return Args.data(); }
};

/// \brief A template argument list.
///
/// FIXME: In the future, this class will be extended to support
/// variadic templates and member templates, which will make some of
/// the function names below make more sense.
class TemplateArgumentList {
  /// \brief The template argument list.
  ///
  /// The integer value will be non-zero to indicate that this
  /// template argument list does not own the pointer.
  llvm::PointerIntPair<TemplateArgument *, 1> Arguments;

  /// \brief The number of template arguments in this template
  /// argument list.
  unsigned NumArguments;

public:
  TemplateArgumentList(ASTContext &Context,
                       TemplateArgumentListBuilder &Builder,
                       bool CopyArgs, bool FlattenArgs);

  ~TemplateArgumentList();

  /// \brief Retrieve the template argument at a given index.
  const TemplateArgument &get(unsigned Idx) const { 
    assert(Idx < NumArguments && "Invalid template argument index");
    return getFlatArgumentList()[Idx];
  }

  /// \brief Retrieve the template argument at a given index.
  TemplateArgument &get(unsigned Idx) { 
    assert(Idx < NumArguments && "Invalid template argument index");
    return getFlatArgumentList()[Idx];
  }

  /// \brief Retrieve the template argument at a given index.
        TemplateArgument &operator[](unsigned Idx)       { return get(Idx); }
  const TemplateArgument &operator[](unsigned Idx) const { return get(Idx); }

  /// \brief Retrieve the number of template arguments in this
  /// template argument list.
  unsigned size() const { return NumArguments; }

  /// \brief Retrieve the number of template arguments in the
  /// flattened template argument list.
  unsigned flat_size() const { return NumArguments; }

  /// \brief Retrieve the flattened template argument list.
  TemplateArgument *getFlatArgumentList() { 
    return Arguments.getPointer();
  }
  const TemplateArgument *getFlatArgumentList() const { 
    return Arguments.getPointer();
  }
};

// \brief Describes the kind of template specialization that a
// particular template specialization declaration represents.
enum TemplateSpecializationKind {
  /// This template specialization was formed from a template-id but
  /// has not yet been declared, defined, or instantiated.
  TSK_Undeclared = 0,
  /// This template specialization was declared or defined by an
  /// explicit specialization (C++ [temp.expl.spec]) or partial
  /// specialization (C++ [temp.class.spec]).
  TSK_ExplicitSpecialization,
  /// This template specialization was implicitly instantiated from a
  /// template. (C++ [temp.inst]).
  TSK_ImplicitInstantiation,
  /// This template specialization was instantiated from a template
  /// due to an explicit instantiation request (C++ [temp.explicit]).
  TSK_ExplicitInstantiation
};

/// \brief Represents a class template specialization, which refers to
/// a class template with a given set of template arguments.
///
/// Class template specializations represent both explicit
/// specialization of class templates, as in the example below, and
/// implicit instantiations of class templates.
///
/// \code
/// template<typename T> class array;
/// 
/// template<> 
/// class array<bool> { }; // class template specialization array<bool>
/// \endcode
class ClassTemplateSpecializationDecl 
  : public CXXRecordDecl, public llvm::FoldingSetNode {
  /// \brief The template that this specialization specializes
  ClassTemplateDecl *SpecializedTemplate;

  /// \brief The template arguments used to describe this specialization.
  TemplateArgumentList TemplateArgs;

  /// \brief The kind of specialization this declaration refers to.
  /// Really a value of type TemplateSpecializationKind.
  unsigned SpecializationKind : 2;

protected:
  ClassTemplateSpecializationDecl(ASTContext &Context, Kind DK,
                                  DeclContext *DC, SourceLocation L,
                                  ClassTemplateDecl *SpecializedTemplate,
                                  TemplateArgumentListBuilder &Builder);
                                  
public:
  static ClassTemplateSpecializationDecl *
  Create(ASTContext &Context, DeclContext *DC, SourceLocation L,
         ClassTemplateDecl *SpecializedTemplate,
         TemplateArgumentListBuilder &Builder,
         ClassTemplateSpecializationDecl *PrevDecl);

  /// \brief Retrieve the template that this specialization specializes.
  ClassTemplateDecl *getSpecializedTemplate() const { 
    return SpecializedTemplate; 
  }

  const TemplateArgumentList &getTemplateArgs() const { 
    return TemplateArgs;
  }

  /// \brief Determine the kind of specialization that this
  /// declaration represents.
  TemplateSpecializationKind getSpecializationKind() const {
    return static_cast<TemplateSpecializationKind>(SpecializationKind);
  }

  void setSpecializationKind(TemplateSpecializationKind TSK) {
    SpecializationKind = TSK;
  }

  /// \brief Sets the type of this specialization as it was written by
  /// the user. This will be a class template specialization type.
  void setTypeAsWritten(QualType T) {
    TypeForDecl = T.getTypePtr();
  }

  void Profile(llvm::FoldingSetNodeID &ID) const {
    Profile(ID, TemplateArgs.getFlatArgumentList(), TemplateArgs.flat_size());
  }

  static void 
  Profile(llvm::FoldingSetNodeID &ID, const TemplateArgument *TemplateArgs, 
          unsigned NumTemplateArgs) {
    ID.AddInteger(NumTemplateArgs);
    for (unsigned Arg = 0; Arg != NumTemplateArgs; ++Arg)
      TemplateArgs[Arg].Profile(ID);
  }

  static bool classof(const Decl *D) { 
    return D->getKind() == ClassTemplateSpecialization ||
           D->getKind() == ClassTemplatePartialSpecialization;
  }

  static bool classof(const ClassTemplateSpecializationDecl *) {
    return true;
  }

  static bool classof(const ClassTemplatePartialSpecializationDecl *) {
    return true;
  }
};

class ClassTemplatePartialSpecializationDecl 
  : public ClassTemplateSpecializationDecl 
{
  /// \brief The list of template parameters 
  TemplateParameterList* TemplateParams;

  ClassTemplatePartialSpecializationDecl(ASTContext &Context,
                                         DeclContext *DC, SourceLocation L,
                                         TemplateParameterList *Params,
                                         ClassTemplateDecl *SpecializedTemplate,
                                         TemplateArgumentListBuilder &Builder)
    : ClassTemplateSpecializationDecl(Context, ClassTemplatePartialSpecialization,
                                      DC, L, SpecializedTemplate, Builder),
      TemplateParams(Params) { }

public:
  static ClassTemplatePartialSpecializationDecl *
  Create(ASTContext &Context, DeclContext *DC, SourceLocation L,
         TemplateParameterList *Params,
         ClassTemplateDecl *SpecializedTemplate,
         TemplateArgumentListBuilder &Builder,
         ClassTemplatePartialSpecializationDecl *PrevDecl);

  /// Get the list of template parameters
  TemplateParameterList *getTemplateParameters() const {
    return TemplateParams;
  }

  // FIXME: Add Profile support!

  static bool classof(const Decl *D) { 
    return D->getKind() == ClassTemplatePartialSpecialization;
  }

  static bool classof(const ClassTemplatePartialSpecializationDecl *) {
    return true;
  }
};

/// Declaration of a class template.
class ClassTemplateDecl : public TemplateDecl {
protected:
  /// \brief Data that is common to all of the declarations of a given
  /// class template.
  struct Common {
    /// \brief The class template specializations for this class
    /// template, including explicit specializations and instantiations.
    llvm::FoldingSet<ClassTemplateSpecializationDecl> Specializations;

    /// \brief The class template partial specializations for this class
    /// template.
    llvm::FoldingSet<ClassTemplatePartialSpecializationDecl> 
      PartialSpecializations;

    /// \brief The injected-class-name type for this class template.
    QualType InjectedClassNameType;
  };

  /// \brief Previous declaration of this class template.
  ClassTemplateDecl *PreviousDeclaration;

  /// \brief Pointer to the data that is common to all of the
  /// declarations of this class template.
  /// 
  /// The first declaration of a class template (e.g., the declaration
  /// with no "previous declaration") owns this pointer.
  Common *CommonPtr;
  
  ClassTemplateDecl(DeclContext *DC, SourceLocation L, DeclarationName Name,
                    TemplateParameterList *Params, NamedDecl *Decl,
                    ClassTemplateDecl *PrevDecl, Common *CommonPtr)
    : TemplateDecl(ClassTemplate, DC, L, Name, Params, Decl),
      PreviousDeclaration(PrevDecl), CommonPtr(CommonPtr) { }

  ~ClassTemplateDecl();

public:
  /// Get the underlying class declarations of the template.
  CXXRecordDecl *getTemplatedDecl() const {
    return static_cast<CXXRecordDecl *>(TemplatedDecl);
  }

  /// \brief Retrieve the previous declaration of this template.
  ClassTemplateDecl *getPreviousDeclaration() const {
    return PreviousDeclaration;
  }

  /// Create a class template node.
  static ClassTemplateDecl *Create(ASTContext &C, DeclContext *DC,
                                   SourceLocation L,
                                   DeclarationName Name,
                                   TemplateParameterList *Params,
                                   NamedDecl *Decl,
                                   ClassTemplateDecl *PrevDecl);

  /// \brief Retrieve the set of specializations of this class template.
  llvm::FoldingSet<ClassTemplateSpecializationDecl> &getSpecializations() {
    return CommonPtr->Specializations;
  }

  /// \brief Retrieve the set of partial specializations of this class
  /// template.
  llvm::FoldingSet<ClassTemplatePartialSpecializationDecl> &
  getPartialSpecializations() {
    return CommonPtr->PartialSpecializations;
  }

  /// \brief Retrieve the type of the injected-class-name for this
  /// class template.
  ///
  /// The injected-class-name for a class template \c X is \c
  /// X<template-args>, where \c template-args is formed from the
  /// template arguments that correspond to the template parameters of
  /// \c X. For example:
  ///
  /// \code
  /// template<typename T, int N>
  /// struct array {
  ///   typedef array this_type; // "array" is equivalent to "array<T, N>"
  /// };
  /// \endcode
  QualType getInjectedClassNameType(ASTContext &Context);

  // Implement isa/cast/dyncast support
  static bool classof(const Decl *D)
  { return D->getKind() == ClassTemplate; }
  static bool classof(const ClassTemplateDecl *D)
  { return true; }

  virtual void Destroy(ASTContext& C);
};

} /* end of namespace clang */

#endif
