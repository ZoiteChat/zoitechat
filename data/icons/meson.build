icon_gen = find_program('generate_icons.py')
find_program('magick', required: true)

icondir = join_paths(get_option('datadir'), 'icons/hicolor')
custom_target('zoitechat_png',
  input: 'zoitechat.svg',
  output: 'net.zoite.Zoitechat.png',
  command: [icon_gen, '@INPUT@', '@OUTPUT@', '--size', '48'],
  install: true,
  install_dir: join_paths(icondir, '48x48/apps'),
)

if host_machine.system() == 'windows'
  custom_target('zoitechat_ico',
    input: 'zoitechat.svg',
    output: 'zoitechat.ico',
    command: [icon_gen, '@INPUT@', '@PRIVATE_DIR@/zoitechat.png', '--size', '48', '--ico', '@OUTPUT@'],
    build_by_default: true,
  )
endif

install_data(
  'zoitechat.svg',
  rename: 'net.zoite.Zoitechat.svg',
  install_dir: join_paths(icondir, 'scalable/apps')
)
