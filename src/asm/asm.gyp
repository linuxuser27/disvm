{
  'targets': [
    {
      'target_name': 'disvm-asm',
      'type': 'static_library',
      'sources': [
        'opcode_tokens.cpp'
      ],
      'include_dirs': [
        '../include'
      ],
      'includes': [
        '../compiler_settings.gypi'
      ]
    }
  ]
}
