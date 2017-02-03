disvm-exec
========================

Represents a command line example of using the DisVM. The resulting executable contains a
debugger with support loading to limbo symbol files (`.sbl`). The `disvm-exe` executable
expects the supplied module to implement the `Exec` contract defined in `limbo/Exec.m`.

Use the `-h` flag for details on how to use `disvm-exec`.

## Examples

Compile `md5sum.b` limbo file and emit symbol file:
  `disvm-exec -q limbo.dis -g md5sum.b`

Debug the compiled `md5sum.dis` module:
  `disvm-exec -de md5sum.dis md5sum.b`
