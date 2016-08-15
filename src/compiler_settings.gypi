{
  'conditions': [
    [ 'OS=="mac"', {
      'xcode_settings': {
        'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
        'OTHER_CPLUSPLUSFLAGS' : ['-std=c++1y','-stdlib=libc++'],
        'OTHER_LDFLAGS': ['-stdlib=libc++'],
        'MACOSX_DEPLOYMENT_TARGET': '10.10'
      }
    }],
    ['OS=="win"', {
      'configurations': {
        'Release': {
          'msvs_settings': {
            'VCCLCompilerTool': {
              'ExceptionHandling': 1,
              'Optimization': 2, # Optimize for speed
              'PreprocessorDefinitions':  [
                'NDEBUG'
              ]
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
      ]
    }],
    ['OS=="linux"', {
      'cflags': ['-std=c++1y'],
      'cflags_cc': ['-fexceptions'],
    }]
  ]
}