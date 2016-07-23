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