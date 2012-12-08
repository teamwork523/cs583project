//=============- iteration.cpp - HW 1 pass for EECS 583 ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implement a dynamic operation count and dynamic branch number
// count.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "iteration"
#include <fstream>
#include <iostream>
#include <string>
#include <iomanip>
#include "llvm/InstrTypes.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/BasicBlock.h"
#include "llvm/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/ProfileInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/LoopInfo.h"
#include "LAMP/LAMPLoadProfile.h"

using namespace llvm;

namespace {
    // statistic computation on operation counts
    struct iteration : public ModulePass {
        static char ID; // Pass identification, replacement for typeid

        // pass constructor
        iteration() : ModulePass(ID) {
            
            // clear up the file content
            std::ofstream clearFile(OPCOUNT_FILE);
            // add header for Operation count file
            clearFile << "FuncName\tDynOpCount\t%IALU\t%FALU\t%MEM\t%BRANCH\t%OTHER" << std::endl;
            clearFile.close();
            clearFile.open(BRCOUNT_FILE);
            // add header for Branch count file
            clearFile << "FuncName\tDynBrCount\t%50-59\t%60-69\t%70-79\t%80-89\t%90-100" << std::endl;
            clearFile.close();
            
        }

        // get the profile information
        // void getAnalysisUsage(AnalysisUsage &AU) const {
        // }

        virtual bool runOnModule(Module &M) {
            // dump all the basic block
            for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I) {
                if (!I->isDeclaration()) {
                    for (Function::iterator BBB = I->begin(), BBE = I->end(); BBB != BBE; ++BBB) {
                        errs() << "**************** BB ****************" << "\n";
                        for (BasicBlock::iterator IB = BBB->begin(), IE = BBB->end(); IB != IE; IB++) {
                            errs() << *IB << "\n";
                        }
                    }
                }
            }
            return true;
        }
    };
}

char iteration::ID = 0;
static RegisterPass<iteration> X("iteration", "EECS 583 project");

