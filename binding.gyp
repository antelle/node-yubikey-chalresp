{
  'targets': [
    {
      'target_name': 'yubikey-chalresp',
      'cflags!': [ '-fno-exceptions' ],
      'cflags_cc!': [ '-fno-exceptions' ],
      'sources': [
        'src/addon.cpp',
        'src/common.cpp',
        'src/get-yubikeys.cpp',
        'src/challenge-response.cpp',

        'yubico-c/ykcrc.c',

        'yubikey-personalization/ykcore/ykcore.c',
        'yubikey-personalization/ykcore/ykstatus.c'
      ],
      'include_dirs': [
        '<!@(node -p "require(\'node-addon-api\').include")',
        '<(module_root_dir)/yubikey-personalization',
        '<(module_root_dir)/yubikey-personalization/ykcore',
        '<(module_root_dir)/yubico-c'
      ],
      'defines': [ 'NAPI_DISABLE_CPP_EXCEPTIONS', 'ADDON_VERSION="<!(node -p "require(\'./package.json\').version")"' ],
      'conditions': [
        [ 'OS=="mac"', {
          'LDFLAGS': [
            '-framework IOKit',
            '-framework CoreFoundation'
          ],
          'sources': [
            'yubikey-personalization/ykcore/ykcore_osx.c'
          ],
          'xcode_settings': {
            'CLANG_CXX_LIBRARY': 'libc++',
            'MACOSX_DEPLOYMENT_TARGET': '10.7',
            'GCC_ENABLE_CPP_EXCEPTIONS': 'NO',
            'OTHER_LDFLAGS': [
              '-framework IOKit',
              '-framework CoreFoundation'
            ],
          }
        }],
        [ 'OS=="win"', {
          'sources': [
            'yubikey-personalization/ykcore/ykcore_windows.c'
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalDependencies': ['HID.lib', 'setupapi.lib']
            }
          }
        }],
        [ 'OS=="linux"', {
          'sources': [
            'yubikey-personalization/ykcore/ykcore_libusb-1.0.c'
          ],
          'include_dirs': [
            '<!(pkg-config libusb-1.0 --cflags-only-I | sed s/-I//g)'
          ],
          'libraries': [
            '<!(pkg-config libusb-1.0 --libs)'
          ]
        }]
      ]
    }
  ]
}