#include <unordered_map>
#include <unordered_set>

#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopIterator.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"

using namespace llvm;

namespace {
struct CIMAPass : public PassInfoMixin<CIMAPass> {
    PreservedAnalyses run(Function& F, FunctionAnalysisManager& FAM) {
        llvm::BlockFrequencyAnalysis::Result& bfi =
            FAM.getResult<BlockFrequencyAnalysis>(F);
        llvm::BranchProbabilityAnalysis::Result& bpi =
            FAM.getResult<BranchProbabilityAnalysis>(F);
        llvm::LoopAnalysis::Result& li = FAM.getResult<LoopAnalysis>(F);
        llvm::DominatorTreeAnalysis::Result& dt = FAM.getResult<DominatorTreeAnalysis>(F);

        std::vector<CallInst*> AsanCalls;

        // Scan for __asan_report_* calls
        for (auto& BB : F) {
            for (auto& I : BB) {
                if (CallInst* CI = dyn_cast<CallInst>(&I)) {
                    if (Function* CalledF = CI->getCalledFunction()) {
                        if (CalledF->getName().starts_with("__asan_report")) {
                            AsanCalls.push_back(CI);
                        }
                    }
                }
            }
        }

        std::unordered_map<Instruction*, BasicBlock*> MemInstToTargetBB;

        // Process each ASan crash report
        for (CallInst* CI : AsanCalls) {
            BasicBlock* CrashBB = CI->getParent();
            std::vector<BasicBlock*> Preds(pred_begin(CrashBB), pred_end(CrashBB));

            for (BasicBlock* CheckBB : Preds) {
                auto* BI = dyn_cast<BranchInst>(CheckBB->getTerminator());

                if (!BI || !BI->isConditional()) continue;
                BasicBlock* SafeBB = nullptr;
                unsigned CrashSuccIdx = 0;

                if (BI->getSuccessor(0) == CrashBB) {
                    SafeBB = BI->getSuccessor(1);
                    CrashSuccIdx = 0;
                } else if (BI->getSuccessor(1) == CrashBB) {
                    SafeBB = BI->getSuccessor(0);
                    CrashSuccIdx = 1;
                } else {
                    continue;
                }

                Instruction* MemInst = nullptr;
                for (auto& I : *SafeBB) {
                    if (isa<LoadInst>(&I) || isa<StoreInst>(&I) ||
                        isa<AtomicRMWInst>(&I) || isa<AtomicCmpXchgInst>(&I)) {
                        MemInst = &I;
                        break;
                    }
                    if (auto* MCI = dyn_cast<MemIntrinsic>(&I)) {
                        MemInst = &I;
                        break;
                    }
                }

                if (!MemInst) continue;

                BasicBlock* TargetBB = nullptr;

                if (MemInstToTargetBB.count(MemInst)) {
                    TargetBB = MemInstToTargetBB[MemInst];
                } else {
                    if (MemInst->isTerminator()) continue;

                    TargetBB = SplitBlock(SafeBB, MemInst->getNextNode(), &dt, &li);
                    MemInstToTargetBB[MemInst] = TargetBB;

                    if (!MemInst->getType()->isVoidTy()) {
                        PHINode* Phi = PHINode::Create(MemInst->getType(), 0,
                                                       "cima.skipped", TargetBB->begin());

                        Phi->addIncoming(MemInst, SafeBB);
                        MemInst->replaceUsesWithIf(Phi, [&](Use& U) {
                            Instruction* User = cast<Instruction>(U.getUser());
                            return User != Phi;
                        });
                    }
                }

                BI->setSuccessor(CrashSuccIdx, TargetBB);

                if (TargetBB && !MemInst->getType()->isVoidTy()) {
                    if (PHINode* Phi = dyn_cast<PHINode>(&TargetBB->front())) {
                        Phi->addIncoming(UndefValue::get(MemInst->getType()), CheckBB);
                    }
                }
            }
        }

        errs() << "CIMA: Instrumented function " << F.getName() << "\n";

        return PreservedAnalyses::none();
    }
};
}  // namespace
extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK llvmGetPassPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "CIMAPass", "v0.1", [](PassBuilder& PB) {
                PB.registerPipelineParsingCallback(
                    [](StringRef Name, FunctionPassManager& FPM,
                       ArrayRef<PassBuilder::PipelineElement>) {
                        if (Name == "CIMAPass") {
                            FPM.addPass(CIMAPass());
                            return true;
                        }
                        return false;
                    });

                PB.registerOptimizerLastEPCallback([](llvm::ModulePassManager& MPM,
                                                      llvm::OptimizationLevel Level,
                                                      llvm::ThinOrFullLTOPhase Phase) {
                    MPM.addPass(createModuleToFunctionPassAdaptor(CIMAPass()));
                });
            }};
}
