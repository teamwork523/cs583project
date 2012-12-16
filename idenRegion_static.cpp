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
#include <set>
#include <map>
#include <vector>
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
        LAMPLoadProfile *LLP;    // LAMP profiling
        PredIteratorCache PredCache_;   // Cache fetch predecessor of a BB
          
        typedef std::pair<Instruction *, Instruction *> AntiDepPairTy;
        typedef SmallVector<Instruction *, 16> AntiDepPathTy;  

        // Intermediary data structure 1.
        typedef SmallVector<AntiDepPairTy, 16> AntiDepPairs;
        AntiDepPairs AntiDepPairs_;

        // Intermediary data structure 2.
        typedef SmallVector<AntiDepPathTy, 16> AntiDepPaths;
        AntiDepPaths AntiDepPaths_;
        
        // Hitting set of instructions
        typedef SmallPtrSet<Instruction *, 16> SmallPtrSetTy;
        SmallPtrSetTy HittingSet_;

        // pass constructor
        idenRegion() : FunctionPass(ID) {}

        // get the profile information
        void getAnalysisUsage(AnalysisUsage &AU) const {
            AU.addRequired<DominatorTree>();
            AU.addRequired<LoopInfo>();
            AU.addRequired<AliasAnalysis>();
            AU.addRequired<LAMPLoadProfile>();
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
        void computeHittingSet();
        // return a set of BB that need cut
        std::set<BasicBlock *> computeHittingSetinBB();

        Instruction* findLargestCount(std::map<Instruction *, int> Map);
        
        //===----------------------------------------------------------------------===//
        // Printers
        //===----------------------------------------------------------------------===//
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

        // display anti-dependency pair
        void printPair(const AntiDepPairTy &P) {
            errs() << "Antidependence Pair ( " << getLocator(*P.first) << ", " 
            << getLocator(*P.second) << " )";
        }
        
        // display anti-dependency path
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

        // display anti-dependency collection
        void printCollection(const AntiDepPaths &PS) {
            errs() << "{ ";
            for (AntiDepPaths::const_iterator I = PS.begin(), First = I, 
                 E = PS.end(); I != E; ++I) {
                if (I != First)
                    errs() << ", ";
                printPath(*I);
            }
            errs() << " }";
        }

        // print Inst -> index
        void printMap(std::map<Instruction *, int> Map) {
            for (std::map<Instruction *, int>::iterator I = Map.begin(), E = Map.end(); I != E; I++) {
                errs() << "   " << getLocator(*(I->first)) << " --> " << I->second << "\n";
            }
        }
        
        // print index -> Inst
        void printMap(std::map<int, Instruction *> Map) {
            for (std::map<int, Instruction *>::iterator I = Map.begin(), E = Map.end(); I != E; I++) {
                errs() << "   " << I->first << "-->" << getLocator(*(I->second)) << "\n";
            }
        }
        
        // print inst -> list of position
        void printMap(std::map<Instruction *, std::set<int> > Map) {
            for (std::map<Instruction *, std::set<int> >::iterator I = Map.begin(), E = Map.end(); I != E; I++) {
                errs() << "   " << getLocator(*(I->first)) << " --> ";
                for (std::set<int>::iterator II = I->second.begin(), EE = I->second.end(); II != EE; II++) {
                    errs() << *II << " ";
                }
                errs() << "\n";
            }
        }

        // print 2D array
        void print2Darray(int *array, int len, std::map<int, Instruction *>idToinst) {
            std::string del = " ";
            errs() << del;
            for (std::map<int, Instruction *>::iterator I = idToinst.begin(), E = idToinst.end(); I != E; I++) {
                errs() << getLocator(*(I->second)) << del;
            }
            errs() << "\n";
            for (int i = 0; i < len; i++) {
                errs() << getLocator(*(idToinst[i])) << del;
                for (int j = 0; j < len; j++) {
                    errs() << array[i * len + j] << del;
                }
                errs() << "\n";
            }
        }

        // print Hitting set
        void printHittingSet(const SmallPtrSetTy &SPS) {
            errs() << "[ ";
            for (SmallPtrSetTy::iterator I = SPS.begin(), E = SPS.end(), First = I; I != E; I++) {
                if (First != I)
                    errs() << ", ";
                errs() << getLocator(**I);
            }
            errs() << " ]\n";
        }

        // print Set
        void printSet(std::set<BasicBlock *> BBSet) {
            errs() << "[ ";
            for (std::set<BasicBlock *>::iterator I = BBSet.begin(), E = BBSet.end(), First = I; I != E; I++) {
                if (First != I)
                    errs() << ", ";
                errs() << (*I)->getName().str();
            }
            errs() << " ]\n";
        }
    };
}

char idenRegion::ID = 0;
static RegisterPass<idenRegion> X("idenRegion-static", "EECS 583 project", false, false);

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
    errs() << "---------------------------------------------\n";
    errs() << "----------Find anti-dependency Path----------\n";
    errs() << "---------------------------------------------\n";
    computeAntidependencePaths();
    
    errs() << "---------------------------------------------\n";
    errs() << "----------Compute the Hitting Set------------\n";
    errs() << "---------------------------------------------\n";
    computeHittingSet();
    errs() << "!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
    errs() << "!!!!! Hitting Set is !!!!!!\n";
    errs() << "!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
    printHittingSet(HittingSet_);
    errs() << "!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
    errs() << "!!!! Hitting Set BB is !!!!\n";
    errs() << "!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
    printSet(computeHittingSetinBB());
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
    errs() << "#########################################################\n";
    errs() << "#################### Paths Summary ######################\n";
    errs() << "#########################################################\n";
    printCollection(AntiDepPaths_);
    errs() << "\n";
}

Instruction* idenRegion::findLargestCount(std::map<Instruction *, int> Map) {
    // TODO: linear right now, optimize later
    Instruction* temp_max;
    int max = 0;
    for (std::map<Instruction *, int>::iterator I = Map.begin(), E = Map.end(); I != E; I++) {
        // exclude those in the hitting set
        if (I->second > max) {
            temp_max = I->first;
            max = I->second;
        }
    }
    return temp_max;
}

void idenRegion::computeHittingSet() {
    std::vector<AntiDepPathTy> collectionPaths;
    typedef std::map<Instruction *, int> instCountTy;
    typedef std::map<Instruction *, std::set<int> > instPosTy; 
    instCountTy instCount;
    instPosTy instPos;
    int index = 0;
    for (AntiDepPaths::iterator I = AntiDepPaths_.begin(), E = AntiDepPaths_.end(); I != E; I++, index++) {
        collectionPaths.push_back(*I);
        errs() << "   " << index << ": ";
        printPath(*I);
        errs() << "\n";
        // construct the map
        for (AntiDepPathTy::iterator II = I->begin(), EE = I->end(); II != EE; II++) {
            instCount[*II] += 1;
            instPos[*II].insert(index);
        }
    }
    errs() << "~~~~ Inst Count Map:\n";
    printMap(instCount);
    errs() << "~~~~ Inst Position Map:\n";
    printMap(instPos);
    
    // Generate the Hitting set based on Map information
    int totalPaths = collectionPaths.size();
    std::set<int> HittedSet;
    while ((int)HittedSet.size() < totalPaths) {
        bool increase = false;
        int oldLen = HittedSet.size();
        while (!increase) {
            Instruction* maxCountInst = findLargestCount(instCount);
            std::set<int> tempPos = instPos[maxCountInst];
            for (std::set<int>::iterator I = tempPos.begin(), E = tempPos.end(); I != E; I++) {
                HittedSet.insert(*I);
            }
            instCount.erase(maxCountInst);
            if ((int)HittedSet.size() > oldLen) {
                increase = true;
                HittingSet_.insert(maxCountInst);
            }
        }
    }
}

std::set<BasicBlock *> idenRegion::computeHittingSetinBB() {
    std::set<BasicBlock *> HittingSetBB;
    for (SmallPtrSetTy::iterator I = HittingSet_.begin(), E = HittingSet_.end(); I != E; I++) {
        HittingSetBB.insert((*I)->getParent());
    }
    return HittingSetBB;
}


