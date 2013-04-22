{
  'variables': {
    # We need python as a parameter to support Windows correctly. There are two 
    # Windows versions of python commonly installed, one for cygwin and one not. 
    # Rather than rely on whatever happens to be in the path (if anything), we 
    # provide the option to explicitly specify an known executable.
    'python%': 'python',
  },
  'conditions': [
    [
        # Put OS values known not to support curl in the expression below
        'OS=="win"',      
        { 'variables': { 'have_curl': 0 }},
        { 'variables' :{ 'have_curl': '<!(<(python) check_curl.py' }}
    ],[
        'OS=="win"', { 
          'variables': {
            'SHARED_INTERMEDIATE_DIR': 'build_solo/shared_intermediate',
            'PRODUCT_DIR': 'build_solo' 
        }}        
    ]
  ],
  'includes': ['third_party/v8/build/common.gypi'],
  'target_defaults': {
    'configurations': {
      'Base': {
        # 'abstract' is an undocumented attribute that suppresses generating
        # the configuration in output files.
        # See http://www.mail-archive.com/chromium-dev@googlegroups.com/msg10147.html
        'abstract': 1,
        # No need to put conditionals around generator-specific attributes.
        # They're simply ignored if not used.
        'msvs_configuration_attributes': {
          'OutputDirectory': '<(PRODUCT_DIR)/$(ConfigurationName)',
        },
        'msvs_settings': {
          'VCCLCompilerTool': {
            # Definition for _VARIADIC_MAX is a workaround for VS 2012 defect.
            # See http://code.google.com/p/googletest/issues/detail?id=412
            'PreprocessorDefinitions': ['_VARIADIC_MAX=10'],
          },
          'VCLinkerTool': {
            'AdditionalDependencies': [
              'googletest.lib', 'googletest_main.lib', 
              'v8_base.lib', 'v8_nosnapshot.lib', 'v8_snapshot.lib',
              'winmm.lib', 'ws2_32.lib',
              'Shlwapi.lib', 'Winhttp.lib'
            ],
            'AdditionalLibraryDirectories': ['external_library/$(ConfigurationName)'],
            'EntryPointSymbol': 'mainCRTStartup',            
          },
        }
      },
      'Debug': {
        'inherit_from': ['Base'],
      },
      'Release': {
        'inherit_from': ['Base'],
      },
    },
  },
  'targets': [{
    'target_name': 'libadblockplus_solo',
    'type': '<(library)',
    'msvs_cygwin_shell': 0,
    'include_dirs': [
      'include',
      'third_party/v8/include'
    ],
    'sources': [
      'src/AppInfoJsObject.cpp',
      'src/ConsoleJsObject.cpp',
      'src/DefaultErrorCallback.cpp',
      'src/DefaultFileSystem.cpp',
      'src/ErrorCallback.cpp',
      'src/FileSystemJsObject.cpp',
      'src/FilterEngine.cpp',
      'src/GlobalJsObject.cpp',
      'src/JsEngine.cpp',
      'src/JsValue.cpp',
      'src/Thread.cpp',
      'src/Utils.cpp',
      'src/WebRequestJsObject.cpp',
      '<(SHARED_INTERMEDIATE_DIR)/adblockplus.js.cpp'
    ],
    'direct_dependent_settings': {
      'include_dirs': ['include']
    },
    'conditions': [
      ['have_curl==1',
        {
          'sources': [
            'src/DefaultWebRequestCurl.cpp',
          ],
          'all_dependent_settings': {
            'defines': ['HAVE_CURL'],
            'libraries': ['-lcurl']
          }
        }
      ],
      ['have_curl!=1',
        {
          'sources': [
            'src/DefaultWebRequestDummy.cpp',
          ]
        }
      ]
    ],
    'actions': [{
      'action_name': 'convert_js',
      'variables': {
        'library_files': [
          'lib/info.js',
          'lib/io.js',
          'lib/prefs.js',
          'lib/utils.js',
          'lib/elemHideHitRegistration.js',
          'adblockplus/lib/filterNotifier.js',
          'adblockplus/lib/filterClasses.js',
          'adblockplus/lib/subscriptionClasses.js',
          'adblockplus/lib/filterStorage.js',
          'adblockplus/lib/elemHide.js',
          'adblockplus/lib/matcher.js',
          'adblockplus/lib/filterListener.js',
          'adblockplus/lib/synchronizer.js',
          'adblockplus/chrome/content/ui/subscriptions.xml',
        ],
        'load_before_files': [
          'lib/compat.js'
        ],
        'load_after_files': [
          'lib/api.js'
        ],
      },
      'inputs': [
        'convert_js.py',
        '<@(library_files)',
        '<@(load_before_files)',
        '<@(load_after_files)',
      ],
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/adblockplus.js.cpp'
      ],
      'msvs_quote_cmd': 0,
      'action': [
        '<(python)',
        'convert_js.py',
        '<@(_outputs)',
        '--before', '<@(load_before_files)',
        '--convert', '<@(library_files)',
        '--after', '<@(load_after_files)',
      ]
    }]
  },
  {
    'target_name': 'tests',
    'type': 'executable',
    'include_dirs': [
      'third_party/v8/include',
      'third_party/googletest/include'
    ],
    'dependencies': [
      'libadblockplus_solo',
      #'third_party/googletest.gyp:googletest_main',
    ],
    'sources': [
      'test/AppInfoJsObject.cpp',
      'test/ConsoleJsObject.cpp',
      'test/DefaultFileSystem.cpp',
      'test/FileSystemJsObject.cpp',
      'test/FilterEngine.cpp',
      'test/GlobalJsObject.cpp',
      'test/JsEngine.cpp',
      'test/JsValue.cpp',
      'test/Thread.cpp',
      'test/WebRequest.cpp'
    ]
  }]
}
