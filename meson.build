project('is3-status', 'c',
    license: 'GPL3',
    version: '1.0',
    default_options: ['c_std=c11', 'warning_level=3', 'b_ndebug=if-release'])

sources = [
  'src/cmd_date.c',
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

needs_dbus = false

deps = [dependency('yajl')]

if (get_option('alsa').enabled())
    deps += dependency('alsa')
    sources += 'src/cmd_volume_alsa.c'
endif

if (get_option('battery').enabled())
    sources += 'src/cmd_battery.c'
endif

if (get_option('cpu-temperature').enabled())
    sources += 'src/cmd_cpu_temperature.c'
endif

if (get_option('disk-usage').enabled())
    sources += 'src/cmd_disk_usage.c'
endif

if (get_option('mpris').enabled())
    needs_dbus = true
    sources += 'src/cmd_mpris.c'
endif

if (get_option('load').enabled())
    sources += 'src/cmd_load.c'
endif

if (get_option('memory').enabled())
    sources += 'src/cmd_memory.c'
endif

if (get_option('run-watch').enabled())
    sources += 'src/cmd_run_watch.c'
endif

if (get_option('sway').enabled())
    sources += 'src/cmd_sway_language.c'
endif

if (get_option('systemd').enabled())
    needs_dbus = true
    sources += 'src/cmd_systemd_watch.c'
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

if (needs_dbus)
    deps += dependency('libsystemd', version : '>=221')
    sources += 'src/dbus_monitor.c'
endif

exe = executable('is3-status', sources, install: true, dependencies: deps, c_args: ['-D_GNU_SOURCE'])

scdoc = find_program('scdoc', native: true, required: get_option('man-pages'))
if scdoc.found()
    sh = find_program('sh', native: true)
    #ide:editable-filelist
    man_files = [
        'is3-status.1.scd',
    ]
    foreach filename : man_files
        topic = filename.split('.')[-3].split('/')[-1]
        section = filename.split('.')[-2]
        output = '@0@.@1@'.format(topic, section)

        custom_target(
            output,
            input: filename,
            output: output,
            command: [
                sh, '-c', '@0@ < @INPUT@ > @1@'.format(scdoc.path(), output)
            ],
            install: true,
            install_dir: '@0@/man@1@'.format(get_option('mandir'), section)
        )
    endforeach
endif
