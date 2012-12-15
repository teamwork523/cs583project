//=============- idenRegion.cpp - Final Project for EECS 583 ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implement a static analysis on Idenpotent Region process
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "idenRegion"
#include <sstream>
#include <string>
#include <iomanip>
#include "llvm/InstrTypes.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/BasicBlock.h"
#include "llvm/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/PredIteratorCache.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/LoopInfo.h"
#include "LAMP/LAMPLoadProfile.h"

using namespace llvm;


//===----------------------------------------------------------------------===//
// idenRegion
//===----------------------------------------------------------------------===//
namespace {
    // statistic computation on operation counts
    struct idenRegion : public FunctionPass {
        static char ID; // Pass identification, replacement for typeid

        AliasAnalysis *AA;       // Current AliasAnalysis information
        LoopInfo      *LI;       // Current LoopInfo
        DominatorTree *DT;       // Dominator Tree for the current Loop.
        PredIteratorCache PredCache_;   // Cache fetch predecessor of a BB
          
        typedef std::pair<Instruction *, Instruction *> AntiDepPairTy;
        typedef SmallVector<Instruction *, 16> AntiDepPathTy;  

        // Intermediary data structure 1.
        typedef SmallVector<AntiDepPairTy, 16> AntiDepPairs;
        AntiDepPairs AntiDepPairs_;

        // Intermediary data structure 2.
        typedef SmallVector<AntiDepPathTy, 16> AntiDepPaths;
        AntiDepPaths AntiDepPaths_;
        
        // pass constructor
        idenRegion() : FunctionPass(ID) {}

        // get the profile information
        void getAnalysisUsage(AnalysisUsage &AU) const {
            AU.addRequired<DominatorTree>();
            AU.addRequired<LoopInfo>();
            AU.addRequired<AliasAnalysis>();
        }
        
        // Find all necessary information about Function
        virtual bool runOnFunction(Function &F);          
        
        //===----------------------------------------------------------------------===//
        // Helpers
        //===----------------------------------------------------------------------===//
        void findAntidependencePairs(StoreInst *Store);
        bool scanForAliasingLoad(BasicBlock::iterator I,
                                 BasicBlock::iterator E,
                                 StoreInst *Store,
                                 Value *StoreDst,
                                 unsigned StoreDstSize);
        void computeAntidependencePaths();
        
        // print instruction and its BB location
        std::string getLocator(const Instruction &I) {
            unsigned Offset = 1;
            const BasicBlock *BB = I.getParent();
            for (BasicBlock::const_iterator It = I; It != BB->begin(); --It)
                ++Offset;

            std::stringstream SS;
            SS << BB->getName().str() << ":" << Offset;
            return SS.str();
        }

        // fetch anti-dep pair
        void printPair(const AntiDepPairTy &P) {
            errs() << "Antidependence Pair ( " << getLocator(*P.first) << ", " 
            << getLocator(*P.second) << " )";
        }
        
        // fetch anti-dep path
        void printPath(const AntiDepPathTy &P) {
            errs() << "[ ";
            for (AntiDepPathTy::const_iterator I = P.begin(), First = I,
                 E = P.end(); I != E; ++I) {
                if (I != First)
                    errs() << ", ";
                errs() << getLocator(**I);
            }
            errs() << " ]";
        }
    };
}

char idenRegion::ID = 0;
static RegisterPass<idenRegion> X("idenRegion", "EECS 583 project", false, false);

bool idenRegion::runOnFunction(Function &F) {
    // Get our Loop and Alias Analysis information...
    LI = &getAnalysis<LoopInfo>();
    AA = &getAnalysis<AliasAnalysis>();
    DT = &getAnalysis<DominatorTree>();
    errs() << "---------------------------------------------\n";
    errs() << "----------Find Anti-dependency region--------\n";
    errs() << "---------------------------------------------\n";

    errs() << "----------Compute Memory Antidependency Pairs---------\n";
    for (Function::iterator BB = F.begin(); BB != F.end(); ++BB) {
        errs() << "##### BB #####" << "\n";
        for (BasicBlock::iterator I = BB->begin(); I != BB->end(); ++I) {
            if (StoreInst *Store = dyn_cast<StoreInst>(I)) {
                findAntidependencePairs(Store);
            }
        }
    }
    
    if (AntiDepPairs_.empty())
        return false;

    errs() << "----------Find anti-dependency Path------------\n";
    computeAntidependencePaths();

    return false;
}

void idenRegion::findAntidependencePairs(StoreInst *Store) {
    // errs() << "** Analyzing Store: " << *Store << "\n";
    // errs() << "** At location:     " << getLocator(*Store) << "\n";
    Value *StoreDst = Store->getOperand(1); // dst
    // Value *First = Store->getOperand(0); // src
    // errs() << "Store src type is " << *(First->getType()) << "\n";
    unsigned StoreDstSize = AA->getTypeStoreSize(Store->getOperand(0)->getType());

    // Perform a reverse depth-first search to find aliasing loads.
    typedef std::pair<BasicBlock *, BasicBlock::iterator> WorkItem;
    SmallVector<WorkItem, 8> Worklist;
    SmallPtrSet<BasicBlock *, 32> Visited;

    BasicBlock *StoreBB = Store->getParent();
    Worklist.push_back(WorkItem(StoreBB, Store));

    do {
        BasicBlock *BB;
        BasicBlock::iterator I, E;
        tie(BB, I) = Worklist.pop_back_val();

        // If we are revisiting StoreBB, we scan to Store to complete the cycle.
        // Otherwise we end at BB->begin().
        E = (BB == StoreBB && I == BB->end()) ? Store : BB->begin();
        
        // Scan for an aliasing load.  Terminate this path if we see one or a cut is
        // already forced.
        if (scanForAliasingLoad(I, E, Store, StoreDst, StoreDstSize))
            continue;
            
        // If the path didn't terminate, continue on to predecessors.
        // errs() << "###### Predecessor Info #######" << "\n";
        for (BasicBlock **P = PredCache_.GetPreds(StoreBB); *P; ++P) {
            //errs() << "## Name is " << (*P)->getName() << "\n";
            if (Visited.insert(*P))
                Worklist.push_back(WorkItem((*P), (*P)->end()));
        }
    } while (!Worklist.empty());
}

bool idenRegion::scanForAliasingLoad (BasicBlock::iterator I,
                                      BasicBlock::iterator E,
                                      StoreInst *Store,
                                      Value *StoreDst,
                                      unsigned StoreDstSize) {
    // I is the end of the instruction, E is the begining of the instruction
    while (I != E) {
        --I;
        if (LoadInst *Load = dyn_cast<LoadInst>(I)) {
            // Load all the may alias case
            if (AA->getModRefInfo(Load, StoreDst, StoreDstSize) & AliasAnalysis::Ref) {
                errs() << "!!!!Detect AntiDep Pair!!!!\n";
                AntiDepPairTy Pair = AntiDepPairTy(I, Store);
                errs() << "~~~ First:  " << *(Pair.first) << "\n";
                errs() << "~~~ At location " << getLocator(*(Pair.first)) << "\n";
                errs() << "~~~ Second: " << *(Pair.second) << "\n";
                errs() << "~~~ At location " << getLocator(*(Pair.second)) << "\n";
                AntiDepPairs_.push_back(Pair);
                return true;
            }
        }
    }
    return false;
}

void idenRegion::computeAntidependencePaths() {
    // Iterate through every pair of antidependency
    // Record all the store along the path
    for (AntiDepPairs::iterator I = AntiDepPairs_.begin(), E = AntiDepPairs_.end(); I != E; I++) {
        BasicBlock::iterator Load, Store;
        tie(Load, Store) = *I;

        // create a new path
        AntiDepPaths_.resize(AntiDepPaths_.size()+1);
        AntiDepPathTy &newPath = AntiDepPaths_.back();

        // Always record current store
        newPath.push_back(Store);

        // Load and store in the same basic block
        BasicBlock::iterator curInst = Store;
        BasicBlock *LoadBB = Load->getParent(), *StoreBB = Store->getParent();
        if (LoadBB == StoreBB && DT->dominates(Load, Store)) {
            while(--curInst != Load) {
                if (isa<StoreInst>(curInst))
                    newPath.push_back(curInst);
            }
            errs() << "@@@ Local BB: \n@@@ ";
            printPath(newPath);
            errs() << "\n";
            continue;
        }

        // Load and store in different basic block
        BasicBlock *curBB = StoreBB;
        DomTreeNode *curDTNode = DT->getNode(StoreBB), *LoadDTNode = DT->getNode(LoadBB);
        // loop until load is not 
        while (DT->dominates(LoadDTNode, curDTNode)) {
            errs() << "^^^^^ Current BB is " << curBB->getName() << "\n";
            BasicBlock::iterator E;
            // check if Load and current node in the same BB
            if (curBB == LoadBB) {
                E = Load;
            } else {
                E = curBB->begin();
            }
            // scan current BB
            while(curInst != E) {
                if (isa<StoreInst>(--curInst)) {
                    newPath.push_back(curInst);
                }
            }
            // find current Node's iDOM
            curDTNode = curDTNode->getIDom();
            if (curDTNode == NULL)
                break;
            curBB = curDTNode->getBlock();
            curInst = curBB->end();
        }
        errs() << "@@@ Inter BB: \n@@@ ";
        printPath(newPath);
        errs() << "\n";
    }
    
    errs() << "Path cap is " << AntiDepPaths_.capacity() << "\n";
    errs() << "Path size is " << AntiDepPaths_.size() << "\n";
}






