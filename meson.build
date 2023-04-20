project('q2pro', 'c',
  license: 'GPL-2.0-or-later',
  version: run_command(find_program('python3'), 'version.py', check: true).stdout().strip(),
  meson_version: '>= 0.59.0',
  default_options: [
    'c_std=gnu99',
    'buildtype=debugoptimized',
  ],
)

common_src = [
  'src/common/bsp.c',
  'src/common/cmd.c',
  'src/common/cmodel.c',
  'src/common/common.c',
  'src/common/cvar.c',
  'src/common/error.c',
  'src/common/field.c',
  'src/common/fifo.c',
  'src/common/files.c',
  'src/common/math.c',
  'src/common/mdfour.c',
  'src/common/msg.c',
  'src/common/net/chan.c',
  'src/common/net/net.c',
  'src/common/pmove.c',
  'src/common/prompt.c',
  'src/common/sizebuf.c',
  'src/common/utils.c',
  'src/common/zone.c',
  'src/shared/shared.c',
]

client_src = [
  'src/client/ascii.c',
  'src/client/cin.c',
  'src/client/console.c',
  'src/client/crc.c',
  'src/client/demo.c',
  'src/client/download.c',
  'src/client/effects.c',
  'src/client/entities.c',
  'src/client/input.c',
  'src/client/keys.c',
  'src/client/locs.c',
  'src/client/main.c',
  'src/client/newfx.c',
  'src/client/parse.c',
  'src/client/precache.c',
  'src/client/predict.c',
  'src/client/refresh.c',
  'src/client/screen.c',
  'src/client/sound/main.c',
  'src/client/sound/mem.c',
  'src/client/tent.c',
  'src/client/view.c',
  'src/common/async.c',
  'src/server/commands.c',
  'src/server/entities.c',
  'src/server/ghud.c',
  'src/server/game.c',
  'src/server/init.c',
  'src/server/main.c',
  'src/server/send.c',
  'src/server/user.c',
  'src/server/world.c',
  'src/shared/m_flash.c',
]

ui_src = [
  'src/client/ui/demos.c',
  'src/client/ui/menu.c',
  'src/client/ui/playerconfig.c',
  'src/client/ui/playermodels.c',
  'src/client/ui/script.c',
  'src/client/ui/servers.c',
  'src/client/ui/ui.c',
]

refresh_src = [
  'src/refresh/draw.c',
  'src/refresh/hq2x.c',
  'src/refresh/images.c',
  'src/refresh/legacy.c',
  'src/refresh/main.c',
  'src/refresh/mesh.c',
  'src/refresh/models.c',
  'src/refresh/qgl.c',
  'src/refresh/shader.c',
  'src/refresh/sky.c',
  'src/refresh/state.c',
  'src/refresh/surf.c',
  'src/refresh/tess.c',
  'src/refresh/texture.c',
  'src/refresh/world.c',
]

server_src = [
  'src/client/null.c',
  'src/server/commands.c',
  'src/server/entities.c',
  'src/server/ghud.c',
  'src/server/game.c',
  'src/server/init.c',
  'src/server/main.c',
  'src/server/send.c',
  'src/server/user.c',
  'src/server/world.c',
]

game_src = [
  'src/baseq2/g_ai.c',
  'src/baseq2/g_chase.c',
  'src/baseq2/g_cmds.c',
  'src/baseq2/g_combat.c',
  'src/baseq2/g_func.c',
  'src/baseq2/g_items.c',
  'src/baseq2/g_main.c',
  'src/baseq2/g_misc.c',
  'src/baseq2/g_monster.c',
  'src/baseq2/g_phys.c',
  'src/baseq2/g_ptrs.c',
  'src/baseq2/g_save.c',
  'src/baseq2/g_spawn.c',
  'src/baseq2/g_svcmds.c',
  'src/baseq2/g_target.c',
  'src/baseq2/g_trigger.c',
  'src/baseq2/g_turret.c',
  'src/baseq2/g_utils.c',
  'src/baseq2/g_weapon.c',
  'src/baseq2/m_actor.c',
  'src/baseq2/m_berserk.c',
  'src/baseq2/m_boss2.c',
  'src/baseq2/m_boss31.c',
  'src/baseq2/m_boss32.c',
  'src/baseq2/m_boss3.c',
  'src/baseq2/m_brain.c',
  'src/baseq2/m_chick.c',
  'src/baseq2/m_flipper.c',
  'src/baseq2/m_float.c',
  'src/baseq2/m_flyer.c',
  'src/baseq2/m_gladiator.c',
  'src/baseq2/m_gunner.c',
  'src/baseq2/m_hover.c',
  'src/baseq2/m_infantry.c',
  'src/baseq2/m_insane.c',
  'src/baseq2/m_medic.c',
  'src/baseq2/m_move.c',
  'src/baseq2/m_mutant.c',
  'src/baseq2/m_parasite.c',
  'src/baseq2/m_soldier.c',
  'src/baseq2/m_supertank.c',
  'src/baseq2/m_tank.c',
  'src/baseq2/p_client.c',
  'src/baseq2/p_hud.c',
  'src/baseq2/p_trail.c',
  'src/baseq2/p_view.c',
  'src/baseq2/p_weapon.c',
  'src/shared/m_flash.c',
  'src/shared/shared.c',
]

action_src = [
  'src/action/a_cmds.c',
  'src/action/a_ctf.c',
  'src/action/a_dom.c',
  'src/action/a_doorkick.c',
  'src/action/a_game.c',
  'src/action/a_items.c',
  'src/action/a_match.c',
  'src/action/a_menu.c',
  'src/action/a_radio.c',
  'src/action/a_team.c',
  'src/action/a_tourney.c',
  'src/action/a_vote.c',
  'src/action/a_xcmds.c',
  'src/action/a_xgame.c',
  'src/action/a_xmenu.c',
  'src/action/a_xvote.c',
  'src/action/cgf_sfx_glass.c',
  'src/action/g_chase.c',
  'src/action/g_cmds.c',
  'src/action/g_combat.c',
  'src/action/g_ext.c',
  'src/action/g_func.c',
  'src/action/g_grapple.c',
  'src/action/g_items.c',
  'src/action/g_main.c',
  'src/action/g_misc.c',
  'src/action/g_phys.c',
  'src/action/g_save.c',
  'src/action/g_spawn.c',
  'src/action/g_svcmds.c',
  'src/action/g_target.c',
  'src/action/g_trigger.c',
  'src/action/g_utils.c',
  'src/action/g_weapon.c',
  'src/action/p_antilag.c',
  'src/action/p_client.c',
  'src/action/p_hud.c',
  'src/action/p_view.c',
  'src/action/p_weapon.c',
  'src/action/tng_balancer.c',
  'src/action/tng_flashlight.c',
  'src/action/tng_ini.c',
  'src/action/tng_irc.c',
  'src/action/tng_jump.c',
  'src/action/tng_stats.c',
  'src/shared/shared.c'
]

ltk_src = [
  'src/action/acesrc/acebot_ai.c',
  'src/action/acesrc/acebot_cmds.c',
  'src/action/acesrc/acebot_compress.c',
  'src/action/acesrc/acebot_items.c',
  'src/action/acesrc/acebot_movement.c',
  'src/action/acesrc/acebot_nodes.c',
  'src/action/acesrc/acebot_spawn.c',
  'src/action/acesrc/bot_ai.c',
  'src/action/acesrc/bot_collision.c',
  'src/action/acesrc/bot_combat.c',
  'src/action/acesrc/bot_env_doors.c',
  'src/action/acesrc/bot_movement.c',
  'src/action/acesrc/bot_states.c',
  'src/action/acesrc/bot_utility.c',
  'src/action/acesrc/bot_weapon.c',
  'src/action/acesrc/botchat.c',
  'src/action/acesrc/botnav.c',
  'src/action/acesrc/botscan.c'
]

cc = meson.get_compiler('c')

win32 = host_machine.system() == 'windows'
x86 = host_machine.cpu_family() == 'x86'

cpuremap = {
  'x86': win32 ? 'x86' : 'i386',
  'aarch64': 'arm64',
}

cpu = host_machine.cpu_family()
if cpu in cpuremap
  cpu = cpuremap[cpu]
endif

if win32
  sys = cpu.endswith('64') ? 'Win64' : 'Win32'
else
  sys = host_machine.system()
  if sys.endswith('bsd')
    sys = sys.substring(0, -3) + sys.substring(-3).to_upper()
  endif
  sys = sys[0].to_upper() + sys.substring(1)
endif

datadir = '.'
libdir = '.'
homedir = ''

system_wide = not win32 and get_option('system-wide')
if system_wide
  datadir = get_option('prefix') / get_option('datadir') / meson.project_name()
  libdir = get_option('prefix') / get_option('libdir') / meson.project_name()
  homedir = get_option('homedir')
endif

common_args = ['-DHAVE_CONFIG_H']
if not win32
  common_args += '-D_GNU_SOURCE'
endif

engine_args = []
if win32
  engine_args += '-D_WIN32_WINNT=0x0601'
endif

common_link_args = []
exe_link_args = []
dll_link_args = []

if cc.get_argument_syntax() == 'gcc'
  if x86
    add_global_arguments('-msse2', '-mfpmath=sse', language: 'c')
  endif

  test_args = [
    '-fsigned-char',
    '-Wpointer-arith',
    '-Wformat-security',
    '-Werror=vla',
    '-Wstrict-prototypes',
  ]

  common_args += cc.get_supported_arguments(test_args)
  engine_args += cc.get_supported_arguments(['-Wmissing-prototypes'])

  if win32
    common_args += '-D__USE_MINGW_ANSI_STDIO=1'
    common_link_args += '-Wl,--nxcompat,--dynamicbase'

    if cpu == 'x86_64'
      exe_link_args += '-Wl,--high-entropy-va,--image-base=0x140000000'
      dll_link_args += '-Wl,--high-entropy-va,--image-base=0x180000000'
    endif

    dll_link_args += '-static-libgcc'
  endif
elif cc.get_id() == 'msvc'
  common_args += ['/D_USE_MATH_DEFINES', '/wd4146', '/wd4244', '/wd4305']
endif

add_project_arguments(common_args, language: 'c')
add_project_link_arguments(common_link_args, language: 'c')

config = configuration_data()

fallback_opt = ['default_library=static']

zlib = dependency('zlib',
  required:        get_option('zlib'),
  default_options: fallback_opt,
)

if not zlib.found()
  warning('zlib not found, client will be unable to connect to protocol 35 servers')
endif

png = dependency('libpng',
  required:        get_option('libpng'),
  default_options: fallback_opt,
)

config.set10('USE_ZLIB', zlib.found())
config.set10('USE_PNG', png.found())

curl = dependency('libcurl',
  version:         '>= 7.68.0',
  required:        get_option('libcurl'),
  default_options: fallback_opt,
)

if curl.found()
  client_src += 'src/client/http.c'
  config.set10('USE_CURL', true)
endif

sdl2 = dependency('sdl2', required: get_option('sdl2'))
if sdl2.found()
  client_src += 'src/unix/video/sdl.c'
  config.set('USE_SDL', 'USE_CLIENT')
endif

common_deps = [zlib]
client_deps = [png, curl, sdl2]
server_deps = []
game_deps = [zlib]

jpeg = dependency('libjpeg',
  required:        get_option('libjpeg'),
  default_options: fallback_opt,
)

if jpeg.found()
  if jpeg.type_name() == 'internal'
    has_rgba = true
  else
    has_rgba = cc.has_header_symbol('jpeglib.h', 'JCS_EXT_RGBA',
      prefix:       '#include <stdio.h>',
      dependencies: jpeg,
      required:     get_option('libjpeg'))
  endif
  if has_rgba
    client_deps += jpeg
    config.set10('USE_JPG', true)
  endif
endif

openal = dependency('openal',
  required:        get_option('openal'),
  default_options: fallback_opt,
)
if openal.found()
  client_src += [ 'src/client/sound/al.c', 'src/client/sound/qal.c' ]
  client_deps += openal.partial_dependency(compile_args: true, includes: true)
  config.set10('USE_OPENAL', true)
endif

if not win32
  uuid = dependency('uuid',
    required:        get_option('uuid'),
    allow_fallback:  win32,
    default_options: fallback_opt,
  )
  game_deps += uuid
endif

if host_machine.system() == 'darwin'
  opengl = dependency('opengl',
    allow_fallback:  win32,
    default_options: fallback_opt,
  )
  client_deps += opengl
endif

ogg = dependency('ogg',
  required:        get_option('ogg'),
  default_options: fallback_opt,
)
vorbisfile = dependency('vorbisfile',
  required:        get_option('ogg'),
  default_options: fallback_opt,
)
if ogg.found() and vorbisfile.found()
  client_src += 'src/client/sound/ogg.c'
  client_deps += ogg.partial_dependency(compile_args: true, includes: true)
  client_deps += vorbisfile
  config.set10('USE_OGG', true)
endif

if win32
  subdir('src/windows')
else
  subdir('src/unix')
endif

if get_option('client-gtv')
  client_src += 'src/client/gtv.c'
  config.set10('USE_CLIENT_GTV', true)
endif

if get_option('client-ui')
  client_src += ui_src
  config.set10('USE_UI', true)
  if system_wide
    install_data('src/client/ui/q2pro.menu', install_dir: datadir / get_option('base-game'))
  endif
endif

if get_option('software-sound')
  client_src += 'src/client/sound/dma.c'
  if sdl2.found()
    client_src += 'src/unix/sound/sdl.c'
  endif
  config.set10('USE_SNDDMA', true)
elif not openal.found()
  warning('Neither software sound nor OpenAL enabled, this is not supported')
endif

if get_option('anticheat-server')
  server_src += 'src/server/ac.c'
  config.set('USE_AC_SERVER', 'USE_SERVER')
endif

if get_option('mvd-server')
  common_src += 'src/server/mvd.c'
  config.set10('USE_MVD_SERVER', true)
endif

if get_option('mvd-client')
  common_src += [
    'src/server/mvd/client.c',
    'src/server/mvd/game.c',
    'src/server/mvd/parse.c'
  ]
  config.set10('USE_MVD_CLIENT', true)
endif

if get_option('save-games')
  common_src += 'src/server/save.c'
  config.set10('USE_SAVEGAMES', true)
endif

if get_option('aqtion-build')
  config.set10('USE_AQTION', true)
  config.set10('AQTION_EXTENSION', true)
  config.set10('AQTION_HUD', true)
  action_src += ltk_src

  # Win32 winsock compat
  if win32
    dll_link_args += '-lws2_32'
  endif
endif

if get_option('discord-sdk')
  #if host_machine.system() == 'linux'
  if host_machine.cpu_family() == 'aarch64' # I am Linux ARM 64-bit
    # Discord does not have an SDK for this platform yet, setting USE_DISCORD to false
    config.set10('USE_DISCORD', false)
  else
    config.set10('USE_DISCORD', true)
    discord_inc_path = join_paths('extern/discord/lib/', host_machine.cpu_family())
    dis_inc = include_directories(discord_inc_path)
    discord_dep_path = join_paths(meson.current_source_dir(), discord_inc_path)
    discord_dep = cc.find_library('discord_game_sdk', dirs : discord_dep_path)
    client_deps += discord_dep
  endif
  #endif
endif

if get_option('system-console')
  if not win32
    common_src += 'src/unix/tty.c'
  endif
  config.set10('USE_SYSCON', true)
endif

if get_option('tests')
  common_src += 'src/common/tests.c'
  config.set10('USE_TESTS', true)
endif

executable('q2pro', common_src, client_src, refresh_src,
  dependencies:          common_deps + client_deps,
  include_directories:   'inc',
  gnu_symbol_visibility: 'hidden',
  win_subsystem:         'windows,6.0',
  link_args:             exe_link_args,
  c_args:                ['-DUSE_CLIENT=1', '-DUSE_REF=1', engine_args],
  install:               system_wide,
)

executable('q2proded', common_src, server_src,
  dependencies:          common_deps + server_deps,
  include_directories:   'inc',
  gnu_symbol_visibility: 'hidden',
  win_subsystem:         'console,6.0',
  link_args:             exe_link_args,
  c_args:                ['-DUSE_SERVER=1', engine_args],
  install:               system_wide,
)

shared_library('game' + cpu, action_src,
  name_prefix:           '',
  dependencies:          game_deps,
  include_directories:   'inc',
  gnu_symbol_visibility: 'hidden',
  link_args:             dll_link_args,
  install:               system_wide,
  install_dir:           libdir / get_option('base-game'),
  override_options:      get_option('game-build-options'),
)

config.set('REVISION',            meson.project_version().substring(1).split('~')[0].to_int())
config.set_quoted('VERSION',      meson.project_version())
config.set_quoted('CPUSTRING',    cpu)
config.set_quoted('BUILDSTRING',  sys)
config.set_quoted('BASEGAME',     get_option('base-game'))
config.set_quoted('DEFGAME',      get_option('default-game'))
if not win32
  config.set_quoted('DATADIR',    datadir)
  config.set_quoted('LIBDIR',     libdir)
  config.set_quoted('HOMEDIR',    homedir)
endif
config.set_quoted('VID_GEOMETRY', get_option('vid-geometry'))
config.set_quoted('VID_MODELIST', get_option('vid-modelist'))

config.set10('USE_AUTOREPLY',     get_option('auto-reply'))
config.set10('USE_DEBUG',         get_option('debug'))
config.set10('USE_FPS',           get_option('variable-fps'))
config.set10('USE_GAME_ABI_HACK', get_option('game-abi-hack').require(x86 and cc.get_id() == 'gcc').allowed())
config.set10('USE_ICMP',          get_option('icmp-errors').require(win32 or cc.has_header('linux/errqueue.h')).allowed())
config.set10('USE_MD3',           get_option('md3'))
config.set10('USE_PACKETDUP',     get_option('packetdup-hack'))
config.set10('USE_TGA',           get_option('tga'))
config.set10('USE_' + host_machine.endian().to_upper() + '_ENDIAN', true)

configure_file(output: 'config.h', configuration: config)

if system_wide
  summary({'datadir': datadir, 'libdir': libdir, 'homedir': homedir}, section: 'Directories')
endif

summary({
  'anticheat-server'   : config.get('USE_AC_SERVER', '') != '',
  'auto-reply'         : config.get('USE_AUTOREPLY', 0) != 0,
  'client-gtv'         : config.get('USE_CLIENT_GTV', 0) != 0,
  'client-ui'          : config.get('USE_UI', 0) != 0,
  'debug'              : config.get('USE_DEBUG', 0) != 0,
  'game-abi-hack'      : config.get('USE_GAME_ABI_HACK', 0) != 0,
  'icmp-errors'        : config.get('USE_ICMP', 0) != 0,
  'libcurl'            : config.get('USE_CURL', 0) != 0,
  'libjpeg'            : config.get('USE_JPG', 0) != 0,
  'libpng'             : config.get('USE_PNG', 0) != 0,
  'md3'                : config.get('USE_MD3', 0) != 0,
  'mvd-client'         : config.get('USE_MVD_CLIENT', 0) != 0,
  'mvd-server'         : config.get('USE_MVD_SERVER', 0) != 0,
  'ogg'                : config.get('USE_OGG', 0) != 0,
  'openal'             : config.get('USE_OPENAL', 0) != 0,
  'packetdup-hack'     : config.get('USE_PACKETDUP', 0) != 0,
  'save-games'         : config.get('USE_SAVEGAMES', 0) != 0,
  'sdl2'               : config.get('USE_SDL', '') != '',
  'software-sound'     : config.get('USE_SNDDMA', 0) != 0,
  'system-console'     : config.get('USE_SYSCON', 0) != 0,
  'tests'              : config.get('USE_TESTS', 0) != 0,
  'tga'                : config.get('USE_TGA', 0) != 0,
  'variable-fps'       : config.get('USE_FPS', 0) != 0,
  'wayland'            : config.get('USE_WAYLAND', 0) != 0,
  'windows-crash-dumps': config.get('USE_DBGHELP', 0) != 0,
  'windows-egl'        : config.get('USE_WIN32EGL', 0) != 0,
  'windows-service'    : config.get('USE_WINSVC', '') != '',
  'x11'                : config.get('USE_X11', 0) != 0,
  'zlib'               : config.get('USE_ZLIB', 0) != 0,
  }, section: 'Features', bool_yn: true)
