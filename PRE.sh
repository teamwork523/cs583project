#!/bin/bash
fname=$1
rm llvmprof.out # Otherwise your profile runs are added together
clang -emit-llvm -o $fname.ls.bc -c $fname.c || { echo "Failed to emit llvm bc"; exit 1; }
#opt -loop-simplify < $fname.bc > $fname.ls.bc || { echo "Failed to opt loop simplify"; exit 1; }
#cat $fname.bc > $fname.ls.bc

opt -load /home/tjandrew/Install/llvm/projects/PRE/Debug+Asserts/lib/PRE.so -gvn < $fname.ls.bc > $fname.pre1.bc || { echo "Failed to opt-load"; exit 1; }

opt -load /home/tjandrew/Install/llvm/projects/PRE/Debug+Asserts/lib/PRE.so -enable-pre2 -enable-load-pre2 < $fname.pre1.bc > $fname.pre.bc || { echo "Failed to opt-load"; exit 1; }

opt -dot-cfg $fname.pre.bc >& /dev/null

#opt -mem2reg < $fname.pre.bc > $fname.pre.m2r.bc
#cat $fname.pre.bc > $fname.pre.m2r.bc

llc $fname.pre.bc -o $fname.pre.m2r.s
g++ $fname.pre.m2r.s -o $fname.pre.m2r
echo "Execute: PRE"
./$fname.pre.m2r $2

