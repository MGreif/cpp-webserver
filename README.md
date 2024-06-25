# CPP-Webserver
A very simple lowlevel webserver in C/CPP.

## Information

This project is for the sole purpose of learning C/CPP. Its not performant, neither its secure or optimized.

**YOU SHOULD NOT USE IT!**

I also did not use simplified pointer-functions like `shared_ptr` and `unique_ptr`, ... as i wanted to do the memory management myself.
This helps me to understand the caveats of doing it yourself and it also helps me to better understand memory management and optimization strategies itself.

I mostly wanted to understand C, so i tried to use mostly C related functions and datatypes.

## This projects helps me to understand:
- The build process of C/CPP
- The actual usecase of Makefiles
- Simple memory management (Yes i know, the webserver has a memory leak. But still)
- Socket connections on a low level
- HTTP parsers as i wrote one myself. Still WIP
- C/CPP modularization using Header files
- CPP related concepts like vtables
- Pointer arithmetics
- C string (char[]) operations (Im sure there are more performant ways)