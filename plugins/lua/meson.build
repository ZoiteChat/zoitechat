if cc.get_id() == 'msvc'
  lua_dep = cc.find_library('lua51')
else
  lua_opt = get_option('with-lua')
  lua_dep = dependency(lua_opt, required: false)
  if not lua_dep.found() and lua_opt == 'lua-5.4'
    foreach lua_name : ['lua5.4', 'lua-5.3', 'lua5.3', 'lua']
      lua_dep = dependency(lua_name, required: false)
      if lua_dep.found()
        break
      endif
    endforeach
  endif
  if not lua_dep.found()
    error('Dependency "' + lua_opt + '" not found')
  endif
endif

shared_module('lua', 'lua.c',
  dependencies: [libgio_dep, zoitechat_plugin_dep, lua_dep],
  install: true,
  install_dir: plugindir,
  name_prefix: '',
)
