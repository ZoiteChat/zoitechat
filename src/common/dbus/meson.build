dbus_deps = []

dbus_sources = [
  'dbus-plugin.c',
  'dbus-client.c'
]

dbus_includes = [
 include_directories('..'),
 config_h_include
]

dbus_service_dir = join_paths(get_option('datadir'), 'dbus-1/services')

if get_option('dbus-service-use-appid')
  dbus_service_name = 'io.github.Hexchat'
  dbus_output_file = 'io.github.Hexchat.service'
  dbus_cargs = '-DDBUS_SERVICE="io.github.Hexchat"'
else
  dbus_service_name = 'org.zoitechat.service'
  dbus_output_file = 'org.zoitechat.service.service'
  dbus_cargs = '-DDBUS_SERVICE="org.zoitechat.service"'
endif

dbus_conf = configuration_data()
dbus_conf.set('bindir', join_paths(get_option('prefix'), get_option('bindir')))
dbus_conf.set('service_name', dbus_service_name)
configure_file(
  input: 'org.zoitechat.service.service.in',
  output: dbus_output_file,
  configuration: dbus_conf,
  install: true,
  install_dir: dbus_service_dir
)

zoitechat_dbus = static_library('zoitechatdbus',
  sources: marshal + dbus_sources,
  c_args: dbus_cargs,
  dependencies: common_deps + dbus_deps,
  include_directories: dbus_includes,
  pic: true
)

zoitechat_dbus_dep = declare_dependency(
  link_with: zoitechat_dbus
)
