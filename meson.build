project('spifast', 'c',
    version: '1.0.0',
    default_options: [
        'buildtype=release',
        'c_std=gnu11',
        'warning_level=3'
    ]
)

dpdk_dep = dependency('libdpdk', required: true)

inc = include_directories('src')

sources = files(
    'src/main.c',
    'src/stats.c',
    'src/parser.c',
    'src/matcher.c',
    'src/worker.c',
    'src/master.c'
)

executable('spifast',
    sources,
    include_directories: inc,
    dependencies: dpdk_dep,
    install: false
)
