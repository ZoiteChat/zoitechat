plugindir = join_paths(get_option('libdir'), 'zoitechat/plugins')

if host_machine.system() == 'windows'
  if get_option('with-exec')
    subdir('exec')
  endif

  if get_option('with-upd')
    subdir('upd')
  endif

  if get_option('with-winamp')
    subdir('winamp')
  endif
endif

if get_option('with-checksum')
  subdir('checksum')
endif

if get_option('with-fishlim')
  subdir('fishlim')
endif

lua_option = get_option('with-lua')
if lua_option == 'auto' or lua_option == 'luajit'
  lua_auto_dep = dependency('luajit', required: false)
  if not lua_auto_dep.found()
    lua_auto_dep = dependency('lua-5.4', required: false)
  endif
  if not lua_auto_dep.found()
    lua_auto_dep = dependency('lua5.4', required: false)
  endif
  if not lua_auto_dep.found()
    lua_auto_dep = dependency('lua-5.3', required: false)
  endif
  if not lua_auto_dep.found()
    lua_auto_dep = dependency('lua5.3', required: false)
  endif
  if not lua_auto_dep.found()
    lua_auto_dep = dependency('lua', required: false)
  endif

  if lua_auto_dep.found()
    subdir('lua')
  else
    warning('LuaJIT/Lua development files not found, disabling Lua plugin')
  endif
elif lua_option != 'false'
  subdir('lua')
endif

if get_option('with-perl') != 'false'
  subdir('perl')
endif

if get_option('with-python') != 'false'
  subdir('python')
endif

if get_option('with-sysinfo')
  subdir('sysinfo')
endif
