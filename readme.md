DisVM Project
========================

The DisVM project is a re-implementation of the [Dis virtual machine specification](http://www.vitanuova.com/inferno/papers/dis.html) that defined the user space of the [Inferno OS](https://en.wikipedia.org/wiki/Inferno_(operating_system)). The Inferno OS was designed to re-hash the ideas in the [Plan 9 From Bell Labs](https://en.wikipedia.org/wiki/Plan_9_from_Bell_Labs) operating system but implemented using a platform agnostic language - [Limbo](https://en.wikipedia.org/wiki/Limbo_(programming_language)). The Dis virtual machine was intrinsic to the Inferno OS and ran the byte-code compiled Limbo language. This meant that only the Inferno OS need be ported to a platform and all Limbo compiled applications would 'just-work'. If this sounds familiar, it should, this was in the early 90's and [project Oak](https://en.wikipedia.org/wiki/Oak_(programming_language)) - what would become Java - was being developed by Sun Microsystems. The major difference between the two approaches was that Oak's virtual machine would be ported to various platforms and work with the host OS whereas the Dis virtual machine was tied to Inferno OS which meant there was yet another layer between the host and virtual machine. This decision had advantages, but also had the large disadvantage of requiring and entire separate OS to run Limbo code. 

This project's intent is to take the Java approach and provide an implementation of the Dis virtual machine that is separate from any OS, including the Inferno OS. The project is written in C++ - using only the standard library where possible - and is intended to support compilation on any platform that has a conforming [C++ 11](https://en.wikipedia.org/wiki/C%2B%2B11) compiler. At present various modules written in Limbo (even the official Limbo compiler) have been verified to run successfully using this implementation. Some precompiled byte-code modules (`.dis` extension) have been provided in the [Downloads](https://bitbucket.org/linuxuser27/disvm/downloads) section.

Notes:

  * Only parts of the built-in [`Sys`](http://www.vitanuova.com/inferno/man/2/sys-0intro.html) and [`Math`](http://www.vitanuova.com/inferno/man/2/math-0intro.html) modules have been written
  * The `disvm-exec` project does not support the original [`Command`](http://www.vitanuova.com/inferno/man/2/command.html) entry module since the official [`Draw`](http://www.vitanuova.com/inferno/man/2/draw-0intro.html) module is not currently supported. Instead the entry point is defined by `limbo/Exec.m`.

# Build instructions

Requirements

  * gyp - http://gyp.gsrc.io
  * Python 2.7 - http://www.python.org/download/releases/2.7/

Dis VM build scripts can be generated using gyp (Generate-Your-Project)

1. Install Python 2.7
2. Get current source for gyp
3. Generate build file for your platform
    - Windows -- `gyp.bat src\exec\exec.gyp`
    - Other -- `gyp_main.py src/exec/exec.gyp`

Python and gyp are only needed for build file generation.

# Source

The source tree is as follows:

 - `limbo/` - Source code written in Limbo for testing the VM
     - `compiler/` - Copied and slightly modified source code for the official Limbo compiler
 - `src/`
     - `asm/` - Library for manipulating byte code
     - `include/` - Global include files
     - `exec/` - Hosting binary for VM (includes debugger)
     - `vm/` - VM as a static library


| Comment prefix | Description |
|----------------|-------------|
| `[TODO]`       | Work to be done in the future. |
| `[SPEC]`       | A departure or undocumented assumption in the Dis VM specification. |
| `[PERF]`       | Potential for performance improvement. |
| `[PAL]`        | Work that should be done in a platform abstraction layer (PAL). |

See `readme.md` or `license.txt` in source directories for further details or specific licensing.

# References

Dis virtual machine specification: http://www.vitanuova.com/inferno/papers/dis.html

Limbo language addendum: http://www.vitanuova.com/inferno/papers/addendum.pdf

Inferno OS - Original Dis virtual machine implementation: https://bitbucket.org/inferno-os/inferno-os/

# Licenses

See source directory for a `license.txt` that defines the license, otherwise licenses for the DisVM project are defined below.

### [RFC1321](https://www.ietf.org/rfc/rfc1321.txt) ###

   Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
   rights reserved.

   License to copy and use this software is granted provided that it
   is identified as the "RSA Data Security, Inc. MD5 Message-Digest
   Algorithm" in all material mentioning or referencing this software
   or this function.

   License is also granted to make and use derivative works provided
   that such works are identified as "derived from the RSA Data
   Security, Inc. MD5 Message-Digest Algorithm" in all material
   mentioning or referencing the derived work.  
                                                                    
   RSA Data Security, Inc. makes no representations concerning either
   the merchantability of this software or the suitability of this
   software for any particular purpose. It is provided "as is"
   without express or implied warranty of any kind.  
                                                                    
   These notices must be retained in any copies of any part of this
   documentation and/or software.  

### [UTF8 Parsing](http://bjoern.hoehrmann.de/utf-8/decoder/dfa/) ###

Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

### Unless otherwise stated in a `license.txt` or above - [MIT License](https://opensource.org/licenses/MIT) ###

Copyright (c) 2017 arr

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.