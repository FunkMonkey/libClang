//===- CIndexCXX.cpp - Clang-C Source Indexing Library --------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the libclang support for C++ cursors.
//
//===----------------------------------------------------------------------===//

#include "CIndexer.h"
#include "CXCursor.h"
#include "CXType.h"
#include "CXString.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"

using namespace clang;
using namespace clang::cxcursor;

extern "C" {

unsigned clang_isVirtualBase(CXCursor C) {
  if (C.kind != CXCursor_CXXBaseSpecifier)
    return 0;
  
  CXXBaseSpecifier *B = getCursorCXXBaseSpecifier(C);
  return B->isVirtual();
}

enum CX_CXXAccessSpecifier clang_getCXXAccessSpecifier(CXCursor C) {
  AccessSpecifier spec = AS_none;

  if (C.kind == CXCursor_CXXAccessSpecifier)
    spec = getCursorDecl(C)->getAccess();
  else if (C.kind == CXCursor_CXXBaseSpecifier)
    spec = getCursorCXXBaseSpecifier(C)->getAccessSpecifier();
  else
    return CX_CXXInvalidAccessSpecifier;
  
  switch (spec) {
    case AS_public: return CX_CXXPublic;
    case AS_protected: return CX_CXXProtected;
    case AS_private: return CX_CXXPrivate;
    case AS_none: return CX_CXXInvalidAccessSpecifier;
  }

  llvm_unreachable("Invalid AccessSpecifier!");
}

enum CXCursorKind clang_getTemplateCursorKind(CXCursor C) {
  using namespace clang::cxcursor;
  
  switch (C.kind) {
  case CXCursor_ClassTemplate: 
  case CXCursor_FunctionTemplate:
    if (TemplateDecl *Template
                           = dyn_cast_or_null<TemplateDecl>(getCursorDecl(C)))
      return MakeCXCursor(Template->getTemplatedDecl(), 
                          static_cast<CXTranslationUnit>(C.data[2])).kind;
    break;
      
  case CXCursor_ClassTemplatePartialSpecialization:
    if (ClassTemplateSpecializationDecl *PartialSpec
          = dyn_cast_or_null<ClassTemplatePartialSpecializationDecl>(
                                                            getCursorDecl(C))) {
      switch (PartialSpec->getTagKind()) {
      case TTK_Class: return CXCursor_ClassDecl;
      case TTK_Struct: return CXCursor_StructDecl;
      case TTK_Union: return CXCursor_UnionDecl;
      case TTK_Enum: return CXCursor_NoDeclFound;
      }
    }
    break;
      
  default:
    break;
  }
  
  return CXCursor_NoDeclFound;
}

CXCursor clang_getSpecializedCursorTemplate(CXCursor C) {
  if (!clang_isDeclaration(C.kind))
    return clang_getNullCursor();
    
  Decl *D = getCursorDecl(C);
  if (!D)
    return clang_getNullCursor();
  
  Decl *Template = 0;
  if (CXXRecordDecl *CXXRecord = dyn_cast<CXXRecordDecl>(D)) {
    if (ClassTemplatePartialSpecializationDecl *PartialSpec
          = dyn_cast<ClassTemplatePartialSpecializationDecl>(CXXRecord))
      Template = PartialSpec->getSpecializedTemplate();
    else if (ClassTemplateSpecializationDecl *ClassSpec 
               = dyn_cast<ClassTemplateSpecializationDecl>(CXXRecord)) {
      llvm::PointerUnion<ClassTemplateDecl *,
                         ClassTemplatePartialSpecializationDecl *> Result
        = ClassSpec->getSpecializedTemplateOrPartial();
      if (Result.is<ClassTemplateDecl *>())
        Template = Result.get<ClassTemplateDecl *>();
      else
        Template = Result.get<ClassTemplatePartialSpecializationDecl *>();
      
    } else 
      Template = CXXRecord->getInstantiatedFromMemberClass();
  } else if (FunctionDecl *Function = dyn_cast<FunctionDecl>(D)) {
    Template = Function->getPrimaryTemplate();
    if (!Template)
      Template = Function->getInstantiatedFromMemberFunction();
  } else if (VarDecl *Var = dyn_cast<VarDecl>(D)) {
    if (Var->isStaticDataMember())
      Template = Var->getInstantiatedFromStaticDataMember();
  } else if (RedeclarableTemplateDecl *Tmpl
                                        = dyn_cast<RedeclarableTemplateDecl>(D))
    Template = Tmpl->getInstantiatedFromMemberTemplate();
  
  if (!Template)
    return clang_getNullCursor();
  
  return MakeCXCursor(Template, static_cast<CXTranslationUnit>(C.data[2]));
}

// =========================================================================================================================================

unsigned clang_getTemplateSpecializationNumArguments(CXCursor C)
{
  if (!clang_isDeclaration(C.kind))
    return UINT_MAX;
    
  Decl *D = getCursorDecl(C);
  if (!D)
    return UINT_MAX;

  const TemplateArgumentList* TemplateArgList = 0;

  if (ClassTemplateSpecializationDecl *ClassSpec 
               = dyn_cast<ClassTemplateSpecializationDecl>(D)) {
    
    TemplateArgList =  &(ClassSpec->getTemplateArgs());

  } else if (FunctionDecl *Function = dyn_cast<FunctionDecl>(D)) {
	FunctionDecl::TemplatedKind TemplKind = Function->getTemplatedKind();
	switch(TemplKind){
	  case FunctionDecl::TK_MemberSpecialization: 
	  case FunctionDecl::TK_DependentFunctionTemplateSpecialization: 
	  case FunctionDecl::TK_FunctionTemplateSpecialization: 
		TemplateArgList = Function->getTemplateSpecializationArgs();
		break;
	  default: break;
	}
  }

  return (TemplateArgList == 0) ? UINT_MAX : TemplateArgList->size();
}

CXCursor clang_getTemplateSpecializationArgument(CXCursor C, unsigned Index)
{
  if (!clang_isDeclaration(C.kind))
    return clang_getNullCursor();
    
  Decl *D = getCursorDecl(C);
  if (!D)
    return clang_getNullCursor();

  const TemplateArgumentList* TemplateArgList = 0;
  if (ClassTemplateSpecializationDecl *ClassSpec 
               = dyn_cast<ClassTemplateSpecializationDecl>(D)) {
    
    TemplateArgList = &(ClassSpec->getTemplateArgs());

  } else if (FunctionDecl *Function = dyn_cast<FunctionDecl>(D)) {
	  
	FunctionDecl::TemplatedKind TemplKind = Function->getTemplatedKind();
	switch(TemplKind){
	  case FunctionDecl::TK_MemberSpecialization: 
	  case FunctionDecl::TK_DependentFunctionTemplateSpecialization: 
	  case FunctionDecl::TK_FunctionTemplateSpecialization: 
		TemplateArgList = Function->getTemplateSpecializationArgs();
		break;
	  default: break;
	}
	
  }

  if(!TemplateArgList)
	  return clang_getNullCursor();

  assert(Index < TemplateArgList->size() && "getTemplateSpecializationArgument(): Index out of bounds");

  return MakeCursorTemplateArgument(&((*TemplateArgList)[Index]), D, static_cast<CXTranslationUnit>(C.data[2]));
}

unsigned clang_isTemplateArgument(CXCursor C)
{
	return (C.kind >= CXCursor_TemplateNullArgument && C.kind <= CXCursor_TemplatePackArgument) ? 1 : 0;
}

const TemplateArgument* getTemplateArgumentFromCursor(CXCursor C)
{
  if(clang_isTemplateArgument(C) == 0)
    return 0;

  return static_cast<const TemplateArgument*>(C.data[1]);
}

CXType clang_getTemplateArgumentValueAsType(CXCursor C)
{
  CXTranslationUnit TU = cxcursor::getCursorTU(C);
  const TemplateArgument* TemplateArg = getTemplateArgumentFromCursor(C);

  if(!TemplateArg || TemplateArg->getKind() != TemplateArgument::Type)
    return cxtype::MakeCXType(QualType(), TU);

  return cxtype::MakeCXType(TemplateArg->getAsType(), TU);
}

long long clang_getTemplateArgumentValueAsIntegral(CXCursor C)
{
  const TemplateArgument* TemplateArg = getTemplateArgumentFromCursor(C);

  if(!TemplateArg || TemplateArg->getKind() != TemplateArgument::Integral)
    return LLONG_MIN; 

  return TemplateArg->getAsIntegral()->getSExtValue();
}

CXCursor clang_getTemplateArgumentValueAsDeclaration(CXCursor C)
{
  const TemplateArgument* TemplateArg = getTemplateArgumentFromCursor(C);

  if(!TemplateArg || TemplateArg->getKind() != TemplateArgument::Declaration)
    return clang_getNullCursor(); 

  Decl* D = TemplateArg->getAsDecl();
  if(!D)
	return clang_getNullCursor(); 

  CXTranslationUnit TU = cxcursor::getCursorTU(C);

  return MakeCXCursor(D, TU);
}

CXCursor clang_getTemplateArgumentValueAsTemplate(CXCursor C)
{
  const TemplateArgument* TemplateArg = getTemplateArgumentFromCursor(C);

  if(!TemplateArg || TemplateArg->getKind() != TemplateArgument::Template)
    return clang_getNullCursor(); 

  TemplateDecl* D = TemplateArg->getAsTemplate().getAsTemplateDecl();
  if(!D)
	return clang_getNullCursor();

  CXTranslationUnit TU = cxcursor::getCursorTU(C);

  return MakeCXCursor(D, TU);
}

CXCursor clang_getTemplateArgumentValueAsExpression(CXCursor C)
{
  const TemplateArgument* TemplateArg = getTemplateArgumentFromCursor(C);

  if(!TemplateArg || TemplateArg->getKind() != TemplateArgument::Expression)
    return clang_getNullCursor(); 

  Expr* E = TemplateArg->getAsExpr();
  if(!E)
	return clang_getNullCursor();

  CXTranslationUnit TU = cxcursor::getCursorTU(C);

  // FIXME: Currently passes 0 as the parent - how do we get the real parent?
  return MakeCXCursor(E, 0, TU);
}

// =========================================================================================================================================

unsigned clang_getTemplateNumParameters(CXCursor C)
{
  if (!clang_isDeclaration(C.kind))
    return UINT_MAX;
    
  Decl *D = getCursorDecl(C);
  if (!D)
    return UINT_MAX;

  TemplateParameterList* TemplateParamList = 0;
  if (RedeclarableTemplateDecl *TempDecl
               = dyn_cast<RedeclarableTemplateDecl>(D)) {
    
	TemplateParamList =  TempDecl->getTemplateParameters();

  } else if (ClassTemplatePartialSpecializationDecl *ClassPartialSpec
               = dyn_cast<ClassTemplatePartialSpecializationDecl>(D)) {

	TemplateParamList =  ClassPartialSpec->getTemplateParameters();
  }

  return (TemplateParamList == 0) ? UINT_MAX : TemplateParamList->size();
}

CXCursor clang_getTemplateParameter(CXCursor C, unsigned Index)
{
  if (!clang_isDeclaration(C.kind))
    return clang_getNullCursor();
    
  Decl *D = getCursorDecl(C);
  if (!D)
    return clang_getNullCursor();

  TemplateParameterList* TemplateParamList = 0;
  if (RedeclarableTemplateDecl *TempDecl
               = dyn_cast<RedeclarableTemplateDecl>(D)) {
    
	TemplateParamList =  TempDecl->getTemplateParameters();

  } else if (ClassTemplatePartialSpecializationDecl *ClassPartialSpec
               = dyn_cast<ClassTemplatePartialSpecializationDecl>(D)) {

	TemplateParamList =  ClassPartialSpec->getTemplateParameters();
  }

  if(!TemplateParamList)
	  return clang_getNullCursor();

  assert(Index < TemplateParamList->size() && "getTemplateParameter(): Index out of bounds");

  CXTranslationUnit TU = cxcursor::getCursorTU(C);
  return MakeCXCursor(TemplateParamList->getParam(Index), TU);
}

// =========================================================================================================================================
  

enum CX_CXXAccessSpecifier clang_getCXXMemberAccessSpecifier(CXCursor C)
{
	AccessSpecifier spec = AS_none;

	switch(C.kind)
	{
	case CXCursor_CXXMethod:
	case CXCursor_ClassDecl:
	case CXCursor_StructDecl:
	case CXCursor_FieldDecl:
	case CXCursor_VarDecl:
	case CXCursor_EnumDecl:
	case CXCursor_EnumConstantDecl:
	case CXCursor_ClassTemplate:
	case CXCursor_FunctionTemplate:
	case CXCursor_ClassTemplatePartialSpecialization:
		spec = getCursorDecl(C)->getAccess();
		break;
	default:
		spec = AS_none;
	}

	switch (spec) {
	case AS_public: return CX_CXXPublic;
	case AS_protected: return CX_CXXProtected;
	case AS_private: return CX_CXXPrivate;
	case AS_none: return CX_CXXInvalidAccessSpecifier;
	}

	llvm_unreachable("Invalid AccessSpecifier!");
};

} // end extern "C"
