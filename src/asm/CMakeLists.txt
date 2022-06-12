set(SOURCES
    opcode_tokens.cpp
    print_to_stream.cpp
    sbl_reader.cpp
    type_signature.cpp
)

add_library(disvm-asm
  STATIC
  ${SOURCES}
)

target_include_directories(disvm-asm PUBLIC ../include)

install(TARGETS disvm-asm)
