common_src += files('system.c', 'hunk.c')

libm = cc.find_library('m', required: false)
if libm.found()
  common_deps += libm
  game_deps += libm
endif

librt = cc.find_library('rt', required: false)
if librt.found()
  common_deps += librt
endif

libdl = cc.find_library('dl', required: false)
if cc.has_function('dlopen', dependencies: libdl, prefix: '#include <dlfcn.h>')
  common_deps += libdl
endif

client_deps += dependency('threads')

if cc.has_header_symbol('sys/soundcard.h', 'SNDCTL_DSP_SETFMT',
                        required: get_option('oss').require(get_option('software-sound')))
  client_src += files('sound/oss.c')
  config.set10('USE_OSS', true)
endif

if not cc.has_header_symbol('GL/glext.h', 'GL_VERSION_4_3', prefix: '#include <GL/gl.h>')
  warning('OpenGL 4.3 headers not found, client will not be built')
  client_deps += disabler()
endif

subdir('video')

if not config.has('USE_SDL') and not config.has('USE_X11') and not config.has('USE_WAYLAND')
  warning('No video drivers enabled, client will not be built')
  client_deps += disabler()
endif
