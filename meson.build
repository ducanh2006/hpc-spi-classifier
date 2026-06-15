project('spifast', 'c',
    version: '2.0.0',
    default_options: [
        'buildtype=release',
        'c_std=gnu11',
        'warning_level=3',
        'b_lto=true'
    ]
)

dpdk_dep = dependency('libdpdk', required: true)

inc = include_directories('src')

# Main spifast data-plane application
sources = files(
    'src/main.c',
    'src/stats.c',
    'src/matcher.c',
    'src/worker.c',
    'src/master.c',
    'src/control.c'
)

executable('spifast',
    sources,
    include_directories: inc,
    dependencies: dpdk_dep,
    c_args: ['-march=native', '-mtune=native'],
    install: false
)

# Standalone CLI tool — no DPDK dependency
executable('spi_cli',
    files('src/spi_cli.c'),
    install: false
)
