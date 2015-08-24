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
        'src/usb_driver_module.cc'
      ],
      'conditions': [
        ['OS=="mac"', {
          'sources': [
            'src/mac/usb_driver.mm',
          ],
          'xcode_settings': {
            'MACOSX_DEPLOYMENT_TARGET': '10.9',
            'OTHER_LDFLAGS': [
              '-framework Foundation',
              '-framework IOKit',
              '-framework DiskArbitration'
            ],
          },
        }],
        ['OS=="win"', {
          'sources': [
            'src/win/usb_driver.cc',
          ],
          'link_settings': {
             'libraries': [
                '-lsetupapi.lib'
	     ]
	  }
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
