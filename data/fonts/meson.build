emoji_font_dir = join_paths(get_option('datadir'), 'fonts', 'zoitechat')
emoji_font_doc_dir = join_paths(get_option('datadir'), 'doc', 'zoitechat', 'fonts')

if get_option('install-emoji-font')
  install_data(
    'NotoColorEmoji.ttf',
    install_dir: emoji_font_dir,
  )

  install_data(
    'OFL.txt',
    'README.md',
    install_dir: emoji_font_doc_dir,
  )
endif
