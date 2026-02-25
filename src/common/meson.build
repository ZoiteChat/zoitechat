common_sources = [
  'cfgfiles.c',
  'chanopt.c',
  'ctcp.c',
  'dcc.c',
  'zoitechat.c',
  'history.c',
  'ignore.c',
  'inbound.c',
  'modes.c',
  'network.c',
  'notify.c',
  'outbound.c',
  'plugin.c',
  'plugin-identd.c',
  'plugin-timer.c',
  'proto-irc.c',
  'scram.c',
  'server.c',
  'servlist.c',
  'sts.c',
	'text.c',
  'tree.c',
  'url.c',
  'userlist.c',
  'util.c'
]

common_sysinfo_deps = []

common_deps = [
  libgio_dep,
  libcanberra_dep,
] + global_deps

common_includes = [
  config_h_include,
  include_directories('.')
]

common_cflags = [
  '-DHAVE_CONFIG_H',
]

if host_machine.system() == 'windows'
  common_deps += [
    cc.find_library('ws2_32'), # winsock
    cc.find_library('winmm'), # playsound
  ]
  common_sysinfo_deps += [
    cc.find_library('wbemuuid'), # sysinfo
  ]

  common_sources += 'sysinfo/win32/backend.c'
  common_includes += include_directories('sysinfo')
endif

glib_genmarshal = find_program(
  'glib-genmarshal',
  'glib-genmarshal-2.0',
  required: true,
)

marshal = [
  custom_target('marshal_h',
    input: 'marshalers.list',
    output: 'marshal.h',
    command: [
      glib_genmarshal,
      '--prefix=_zoitechat_marshal',
      '--header',
      '@INPUT@',
      '--output',
      '@OUTPUT@',
    ]
  ),
  custom_target('marshal_c',
    input: 'marshalers.list',
    output: 'marshal.c',
    command: [
      glib_genmarshal,
      '--prefix=_zoitechat_marshal',
      '--body',
      '@INPUT@',
      '--output',
      '@OUTPUT@',
    ]
  )
]

make_te = find_program('make-te.py')

textevents = custom_target('textevents',
  input: 'textevents.in',
  output: ['textevents.h', 'textenums.h'],
  command: [make_te, '@INPUT@', '@OUTPUT0@', '@OUTPUT1@']
)

if libssl_dep.found()
  common_sources += 'ssl.c'
  common_deps += libssl_dep
endif

if dbus_glib_dep.found()
  subdir('dbus')
  common_deps += zoitechat_dbus_dep
  common_includes += include_directories('dbus')
endif

if get_option('plugin')
  common_deps += libgmodule_dep
  install_headers('zoitechat-plugin.h')
endif

zoitechat_common = static_library('zoitechatcommon',
  sources: [textevents] + marshal + common_sources,
  include_directories: config_h_include,
  dependencies: common_deps + common_sysinfo_deps,
  c_args: common_cflags,
  pic: true
)

zoitechat_common_dep = declare_dependency(
  sources: [textevents] + marshal,
  link_with: zoitechat_common,
  include_directories: common_includes,
  compile_args: common_cflags,
  dependencies: libgio_dep,
)

zoitechat_plugin_dep = declare_dependency(
  include_directories: common_includes,
  compile_args: common_cflags,
  dependencies: global_deps,
)
