{
  "targets": [
    {
      "target_name": "yubikey-otp",
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "sources": [
        "src/addon.cpp",
        "src/common.cpp",
        "src/get-yubikeys.cpp",
        "src/challenge-response.cpp"
      ],
      "include_dirs": [
        "<(module_root_dir)/node_modules/node-addon-api",
        "<(module_root_dir)/yubikey-personalization",
        "<(module_root_dir)/yubikey-personalization/ykcore"
      ],
      'libraries': [
        '<(module_root_dir)/yubikey-personalization/ykcore/.libs/libykcore.a',
        '<(module_root_dir)/yubikey-personalization/.libs/libykpers-1.a',
        '<(module_root_dir)/yubico-c/.libs/libyubikey.a'
      ],
      'defines': [ 'NAPI_DISABLE_CPP_EXCEPTIONS', 'ADDON_VERSION="<!(node -p "require(\'./package.json\').version")"' ],
      'conditions': [
        [ 'OS=="mac"', {
          'LDFLAGS': [
            '-framework IOKit',
            '-framework CoreFoundation'
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
      ]
    }
  ]
}