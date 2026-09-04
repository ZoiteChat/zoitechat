if get_option('plugin')
  subdir('pkgconfig')
endif

if get_option('gtk-frontend')
  # The bundled emoji font ships only with Windows builds; Linux and other
  # Unix systems use the system emoji font package instead.
  if host_machine.system() == 'windows'
    subdir('fonts')
  endif
  subdir('icons')
  subdir('misc')
  subdir('man')
endif
