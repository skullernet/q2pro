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

if not sdl2.found() and not cc.has_header_symbol('GL/glext.h', 'GL_VERSION_4_3', prefix: '#include <GL/gl.h>')
  warning('Neither SDL2 nor OpenGL 4.3 headers found, client will not be built')
  client_deps += disabler()
endif

subdir('video')

if not config.has('USE_SDL') and not config.has('USE_X11') and not config.has('USE_WAYLAND')
  warning('No video drivers enabled, client will not be built')
  client_deps += disabler()
endif

test_funcs = [
  'memccpy',
  'strchrnul',
  'strnlen',
]

foreach func: test_funcs
  if cc.has_function(func, args: '-D_GNU_SOURCE', prefix: '#include <string.h>')
    config.set('HAVE_' + func.to_upper(), true)
  endif
endforeach
