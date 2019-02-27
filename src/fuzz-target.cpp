/*
Copyright (c) 2015-2017, Intel Corporation

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

//////////////////////////////////////////////////////////////////////////////

#include <ostream>
#include <sstream>

#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Option/Option.h"
#include "llvm/InitializePasses.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/TargetSelect.h"

#include "cxx_proto.pb.h"

#include "libfuzzer_macro.h"

#include "program.h"

using namespace yarpgen;

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv) {
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmPrinters();
    llvm::InitializeAllAsmParsers();

    llvm::PassRegistry &Registry = *llvm::PassRegistry::getPassRegistry();
    llvm::initializeCore(Registry);
    llvm::initializeScalarOpts(Registry);
    llvm::initializeVectorization(Registry);
    llvm::initializeIPO(Registry);
    llvm::initializeAnalysis(Registry);
    llvm::initializeTransformUtils(Registry);
    llvm::initializeInstCombine(Registry);
    llvm::initializeAggressiveInstCombine(Registry);
    llvm::initializeInstrumentation(Registry);
    llvm::initializeTarget(Registry);

    return 0;
}

void HandleCXX(const std::string &S,
               const std::vector<const char *> &ExtraArgs) {
    llvm::opt::ArgStringList CC1Args;
    CC1Args.push_back("-cc1");
    for (auto &A : ExtraArgs)
        CC1Args.push_back(A);
    CC1Args.push_back("./test.cc");

    llvm::IntrusiveRefCntPtr<clang::FileManager> Files(
            new clang::FileManager(clang::FileSystemOptions()));
    clang::IgnoringDiagConsumer Diags;
    clang::IntrusiveRefCntPtr<clang::DiagnosticOptions> DiagOpts = new clang::DiagnosticOptions();
    clang::DiagnosticsEngine Diagnostics(
            clang::IntrusiveRefCntPtr<clang::DiagnosticIDs>(new clang::DiagnosticIDs()), &*DiagOpts,
            &Diags, false);
    std::unique_ptr<clang::CompilerInvocation> Invocation(
            clang::tooling::newInvocation(&Diagnostics, CC1Args));
    std::unique_ptr<llvm::MemoryBuffer> Input =
            llvm::MemoryBuffer::getMemBuffer(S);
    Invocation->getPreprocessorOpts().addRemappedFile("./test.cc",
                                                      Input.release());
    std::unique_ptr<clang::tooling::ToolAction> action(
            clang::tooling::newFrontendActionFactory<clang::EmitObjAction>());
    std::shared_ptr<clang::PCHContainerOperations> PCHContainerOps =
            std::make_shared<clang::PCHContainerOperations>();
    action->runInvocation(std::move(Invocation), Files.get(), PCHContainerOps,
                          &Diags);
}

DEFINE_BINARY_PROTO_FUZZER(const ProgSeed& input) {
    options = new Options;

    std::cout << input.DebugString() << std::endl;

    RandValGen::init(input.base_seed());
    rand_val_gen = std::make_shared<RandValGen>(RandValGen(0));
    default_gen_policy.init_from_config();
    Program mas("/dev/null");
    mas.generate((yarpgen::ProgSeed *) &input);
    std::string S = mas.emit_main();

    HandleCXX(S, std::vector<const char *>{});
    delete (options);
}
