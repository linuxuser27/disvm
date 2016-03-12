{
  'targets': [
    {
      'target_name': 'disvm-sh',
      'type': 'executable',
      'sources': [
        'main.cpp'
      ],
      'configurations': {
        'Release': {
          'msvs_settings': {
            'VCCLCompilerTool': {
              'ExceptionHandling': 1,
            }
          }
        }
      },
      'include_dirs': [
        '../include'
      ],
      'dependencies': [
        '../vm/disvm.gyp:disvm'
      ],
      'conditions': [
        [ 'OS=="mac"', {
          'include_dirs': [
            '../include/osx'
          ],
          'xcode_settings': {
            'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
            'OTHER_CPLUSPLUSFLAGS' : ['-std=c++1y','-stdlib=libc++'],
            'OTHER_LDFLAGS': ['-stdlib=libc++'],
            'MACOSX_DEPLOYMENT_TARGET': '10.10'
          }
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
                }
              }
            },
            'Debug': {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'ExceptionHandling': 1,
                }
              }
            }
          },
          'msvs_settings': {
            'VCCLCompilerTool': {
              'ExceptionHandling': '1', # /EHsc - This seems to only impact Debug, not Release
              'TreatWChar_tAsBuiltInType': 'false',
              'WarningLevel': '3',
              'DebugInformationFormat': '3', # PDB
            }
          }
        }],
        ['OS=="linux"', {
          'cflags': ['-std=c++1y'],
          'cflags_cc': ['-fexceptions'],
        }]
      ]
    }
  ]
}
