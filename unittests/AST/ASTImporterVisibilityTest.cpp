//===- unittest/AST/ASTImporterGenericRedeclTest.cpp - AST import test ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Type-parameterized tests for the correct import of Decls with different
// visibility.
//
//===----------------------------------------------------------------------===//

// Define this to have ::testing::Combine available.
// FIXME: Better solution for this?
#define GTEST_HAS_COMBINE 1

#include "ASTImporterFixtures.h"

namespace clang {
namespace ast_matchers {

using internal::BindableMatcher;

// Type parameters for type-parameterized test fixtures.
struct GetFunPattern {
  using DeclTy = FunctionDecl;
  BindableMatcher<Decl> operator()() { return functionDecl(hasName("f")); }
};
struct GetVarPattern {
  using DeclTy = VarDecl;
  BindableMatcher<Decl> operator()() { return varDecl(hasName("v")); }
};
struct GetClassPattern {
  using DeclTy = CXXRecordDecl;
  BindableMatcher<Decl> operator()() { return cxxRecordDecl(hasName("X")); }
};
struct GetEnumPattern {
  using DeclTy = EnumDecl;
  BindableMatcher<Decl> operator()() { return enumDecl(hasName("E")); }
};

// Values for the value-parameterized test fixtures.
// FunctionDecl:
auto *ExternF = "void f();";
auto *StaticF = "static void f();";
auto *AnonF = "namespace { void f(); }";
// VarDecl:
auto *ExternV = "extern int v;";
auto *StaticV = "static int v;";
auto *AnonV = "namespace { extern int v; }";
// CXXRecordDecl:
auto *ExternC = "class X;";
auto *AnonC = "namespace { class X; }";
// EnumDecl:
auto *ExternE = "enum E {};";
auto *AnonE = "namespace { enum E {}; }";
auto *ExternEC = "enum class E;";
auto *AnonEC = "namespace { enum class E; }";

// First value in tuple: Compile options.
// Second value in tuple: Source code to be used in the test.
using ImportVisibilityChainParams =
    ::testing::WithParamInterface<std::tuple<ArgVector, const char *>>;
// Fixture to test the redecl chain of Decls with the same visibility.  Gtest
// makes it possible to have either value-parameterized or type-parameterized
// fixtures.  However, we cannot have both value- and type-parameterized test
// fixtures.  This is a value-parameterized test fixture in the gtest sense. We
// intend to mimic gtest's type-parameters via the PatternFactory template
// parameter.  We manually instantiate the different tests with the each types.
template <typename PatternFactory>
class ImportVisibilityChain
    : public ASTImporterTestBase, public ImportVisibilityChainParams {
protected:
  using DeclTy = typename PatternFactory::DeclTy;
  ArgVector getExtraArgs() const override { return std::get<0>(GetParam()); }
  std::string getCode() const { return std::get<1>(GetParam()); }
  BindableMatcher<Decl> getPattern() const { return PatternFactory()(); }

  // Type-parameterized test.
  void TypedTest_ImportChain() {
    std::string Code = getCode() + getCode();
    auto Pattern = getPattern();

    TranslationUnitDecl *FromTu = getTuDecl(Code, Lang_CXX11, "input0.cc");

    auto *FromF0 = FirstDeclMatcher<DeclTy>().match(FromTu, Pattern);
    auto *FromF1 = LastDeclMatcher<DeclTy>().match(FromTu, Pattern);

    auto *ToF0 = Import(FromF0, Lang_CXX11);
    auto *ToF1 = Import(FromF1, Lang_CXX11);

    EXPECT_TRUE(ToF0);
    ASSERT_TRUE(ToF1);
    EXPECT_NE(ToF0, ToF1);
    EXPECT_EQ(ToF1->getPreviousDecl(), ToF0);
  }
};

// Manual instantiation of the fixture with each type.
using ImportFunctionsVisibilityChain = ImportVisibilityChain<GetFunPattern>;
using ImportVariablesVisibilityChain = ImportVisibilityChain<GetVarPattern>;
using ImportClassesVisibilityChain = ImportVisibilityChain<GetClassPattern>;
using ImportScopedEnumsVisibilityChain = ImportVisibilityChain<GetEnumPattern>;
// Value-parameterized test for functions.
TEST_P(ImportFunctionsVisibilityChain, ImportChain) {
  TypedTest_ImportChain();
}
// Value-parameterized test for variables.
TEST_P(ImportVariablesVisibilityChain, ImportChain) {
  TypedTest_ImportChain();
}
// Value-parameterized test for classes.
TEST_P(ImportClassesVisibilityChain, ImportChain) {
  TypedTest_ImportChain();
}
// Value-parameterized test for scoped enums.
TEST_P(ImportScopedEnumsVisibilityChain, ImportChain) {
  TypedTest_ImportChain();
}

// Automatic instantiation of the value-parameterized tests.
INSTANTIATE_TEST_CASE_P(ParameterizedTests, ImportFunctionsVisibilityChain,
                        ::testing::Combine(
                           DefaultTestValuesForRunOptions,
                           ::testing::Values(ExternF, StaticF, AnonF)), );
INSTANTIATE_TEST_CASE_P(
    ParameterizedTests, ImportVariablesVisibilityChain,
    ::testing::Combine(
        DefaultTestValuesForRunOptions,
        // There is no point to instantiate with StaticV, because in C++ we can
        // forward declare a variable only with the 'extern' keyword.
        // Consequently, each fwd declared variable has external linkage.  This
        // is different in the C language where any declaration without an
        // initializer is a tentative definition, subsequent definitions may be
        // provided but they must have the same linkage.  See also the test
        // ImportVariableChainInC which test for this special C Lang case.
        ::testing::Values(ExternV, AnonV)), );
INSTANTIATE_TEST_CASE_P(
    ParameterizedTests, ImportClassesVisibilityChain,
    ::testing::Combine(
        DefaultTestValuesForRunOptions,
        ::testing::Values(ExternC, AnonC)), );
INSTANTIATE_TEST_CASE_P(
    ParameterizedTests, ImportScopedEnumsVisibilityChain,
    ::testing::Combine(
        DefaultTestValuesForRunOptions,
        ::testing::Values(ExternEC, AnonEC)), );

// First value in tuple: Compile options.
// Second value in tuple: Tuple with informations for the test.
// Code for first import (or initial code), code to import, whether the `f`
// functions are expected to be linked in a declaration chain.
// One value of this tuple is combined with every value of compile options.
// The test can have a single tuple as parameter only.
using ImportVisibilityParams = ::testing::WithParamInterface<
    std::tuple<ArgVector, std::tuple<const char *, const char *, bool>>>;

template <typename PatternFactory>
class ImportVisibility
    : public ASTImporterTestBase,
      public ImportVisibilityParams {
protected:
  using DeclTy = typename PatternFactory::DeclTy;
  ArgVector getExtraArgs() const override { return std::get<0>(GetParam()); }
  std::string getCode0() const { return std::get<0>(std::get<1>(GetParam())); }
  std::string getCode1() const { return std::get<1>(std::get<1>(GetParam())); }
  bool shouldBeLinked() const { return std::get<2>(std::get<1>(GetParam())); }
  BindableMatcher<Decl> getPattern() const { return PatternFactory()(); }

  void TypedTest_ImportAfter() {
    TranslationUnitDecl *ToTu = getToTuDecl(getCode0(), Lang_CXX11);
    TranslationUnitDecl *FromTu = getTuDecl(getCode1(), Lang_CXX11, "input1.cc");

    auto *ToF0 = FirstDeclMatcher<DeclTy>().match(ToTu, getPattern());
    auto *FromF1 = FirstDeclMatcher<DeclTy>().match(FromTu, getPattern());

    auto *ToF1 = Import(FromF1, Lang_CXX);

    ASSERT_TRUE(ToF0);
    ASSERT_TRUE(ToF1);
    EXPECT_NE(ToF0, ToF1);

    if (shouldBeLinked())
      EXPECT_EQ(ToF1->getPreviousDecl(), ToF0);
    else
      EXPECT_FALSE(ToF1->getPreviousDecl());
  }

  void TypedTest_ImportAfterImport() {
    TranslationUnitDecl *FromTu0 = getTuDecl(getCode0(), Lang_CXX11, "input0.cc");
    TranslationUnitDecl *FromTu1 = getTuDecl(getCode1(), Lang_CXX11, "input1.cc");
    auto *FromF0 =
        FirstDeclMatcher<DeclTy>().match(FromTu0, getPattern());
    auto *FromF1 =
        FirstDeclMatcher<DeclTy>().match(FromTu1, getPattern());
    auto *ToF0 = Import(FromF0, Lang_CXX);
    auto *ToF1 = Import(FromF1, Lang_CXX);
    ASSERT_TRUE(ToF0);
    ASSERT_TRUE(ToF1);
    EXPECT_NE(ToF0, ToF1);
    if (shouldBeLinked())
      EXPECT_EQ(ToF1->getPreviousDecl(), ToF0);
    else
      EXPECT_FALSE(ToF1->getPreviousDecl());
  }

  void TypedTest_ImportAfterWithMerge() {
    TranslationUnitDecl *ToTu = getToTuDecl(getCode0(), Lang_CXX);
    TranslationUnitDecl *FromTu = getTuDecl(getCode1(), Lang_CXX, "input1.cc");

    auto *ToF0 = FirstDeclMatcher<DeclTy>().match(ToTu, getPattern());
    auto *FromF1 = FirstDeclMatcher<DeclTy>().match(FromTu, getPattern());

    auto *ToF1 = Import(FromF1, Lang_CXX);

    ASSERT_TRUE(ToF0);
    ASSERT_TRUE(ToF1);

    if (shouldBeLinked())
      EXPECT_EQ(ToF0, ToF1);
    else
      EXPECT_NE(ToF0, ToF1);

    // We expect no (ODR) warning during the import.
    EXPECT_EQ(0u, ToTu->getASTContext().getDiagnostics().getNumWarnings());
  }

  void TypedTest_ImportAfterImportWithMerge() {
    TranslationUnitDecl *FromTu0 = getTuDecl(getCode0(), Lang_CXX, "input0.cc");
    TranslationUnitDecl *FromTu1 = getTuDecl(getCode1(), Lang_CXX, "input1.cc");
    auto *FromF0 = FirstDeclMatcher<DeclTy>().match(FromTu0, getPattern());
    auto *FromF1 = FirstDeclMatcher<DeclTy>().match(FromTu1, getPattern());
    auto *ToF0 = Import(FromF0, Lang_CXX);
    auto *ToF1 = Import(FromF1, Lang_CXX);
    ASSERT_TRUE(ToF0);
    ASSERT_TRUE(ToF1);
    if (shouldBeLinked())
      EXPECT_EQ(ToF0, ToF1);
    else
      EXPECT_NE(ToF0, ToF1);

    // We expect no (ODR) warning during the import.
    EXPECT_EQ(0u, ToF0->getTranslationUnitDecl()
                      ->getASTContext()
                      .getDiagnostics()
                      .getNumWarnings());
  }
};

using ImportFunctionsVisibility = ImportVisibility<GetFunPattern>;
using ImportVariablesVisibility = ImportVisibility<GetVarPattern>;
using ImportClassesVisibility = ImportVisibility<GetClassPattern>;
using ImportEnumsVisibility = ImportVisibility<GetEnumPattern>;
using ImportScopedEnumsVisibility = ImportVisibility<GetEnumPattern>;

// FunctionDecl.
TEST_P(ImportFunctionsVisibility, ImportAfter) {
  TypedTest_ImportAfter();
}
TEST_P(ImportFunctionsVisibility, ImportAfterImport) {
  TypedTest_ImportAfterImport();
}
// VarDecl.
TEST_P(ImportVariablesVisibility, ImportAfter) {
  TypedTest_ImportAfter();
}
TEST_P(ImportVariablesVisibility, ImportAfterImport) {
  TypedTest_ImportAfterImport();
}
// CXXRecordDecl.
TEST_P(ImportClassesVisibility, ImportAfter) {
  TypedTest_ImportAfter();
}
TEST_P(ImportClassesVisibility, ImportAfterImport) {
  TypedTest_ImportAfterImport();
}
// EnumDecl.
TEST_P(ImportEnumsVisibility, ImportAfter) {
  TypedTest_ImportAfterWithMerge();
}
TEST_P(ImportEnumsVisibility, ImportAfterImport) {
  TypedTest_ImportAfterImportWithMerge();
}
TEST_P(ImportScopedEnumsVisibility, ImportAfter) {
  TypedTest_ImportAfter();
}
TEST_P(ImportScopedEnumsVisibility, ImportAfterImport) {
  TypedTest_ImportAfterImport();
}

bool ExpectLink = true;
bool ExpectNotLink = false;

INSTANTIATE_TEST_CASE_P(
    ParameterizedTests, ImportFunctionsVisibility,
    ::testing::Combine(
        DefaultTestValuesForRunOptions,
        ::testing::Values(std::make_tuple(ExternF, ExternF, ExpectLink),
                          std::make_tuple(ExternF, StaticF, ExpectNotLink),
                          std::make_tuple(ExternF, AnonF, ExpectNotLink),
                          std::make_tuple(StaticF, ExternF, ExpectNotLink),
                          std::make_tuple(StaticF, StaticF, ExpectNotLink),
                          std::make_tuple(StaticF, AnonF, ExpectNotLink),
                          std::make_tuple(AnonF, ExternF, ExpectNotLink),
                          std::make_tuple(AnonF, StaticF, ExpectNotLink),
                          std::make_tuple(AnonF, AnonF, ExpectNotLink))), );
INSTANTIATE_TEST_CASE_P(
    ParameterizedTests, ImportVariablesVisibility,
    ::testing::Combine(
        DefaultTestValuesForRunOptions,
        ::testing::Values(std::make_tuple(ExternV, ExternV, ExpectLink),
                          std::make_tuple(ExternV, StaticV, ExpectNotLink),
                          std::make_tuple(ExternV, AnonV, ExpectNotLink),
                          std::make_tuple(StaticV, ExternV, ExpectNotLink),
                          std::make_tuple(StaticV, StaticV, ExpectNotLink),
                          std::make_tuple(StaticV, AnonV, ExpectNotLink),
                          std::make_tuple(AnonV, ExternV, ExpectNotLink),
                          std::make_tuple(AnonV, StaticV, ExpectNotLink),
                          std::make_tuple(AnonV, AnonV, ExpectNotLink))), );
INSTANTIATE_TEST_CASE_P(
    ParameterizedTests, ImportClassesVisibility,
    ::testing::Combine(
        DefaultTestValuesForRunOptions,
        ::testing::Values(std::make_tuple(ExternC, ExternC, ExpectLink),
                          std::make_tuple(ExternC, AnonC, ExpectNotLink),
                          std::make_tuple(AnonC, ExternC, ExpectNotLink),
                          std::make_tuple(AnonC, AnonC, ExpectNotLink))), );
INSTANTIATE_TEST_CASE_P(
    ParameterizedTests, ImportEnumsVisibility,
    ::testing::Combine(
        DefaultTestValuesForRunOptions,
        ::testing::Values(std::make_tuple(ExternE, ExternE, ExpectLink),
                          std::make_tuple(ExternE, AnonE, ExpectNotLink),
                          std::make_tuple(AnonE, ExternE, ExpectNotLink),
                          std::make_tuple(AnonE, AnonE, ExpectNotLink))), );
INSTANTIATE_TEST_CASE_P(
    ParameterizedTests, ImportScopedEnumsVisibility,
    ::testing::Combine(
        DefaultTestValuesForRunOptions,
        ::testing::Values(std::make_tuple(ExternEC, ExternEC, ExpectLink),
                          std::make_tuple(ExternEC, AnonEC, ExpectNotLink),
                          std::make_tuple(AnonEC, ExternEC, ExpectNotLink),
                          std::make_tuple(AnonEC, AnonEC, ExpectNotLink))), );

} // end namespace ast_matchers
} // end namespace clang
