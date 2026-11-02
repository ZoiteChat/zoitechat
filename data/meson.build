if get_option('plugin')
  subdir('pkgconfig')
endif

if get_option('gtk-frontend')
  subdir('fonts')
  subdir('man')
endif

if get_option('gtk-frontend') or get_option('qt-frontend')
  subdir('icons')
  subdir('misc')
endif
