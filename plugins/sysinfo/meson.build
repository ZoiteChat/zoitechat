sysinfo_sources = [
  'format.c',
  'sysinfo.c',
]

sysinfo_deps = [
  libgio_dep,
  zoitechat_plugin_dep,
]

sysinfo_includes = []
sysinfo_cargs = ['-DHAVE_CONFIG_H']

system = host_machine.system()
if system == 'linux' or system == 'gnu' or system.startswith('gnu/')
  if get_option('gtk-frontend')
    sysinfo_deps += dependency('gtk+-3.0', version: '>= 3.22')
  endif

  libpci = dependency('libpci', required: false, method: 'pkg-config')
  if libpci.found()
    sysinfo_deps += libpci
    sysinfo_cargs += '-DHAVE_LIBPCI'
  endif

  sysinfo_includes += 'unix'
  sysinfo_sources += 'unix/modern-backend.c'

elif system == 'freebsd'
  sysinfo_includes += ['shared', 'unix']
  sysinfo_sources += [
    'shared/df.c',
    'unix/backend.c',
    'unix/match.c',
    'unix/parse.c',
  ]

  libpci = dependency('libpci', required: false, method: 'pkg-config')
  if libpci.found()
    sysinfo_deps += libpci
    sysinfo_cargs += '-DHAVE_LIBPCI'
    sysinfo_sources += 'unix/pci.c'

    picidsdir = libpci.get_pkgconfig_variable('idsdir')
    pciids = join_paths(picidsdir, 'pci.ids')
    sysinfo_cargs += '-DPCIIDS_FILE="@0@"'.format(pciids)
  endif

  if get_option('gtk-frontend')
    sysinfo_deps += dependency('gtk+-3.0', version: '>= 3.22')
  endif

elif system == 'darwin'
  sysinfo_includes += 'shared'
  sysinfo_sources += 'shared/df.c'
  add_languages('objc')
  sysinfo_sources += 'osx/backend.m'

elif system == 'windows'
  cc = meson.get_compiler('c')
  sysinfo_deps += [
    cc.find_library('advapi32', required: true),
    cc.find_library('dxgi', required: true),
    cc.find_library('dxguid', required: true),
    cc.find_library('iphlpapi', required: true),
    cc.find_library('winmm', required: true),
  ]
  sysinfo_sources += 'win32/backend.c'
else
  error('sysinfo: Unknown system?')
endif

shared_module('sysinfo', sysinfo_sources,
  dependencies: sysinfo_deps,
  include_directories: [config_h_include, include_directories(sysinfo_includes)],
  c_args: sysinfo_cargs,
  install: true,
  install_dir: plugindir,
  name_prefix: '',
  vs_module_defs: 'sysinfo.def',
)
