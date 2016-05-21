{
  'targets': [
    {
      'target_name': 'disvm-exec',
      'type': 'executable',
      'sources': [
        'main.cpp',
        'debugger.cpp'
      ],
      'include_dirs': [
        '../include'
      ],
      'includes': [
        '../compiler_settings.gypi'
      ],
      'dependencies': [
        '../vm/disvm.gyp:disvm',
        '../asm/asm.gyp:disvm-asm'
      ]
    }
  ]
}
