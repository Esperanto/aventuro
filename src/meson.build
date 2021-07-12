
test_utf8_src = [
        'pcx-utf8.c',
        'test-utf8.c',
]

test_utf8 = executable('test-utf8', test_utf8_src,
                       include_directories: configinc)
test('utf8', test_utf8)

avt_load_src = [
        'pcx-util.c',
        'pcx-file-error.c',
        'pcx-error.c',
        'pcx-avt.c',
        'pcx-avt-load.c',
        'pcx-buffer.c',
        'avt-load.c',
]
avt_load = executable('avt-load', avt_load_src,
                      include_directories: configinc)
