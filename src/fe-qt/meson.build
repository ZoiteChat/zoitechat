# A separate executable sharing the ZoiteChat C core.
qt6_widgets = dependency('qt6', modules: ['Core', 'Gui', 'Widgets'], version: '>= 6.2')
qt_pango = dependency('pangocairo', version: '>= 1.44')
qt_menumarkup = static_library('qt-menumarkup', 'menumarkup.cpp', dependencies: [qt6_widgets, qt_pango])
qt_menumarkup_dep = declare_dependency(link_with: qt_menumarkup, dependencies: [qt6_widgets, qt_pango])
qt_resources = import('qt6').compile_resources(sources: 'icons.qrc')
qt_enchant = dependency('enchant-2', required: get_option('qt-spellchecking'))
qt_spell_args = []
if qt_enchant.found()
  qt_spell_args += '-DHAVE_QT_ENCHANT'
  if meson.get_compiler('c').has_function('enchant_broker_request_dict_with_pwl', prefix: '#include <enchant.h>', dependencies: qt_enchant)
    qt_spell_args += '-DHAVE_QT_ENCHANT_PWL'
  endif
endif
qt_spellchecker = static_library('qt-spellchecker', 'spellchecker.cpp', dependencies: [qt6_widgets, qt_enchant], cpp_args: qt_spell_args)
qt_spellchecker_dep = declare_dependency(link_with: qt_spellchecker, dependencies: [qt6_widgets, qt_enchant])
qt_spellchecker_test = executable('qt-spellchecker-test',
  sources: ['spellchecker-test.cpp', 'chatinput.cpp', 'irclineedit.cpp', 'ircformat.cpp'],
  dependencies: [qt6_widgets, qt_spellchecker_dep])
test('qt-spellchecker', qt_spellchecker_test, env: ['QT_QPA_PLATFORM=offscreen'])
zoitechat_qt = executable('zoitechat-qt',
  sources: [qt_resources, 'alerts-bridge.c', 'activity-bridge.c', 'keypress-bridge.c', 'command-bridge.c', 'completion-bridge.c', 'preferences-bridge.c', 'fe-qt.c', 'qtfrontend.cpp', 'chatbackground.cpp', 'buffersearch.cpp', 'ircformat.cpp', 'irclineedit.cpp', 'chatinput.cpp', 'nicklist.cpp',
            'network-bridge.c', 'networkdialog.cpp', 'tools-bridge.c', 'irctools.cpp', 'moderation-bridge.c', 'moderation.cpp', 'desktopintegration.cpp', 'urlhistory.cpp', 'friends-bridge.c', 'friends.cpp', 'pastepreview.cpp', 'customization-bridge.c', 'customization.cpp', 'textevents-bridge.c', 'textevents.cpp', 'paletteeditor.cpp', 'addons-bridge.c', 'addons.cpp', 'conversationoptions-bridge.c', 'transfers-bridge.c', 'transfers.cpp', 'characterchart.cpp', 'conversationoptions.cpp', 'keybindings.cpp'],
  dependencies: [zoitechat_common_dep, qt6_widgets, qt_spellchecker_dep, qt_menumarkup_dep],
  include_directories: config_h_include,
  c_args: ['-DHAVE_CONFIG_H'],
  # This frontend adapter uses C11; the original engine stays GNU89.
  override_options: ['c_std=gnu11'],
  install: true,
  win_subsystem: 'windows',
)

# Parser tests do not need the ZoiteChat engine or an IRC connection.
qt_ircformat_test = executable('qt-ircformat-test',
  sources: ['ircformat-test.cpp', 'ircformat.cpp'],
  dependencies: [qt6_widgets],
)
test('qt-ircformat', qt_ircformat_test, env: ['QT_QPA_PLATFORM=offscreen'])

# Validate that Qt's GLib dispatcher processes the default GMainContext.
qt_glib_dispatcher_test = executable('qt-glib-dispatcher-test',
  sources: ['glib-dispatcher-test.cpp'],
  dependencies: [qt6_widgets, dependency('glib-2.0')],
)
test('qt-glib-dispatcher', qt_glib_dispatcher_test, env: ['QT_QPA_PLATFORM=offscreen'])

qt_chatinput_test = executable('qt-chatinput-test',
  sources: ['chatinput-test.cpp', 'chatinput.cpp', 'irclineedit.cpp', 'ircformat.cpp'],
  dependencies: [qt6_widgets, qt_spellchecker_dep],
)
test('qt-chatinput', qt_chatinput_test, env: ['QT_QPA_PLATFORM=offscreen'])

qt_nicklist_test = executable('qt-nicklist-test',
  sources: [qt_resources, 'nicklist-test.cpp', 'nicklist.cpp'],
  dependencies: [qt6_widgets],
)
test('qt-nicklist', qt_nicklist_test, env: ['QT_QPA_PLATFORM=offscreen'])

# Exercise the real window against a fake engine, including modal-event races.
qt_frontend_test = executable('qt-frontend-test',
  sources: [qt_resources, 'frontend-test.cpp', 'chatbackground.cpp', 'buffersearch.cpp', 'ircformat.cpp', 'irclineedit.cpp', 'chatinput.cpp', 'nicklist.cpp', 'irctools.cpp', 'moderation.cpp', 'desktopintegration.cpp', 'urlhistory.cpp', 'friends.cpp', 'pastepreview.cpp', 'customization.cpp', 'textevents.cpp', 'paletteeditor.cpp', 'addons.cpp', 'transfers.cpp', 'characterchart.cpp', 'conversationoptions.cpp', 'keybindings.cpp'],
  dependencies: [qt6_widgets, qt_spellchecker_dep, qt_menumarkup_dep, dependency('glib-2.0')],
)
test('qt-frontend', qt_frontend_test, env: ['QT_QPA_PLATFORM=offscreen'], timeout: 30)

qt_irctools_test = executable('qt-irctools-test',
  sources: ['irctools-test.cpp', 'irctools.cpp', 'ircformat.cpp'],
  dependencies: [qt6_widgets],
)
test('qt-irctools', qt_irctools_test, env: ['QT_QPA_PLATFORM=offscreen'], timeout: 30)

qt_moderation_bridge_test = executable('qt-moderation-bridge-test',
  sources: ['moderation-bridge-test.c', 'moderation-bridge.c'],
  dependencies: [dependency('glib-2.0')],
  include_directories: config_h_include,
  c_args: ['-DHAVE_CONFIG_H'],
  override_options: ['c_std=gnu11'],
)
test('qt-moderation-bridge', qt_moderation_bridge_test)

qt_moderation_test = executable('qt-moderation-test',
  sources: ['moderation-test.cpp', 'moderation.cpp'],
  dependencies: [qt6_widgets],
)
test('qt-moderation', qt_moderation_test, env: ['QT_QPA_PLATFORM=offscreen'], timeout: 30)

qt_desktop_test = executable('qt-desktop-test',
  sources: ['desktopintegration-test.cpp', 'desktopintegration.cpp'],
  dependencies: [qt6_widgets],
)
test('qt-desktop', qt_desktop_test, env: ['QT_QPA_PLATFORM=offscreen'])

qt_urlhistory_test = executable('qt-urlhistory-test',
  sources: ['urlhistory-test.cpp', 'urlhistory.cpp'],
  dependencies: [qt6_widgets],
)
test('qt-urlhistory', qt_urlhistory_test, env: ['QT_QPA_PLATFORM=offscreen'], timeout: 30)

qt_friends_bridge_test = executable('qt-friends-bridge-test',
  sources: ['friends-bridge-test.c', 'friends-bridge.c'],
  dependencies: [dependency('glib-2.0')],
  include_directories: config_h_include,
  c_args: ['-DHAVE_CONFIG_H'],
  override_options: ['c_std=gnu11'],
)
test('qt-friends-bridge', qt_friends_bridge_test)

qt_friends_test = executable('qt-friends-test',
  sources: ['friends-test.cpp', 'friends.cpp'],
  dependencies: [qt6_widgets],
)
test('qt-friends', qt_friends_test, env: ['QT_QPA_PLATFORM=offscreen'])

qt_pastepreview_test = executable('qt-pastepreview-test',
  sources: ['pastepreview-test.cpp', 'pastepreview.cpp'],
  dependencies: [qt6_widgets],
)
test('qt-pastepreview', qt_pastepreview_test, env: ['QT_QPA_PLATFORM=offscreen'])

qt_customization_bridge_test = executable('qt-customization-bridge-test',
  sources: ['customization-bridge-test.c', 'customization-bridge.c'],
  dependencies: [dependency('gio-2.0'), qt_pango],
  include_directories: config_h_include,
  c_args: ['-DHAVE_CONFIG_H'],
  override_options: ['c_std=gnu11'],
)
test('qt-customization-bridge', qt_customization_bridge_test)

qt_customization_test = executable('qt-customization-test',
  sources: ['customization-test.cpp', 'customization.cpp'],
  dependencies: [qt6_widgets, qt_menumarkup_dep],
)
test('qt-customization', qt_customization_test, env: ['QT_QPA_PLATFORM=offscreen'], timeout: 30)

qt_textevents_test = executable('qt-textevents-test',
  sources: ['textevents-test.cpp', 'textevents.cpp', 'ircformat.cpp'],
  dependencies: [qt6_widgets],
)
test('qt-textevents', qt_textevents_test, env: ['QT_QPA_PLATFORM=offscreen'])

qt_textevents_bridge_test = executable('qt-textevents-bridge-test',
  sources: [textevents, 'textevents-bridge-test.c', 'textevents-bridge.c'],
  dependencies: [dependency('gio-2.0')],
  include_directories: [config_h_include, include_directories('../common')],
  c_args: ['-DHAVE_CONFIG_H'],
  override_options: ['c_std=gnu11'],
)
test('qt-textevents-bridge', qt_textevents_bridge_test)

qt_buffersearch_test = executable('qt-buffersearch-test',
  sources: ['buffersearch-test.cpp', 'buffersearch.cpp', 'ircformat.cpp'],
  dependencies: [qt6_widgets],
)
test('qt-buffersearch', qt_buffersearch_test, env: ['QT_QPA_PLATFORM=offscreen'])

qt_paletteeditor_test = executable('qt-paletteeditor-test',
  sources: ['paletteeditor-test.cpp', 'paletteeditor.cpp', 'ircformat.cpp'],
  dependencies: [qt6_widgets],
)
test('qt-paletteeditor', qt_paletteeditor_test, env: ['QT_QPA_PLATFORM=offscreen'])

qt_addons_test = executable('qt-addons-test',
  sources: ['addons-test.cpp', 'addons.cpp'], dependencies: [qt6_widgets],
)
test('qt-addons', qt_addons_test, env: ['QT_QPA_PLATFORM=offscreen'])
foreach enabled : [false, true]
  addon_test_args = ['-DHAVE_CONFIG_H', '-DQT_TEST_PLUGIN_ENABLED=' + (enabled ? '1' : '0')]
  qt_addons_bridge_test = executable('qt-addons-bridge-test-' + enabled.to_string(),
    sources: ['addons-bridge-test.c', 'addons-bridge.c'],
    dependencies: [dependency('gmodule-2.0')], include_directories: config_h_include,
    c_args: addon_test_args, override_options: ['c_std=gnu11'],
  )
  test('qt-addons-bridge-' + enabled.to_string(), qt_addons_bridge_test)
endforeach

qt_transfers_test = executable('qt-transfers-test',
  sources: ['transfers-test.cpp', 'transfers.cpp'], dependencies: [qt6_widgets],
)
test('qt-transfers', qt_transfers_test, env: ['QT_QPA_PLATFORM=offscreen'])
qt_transfers_bridge_test = executable('qt-transfers-bridge-test',
  sources: ['transfers-bridge-test.c', 'transfers-bridge.c'],
  dependencies: [dependency('glib-2.0')], include_directories: config_h_include,
  c_args: ['-DHAVE_CONFIG_H'], override_options: ['c_std=gnu11'],
)
test('qt-transfers-bridge', qt_transfers_bridge_test)

qt_characterchart_test = executable('qt-characterchart-test',
  sources: ['characterchart-test.cpp', 'characterchart.cpp'], dependencies: [qt6_widgets],
)
test('qt-characterchart', qt_characterchart_test, env: ['QT_QPA_PLATFORM=offscreen'])

qt_conversationoptions_test = executable('qt-conversationoptions-test',
  sources: ['conversationoptions-test.cpp', 'conversationoptions.cpp'], dependencies: [qt6_widgets],
)
test('qt-conversationoptions', qt_conversationoptions_test, env: ['QT_QPA_PLATFORM=offscreen'])
qt_conversationoptions_bridge_test = executable('qt-conversationoptions-bridge-test',
  sources: [textevents, 'conversationoptions-bridge-test.c', 'conversationoptions-bridge.c'],
  dependencies: [dependency('glib-2.0')], include_directories: [config_h_include, include_directories('../common')],
  c_args: ['-DHAVE_CONFIG_H'], override_options: ['c_std=gnu11'],
)
test('qt-conversationoptions-bridge', qt_conversationoptions_bridge_test)

qt_keybindings_test = executable('qt-keybindings-test',
  sources: ['keybindings-test.cpp', 'keybindings.cpp'], dependencies: [qt6_widgets],
)
test('qt-keybindings', qt_keybindings_test, env: ['QT_QPA_PLATFORM=offscreen'])

qt_preferences_bridge_test = executable('qt-preferences-bridge-test',
  sources: [textevents, 'preferences-bridge-test.c', 'preferences-bridge.c'],
  dependencies: [dependency('glib-2.0')],
  include_directories: [config_h_include, include_directories('../common')],
  c_args: ['-DHAVE_CONFIG_H'],
  override_options: ['c_std=gnu11'],
)
test('qt-preferences-bridge', qt_preferences_bridge_test)

qt_completion_bridge_test = executable('qt-completion-bridge-test',
  sources: ['completion-bridge-test.c', 'completion-bridge.c'],
  dependencies: [dependency('glib-2.0')],
  include_directories: config_h_include,
  c_args: ['-DHAVE_CONFIG_H'], override_options: ['c_std=gnu11'],
)
test('qt-completion-bridge', qt_completion_bridge_test)

qt_chatbackground_test = executable('qt-chatbackground-test',
  sources: ['chatbackground-test.cpp', 'chatbackground.cpp'], dependencies: [qt6_widgets],
)
test('qt-chatbackground', qt_chatbackground_test)

qt_command_bridge_test = executable('qt-command-bridge-test',
  sources: ['command-bridge-test.c', 'command-bridge.c'], dependencies: [dependency('glib-2.0')],
  include_directories: config_h_include, c_args: ['-DHAVE_CONFIG_H'], override_options: ['c_std=gnu11'],
)
test('qt-command-bridge', qt_command_bridge_test)

qt_alerts_bridge_test = executable('qt-alerts-bridge-test',
  sources: ['alerts-bridge-test.c', 'alerts-bridge.c'], dependencies: [dependency('glib-2.0')],
  include_directories: config_h_include, c_args: ['-DHAVE_CONFIG_H'], override_options: ['c_std=gnu11'],
)
test('qt-alerts-bridge', qt_alerts_bridge_test)

# Real loopback protocol tests, each with an isolated probe plugin and config.
if get_option('plugin') and host_machine.system() == 'linux' and not meson.is_cross_build()
  test('qt-irc-runtime', find_program('python3'),
    args: [meson.current_source_dir() / 'irc-runtime-test.py', zoitechat_qt],
    suite: 'runtime', timeout: 150, is_parallel: false,
  )
  test('qt-scripts-runtime', find_program('python3'),
    args: [meson.current_source_dir() / 'scripts-runtime-test.py', zoitechat_qt],
    suite: 'runtime', timeout: 30, is_parallel: false,
  )
  test('qt-dcc-runtime', find_program('python3'),
    args: [meson.current_source_dir() / 'dcc-runtime-test.py', zoitechat_qt],
    suite: 'runtime', timeout: 30, is_parallel: false,
  )
endif

qt_networkdialog_test = executable('qt-networkdialog-test', sources: ['networkdialog-test.cpp', 'networkdialog.cpp'], dependencies: [qt6_widgets])
test('qt-networkdialog', qt_networkdialog_test, env: ['QT_QPA_PLATFORM=offscreen'])

test('qt-startup-cli', find_program('python3'),
  args: [meson.current_source_dir() / 'startup-cli-test.py', zoitechat_qt], timeout: 30)

qt_activity_bridge_test = executable('qt-activity-bridge-test',
  sources: ['activity-bridge-test.c', 'activity-bridge.c'], dependencies: [dependency('glib-2.0')],
  include_directories: config_h_include, c_args: ['-DHAVE_CONFIG_H'], override_options: ['c_std=gnu11'],
)
test('qt-activity-bridge', qt_activity_bridge_test)

qt_keypress_bridge_test = executable('qt-keypress-bridge-test',
  sources: ['keypress-bridge-test.c', 'keypress-bridge.c'], dependencies: [dependency('glib-2.0')],
  include_directories: config_h_include, c_args: ['-DHAVE_CONFIG_H'], override_options: ['c_std=gnu11'],
)
test('qt-keypress-bridge', qt_keypress_bridge_test)

qt_menumarkup_test = executable('qt-menumarkup-test', sources: ['menumarkup-test.cpp'], dependencies: [qt_menumarkup_dep])
test('qt-menumarkup', qt_menumarkup_test, env: ['QT_QPA_PLATFORM=offscreen'])

qt_irclineedit_test = executable('qt-irclineedit-test', sources: ['irclineedit-test.cpp', 'irclineedit.cpp', 'ircformat.cpp'], dependencies: [qt6_widgets])
test('qt-irclineedit', qt_irclineedit_test, env: ['QT_QPA_PLATFORM=offscreen'])
