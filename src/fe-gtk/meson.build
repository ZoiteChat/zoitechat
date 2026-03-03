zoitechat_theme_sources = [
  'theme/theme-access.c',
  'theme/theme-application.c',
  'theme/theme-css.c',
  'theme/theme-manager.c',
  'theme/theme-palette.c',
  'theme/theme-preferences.c',
  'theme/theme-policy.c',
  'theme/theme-runtime.c',
]

zoitechat_gtk_sources = [
  'ascii.c',
  'banlist.c',
  'chanlist.c',
  'chanview.c',
  'custom-list.c',
  'dccgui.c',
  'editlist.c',
  'fe-gtk.c',
  'fkeys.c',
  'gtkutil.c',
  'icon-resolver.c',
  'ignoregui.c',
  'joind.c',
  'menu.c',
  'maingui.c',
  'notifygui.c',
  'pixmaps.c',
  'plugin-tray.c',
  'plugin-notification.c',
  'rawlog.c',
  'servlistgui.c',
  'setup.c',
  'sexy-spell-entry.c',
  'textgui.c',
  'urlgrab.c',
  'userlistgui.c',
  'xtext.c'
]

zoitechat_gtk_cflags = []

zoitechat_gtk_deps = [
  zoitechat_common_dep,
  libgmodule_dep, # used by libsexy
]

gtk_dep = dependency('gtk+-3.0', version: '>= 3.22')

zoitechat_theme_deps = [gtk_dep]

if host_machine.system() != 'windows'
  appindicator_opt = get_option('appindicator')

  if appindicator_opt.allowed()
    appindicator_dep = dependency('ayatana-appindicator3-0.1', required: false)
    if appindicator_dep.found()
      zoitechat_gtk_deps += appindicator_dep
      zoitechat_gtk_cflags += '-DHAVE_AYATANA_APPINDICATOR'
    else
      appindicator_dep = dependency('appindicator3-0.1', required: false)
      if appindicator_dep.found()
        zoitechat_gtk_deps += appindicator_dep
        zoitechat_gtk_cflags += '-DHAVE_APPINDICATOR'
      elif appindicator_opt.enabled()
        error('appindicator=enabled, but neither ayatana-appindicator3-0.1 nor appindicator3-0.1 was found')
      endif
    endif
  endif
endif

zoitechat_gtk_deps += gtk_dep

if host_machine.system() != 'windows'
  gdk_x11_dep = dependency('gdk-x11-3.0', required: false)
  if gdk_x11_dep.found()
    zoitechat_gtk_deps += gdk_x11_dep
  endif

  x11_dep = dependency('x11', required: false)
  if x11_dep.found()
    zoitechat_gtk_deps += x11_dep
  endif
endif

zoitechat_gtk_ldflags = []

if host_machine.system() == 'windows'
  zoitechat_gtk_sources += 'notifications/notification-windows.c'
  zoitechat_gtk_deps += cc.find_library('dwmapi', required: true)
  zoitechat_theme_deps += cc.find_library('dwmapi', required: true)

else
  zoitechat_gtk_sources += 'notifications/notification-freedesktop.c'
endif

zoitechat_theme_lib = static_library('zoitechat_theme',
  sources: zoitechat_theme_sources,
  include_directories: [config_h_include],
  dependencies: zoitechat_theme_deps,
  install: false,
)

iso_codes = dependency('iso-codes', required: false)
if iso_codes.found()
  zoitechat_gtk_sources += 'sexy-iso-codes.c'
  iso_codes_prefix = iso_codes.get_pkgconfig_variable('prefix')
  zoitechat_gtk_cflags += '-DISO_CODES_PREFIX="@0@"'.format(iso_codes_prefix)
  zoitechat_gtk_cflags += '-DISO_CODES_LOCALEDIR="@0@"'.format(
                        join_paths(iso_codes_prefix, 'share/locale'))
endif

if get_option('plugin')
  zoitechat_gtk_sources += 'plugingui.c'
endif

resources = gnome.compile_resources('resources',
  '../../data/zoitechat.gresource.xml',
  source_dir: '../../data',
  c_name: 'zoitechat',
  extra_args: ['--manual-register']
)

if host_machine.system() == 'windows'
  zoitechat_gtk_ldflags += '-Wl,-e,mainCRTStartup'
endif

executable('zoitechat',
  sources:  resources + zoitechat_gtk_sources,
  dependencies: zoitechat_gtk_deps,
  link_with: zoitechat_theme_lib,
  c_args: zoitechat_gtk_cflags,
  link_args: zoitechat_gtk_ldflags,
  pie: true,
  install: true,
  gui_app: true,
)

theme_manager_policy_tests = executable('theme_manager_policy_tests',
  [
    'theme/tests/test-theme-manager-policy.c',
    'theme/theme-manager.c',
    'theme/theme-palette.c',
  ],
  include_directories: [config_h_include],
  dependencies: [gtk_dep],
)

test('Theme Manager/Policy Tests', theme_manager_policy_tests,
  protocol: 'tap',
  timeout: 120,
)


theme_manager_dispatch_tests = executable('theme_manager_dispatch_routing_tests',
  [
    'theme/tests/test-theme-manager-dispatch-routing.c',
    'theme/theme-manager.c',
    'theme/theme-palette.c',
  ],
  include_directories: [config_h_include],
  dependencies: [gtk_dep],
)

test('Theme Manager Dispatch Routing Tests', theme_manager_dispatch_tests,
  protocol: 'tap',
  timeout: 120,
)

theme_manager_auto_refresh_tests = executable('theme_manager_auto_refresh_tests',
  [
    'theme/tests/test-theme-manager-auto-refresh.c',
    'theme/theme-manager.c',
    'theme/theme-palette.c',
  ],
  include_directories: [config_h_include],
  dependencies: [gtk_dep],
)

test('Theme Manager Auto Refresh Tests', theme_manager_auto_refresh_tests,
  protocol: 'tap',
  timeout: 120,
)

theme_application_input_style_tests = executable('theme_application_input_style_tests',
  [
    'theme/tests/test-theme-application-input-style.c',
    'theme/theme-application.c',
  ],
  include_directories: [config_h_include],
  dependencies: [gtk_dep],
)

test('Theme Application Input Style Tests', theme_application_input_style_tests,
  protocol: 'tap',
  timeout: 120,
)

theme_runtime_tests = executable('theme_runtime_persistence_tests',
  'theme/tests/test-theme-runtime-persistence.c',
  include_directories: [config_h_include],
  dependencies: [gtk_dep],
  link_with: zoitechat_theme_lib,
)

test('Theme Runtime Persistence Tests', theme_runtime_tests,
  protocol: 'tap',
  timeout: 120,
)

theme_access_tests = executable('theme_access_routing_tests',
  'theme/tests/test-theme-access-routing.c',
  include_directories: [config_h_include],
  dependencies: [gtk_dep],
  link_with: zoitechat_theme_lib,
)

test('Theme Access Routing Tests', theme_access_tests,
  protocol: 'tap',
  timeout: 120,
)
