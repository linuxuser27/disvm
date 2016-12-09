{
  'targets': [
    {
      'target_name': 'disvm-asm',
      'type': 'static_library',
      'sources': [
        'opcode_tokens.cpp',
        'print_to_stream.cpp',
        'type_signature.cpp',
        'sbl_reader.cpp',
        'rfc1321/md5c.c'
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
