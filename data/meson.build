if get_option('plugin')
  subdir('pkgconfig')
endif

if get_option('gtk-frontend')
  subdir('icons')
  subdir('misc')
  subdir('man')
endif

offline_docs_url = get_option('offline-docs-url')
offline_docs = custom_target('offline-docs',
  output: 'offline-docs.stamp',
  command: [find_program('python3'), files('misc/fetch_offline_docs.py'), offline_docs_url, '@OUTDIR@', meson.source_root()],
  build_by_default: true,
)
meson.add_install_script(
  files('misc/install_offline_docs.py'),
  join_paths(meson.current_build_dir(), 'offline-docs'),
  offline_docs_dir,
)
