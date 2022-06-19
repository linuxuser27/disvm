DisVM Project
========================

The DisVM project is a re-implementation of the [Dis virtual machine specification](http://www.vitanuova.com/inferno/papers/dis.html) that defined the user space of the [Inferno OS](https://en.wikipedia.org/wiki/Inferno_(operating_system)). The Inferno OS was designed to re-hash the ideas in the [Plan 9 from Bell Labs](https://en.wikipedia.org/wiki/Plan_9_from_Bell_Labs) operating system but implemented using a platform-agnostic language. The Dis virtual machine was intrinsic to the Inferno OS and ran the byte-code compiled [Limbo](https://en.wikipedia.org/wiki/Limbo_(programming_language)) language. This meant that only the Inferno OS need be ported to a platform and all Limbo compiled applications would 'just-work'. If this sounds familiar, it should. Inferno OS was developed in the early 90's and [project Oak](https://en.wikipedia.org/wiki/Oak_(programming_language)) - what would become Java - was being developed by Sun Microsystems. The major difference between the two approaches was that Oak's virtual machine would be ported to various platforms and work with the host OS, whereas the Dis virtual machine was tied to Inferno OS which meant there was yet another layer between the host and the Dis virtual machine. This decision had advantages, but also had the large disadvantage of requiring an entire separate OS to run Limbo code.

This project's intent is to take the Java approach and provide an implementation of the Dis virtual machine that is separate from any OS, including the Inferno OS. The project is written in C++ - using only the standard library where possible - and is intended to support compilation on any platform that has a conforming [C++ 11](https://en.wikipedia.org/wiki/C%2B%2B11) compiler. At present various modules written in Limbo (even the official Limbo compiler) have been verified to run successfully using this implementation. Some precompiled byte-code modules (`.dis` extension) have been provided in the [Downloads](https://bitbucket.org/linuxuser27/disvm/downloads) section.

Current Implementation Notes:

  * Only parts of the built-in [`Sys`](http://www.vitanuova.com/inferno/man/2/sys-0intro.html) and [`Math`](http://www.vitanuova.com/inferno/man/2/math-0intro.html) modules have been written
  * The `disvm-exec` project does not support the original [`Command`](http://www.vitanuova.com/inferno/man/2/command.html) entry module since the official [`Draw`](http://www.vitanuova.com/inferno/man/2/draw-0intro.html) module is not currently supported. Instead the entry point is defined by `limbo/Exec.m`.

# Components

This section contains implementation details on DisVM components.

### Garbage Collection - `src/vm/garbage_collector.cpp`

The Dis virtual machine specification described a hybrid garbage collection mechanism that utilized reference counting and a [concurrent mark-and-sweep algorithm](http://doc.cat-v.org/inferno/concurrent_gc/). The purpose of the hybrid approach was to mitigate cases where reference counting fails (e.g. cyclic references). The algorithm in this project fully implements reference counting, but at present has a naïve synchronous mark-and-sweep implementation.

It has been observed during usage of the Limbo compiler running in DisVM, that the mark-and-sweep implementation has a 20 - 25 % performance impact. This is clearly unacceptable for a modern virtual machine implementation, but was written to satisfy the current needs of the virtual machine as a learning tool. Even though the current mark-and-sweep implementation has poor performance characteristics, the original reason for it was cyclical references, so if data structures are written so these types of references don't or can't exist, reference counting will suffice. The option to disable the mark-and-sweep garbage collector is exposed as a flag on the `disvm-exec` program or if consumed directly - during instantiation of the VM.

This component can also be replaced with a custom implementation if the DisVM is consumed as a library.

### Scheduler - `src/vm/scheduler.cpp`

The DisVM default scheduler supports utilization of 1 to 4 system threads, which is useful if parallelism is desired at runtime. The current default is for the scheduler to use 1 system thread, but this can be altered from the `disvm-exec` command line or programmatically.

Like the garbage collector, this component can also be replaced with a custom implementation.

### Just-In-Time compilation

There is currently no support for the JIT compilation described in the Dis virtual machine specification.

# Build instructions

Requirements

* [C++ 11](https://en.cppreference.com/w/c/language/history) compliant compiler.
* [CMake](https://cmake.org/download/) &ndash; minimum is 3.20.

1. `build.cmd generate (Debug|Release)?`

# Source

The source tree is as follows:

 - `limbo/` - Source code written in Limbo for testing DisVM
     - `compiler/` - Copied and slightly modified source code for the official Limbo compiler
 - `src/`
     - `asm/` - Library for manipulating byte code
     - `include/` - Global include files
     - `exec/` - Hosting binary for DisVM (includes debugger)
     - `vm/` - DisVM as a static library


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

### [LAPACK](http://www.netlib.org/lapack/#_licensing) ###

Copyright (c) 1992-2013 The University of Tennessee and The University
                        of Tennessee Research Foundation.  All rights
                        reserved.
Copyright (c) 2000-2013 The University of California Berkeley. All
                        rights reserved.
Copyright (c) 2006-2013 The University of Colorado Denver.  All rights
                        reserved.

$COPYRIGHT$

Additional copyrights may follow

$HEADER$

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

- Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer listed
  in this license in the documentation and/or other materials
  provided with the distribution.

- Neither the name of the copyright holders nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

The copyright holders provide no reassurances that the source code
provided does not infringe any patent, copyright, or any other
intellectual property rights of third parties.  The copyright holders
disclaim any liability to any recipient for claims brought against
recipient by any third party for infringement of that parties
intellectual property rights.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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