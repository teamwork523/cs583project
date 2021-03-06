
#include <iostream>
#include <fstream>
#include <map>

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Instructions.h"

#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Analysis/ProfileInfo.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/Compiler.h"

using namespace std;
using namespace llvm;

static std::map<Instruction*, Instruction*> *clone_map;
static std::ofstream stat_file;

static int function = 0;

namespace
{
    struct IP : public FunctionPass
    {
    
      static char ID;
      ProfileInfo* PI;

      void Copy(Instruction *i);

      IP() : FunctionPass(ID) {}
      void getAnalysisUsage(AnalysisUsage &AU) const
      {
	      AU.addRequired<ProfileInfo>();
      }

      virtual bool runOnFunction(Function &F)
      {
          if(function == 0)
          {
              stat_file.open("stats.txt");
          }
          else
          {
              stat_file.open("stats.txt", std::fstream::app);
          }

          function++;

          list<Instruction*>* copy_instructions = new list<Instruction*>();
          clone_map = new std::map<Instruction*, Instruction*>();

          for(Function::iterator b = F.begin(), be = F.end(); b != be; ++b)
          {
              for(BasicBlock::iterator I=b->begin(), ie = b->end(); I!=ie; ++I)
              {
                  unsigned opcode = I->getOpcode();

                  //branch
		          if(opcode >= 1 && opcode <= 7)
                  {
			          //do not copy branch instructions (they would be skipped anyway)
                      //copy_instructions->push_back(&*I);
		          }
                  //memory
		          else if(opcode >= 26 && opcode <= 32)
		          {
			          //memory instructions are protected by ECC
                      if(opcode == 27)// || opcode == 28)
                      {
                          copy_instructions->push_back(&*I);
                      }
		          }
                  //floating point ALU
		          else if(opcode == 9 || opcode == 11 || opcode == 13 || 
                    opcode == 16 || opcode == 19 || opcode == 36 || 
                    opcode == 37 || opcode == 40 || opcode == 41 || 
                    opcode == 46)
		          {
			          //floating point arithmetic must be checked
                      copy_instructions->push_back(&*I);
		          }
                  //integer ALU
		          else if(opcode == 8 || opcode == 10 || opcode == 12 || 
                    opcode == 14 || opcode == 15 || opcode == 17 || 
                    opcode == 17 || (opcode >= 20 && opcode <= 25) || 
                    (opcode >= 33 && opcode <= 35) || opcode == 38 || 
                    opcode == 39 || (opcode >= 42 && opcode <= 45))
		          {
			          //integer arithmetic must be checked
                      copy_instructions->push_back(&*I);
		          }
                  //other
		          else
		          {
                      //other instructions can cause errors and will
                      //not be copied for the time being

			          //other instructions will also be checked
                      //copy_instructions->push_back(&*I);
		          }	
              }
          }

          for (list<Instruction*>::iterator I = copy_instructions->begin(), ie = copy_instructions->end(); I != ie; I++)
          {
              Copy(*I);
          }

          Instruction *first = *copy_instructions->begin();
          BasicBlock *begin = first->getParent();
          BasicBlock *split = SplitBlock(begin, begin->getFirstNonPHI(), this);
          BranchInst::Create(split, begin->getTerminator());
          begin->getTerminator()->eraseFromParent();

          //return true;

          for (list<Instruction*>::iterator I = copy_instructions->begin(), ie = copy_instructions->end(); I != ie; I++)
          {
              Instruction *origi = *I;
              Instruction *clone = (*clone_map)[*I];
              begin = clone->getParent();

              //if(origi->getOpcode() > 19 || origi->getOpcode() < 8) continue;

              BasicBlock *homeBB = clone->getParent();
              BasicBlock *lastBB = SplitBlock(homeBB, homeBB->getTerminator(), this); 

              stat_file << "A:" << clone->getName().str() << ":B:" << origi->getName().str() << ":C\n";

              ICmpInst* compare = new ICmpInst(homeBB->getTerminator(), CmpInst::ICMP_EQ, clone, origi, "compare");
              BranchInst::Create(lastBB, begin, compare, homeBB->getTerminator());
              homeBB->getTerminator()->eraseFromParent();
          }

          stat_file.close();

          return true;
      }

    };
}

char IP::ID = 0;
static RegisterPass<IP> X("idem", "Idempotent Processign World Pass", false, false);

void IP::Copy(Instruction *i)
{
    //clone a new instruction
    Instruction *clone = i->clone();
    std::string name = i->getName().str() + ".clone";

    if(!i->getType()->isVoidTy())
    {
        clone->setName(name);
    }

    clone->insertAfter(i); //DO NOT ADD HERE WHILE ITERATING!!!

    stat_file << "clone\n";

    //store the mapping between the original
    //instruction and the clone
    (*clone_map)[i] = clone;

    //remove references to original registers
    //from the cloned instruction operands
    for (unsigned op = 0; op < clone->getNumOperands(); op++)
    {
        //examine each operand
        Value *v = clone->getOperand(op);

        //ignore non-instruction operands (only want registers)
        if(!isa<Instruction>(v)) continue;

        //cast the value as an instruction
        Instruction *fix = dyn_cast<Instruction>(v);

        //test if this is a mapped register
        if(clone_map->find(fix) != clone_map->end())
        {
            stat_file << "##1:" << fix->getName().str() << " " << clone->getName().str() << ":2##\n";

            //change the operand
            clone->setOperand(op, (*clone_map)[fix]);
        }
    }
}



