{
  'target_defaults': {
    'conditions': [
      ['OS=="win"', {
        'msvs_disabled_warnings': [
          4530,  # C++ exception handler used, but unwind semantics are not enabled
          4506,  # no definition for inline function
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'usb_driver',
      'include_dirs': [ '<!(node -e "require(\'nan\')")' ],
      'sources': [
        'src/usb_driver_module.cc',
        'src/usb_device_info.cc'
      ],
      'conditions': [
        ['OS=="mac"', {
          'sources': [
            'src/mac/usb_driver.mm',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
            ],
          }
        }],
        ['OS=="win"', {
          'sources': [
            'src/win/usb_driver.cc',
          ],
        }],
        ['OS=="linux"', {
          'sources': [
            'src/linux/usb_driver.cc',
          ],
        }],
      ],
    }
  ]
}
