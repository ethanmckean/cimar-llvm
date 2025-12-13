#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopIterator.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/Support/raw_ostream.h" 
#include "llvm/Support/CommandLine.h" 

#include <unordered_set>
#include <vector>

using namespace llvm;

// Define the command line flag "-cima-debug"
static cl::opt<bool> CIMADebug("cima-debug", 
                               cl::desc("Enable CIMA Pass debug logging and runtime printing"), 
                               cl::Hidden, cl::init(false));

namespace {

  struct CIMAPass : public PassInfoMixin<CIMAPass> {

    // Global State
    DenseMap<Value*, Value*> ValTaintMap;    
    DenseMap<Value*, Value*> PtrToShadowPtr; 
    DenseMap<BasicBlock*, Value*> BlockExecTaintMap;

    // Runtime Injection
    FunctionCallee PrintfFunc;
    GlobalVariable *PrintfFormatStr = nullptr;
    GlobalVariable *PrintfWriteFmt = nullptr; 

    // Helper for conditional compile-time logging
    void log(const Twine &Msg) {
        if (CIMADebug) {
            errs() << Msg;
        }
    }

    void setupRuntimeLogging(Module &M) {
        LLVMContext &Ctx = M.getContext();
        IRBuilder<> B(Ctx);
        std::vector<Type*> PrintfArgs = { B.getPtrTy() };
        FunctionType *PrintfType = FunctionType::get(B.getInt32Ty(), PrintfArgs, true);
        PrintfFunc = M.getOrInsertFunction("printf", PrintfType);

        const char *GuardFmt = "[Runtime] Taint Guard Check on '%s': %d\n";
        Constant *GuardConst = ConstantDataArray::getString(Ctx, GuardFmt);
        PrintfFormatStr = new GlobalVariable(M, GuardConst->getType(), true,
                                             GlobalValue::PrivateLinkage, GuardConst,
                                             "cima_guard_fmt");

        const char *WriteFmt = "[Runtime] Shadow Write: Writing %d to shadow address.\n";
        Constant *WriteConst = ConstantDataArray::getString(Ctx, WriteFmt);
        PrintfWriteFmt = new GlobalVariable(M, WriteConst->getType(), true,
                                            GlobalValue::PrivateLinkage, WriteConst,
                                            "cima_write_fmt");
    }

    Value* getTaint(Value *V, IRBuilder<> &B) {
        if (ValTaintMap.count(V)) {
            if (Instruction *I = dyn_cast<Instruction>(ValTaintMap[V])) {
                if (I->getParent() == nullptr) return B.getFalse();
            }
            return ValTaintMap[V];
        }
        return B.getFalse();
    }

    Value* getBlockTaint(BasicBlock *BB, Function &F) {
        if (BB == &F.getEntryBlock()) return ConstantInt::getFalse(F.getContext());
        if (BlockExecTaintMap.count(BB)) return BlockExecTaintMap[BB];
        return ConstantInt::getFalse(F.getContext());
    }

    Value* getTerminatorConditionTaint(BasicBlock *BB) {
        Instruction *Term = BB->getTerminator();
        Value *Cond = nullptr;
        if (auto *BI = dyn_cast<BranchInst>(Term)) {
            if (BI->isConditional()) Cond = BI->getCondition();
        } else if (auto *SI = dyn_cast<SwitchInst>(Term)) {
            Cond = SI->getCondition();
        }

        if (Cond && ValTaintMap.count(Cond)) return ValTaintMap[Cond];
        return nullptr;
    }

    // PHASE 1: Allocas
    void createShadowAllocas(Function &F) {
      log("[CIMA] Phase 1: Allocating Shadow Stack\n");
      BasicBlock &EntryBB = F.getEntryBlock();
      IRBuilder<> EntryBuilder(&*EntryBB.begin());
      
      for (auto &I : EntryBB) {
          if (auto *AI = dyn_cast<AllocaInst>(&I)) {
              AllocaInst *ShadowAI = EntryBuilder.CreateAlloca(AI->getAllocatedType(), 
                                                               AI->getArraySize(), 
                                                               AI->getName() + ".shadow");
              ShadowAI->setAlignment(Align(1));
              PtrToShadowPtr[AI] = ShadowAI;
          }
      }
    }

    // PHASE 1.5: GEPs
    void propagateShadowPointers(Function &F) {
      log("[CIMA] Phase 1.5: Propagating Shadow Pointers\n");
      const DataLayout &DL = F.getParent()->getDataLayout();
      Type *IntPtrTy = DL.getIntPtrType(F.getContext());

      for (auto &BB : F) {
          for (auto &I : BB) {
              if (auto *GEP = dyn_cast<GetElementPtrInst>(&I)) {
                  Value *PtrOp = GEP->getPointerOperand();
                  if (PtrToShadowPtr.count(PtrOp)) {
                      IRBuilder<> B(GEP->getNextNode());
                      Value *OrigBaseInt = B.CreatePtrToInt(PtrOp, IntPtrTy);
                      Value *OrigResultInt = B.CreatePtrToInt(GEP, IntPtrTy);
                      Value *Offset = B.CreateSub(OrigResultInt, OrigBaseInt);
                      Value *ShadowBase = PtrToShadowPtr[PtrOp];
                      Value *ShadowPtr = B.CreateGEP(B.getInt8Ty(), ShadowBase, Offset, GEP->getName() + ".shadow");
                      PtrToShadowPtr[GEP] = ShadowPtr;
                  }
              }
              else if (auto *BC = dyn_cast<BitCastInst>(&I)) {
                  if (PtrToShadowPtr.count(BC->getOperand(0))) {
                      PtrToShadowPtr[BC] = PtrToShadowPtr[BC->getOperand(0)];
                  }
              }
          }
      }
    }

    // PHASE 2: Load
    void instrumentLoads(Function &F) {
      log("[CIMA] Phase 2: Instrumenting Loads\n");
      for (auto &BB : F) {
          for (auto &I : BB) {
              if (auto *LI = dyn_cast<LoadInst>(&I)) {
                  Value *Ptr = LI->getPointerOperand();
                  if (PtrToShadowPtr.count(Ptr)) {
                      IRBuilder<> B(LI);
                      Value *ShadowLoad = B.CreateLoad(B.getInt8Ty(), PtrToShadowPtr[Ptr], "load.taint");
                      Value *IsTainted = B.CreateTrunc(ShadowLoad, B.getInt1Ty());
                      ValTaintMap[LI] = IsTainted;
                  }
              }
          }
      }
    }

    // PHASE 3: Recovery
    void injectRecovery(Function &F, DominatorTreeAnalysis::Result &dt, LoopAnalysis::Result &li) {
      log("[CIMA] Phase 3: Injecting Recovery Logic\n");
      std::vector<CallInst*> AsanCalls;
      for (auto &BB : F) {
        for (auto &I : BB) {
          if (CallInst *CI = dyn_cast<CallInst>(&I)) {
            if (Function *C = CI->getCalledFunction()) {
              if (C->getName().starts_with("__asan_report")) AsanCalls.push_back(CI);
            }
          }
        }
      }

      std::unordered_map<Instruction*, BasicBlock*> MemInstToTargetBB;

      for (CallInst *CI : AsanCalls) {
          BasicBlock *CrashBB = CI->getParent();
          BasicBlock *CheckBB = CrashBB->getSinglePredecessor();
          if (!CheckBB) continue;

          auto *BI = dyn_cast<BranchInst>(CheckBB->getTerminator());
          if (!BI || !BI->isConditional()) continue;

          BasicBlock *SafeBB = (BI->getSuccessor(0) == CrashBB) ? BI->getSuccessor(1) : BI->getSuccessor(0);
          unsigned CrashIdx = (BI->getSuccessor(0) == CrashBB) ? 0 : 1;

          Instruction *MemInst = nullptr;
          for (auto &I : *SafeBB) {
               if (isa<LoadInst>(&I) || isa<StoreInst>(&I) || isa<MemIntrinsic>(&I)) {
                   MemInst = &I; break;
               }
          }
          if (!MemInst) continue;

          BasicBlock *TargetBB = nullptr;
          if (MemInstToTargetBB.count(MemInst)) {
              TargetBB = MemInstToTargetBB[MemInst];
          } else {
              TargetBB = SplitBlock(SafeBB, MemInst->getNextNode(), &dt, &li);
              MemInstToTargetBB[MemInst] = TargetBB;

              if (!MemInst->getType()->isVoidTy()) {
                  IRBuilder<> B(&*TargetBB->begin());
                  PHINode *ValPhi = B.CreatePHI(MemInst->getType(), 2, "cima.val");
                  ValPhi->addIncoming(MemInst, SafeBB);
                  ValPhi->addIncoming(UndefValue::get(MemInst->getType()), CheckBB);

                  PHINode *TaintPhi = B.CreatePHI(B.getInt1Ty(), 2, "cima.taint");
                  
                  Value *ExistingTaint = B.getFalse();
                  if (ValTaintMap.count(MemInst)) ExistingTaint = ValTaintMap[MemInst];

                  TaintPhi->addIncoming(ExistingTaint, SafeBB); 
                  TaintPhi->addIncoming(B.getTrue(), CheckBB); 

                  ValTaintMap[ValPhi] = TaintPhi;
                  
                  MemInst->replaceUsesWithIf(ValPhi, [&](Use &U) { return U.getUser() != ValPhi; });
              }
          }
          BI->setSuccessor(CrashIdx, TargetBB);
          
          if (TargetBB) {
              for (PHINode &Phi : TargetBB->phis()) {
                  if (Phi.getBasicBlockIndex(CheckBB) == -1) {
                      if (Phi.getName().starts_with("cima.taint")) 
                          Phi.addIncoming(ConstantInt::getTrue(Phi.getContext()), CheckBB);
                      else 
                          Phi.addIncoming(UndefValue::get(MemInst->getType()), CheckBB);
                  }
              }
          }
      }
    }

    // PHASE 4: SSA PROPAGATION
    void propagateSSA(Function &F) {
        log("[CIMA] Phase 4: Propagating Taint via SSA (Data + Control)\n");
        BlockExecTaintMap.clear();

        for (auto &BB : F) {
            if (&BB == &F.getEntryBlock()) continue;
            IRBuilder<> B(&BB, BB.begin());
            PHINode *ExecPhi = B.CreatePHI(B.getInt1Ty(), pred_size(&BB), "exec.taint");
            BlockExecTaintMap[&BB] = ExecPhi;
        }

        for (auto &BB : F) {
            for (auto &I : BB) {
                if (auto *PN = dyn_cast<PHINode>(&I)) {
                    if (ValTaintMap.count(PN)) continue;
                    if (PN->getName().starts_with("exec.taint")) continue;
                    if (PN->getName().starts_with("cima.taint")) continue;

                    IRBuilder<> B(&I);
                    PHINode *ShadowPhi = B.CreatePHI(B.getInt1Ty(), PN->getNumIncomingValues(), PN->getName() + ".taint");
                    ValTaintMap[PN] = ShadowPhi;
                }
            }
        }

        ReversePostOrderTraversal<Function*> RPOT(&F);
        for (BasicBlock *BB : RPOT) {
            Value *CurrentBlockTaint = getBlockTaint(BB, F);
            std::vector<Instruction*> Worklist;
            for (auto &I : *BB) Worklist.push_back(&I);

            for (Instruction *Inst : Worklist) {
                Instruction &I = *Inst;
                if (isa<PHINode>(&I)) continue;
                if (I.getType()->isVoidTy()) continue;

                IRBuilder<> B(I.getNextNode() ? I.getNextNode() : &I);

                Value *NewTaint = nullptr;
                if (ValTaintMap.count(&I)) NewTaint = ValTaintMap[&I];

                if (isa<BinaryOperator>(I) || isa<CmpInst>(I) || isa<CastInst>(I) ||
                    isa<GetElementPtrInst>(I) || isa<SelectInst>(I)) {
                    for (Value *Op : I.operands()) {
                        Value *OpT = getTaint(Op, B);
                        if (isa<Constant>(OpT) && cast<Constant>(OpT)->isZeroValue()) continue;
                        if (!NewTaint) NewTaint = OpT;
                        else NewTaint = B.CreateOr(NewTaint, OpT, "taint.or");
                    }
                }
                else if (auto *CI = dyn_cast<CallInst>(&I)) {
                    Function *CalledFunc = CI->getCalledFunction();
                    if (CalledFunc) {
                        StringRef FuncName = CalledFunc->getName();
                        if (FuncName.starts_with("llvm.")) {
                            for (Value *Op : CI->args()) {
                                Value *OpT = getTaint(Op, B);
                                if (isa<Constant>(OpT) && cast<Constant>(OpT)->isZeroValue()) continue;
                                if (!NewTaint) NewTaint = OpT;
                                else NewTaint = B.CreateOr(NewTaint, OpT, "taint.or");
                            }
                        }
                    }
                }

                if (NewTaint) {
                    if (NewTaint != CurrentBlockTaint) {
                         NewTaint = B.CreateOr(NewTaint, CurrentBlockTaint, "taint.flow");
                    }
                } else {
                    NewTaint = CurrentBlockTaint;
                }
                ValTaintMap[&I] = NewTaint;
            }
        }

        for (auto &BB : F) {
            PHINode *ExecPhi = dyn_cast_or_null<PHINode>(BlockExecTaintMap[&BB]);
            if (!ExecPhi) continue;

            for (auto *Pred : predecessors(&BB)) {
                Value *PredExecTaint = getBlockTaint(Pred, F);
                IRBuilder<> PredBuilder(Pred->getTerminator());
                Value *CondTaint = getTerminatorConditionTaint(Pred);
                
                Value *EdgeTaint = PredExecTaint;
                if (CondTaint && CondTaint != PredBuilder.getFalse()) {
                     EdgeTaint = PredBuilder.CreateOr(PredExecTaint, CondTaint, "edge.taint");
                }
                ExecPhi->addIncoming(EdgeTaint, Pred);
            }
        }

        for (auto &BB : F) {
            for (auto &I : BB) {
                auto *PN = dyn_cast<PHINode>(&I);
                if (!PN || !ValTaintMap.count(PN)) continue;
                if (PN->getName().starts_with("cima.")) continue;

                PHINode *ShadowPhi = dyn_cast<PHINode>(ValTaintMap[PN]);
                if (!ShadowPhi) continue;

                for (unsigned i = 0; i < PN->getNumIncomingValues(); ++i) {
                    BasicBlock *IncBB = PN->getIncomingBlock(i);
                    if (ShadowPhi->getBasicBlockIndex(IncBB) != -1) continue;

                    IRBuilder<> B(IncBB->getTerminator());
                    Value *DataTaint = getTaint(PN->getIncomingValue(i), B);
                    ShadowPhi->addIncoming(DataTaint, IncBB);
                }
            }
        }
    }

    // PHASE 5: Store
    void instrumentStores(Function &F, DominatorTreeAnalysis::Result &dt, LoopAnalysis::Result &li) {
      log("[CIMA] Phase 5: Instrumenting Stores\n");
      struct StoreInfo { StoreInst *SI; Value *IsTainted; };
      std::vector<StoreInfo> StoresToInstrument;

      for (auto &BB : F) {
          for (auto &I : BB) {
              if (auto *SI = dyn_cast<StoreInst>(&I)) {
                  IRBuilder<> B(SI);
                  Value *ValOp = SI->getValueOperand();
                  Value *PtrOp = SI->getPointerOperand();

                  Value *ValTaint = getTaint(ValOp, B);
                  Value *PtrTaint = getTaint(PtrOp, B);
                  
                  Value *TotalTaint = ValTaint;
                  if (PtrTaint != B.getFalse()) {
                      TotalTaint = B.CreateOr(ValTaint, PtrTaint);
                  }

                  if (PtrToShadowPtr.count(PtrOp)) {
                      Value *ShadowByte = B.CreateZExt(TotalTaint, B.getInt8Ty());
                      B.CreateStore(ShadowByte, PtrToShadowPtr[PtrOp]);
                      if (CIMADebug && TotalTaint != B.getFalse()) {
                          Value *TaintInt = B.CreateZExt(TotalTaint, B.getInt32Ty());
                          Value *Fmt = B.CreateBitCast(PrintfWriteFmt, B.getPtrTy());
                          B.CreateCall(PrintfFunc, {Fmt, TaintInt});
                      }
                  }

                  if (TotalTaint != B.getFalse()) {
                      StoresToInstrument.push_back({SI, TotalTaint});
                  }
              }
          }
      }

      for (auto &Item : StoresToInstrument) {
          StoreInst *SI = Item.SI;
          if (SI->isTerminator()) continue; 
          BasicBlock *OrigBB = SI->getParent();
          BasicBlock *ExecBB = SplitBlock(OrigBB, SI, &dt, &li);
          BasicBlock *ContBB = SplitBlock(ExecBB, SI->getNextNode(), &dt, &li);
          Instruction *Term = OrigBB->getTerminator();

          if (CIMADebug) {
              IRBuilder<> LogBuilder(Term);
              std::string VarName = "unnamed_loc";
              Value *PtrOp = SI->getPointerOperand();

              if (PtrOp->hasName()) {
                  VarName = PtrOp->getName().str();
              } else {
                  Value *ValOp = SI->getValueOperand();
                  if (ValOp->hasName()) {
                     VarName = "ptr_to_" + ValOp->getName().str();
                  }
              }

              Module *M = SI->getModule();
              Constant *NameConst = ConstantDataArray::getString(M->getContext(), VarName);
              GlobalVariable *NameVar = new GlobalVariable(*M, NameConst->getType(), true,
                                              GlobalValue::PrivateLinkage, NameConst,
                                              "cima_debug_name");

              Value *NameStrPtr = LogBuilder.CreateBitCast(NameVar, LogBuilder.getPtrTy());
              Value *TaintAsInt = LogBuilder.CreateZExt(Item.IsTainted, LogBuilder.getInt32Ty());
              Value *FmtStrPtr = LogBuilder.CreateBitCast(PrintfFormatStr, LogBuilder.getPtrTy());
              LogBuilder.CreateCall(PrintfFunc, { FmtStrPtr, NameStrPtr, TaintAsInt });
          }
          BranchInst *NewBr = BranchInst::Create(ContBB, ExecBB, Item.IsTainted);
          ReplaceInstWithInst(Term, NewBr);
      }
    }

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
      if (!PrintfFormatStr && CIMADebug) setupRuntimeLogging(*F.getParent());

      llvm::DominatorTreeAnalysis::Result &dt = FAM.getResult<DominatorTreeAnalysis>(F);
      llvm::LoopAnalysis::Result &li = FAM.getResult<LoopAnalysis>(F);

      ValTaintMap.clear();
      PtrToShadowPtr.clear();

      createShadowAllocas(F);
      propagateShadowPointers(F);
      instrumentLoads(F);
      injectRecovery(F, dt, li);
      propagateSSA(F); 
      instrumentStores(F, dt, li);

      return PreservedAnalyses::none();
    }
  };
}

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "CIMAPassTainted", "v0.1",
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](StringRef Name, FunctionPassManager &FPM, ArrayRef<PassBuilder::PipelineElement>) {
             if (Name == "CIMAPassTainted") { FPM.addPass(CIMAPass()); return true; }
             return false;
        }
      );
      PB.registerOptimizerLastEPCallback(
        [](llvm::ModulePassManager &MPM, llvm::OptimizationLevel Level, llvm::ThinOrFullLTOPhase Phase) {
             MPM.addPass(createModuleToFunctionPassAdaptor(CIMAPass()));
        }
      );
    }
  };
}