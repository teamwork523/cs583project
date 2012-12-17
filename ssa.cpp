 //use SSA to avoid conflict
for (int ii=0;ii<vecit->size();ii++) {
     Instruction* oriinst=(*vecit)[ii];
     Instruction* newinst=newv[oriinst];

     //change the dependent operands
     for (int kk=ii+1;kk<vecit->size();kk++) {
       Instruction* oriinst2=(*vecit)[kk];
       Instruction* newinst2=newv[oriinst2];
       for (int mm=0;mm<newinst2->getNumOperands();mm++){
         Value * vx=newinst2->getOperand(mm);
         if (vx == oriinst){
           newinst2->setOperand(mm,newinst);
         }
       }
     }

     SmallVector<PHINode*, 8>* NewPHIs=new SmallVector<PHINode*, 8>();
     SSAUpdater* SSA=new SSAUpdater (NewPHIs);
     instToSSA[oriinst]=SSA;
     SSA->Initialize(oriinst->getType(), oriinst->getName());
     SSA->AddAvailableValue(oriinst->getParent(),oriinst);


     if (!oriinst->use_empty()){
       SSA->AddAvailableValue(redoBB,newinst);
       if (oriinst->getType()->isPointerTy())
          for (unsigned i = 0, e = NewPHIs->size(); i != e; ++i)
              urAST->copyValue(oriinst, (*NewPHIs)[i]);
     }
}
