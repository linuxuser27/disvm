Dis VM Project
========================

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

 - `limbo/` - Source code written in limbo for testing the VM
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

# References

Dis VM specification: http://www.vitanuova.com/inferno/papers/dis.html

Limbo language addendum: http://www.vitanuova.com/inferno/papers/addendum.pdf

Inferno OS - Original Dis VM implementation: https://bitbucket.org/inferno-os/inferno-os/

# Licenses

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

