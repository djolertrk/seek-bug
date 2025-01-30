# seek-bug
An LLDB based debugger that uses DeepSeek

## Build from source

### LLVM
```
brew install llvm@19
brew install lit
```

### LLDB

Download LLVM source from https://github.com/llvm/llvm-project/releases/tag/llvmorg-19.1.7.
```
$ git clone https://github.com/llvm/llvm-project.git -b release/20.x --single-branch
$ mkdir build_lldb && cd build_lldb
$ cmake ../llvm-project-llvmorg-19.1.7/llvm -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_PROJECTS="clang;lldb" -DLLVM_ENABLE_ASSERTIONS=ON -DLLDB_INCLUDE_TESTS=OFF -GNinja
$ ninja
```

### llama.cpp

```
$ git clone https://github.com/ggerganov/llama.cpp.git
$ cd llama.cpp
$ mkdir build
$ cd build
$ cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$PWD/../install -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_EXAMPLES=OFF -DLLAMA_BUILD_SERVER=OFF ..
$ ninja
$ ninja install
```

### Deep seek

TODO

### Build seek-bug

```
$ cmake ../seek-bug/ -DLLVM_DIR=/opt/homebrew/opt/llvm@19/lib/cmake/llvm -DClang_DIR=/opt/homebrew/opt/llvm@19/lib/cmake/lldb -DLLVM_EXTERNAL_LIT=/opt/homebrew/bin//lit  -DCMAKE_CXX_FLAGS="-Wno-deprecated-declarations" -Dllama_DIR=/Users/djtodorovic/projects/SeekBug/llama.cpp/install/lib/cmake/llama -DLLVM_BUILD_ROOT=/Users/djtodorovic/projects/SeekBug/build_lldb/ -G Ninja
$ ninja seek-bug
```

## Run

Compile mini example:

```
$ cat test.c 
#include <stdio.h>

int main()
{
  int x = 4;
  int y = x;
  if (x * y  > x / 2)
    return 1;
  return 0;
}
$ clang -O0 test.c
```

Run the tool:

```
$ bin/seek-bug ./a.out
=== SeekBug - Modern, Portable and Deep Debugger
(seek-bug) b main
Breakpoint 1: where = a.out`main + 8 at test.c:5:7, address = 0x0000000100003f50
(seek-bug) r
Process 13852 launched: '/Users/djtodorovic/projects/SeekBug/examples/a.out' (arm64)
Process 13852 stopped
* thread #1, queue = 'com.apple.main-thread', stop reason = breakpoint 1.1
    frame #0: 0x0000000100003f50 a.out`main at test.c:5:7
   2   	
   3   	int main()
   4   	{
-> 5   	  int x = 4;
   6   	  int y = x;
   7   	  if (x * y  > x / 2)
   8   	    return 1;
(seek-bug) n
Process 13852 stopped
* thread #1, queue = 'com.apple.main-thread', stop reason = step over
    frame #0: 0x0000000100003f58 a.out`main at test.c:6:11
   3   	int main()
   4   	{
   5   	  int x = 4;
-> 6   	  int y = x;
   7   	  if (x * y  > x / 2)
   8   	    return 1;
   9   	  return 0;
(seek-bug) n
Process 13852 stopped
* thread #1, queue = 'com.apple.main-thread', stop reason = step over
    frame #0: 0x0000000100003f60 a.out`main at test.c:7:7
   4   	{
   5   	  int x = 4;
   6   	  int y = x;
-> 7   	  if (x * y  > x / 2)
   8   	    return 1;
   9   	  return 0;
   10  	}
(seek-bug) c
Process 13852 resuming
Process 13852 exited with status = 1 (0x00000001)
(seek-bug) q
=== Done, bye!
```
