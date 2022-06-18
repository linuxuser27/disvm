# Compiler configurations

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

  add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/Zc:wchar_t->) # wchar_t is a built-in type
  add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/W4>) # warning level 4
  add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/WX>) # all warnings as errors
  add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/wd4100>) # 'identifier': unreferenced formal parameter
  add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/wd4189>) # 'identifier': local variable is initialized but not referenced
else()
  add_compile_options(-Wall -Werror) # warning level 3 and all warnings as errors.
  add_compile_options(-Wno-unknown-pragmas) # ignore unknown pragmas.
  add_compile_options(-Wno-pragma-pack) # ignore warnings about pragma pack.
endif()