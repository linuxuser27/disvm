{
  'targets': [
    {
      'target_name': 'disvm',
      'type': 'static_library',
      'sources': [
        'array.cpp',
        'builtin_module.cpp',
        'channel.cpp',
        'debug.cpp',
        'execution_table.cpp',
        'garbage_collector.cpp',
        'list.cpp',
        'module_reader.cpp',
        'module_ref.cpp',
        'module_resolver.cpp',
        'scheduler.cpp',
        'stack.cpp',
        'string.cpp',
        'thread.cpp',
        'tool_dispatch.cpp',
        'utf8.cpp',
        'vm.cpp',
        'vm_memory.cpp',
        'vm_exception_handler.cpp',
        'math/Mathmod.cpp',
        'math/dgemm.c',
        'math/lsame.c',
        'sys/print.cpp',
        'sys/Sysmod.cpp',
        'sys/file_system.cpp',
        'sys/fd_manager.cpp',
      ],
      'include_dirs': [
        '../include'
      ],
      'includes': [
        '../compiler_settings.gypi'
      ],
      'conditions': [
        ['OS=="linux"', {
          'include_dirs': [
            '../include/linux'
          ],
          'sources': [ ],
        }],
        ['OS=="mac"', {
          'include_dirs': [
            '../include/osx'
          ],
          'sources': [ ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            '../include/win32'
          ],
          'sources': [ ],
        }]
      ]
    }
  ]
}
