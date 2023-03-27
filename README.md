# opt-chance
An LLVM-based tool used to compare binaries compiled with different compilers in order to find optimizations oportunities.

## Build

1. Build `LLVM`

```
$ mkdir build && cd build
$ mkdir llvm && cd llvm
$ cmake -G "Ninja" ../../opt-chance/llvm-project/llvm -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_ENABLE_PROJECTS="llvm"
$ ninja
```
