project('clockperf', 'c',
    default_options: [
        'buildtype=release',
        'c_std=c99',
    ]
)

compiler = meson.get_compiler('c')


gen_build_h = custom_target('build.h',
                            input : [],
                            output : ['build.h'],
                            command : [meson.current_source_dir() + '/tools/build.pl', '@OUTPUT@'])

gen_license_h = custom_target('license.h',
                              input : ['COPYING'],
                              output : ['license.h'],
                              command : [meson.current_source_dir() + '/tools/license.pl', '@INPUT@', '@OUTPUT@'])

src = ['affinity.c', 'clock.c', 'drift.c', 'main.c', 'version.c']

incdirs = include_directories('.')
lm = compiler.find_library('m')
threads = dependency('threads')
openmp = dependency('openmp')

add_project_arguments(compiler.first_supported_argument('-Wno-deprecated-declarations'), language: 'c')

executable('clockperf',
           src,
           gen_build_h,
           gen_license_h,
           include_directories : incdirs,
           dependencies : [lm, threads, openmp])

# vim: set ts=4 sts=4 sw=4 et:
