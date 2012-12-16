#!/bin/bash

fname=$1

llvm_path=/y/students/haokun/llvm
pass_root=/y/students/haokun/idenpotent/proj
class_name=idenRegion
pass_name=idenRegion-static

rm llvmprof.out 2> /dev/null    # Otherwise your profile runs are added together

clang -emit-llvm -o $fname.bc -c $fname.c || { echo "Failed to emit llvm bc"; exit 1; }

opt -loop-simplify < $fname.bc > $fname.ls.bc || { echo "Failed to opt loop simplify"; exit 1; }

opt -insert-edge-profiling $fname.ls.bc -o $fname.profile.ls.bc

llc $fname.profile.ls.bc -o $fname.profile.ls.s

g++ -o $fname.profile $fname.profile.ls.s $llvm_path/Debug+Asserts/lib/libprofile_rt.so

./$fname.profile $2

# convert to SSA form
opt -mem2reg < $fname.ls.bc > $fname.m2r.bc || { echo "Failed to convert SSA"; exit 1; }

# redundancy elimination
opt -load $pass_root/Debug+Asserts/lib/$class_name.so -gvn -enable-pre2 -enable-load-pre2 < $fname.m2r.bc > $fname.pre.bc || { echo "Fail to GVN"; exit 1; }
# opt -load $pass_root/Debug+Asserts/lib/$class_name.so -enable-pre2 -enable-load-pre2 < $fname.pre.temp.bc > $fname.pre.bc || ( echo "Fail to PRE"; exit 1; )

# create dot file
opt -dot-cfg $fname.pre.bc >& /dev/null

opt -load $pass_root/Debug+Asserts/lib/$class_name.so -$pass_name < $fname.pre.bc > $fname.idenregion.bc || { echo "Fail to opt-load idenRegion"; exit 1; }


llc $fname.idenregion.bc -o $fname.idenregion.s 
g++ $fname.idenregion.s -o $fname.idenregion 

./$fname.idenregion 
