
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
        'pcx-buffer.c',
        'play-avt.c',
        'pcx-avt-state.c',
        'pcx-avt-command.c',
        'pcx-utf8.c',
]
play_avt = executable('play-avt', play_avt_src,
                      include_directories: configinc)
