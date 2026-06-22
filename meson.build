project('spifast', 'c',
    version: '2.0.0',
    default_options: [
        'buildtype=release',
        'c_std=gnu11',
        'warning_level=3',
        'b_lto=true'
    ]
)


# Try system-wide DPDK first
dpdk_dep = dependency('libdpdk', required: false)

if not dpdk_dep.found()
    # Try local DPDK in third_party/ using run_command to invoke pkg-config with local path
    local_pc_dir = join_paths(meson.current_source_dir(), 'third_party/dpdk-24.11/build/meson-uninstalled')
    pkgconf = find_program('pkg-config', required: true)
    
    cflags_res = run_command('env', 'PKG_CONFIG_PATH=' + local_pc_dir, 'pkg-config', '--cflags', 'libdpdk', check: true)
    libs_res = run_command('env', 'PKG_CONFIG_PATH=' + local_pc_dir, 'pkg-config', '--libs', 'libdpdk', check: true)
    
    dpdk_dep = declare_dependency(
        compile_args: cflags_res.stdout().strip().split(),
        link_args: libs_res.stdout().strip().split()
    )
endif

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
    c_args: ['-march=native', '-mtune=native', '-Ofast', '-funroll-loops', '-fomit-frame-pointer', '-falign-functions=64'],
    install: false
)

# Standalone CLI tool — no DPDK dependency
executable('spi_cli',
    files('src/spi_cli.c'),
    install: false
)

# Debug mode spifast data-plane application
executable('spifast_debug',
    sources,
    include_directories: inc,
    dependencies: dpdk_dep,
    c_args: ['-march=native', '-mtune=native', '-DDEBUG_MODE', '-Ofast', '-funroll-loops', '-fomit-frame-pointer', '-falign-functions=64'],
    install: false
)
