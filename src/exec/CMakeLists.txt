set(SOURCES
    debugger.cpp
    main.cpp
)

add_executable(disvm-exec
  ${SOURCES}
)

target_include_directories(disvm-exec PRIVATE ../include)

target_link_libraries(disvm-exec disvm)
target_link_libraries(disvm-exec disvm-asm)
install(TARGETS disvm-exec)
