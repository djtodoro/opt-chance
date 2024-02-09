# opt-chance
An LLVM-based tool used to compare binaries compiled with different compilers in order to find optimizations oportunities.
This is still a WIP.

## Clone

If you want to use upstream llvm:
```
$ git clone https://github.com/djtodoro/opt-chance.git --recursive
```

Otherwise (please do note that if you want to use your version of LLVM, you need to use `EXTERN_LLVM_SRC_DIR` cmake variable):

```
$ git clone https://github.com/djtodoro/opt-chance.git
```

## Build (in-tree-llvm)

1. Build `LLVM`

```
$ mkdir build_llvm && cd build_llvm
$ cmake -G "Ninja" ../opt-chance/llvm-project/llvm -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_ENABLE_PROJECTS="llvm"
$ ninja
```

2. Build `opt-chance`

```
$ cd ..
$ mkdir build_tool && cd build_tool
$ cmake -G "Ninja" ../opt-chance/ -DLLVM_DIR="$PWD/../build_llvm/llvm/lib/cmake/llvm" -DLLVM_ENABLE_ASSERTIONS=ON -DCMAKE_BUILD_TYPE=Release
$ ninja opt-chance
```

## Build (with custom LLVM)

Then, build the plugin with your version of llvm, e.g. `llvm-project-llvmorg-16.0.4` (please note `-DEXTERN_LLVM_SRC_DIR` that contains src of the llvm and proper `-DLLVM_DIR` that contatins build of the llvm):

```
$ cmake -G"Ninja" -DEXTERN_LLVM_SRC_DIR="${PATH_TO_LLVM}/llvm-project-llvmorg-16.0.4" -DCMAKE_BUILD_TYPE=Release -DLLVM_DIR=$PWD/../build_llvm/lib/cmake/llvm -DLLVM_ENABLE_ASSERTIONS=ON ../opt-chance/
$ ninja opt-chance
```

## Usage

```
$ ../build_tool/opt-chance a2.out a.out 
=== opt-chance Tool ===
Analysis...

Candidates to optimize:
for function 'main' the diff is 18

Analysis done.
```

It means that `main` function is larger in the `a.out` for 18 bytes.

There is a ```--verbose``` option as well.

```
$ ../build_tool/opt-chance a2.out a.out --verbose
=== opt-chance Tool ===
Analysis...
Analysis of file: a2.out
 function main: 25 bytes
 function fn: 11 bytes
 function fn4: 11 bytes
 function fn3: 11 bytes
 function fn2: 11 bytes

Analysis of file: a.out
 function main: 43 bytes
 function fn: 11 bytes
 function fn4: 11 bytes
 function fn3: 11 bytes
 function fn2: 11 bytes

Candidates to optimize:
for function 'main' the diff is 18

Analysis done.
```
