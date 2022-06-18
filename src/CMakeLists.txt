# Configure the compiler
include(configure.cmake)

add_subdirectory(asm)
add_subdirectory(exec)
add_subdirectory(vm)