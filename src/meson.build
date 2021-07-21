
test_utf8_src = [
        'pcx-utf8.c',
        'test-utf8.c',
]

test_utf8 = executable('test-utf8', test_utf8_src,
                       include_directories: configinc)
test('utf8', test_utf8)

test_avt_command_src = [
        'pcx-utf8.c',
        'pcx-avt-command.c',
        'test-avt-command.c',
        'pcx-avt-hat.c',
]

test_avt_command = executable('test-avt-command', test_avt_command_src,
                              include_directories: configinc)
test('avt-command', test_avt_command)

play_avt_src = [
        'pcx-util.c',
        'pcx-file-error.c',
        'pcx-error.c',
        'pcx-avt.c',
        'pcx-avt-load.c',
        'pcx-avt-load-file.c',
        'pcx-buffer.c',
        'play-avt.c',
        'pcx-avt-state.c',
        'pcx-avt-command.c',
        'pcx-utf8.c',
        'pcx-list.c',
        'pcx-avt-hat.c',
]
play_avt = executable('play-avt', play_avt_src,
                      include_directories: configinc)

if meson.get_compiler('c', native: false).get_id() == 'emscripten'

  web_lib_src = [
          'pcx-util.c',
          'pcx-error.c',
          'pcx-avt.c',
          'pcx-avt-load.c',
          'pcx-buffer.c',
          'pcx-avt-state.c',
          'pcx-avt-command.c',
          'pcx-utf8.c',
          'pcx-list.c',
          'pcx-avt-hat.c',
  ]
  web_lib = executable('avtlib.js', web_lib_src,
                     include_directories: configinc,
                     link_args: ['-s', 'EXPORTED_FUNCTIONS=_pcx_avt_state_free,_pcx_avt_free,_pcx_avt_load,_pcx_avt_state_new,_pcx_avt_state_get_next_message,_pcx_avt_state_run_command,_malloc,_free', '-s', 'ALLOW_TABLE_GROWTH'],
                     install: true,
                     install_dir: get_option('datadir') / 'web')

endif

test_avt_src = [
        'pcx-util.c',
        'pcx-file-error.c',
        'pcx-error.c',
        'pcx-avt.c',
        'pcx-avt-load.c',
        'pcx-avt-load-file.c',
        'pcx-buffer.c',
        'test-avt.c',
        'pcx-avt-state.c',
        'pcx-avt-command.c',
        'pcx-utf8.c',
        'pcx-list.c',
        'pcx-avt-hat.c',
]
test_avt = executable('test-avt', test_avt_src,
                      include_directories: configinc)

test('rules', test_avt, args : files('tests/rules.avt', 'tests/rules.txt'))
