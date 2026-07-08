if get_option('plugin')
  subdir('pkgconfig')
endif

if get_option('gtk-frontend')
  subdir('icons')
  subdir('misc')
  subdir('man')

  # Fetch and build the offline manual so Help->Contents works without
  # connectivity. 'auto' skips with a warning when the docs cannot be
  # fetched (e.g. network-less distro builds, which ship docs themselves);
  # 'enabled' makes that a hard error.
  offline_docs_opt = get_option('offline-docs')
  if not offline_docs_opt.disabled()
    offline_docs_args = [
      '--repo', get_option('offline-docs-repo'),
      '--output', join_paths(meson.current_build_dir(), 'offline-docs'),
      '--stamp', '@OUTPUT@',
    ]
    if offline_docs_opt.enabled()
      offline_docs_args += ['--strict']
    endif
    custom_target('offline-docs',
      output: 'offline-docs.stamp',
      command: [find_program('python3'), files('misc/build_offline_docs.py')] + offline_docs_args,
      build_by_default: true,
    )
    meson.add_install_script(files('misc/install_offline_docs.py'),
      join_paths(meson.current_build_dir(), 'offline-docs'),
      offline_docs_dir,
    )
  endif
endif
