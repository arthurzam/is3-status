project('is3-status', 'c',
    license: 'GPL3',
    version: '1.0',
    default_options: ['warning_level=3', 'b_ndebug=if-release'])

sources = [
  'src/cmd_date.c',
  'src/cmd_disk_usage.c',
  'src/cmd_eth.c',
  'src/ini_parser.c',
  'src/main.c',
  'src/networking.c',
  'src/vprint.c',
  'src/fdpoll.c'
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

if (get_option('sway').enabled())
    sources += 'src/cmd_sway_language.c'
endif

if (get_option('x11').enabled())
    deps += dependency('x11')
    sources += 'src/cmd_x11_language.c'
endif

if (get_option('tests'))
    add_project_arguments('-DTESTS', language : 'c')
endif

if (get_option('profile'))
    add_project_arguments('-DPROFILE', language : 'c')
endif

exe = executable('is3-status', sources, install: true, dependencies: deps)
