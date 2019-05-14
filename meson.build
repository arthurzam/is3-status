project('is3-status', 'c',
    license: 'GPL3',
    version: '1.0',
    default_options: ['warning_level=3'])

#ide:editable-filelist
sources = [
  'src/cmd_date.c',
  'src/cmd_eth.c',
  'src/cmd_x11_language.c',
  'src/ini_parser.c',
  'src/main.c',
  'src/networking.c',
  'src/vprint.c',
]

#ide:editable-filelist
scripts = [
    'scripts/gen-format.py'
]

pkgconfig = import('pkgconfig')
deps = [dependency('yajl')]

if (get_option('alsa').enabled())
    deps += dependency('alsa')
    sources += 'src/cmd_volume_alsa.c'
endif

deps += dependency('x11')

exe = executable('is3-status', sources, install: true, dependencies: deps)
