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
        'scheduler.cpp',
        'stack.cpp',
        'string.cpp',
        'thread.cpp',
        'utf8.cpp',
        'vm.cpp',
        'vm_memory.cpp',
        'vm_exception_handler.cpp',
        'sys/print.cpp',
        'sys/Sysmod.cpp',
      ],
      'include_dirs': [
        '../include'
      ],
      'conditions': [
        ['OS=="linux"', {
          'include_dirs': [
            '../include/linux'
          ],
          'cflags': ['-std=c++1y'],
          'cflags_cc': ['-fexceptions'],
          'sources': [ ],
        }],
        ['OS=="mac"', {
          'include_dirs': [
            '../include/osx'
          ],
          'xcode_settings': {
            'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
            'OTHER_CPLUSPLUSFLAGS' : ['-std=c++1y', '-stdlib=libc++'],
            'MACOSX_DEPLOYMENT_TARGET': '10.10'
          },
          'sources': [ ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            '../include/win32'
          ],
          'configurations': {
            'Release': {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'ExceptionHandling': 1,
                  'Optimization': 3, # Full optimizations
                }
              }
            },
            'Debug': {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'ExceptionHandling': 1,
                  'Optimization': 0, # Disable optimizations
                }
              }
            }
          },
          'msvs_settings': {
            'VCCLCompilerTool': {
              'TreatWChar_tAsBuiltInType': 'false',
              'WarningLevel': 3,
              'DebugInformationFormat': '3', # PDB
            }
          },
          'defines': [
            '_CRT_SECURE_NO_WARNINGS'
          ],
          'sources': [ ],
        }]
      ]
    }
  ]
}
