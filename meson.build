project('zoitechat', 'c',
  version: '2.18.0-pre1',
  meson_version: '>= 0.55.0',
  default_options: [
    'c_std=c17',
    'buildtype=debugoptimized',
    'warning_level=1',
  ]
)

i18n = import('i18n')
gnome = import('gnome')
cc = meson.get_compiler('c')


libgio_dep = dependency('gio-2.0', version: '>= 2.36.0')
libgmodule_dep = dependency('gmodule-2.0')

libcanberra_dep = dependency('libcanberra', version: '>= 0.22',
                             required: get_option('libcanberra'))
dbus_required = get_option('dbus')
if host_machine.system() == 'darwin'
  if dbus_required.enabled()
    warning('dbus-glib is not available on macOS; disabling DBus support')
  endif
  dbus_required = false
endif
dbus_glib_dep = dependency('dbus-glib-1', required: dbus_required)

global_deps = []
if cc.get_id() == 'msvc'
  libssl_dep = cc.find_library('libssl')
else
  libssl_dep = dependency('openssl', version: '>= 0.9.8',
                          required: get_option('tls'))
endif

darwin_arch_sanity_check = get_option('darwin-arch-sanity-check')
if host_machine.system() == 'darwin' and darwin_arch_sanity_check
  darwin_arch_sanity_check_env = run_command('sh', '-c', 'printf %s "$ZOITECHAT_DARWIN_ARCH_SANITY_CHECK"', check: false).stdout().strip().to_lower()
  if ['0', 'false', 'no', 'off'].contains(darwin_arch_sanity_check_env)
    darwin_arch_sanity_check = false
  endif
endif

if host_machine.system() == 'darwin' and darwin_arch_sanity_check
  cflags_env = run_command('sh', '-c', 'printf %s "$CFLAGS"', check: false).stdout().strip()
  ldflags_env = run_command('sh', '-c', 'printf %s "$LDFLAGS"', check: false).stdout().strip()
  pkgconfig_glib = run_command('pkg-config', '--libs-only-L', 'glib-2.0', check: false)

  targeting_x86_64 = cflags_env.contains('-arch x86_64') or ldflags_env.contains('-arch x86_64')
  targeting_arm64 = cflags_env.contains('-arch arm64') or ldflags_env.contains('-arch arm64')
  targeting_universal = targeting_x86_64 and targeting_arm64
  using_arm_homebrew = pkgconfig_glib.returncode() == 0 and pkgconfig_glib.stdout().contains('/opt/homebrew/')

  if targeting_x86_64 and not targeting_universal and using_arm_homebrew
    error('Detected x86_64 build flags (-arch x86_64) while pkg-config resolves glib from /opt/homebrew (arm64). ' +
          'Use arm64 build flags with /opt/homebrew, use a universal build (-arch arm64 -arch x86_64) with universal dependencies, or run an x86_64/Rosetta environment with an x86_64 dependency stack (typically /usr/local). ' +
          'To bypass this safety check, configure with -Ddarwin-arch-sanity-check=false or set ZOITECHAT_DARWIN_ARCH_SANITY_CHECK=0.')
  endif
endif

config_h = configuration_data()
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())
config_h.set_quoted('PACKAGE_NAME', meson.project_name())
config_h.set_quoted('GETTEXT_PACKAGE', 'zoitechat')
config_h.set_quoted('LOCALEDIR', join_paths(get_option('prefix'),
                                 get_option('datadir'), 'locale'))
config_h.set_quoted('G_LOG_DOMAIN', 'zoitechat')
config_h.set10('ENABLE_NLS', true)

# Optional features
config_h.set('USE_OPENSSL', libssl_dep.found())
config_h.set('USE_LIBCANBERRA', libcanberra_dep.found())
config_h.set('USE_DBUS', dbus_glib_dep.found())
config_h.set('USE_PLUGIN', get_option('plugin'))

config_h.set('G_DISABLE_SINGLE_INCLUDES', true)
config_h.set('GTK_DISABLE_DEPRECATED', true)
config_h.set('GTK_DISABLE_SINGLE_INCLUDES', true)
config_h.set('GDK_PIXBUF_DISABLE_SINGLE_INCLUDES', true)
config_h.set('GLIB_VERSION_MAX_ALLOWED', 'GLIB_VERSION_2_36')
config_h.set('GLIB_VERSION_MIN_REQUIRED', 'GLIB_VERSION_2_36')

# Detected features
config_h.set('HAVE_MEMRCHR', cc.has_function('memrchr'))
config_h.set('HAVE_STRINGS_H', cc.has_header('strings.h'))

config_h.set_quoted('ZOITECHATLIBDIR',
  join_paths(get_option('prefix'), get_option('libdir'), 'zoitechat/plugins')
)

if libssl_dep.found()
  config_h.set('HAVE_X509_GET_SIGNATURE_NID',
    cc.has_function('X509_get_signature_nid', dependencies: libssl_dep)
  )
  config_h.set('HAVE_SSL_CTX_GET_SSL_METHOD',
    cc.has_function('SSL_CTX_get_ssl_method', dependencies: libssl_dep)
  )
  config_h.set('HAVE_DH_SET0_PQG',
    cc.has_function('DH_set0_pqg', dependencies: libssl_dep)
  )
  config_h.set('HAVE_DH_GET0_KEY',
    cc.has_function('DH_get0_key', dependencies: libssl_dep)
  )
  config_h.set('HAVE_DH_SET0_KEY',
    cc.has_function('DH_set0_key', dependencies: libssl_dep)
  )
  config_h.set('HAVE_ERR_REMOVE_THREAD_STATE',
    cc.has_function('ERR_remove_thread_state', dependencies: libssl_dep)
  )
  config_h.set('HAVE_ASN1_STRING_GET0_DATA',
    cc.has_function('ASN1_STRING_get0_data', dependencies: libssl_dep)
  )
endif

configure_file(output: 'config.h', configuration: config_h)
config_h_include = include_directories('.')

if host_machine.system() == 'windows'
  add_project_arguments(
    '-DWIN32',
    '-DNTDDI_VERSION=NTDDI_WIN7',
    '-D_WIN32_WINNT=_WIN32_WINNT_WIN7',
     language: 'c')
endif


global_cflags = []
test_cflags = [
  '-funsigned-char',
  '-Wno-conversion',
  '-Wno-pointer-sign',
  '-Wno-padded',
  '-Wno-unused-parameter',
  '-Wno-missing-prototypes',
  '-Winline',
  '-Wstrict-prototypes',
  '-Werror=implicit-function-declaration',
  '-Werror=pointer-arith',
  '-Werror=init-self',
  ['-Werror=format-security', '-Werror=format=1'],
  '-Werror=missing-include-dirs',
  '-Werror=date-time',
]
foreach cflag : test_cflags
  if cc.has_multi_arguments(cflag)
    global_cflags += cflag
  endif
endforeach
if get_option('buildtype') != 'plain'
  if cc.has_argument('-fstack-protector-strong') and cc.links('''
     int main (void) {
       char buffer[16];
       strcpy(buffer, "foo");
       return 0;
     }
     ''', args: '-fstack-protector-all')
    global_cflags += '-fstack-protector-strong'

    if host_machine.system() == 'windows'
      global_deps += cc.find_library('ssp')
    endif
  endif
endif
add_project_arguments(global_cflags, language: 'c')


global_ldflags = []
test_ldflags = [
  '-Wl,-z,relro',
  '-Wl,-z,now',
  # mingw
  '-Wl,--nxcompat',
]
if not (host_machine.system() == 'windows' and get_option('debug'))
 test_ldflags += '-Wl,--dynamicbase'
endif
foreach ldflag : test_ldflags
  if meson.version().version_compare('>= 0.55.0')
    has_arg = cc.has_link_argument(ldflag)
  else
    has_arg = cc.has_argument(ldflag)
  endif

  if has_arg and cc.links('int main (void) { return 0; }', args: ldflag)
    global_ldflags += ldflag
  endif
endforeach
add_project_link_arguments(global_ldflags, language: 'c')

subdir('src')
if get_option('plugin')
  subdir('plugins')
endif
if cc.get_id() != 'msvc'
  subdir('data')
  subdir('po') # FIXME: build xgettext

  meson.add_install_script('meson_post_install.py')
endif

if meson.version().version_compare('>= 0.55.0')
  summary({
    'prefix': get_option('prefix'),
    'bindir': get_option('bindir'),
    'libdir': get_option('libdir'),
    'datadir': get_option('datadir'),
  }, section: 'Directories')

  summary({
    'TLS (openssl)': libssl_dep.found(),
    'Plugin Support': get_option('plugin'),
    'DBus Support': dbus_glib_dep.found(),
    'libcanberra': libcanberra_dep.found(),
  }, section: 'Features')

  summary({
    'Lua': get_option('with-lua'),
    'Python': get_option('with-python'),
    'Perl': get_option('with-perl'),
    'Perl Legacy API': get_option('with-perl-legacy-api'),
    'FiSH': get_option('with-fishlim'),
    'Sysinfo': get_option('with-sysinfo'),
    'DCC Checksum': get_option('with-checksum'),
  }, section: 'Plugins')
endif
