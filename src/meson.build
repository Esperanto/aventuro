
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
        'pcx-lexer.c',
        'pcx-parser.c',
        'pcx-load-or-parse.c',
]
play_avt = executable('play-avt', play_avt_src,
                      include_directories: configinc)

if meson.get_compiler('c', native: false).get_id() == 'emscripten'

  web_lib_funcs = [
    '_free',
    '_malloc',
    '_pcx_load_or_parse',
    '_pcx_avt_free',
    '_pcx_avt_state_new',
    '_pcx_avt_state_run_command',
    '_pcx_avt_state_get_next_message',
    '_pcx_avt_state_free',
    '_pcx_avt_state_game_is_over',
    '_pcx_avt_state_get_current_room_name',
    '_pcx_error_free',
  ]

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
          'pcx-lexer.c',
          'pcx-parser.c',
          'pcx-load-or-parse.c',
  ]
  web_lib = executable('avtlib.js', web_lib_src,
                     include_directories: configinc,
                     link_args: ['-s',
                                 'EXPORTED_FUNCTIONS=' +
                                 ','.join(web_lib_funcs),
                                 '-s', 'ALLOW_TABLE_GROWTH'],
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
        'pcx-lexer.c',
        'pcx-parser.c',
        'pcx-load-or-parse.c',
]
test_avt = executable('test-avt', test_avt_src,
                      include_directories: configinc)

test('rules', test_avt, args : files('tests/rules.avt', 'tests/rules.txt'))
test('burn', test_avt, args : files('tests/burn.avt', 'tests/burn.txt'))
test('contain', test_avt,
     args : files('tests/contain.avt', 'tests/contain.txt'))
test('multi-verb', test_avt,
     args : files('tests/multi-verb.avt', 'tests/multi-verb.txt'))
test('subroutine', test_avt,
     args : files('tests/subroutine.avt', 'tests/subroutine.txt'))
test('direction-rules', test_avt,
     args : files('tests/direction-rules.avt', 'tests/direction-rules.txt'))
test('extra-command-items', test_avt,
     args : files('tests/extra-command-items.avt',
                  'tests/extra-command-items.txt'))
test('new-actions', test_avt,
     args : files('tests/new-actions.avt', 'tests/new-actions.txt'))
test('aliases', test_avt,
     args : files('tests/aliases.avt', 'tests/aliases.txt'))
test('optional-adjective', test_avt,
     args : files('tests/optional-adjective.avt',
                  'tests/optional-adjective.txt'))

test_parser_src = [
        'pcx-util.c',
        'pcx-error.c',
        'pcx-avt.c',
        'pcx-buffer.c',
        'test-parser.c',
        'pcx-utf8.c',
        'pcx-list.c',
        'pcx-lexer.c',
        'pcx-parser.c',
        'pcx-avt-hat.c',
]
test_parser = executable('test-parser', test_parser_src,
                         include_directories: configinc)

test('parser', test_parser)
