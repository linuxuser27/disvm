set(SOURCES
  opcode_tokens.cpp
  print_to_stream.cpp
  sbl_reader.cpp
  type_signature.cpp
  rfc1321/md5c.c
)

if(MSVC)
  # This should only be applicable to files under rfc1321/.
  add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/wd4131>) # 'function': uses old-style declarator
endif()

add_library(disvm-asm
  STATIC
  ${SOURCES}
)

target_include_directories(disvm-asm PUBLIC ../include)

install(TARGETS disvm-asm)
