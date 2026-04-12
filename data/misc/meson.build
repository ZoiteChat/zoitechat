appdir = join_paths(get_option('datadir'), 'applications')
metainfodir = join_paths(get_option('datadir'), 'metainfo')
mimedir = join_paths(get_option('datadir'), 'mime', 'packages')
desktop_utils = find_program('desktop-file-validate', required: false)

if get_option('gtk-frontend')
  if get_option('install-appdata')
    zoitechat_appdata = i18n.merge_file(
      input: 'org.Zoitechat.appdata.xml.in',
      output: 'org.Zoitechat.appdata.xml',
      po_dir: '../../po',
      install: true,
      install_dir: metainfodir
    )

    appstreamcli = find_program('appstreamcli', required: false)
    if appstreamcli.found()
      test('Validate org.Zoitechat.appdata.xml', appstreamcli,
        args: ['validate', zoitechat_appdata]
      )
    endif
  endif

  desktop_conf = configuration_data()
  if dbus_glib_dep.found()
    desktop_conf.set('exec_command', 'zoitechat --existing %U')
  else
    desktop_conf.set('exec_command', 'zoitechat %U')
  endif

  desktop_file = configure_file(
    input: 'org.Zoitechat.desktop.in.in',
    output: 'org.Zoitechat.desktop.in',
    configuration: desktop_conf
  )

  zoitechat_desktop = i18n.merge_file(
    input: desktop_file,
    output: 'org.Zoitechat.desktop',
    po_dir: '../../po',
    type: 'desktop',
    install: true,
    install_dir: appdir
  )

  if desktop_utils.found()
    test('Validate org.Zoitechat.desktop', desktop_utils,
      args: [zoitechat_desktop]
    )
  endif
endif

if get_option('plugin')
  plugin_metainfo = []

  # FIXME: These should all get translated somewhere
  if get_option('with-checksum')
    plugin_metainfo += [
      ['Checksum', 'Calculates a checksum for all sent and recieved DCC files', 'MIT']
    ]
  endif

  if get_option('with-fishlim')
    plugin_metainfo += [
      ['Fishlim', 'Allows setting a key for encrypted conversations', 'MIT AND GPL-2.0+']
    ]
  endif

  if get_option('with-lua') != 'false'
    plugin_metainfo += [
      ['Lua', 'Provides a scripting interface in Lua', 'MIT']
    ]
  endif

  if get_option('with-perl') != 'false'
    plugin_metainfo += [
      ['Perl', 'Provides a scripting interface in Perl', 'GPL-2.0+']
    ]
  endif

  if get_option('with-python') != 'false'
    plugin_metainfo += [
      ['Python', 'Provides a scripting interface in Python', 'GPL-2.0+']
    ]
  endif

  if get_option('with-sysinfo')
    plugin_metainfo += [
      ['Sysinfo', 'Adds command to display system information', 'GPL-2.0+']
    ]
  endif

  foreach metainfo : plugin_metainfo
    name = metainfo[0]
    conf = configuration_data()
    conf.set('NAME', name)
    conf.set('SUMMARY', metainfo[1])
    conf.set('LICENSE', metainfo[2])

    plugin_appdata = configure_file(
      input: 'org.Zoitechat.Plugin.metainfo.xml.in',
      output: 'org.Zoitechat.Plugin.@0@.metainfo.xml'.format(name),
      configuration: conf,
      install_dir: get_option('install-plugin-metainfo') ? metainfodir : '',
    )

    if appstreamcli.found()
      test('Validate org.Zoitechat.Plugin.@0@.metainfo.xml'.format(name), appstreamcli,
        args: ['validate', plugin_appdata]
    )
    endif

  endforeach
endif
