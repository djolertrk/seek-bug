# seek-bug

<div align=center>
  <img src="https://github.com/user-attachments/assets/c1a33e56-dcfb-43e9-81fe-f38fd4cb1db8">
</div>

Let's have a chat with your debugger!

`seek-bug` - an `LLDB` based debugger that uses `DeepSeek`.
You can use it as a standalone tool, or as an LLDB plugin. The `DeepSeek` LLM is running on your machine.

## Build from source

Here are steps to build the tool for MacOS and Linux.

### LLVM

Download LLVM packages on MacOS:

```
$ brew install llvm@19
$ brew install lit
```

Download LLVM packages on Linux:

```
$ echo "deb http://apt.llvm.org/$(lsb_release -cs)/ llvm-toolchain-$(lsb_release -cs)-19 main" | sudo tee /etc/apt/sources.list.d/llvm.list
$ wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
$ sudo apt-get update
$ sudo apt-get install -y llvm-19-dev clang-19 libclang-19-dev lld-19 pkg-config libgc-dev libssl-dev zlib1g-dev libunwind-dev liblldb-19-dev
```

### LLDB (this is needed for MacOS only)

On MacOS, we need to download LLVM source from https://github.com/llvm/llvm-project/releases/tag/llvmorg-19.1.7. On Linux, you can skip it, since `lldb-dev` package is there.

```
$ git clone https://github.com/llvm/llvm-project.git -b release/20.x --single-branch
$ mkdir build_lldb && cd build_lldb
$ cmake ../llvm-project-llvmorg-19.1.7/llvm -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_PROJECTS="clang;lldb" -DLLVM_ENABLE_ASSERTIONS=ON -DLLDB_INCLUDE_TESTS=OFF -GNinja
$ ninja
```

### llama.cpp

Build `llama.cpp`:

```
$ git clone https://github.com/ggerganov/llama.cpp.git
$ cd llama.cpp && mkdir build && cd build
$ cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$PWD/../install -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_EXAMPLES=OFF -DLLAMA_BUILD_SERVER=OFF ..
$ ninja && ninja install
```

### Deep seek

Download: https://huggingface.co/lmstudio-community/DeepSeek-R1-Distill-Llama-8B-GGUF/tree/main, the `DeepSeek-R1-Distill-Llama-8B-Q8_0.gguf` model.

```
$ wget https://huggingface.co/lmstudio-community/DeepSeek-R1-Distill-Llama-8B-GGUF/resolve/main/DeepSeek-R1-Distill-Llama-8B-Q8_0.gguf
```

### Build seek-bug

NOTE: On Linux, you can avoid `-DLLVM_BUILD_ROOT`, since we are using it from installed packages.

Configure on MacOS:

```
$ cmake ../seek-bug/ -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm -DLLVM_EXTERNAL_LIT=/path/to/lit  -DCMAKE_CXX_FLAGS="-Wno-deprecated-declarations" -DLLAMA_CPP_DIR=/path/to/llama.cpp/install/ -DLLVM_BUILD_ROOT=/path/to/build_lldb/ -G Ninja
```

Configure on Linux:

```
$ cmake ../seek-bug/ -DCMAKE_CXX_FLAGS="-Wno-deprecated-declarations" -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm -DLLVM_EXTERNAL_LIT=/path/to/lit -DLLAMA_CPP_DIR=/path/to/llama.cpp/install/ -DCMAKE_CXX_FLAGS="-fPIC" -G Ninja
```

Build:

```
$ ninja seek-bug
$ ninja SeekBugPlugin
```

## Run the as standalone tool

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

Run the tool (on Linux, put `/path/to/llama.cpp/install/lib` to `LD_LIBRARY_PATH`, it is a hack for it):

```
$ bin/seek-bug --deep-seek-llm-path=/path/to/DeepSeek-R1-Distill-Llama-8B-Q8_0.gguf ./a.out
=== SeekBug - Modern, Portable and Deep Debugger
(seek-bug) b main
Breakpoint 1: where = a.out`main + 8 at test.c:5:7, address = 0x0000000100003f50
(seek-bug) r
Process 13852 launched: '/path/to/a.out' (arm64)
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
=== Happy Debugging! Bye!
```

NOTE: In Linux enviroment, until I fix it, we needed to setup:

```
$ export LLDB_DEBUGSERVER_PATH=/usr/lib/llvm-19/bin/lldb-server-19.1.7
$ export LD_LIBRARY_PATH=/path/to/llama.cpp/install/lib
```

## Build and run as LLDB Plugin

Build:

```
$ ninja SeekBugPlugin
```

Run:

```
$ export DEEP_SEEK_LLM_PATH=/path/to/your/DeepSeek-R1-Distill-Llama-8B-Q8_0.gguf
$ /opt/homebrew/opt/llvm@19/bin/lldb test2.out
(lldb) target create "test2.out"
Current executable set to 'test2.out' (arm64).
(lldb) plugin load libSeekBugPlugin.dylib
SeekBug is using DeepSeek-R1-Distill-Llama-8B-Q8_0.gguf
SeekBug plugin loaded successfully.
(seek-bug) b main
Breakpoint 1: where = test2.out`main + 8 at test2.c:5:7, address = 0x0000000100003f50
(seek-bug) r
Process 34281 launched: 'test2.out' (arm64)
Process 34281 stopped
* thread #1, queue = 'com.apple.main-thread', stop reason = breakpoint 1.1
    frame #0: 0x0000000100003f50 test2.out`main at test2.c:5:7
   2   	
   3   	int main()
   4   	{
-> 5   	  int x = 4;
   6   	  int y = x;
   7   	  if (x * y  > x / 2)
   8   	    return 1;
...
(seek-bug) n
Process 34281 stopped
* thread #1, queue = 'com.apple.main-thread', stop reason = step over
    frame #0: 0x0000000100003f88 test2.out`main at test2.c:8:5
   5   	  int x = 4;
   6   	  int y = x;
   7   	  if (x * y  > x / 2)
-> 8   	    return 1;
   9   	  return 0;
   10  	}
   11  	
(seek-bug) ai suggest "How do I come to this line?"
DeepSeek is thinking...
---

The user is asking how to reach a specific line of code. To figure this out, I need to understand the control flow leading to that line. I'll start by examining the code structure, looking at functions and where they're called. Maybe the line is inside a loop or an if statement. I'll check the parent functions and see how the execution reaches that point. If there's a conditional, I'll determine if the condition is met. I'll also look for any variables that affect the control flow, like boolean values or counters. Once I have this information, I can explain the path taken to execute the target line.
</think>

To reach the specific line, examine the control flow by checking the code structure. Look for functions and where they're called. Determine if the line is inside a loop or an `if` statement. Investigate the parent functions to see how execution reaches that point. If there's a conditional, check if the condition is met. Also, consider variables affecting the control flow, like boolean values or counters. Once you understand the path, you can explain how the line is executed.
```

NOTE: This has a bug. The `SBTarget` and `SBThreads` are invalid when LLDB plugin is initialize. I do not know why at the moment.

## Run tests

Just run:

```
$ ninja check-seek-bug
```
