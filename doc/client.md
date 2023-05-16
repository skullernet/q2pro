# Table of Contents

- [Table of Contents](#table-of-contents)
- [About](#about)
- [Variables](#variables)
  - [Netcode](#netcode)
    - [cl\_protocol](#cl_protocol)
    - [cl\_maxpackets](#cl_maxpackets)
    - [cl\_fuzzhack](#cl_fuzzhack)
    - [cl\_packetdup](#cl_packetdup)
    - [cl\_instantpacket](#cl_instantpacket)
    - [cl\_async](#cl_async)
    - [r\_maxfps](#r_maxfps)
    - [cl\_maxfps](#cl_maxfps)
    - [cl\_gibs](#cl_gibs)
    - [cl\_gun](#cl_gun)
    - [cl\_footsteps](#cl_footsteps)
    - [cl\_updaterate](#cl_updaterate)
    - [cl\_warn\_on\_fps\_rounding](#cl_warn_on_fps_rounding)
  - [Network](#network)
    - [net\_enable\_ipv6](#net_enable_ipv6)
    - [net\_ip](#net_ip)
    - [net\_ip6](#net_ip6)
    - [net\_clientport](#net_clientport)
    - [net\_maxmsglen](#net_maxmsglen)
    - [net\_chantype](#net_chantype)
  - [Triggers](#triggers)
    - [cl\_beginmapcmd](#cl_beginmapcmd)
    - [cl\_changemapcmd](#cl_changemapcmd)
    - [cl\_disconnectcmd](#cl_disconnectcmd)
  - [Effects](#effects)
    - [cl\_railtrail\_type](#cl_railtrail_type)
    - [cl\_railtrail\_time](#cl_railtrail_time)
    - [cl\_railcore\_color](#cl_railcore_color)
    - [cl\_railcore\_width](#cl_railcore_width)
    - [cl\_railspiral\_color](#cl_railspiral_color)
    - [cl\_railspiral\_radius](#cl_railspiral_radius)
    - [cl\_disable\_particles](#cl_disable_particles)
    - [cl\_disable\_explosions](#cl_disable_explosions)
    - [cl\_dlight\_hacks](#cl_dlight_hacks)
    - [cl\_noglow](#cl_noglow)
    - [cl\_gunalpha](#cl_gunalpha)
    - [cl\_gunfov](#cl_gunfov)
    - [cl\_gun\_x; cl\_gun\_y; cl\_gun\_z](#cl_gun_x-cl_gun_y-cl_gun_z)
  - [Sound Subsystem](#sound-subsystem)
    - [s\_enable](#s_enable)
    - [s\_ambient](#s_ambient)
    - [s\_underwater](#s_underwater)
    - [s\_underwater\_gain\_hf](#s_underwater_gain_hf)
    - [s\_auto\_focus](#s_auto_focus)
    - [s\_swapstereo](#s_swapstereo)
    - [s\_driver](#s_driver)
    - [al\_device](#al_device)
  - [Ogg Vorbis music](#ogg-vorbis-music)
    - [ogg\_enable](#ogg_enable)
    - [ogg\_volume](#ogg_volume)
    - [ogg\_shuffle](#ogg_shuffle)
  - [Graphical Console](#graphical-console)
    - [con\_clock](#con_clock)
    - [con\_height](#con_height)
    - [con\_alpha](#con_alpha)
    - [con\_scale](#con_scale)
    - [con\_font](#con_font)
    - [con\_background](#con_background)
    - [con\_notifylines](#con_notifylines)
    - [con\_history](#con_history)
    - [con\_scroll](#con_scroll)
    - [con\_timestamps](#con_timestamps)
    - [con\_timestampsformat](#con_timestampsformat)
    - [con\_timestampscolor](#con_timestampscolor)
    - [con\_auto\_chat](#con_auto_chat)
  - [Game Screen](#game-screen)
    - [scr\_draw2d](#scr_draw2d)
    - [scr\_showturtle](#scr_showturtle)
    - [scr\_demobar](#scr_demobar)
    - [scr\_showpause](#scr_showpause)
    - [scr\_scale](#scr_scale)
    - [scr\_alpha](#scr_alpha)
    - [scr\_font](#scr_font)
    - [scr\_lag\_draw](#scr_lag_draw)
    - [scr\_lag\_x](#scr_lag_x)
    - [scr\_lag\_y](#scr_lag_y)
    - [scr\_lag\_min](#scr_lag_min)
    - [scr\_lag\_max](#scr_lag_max)
    - [scr\_chathud](#scr_chathud)
    - [scr\_chathud\_lines](#scr_chathud_lines)
    - [scr\_chathud\_time](#scr_chathud_time)
    - [scr\_chathud\_x](#scr_chathud_x)
    - [scr\_chathud\_y](#scr_chathud_y)
    - [ch\_health](#ch_health)
    - [ch\_red; ch\_green; ch\_blue](#ch_red-ch_green-ch_blue)
    - [ch\_alpha](#ch_alpha)
    - [ch\_scale](#ch_scale)
    - [ch\_x; ch\_y](#ch_x-ch_y)
    - [xhair\_enabled](#xhair_enabled)
    - [xhair\_firing\_error](#xhair_firing_error)
    - [xhair\_movement\_error](#xhair_movement_error)
    - [xhair\_dot](#xhair_dot)
    - [xhair\_gap](#xhair_gap)
    - [xhair\_length](#xhair_length)
    - [xhair\_deployed\_weapon\_gap](#xhair_deployed_weapon_gap)
    - [xhair\_thickness](#xhair_thickness)
    - [xhair\_elasticity](#xhair_elasticity)
    - [xhair\_x; xhair\_y](#xhair_x-xhair_y)
  - [Video Modes](#video-modes)
    - [vid\_modelist](#vid_modelist)
    - [vid\_fullscreen](#vid_fullscreen)
    - [vid\_geometry](#vid_geometry)
    - [vid\_flip\_on\_switch](#vid_flip_on_switch)
    - [vid\_hwgamma](#vid_hwgamma)
    - [vid\_driver](#vid_driver)
  - [Windows Specific](#windows-specific)
    - [win\_noalttab](#win_noalttab)
    - [win\_disablewinkey](#win_disablewinkey)
    - [win\_noborder](#win_noborder)
    - [win\_noresize](#win_noresize)
    - [win\_notitle](#win_notitle)
    - [win\_alwaysontop](#win_alwaysontop)
    - [sys\_viewlog](#sys_viewlog)
    - [sys\_disablecrashdump](#sys_disablecrashdump)
    - [sys\_exitonerror](#sys_exitonerror)
  - [OpenGL Renderer](#opengl-renderer)
    - [gl\_gamma\_scale\_pics](#gl_gamma_scale_pics)
    - [gl\_noscrap](#gl_noscrap)
    - [gl\_bilerp\_chars](#gl_bilerp_chars)
    - [gl\_bilerp\_pics](#gl_bilerp_pics)
    - [gl\_upscale\_pcx](#gl_upscale_pcx)
    - [gl\_downsample\_skins](#gl_downsample_skins)
    - [gl\_drawsky](#gl_drawsky)
    - [gl\_waterwarp](#gl_waterwarp)
    - [gl\_fontshadow](#gl_fontshadow)
    - [gl\_partscale](#gl_partscale)
    - [gl\_partstyle](#gl_partstyle)
    - [gl\_partshape](#gl_partshape)
    - [gl\_celshading](#gl_celshading)
    - [gl\_dotshading](#gl_dotshading)
    - [gl\_saturation](#gl_saturation)
    - [gl\_invert](#gl_invert)
    - [gl\_anisotropy](#gl_anisotropy)
    - [gl\_brightness](#gl_brightness)
    - [gl\_coloredlightmaps](#gl_coloredlightmaps)
    - [gl\_modulate](#gl_modulate)
    - [gl\_modulate\_world](#gl_modulate_world)
    - [gl\_modulate\_entities](#gl_modulate_entities)
    - [gl\_doublelight\_entities](#gl_doublelight_entities)
    - [gl\_dynamic](#gl_dynamic)
    - [gl\_dlight\_falloff](#gl_dlight_falloff)
    - [gl\_shaders](#gl_shaders)
    - [gl\_colorbits](#gl_colorbits)
    - [gl\_depthbits](#gl_depthbits)
    - [gl\_stencilbits](#gl_stencilbits)
    - [gl\_multisamples](#gl_multisamples)
    - [gl\_texturebits](#gl_texturebits)
    - [gl\_screenshot\_format](#gl_screenshot_format)
    - [gl\_screenshot\_quality](#gl_screenshot_quality)
    - [gl\_screenshot\_compression](#gl_screenshot_compression)
    - [gl\_screenshot\_async](#gl_screenshot_async)
    - [gl\_screenshot\_template](#gl_screenshot_template)
    - [r\_override\_textures](#r_override_textures)
    - [r\_texture\_formats](#r_texture_formats)
    - [r\_texture\_overrides](#r_texture_overrides)
  - [Downloads](#downloads)
    - [allow\_download](#allow_download)
    - [allow\_download\_maps](#allow_download_maps)
    - [allow\_download\_models](#allow_download_models)
    - [allow\_download\_sounds](#allow_download_sounds)
    - [allow\_download\_pics](#allow_download_pics)
    - [allow\_download\_players](#allow_download_players)
    - [allow\_download\_textures](#allow_download_textures)
  - [HTTP Downloads](#http-downloads)
    - [cl\_http\_downloads](#cl_http_downloads)
    - [cl\_http\_filelists](#cl_http_filelists)
    - [cl\_http\_max\_connections](#cl_http_max_connections)
    - [cl\_http\_proxy](#cl_http_proxy)
    - [cl\_http\_default\_url](#cl_http_default_url)
    - [cl\_http\_insecure](#cl_http_insecure)
  - [Locations](#locations)
    - [loc\_trace](#loc_trace)
    - [loc\_dist](#loc_dist)
    - [loc\_draw](#loc_draw)
  - [Mouse Input](#mouse-input)
    - [m\_autosens](#m_autosens)
    - [m\_accel](#m_accel)
    - [m\_filter](#m_filter)
  - [Miscellaneous](#miscellaneous)
    - [cl\_chat\_notify](#cl_chat_notify)
    - [cl\_chat\_sound](#cl_chat_sound)
    - [cl\_chat\_filter](#cl_chat_filter)
    - [cl\_noskins](#cl_noskins)
    - [cl\_ignore\_stufftext](#cl_ignore_stufftext)
    - [cl\_rollhack](#cl_rollhack)
    - [cl\_adjustfov](#cl_adjustfov)
    - [cl\_demosnaps](#cl_demosnaps)
    - [cl\_demomsglen](#cl_demomsglen)
    - [cl\_demowait](#cl_demowait)
    - [cl\_demosuspendtoggle](#cl_demosuspendtoggle)
    - [cl\_autopause](#cl_autopause)
    - [ui\_open](#ui_open)
    - [ui\_background](#ui_background)
    - [ui\_scale](#ui_scale)
    - [ui\_sortdemos](#ui_sortdemos)
    - [ui\_listalldemos](#ui_listalldemos)
    - [ui\_sortservers](#ui_sortservers)
    - [ui\_colorservers](#ui_colorservers)
    - [ui\_pingrate](#ui_pingrate)
    - [com\_time\_format](#com_time_format)
    - [com\_date\_format](#com_date_format)
- [Macros](#macros)
- [Commands](#commands)
  - [Client Demos](#client-demos)
    - [demo \[/\]\<filename\[.ext\]\>](#demo-filenameext)
    - [seek \[+-\]\<timespec|percent\>\[%\]](#seek--timespecpercent)
    - [record \[-hzes\] \<filename\>](#record--hzes-filename)
    - [stop](#stop)
    - [suspend](#suspend)
    - [resume](#resume)
  - [Cvar Operations](#cvar-operations)
    - [toggle \<cvar\> \[value1 value2 …\]](#toggle-cvar-value1-value2-)
    - [inc \<cvar\> \[value\]](#inc-cvar-value)
    - [dec \<cvar\> \[value\]](#dec-cvar-value)
    - [reset \<cvar\>](#reset-cvar)
    - [resetall](#resetall)
    - [set \<cvar\> \<value\> \[u|s|…\]](#set-cvar-value-us)
    - [setu \<cvar\> \<value\> \[…\]](#setu-cvar-value-)
    - [sets \<cvar\> \<value\> \[…\]](#sets-cvar-value-)
    - [seta \<cvar\> \<value\> \[…\]](#seta-cvar-value-)
    - [cvarlist \[-achlmnrstuv\] \[wildcard\]](#cvarlist--achlmnrstuv-wildcard)
    - [macrolist](#macrolist)
  - [Message Triggers](#message-triggers)
    - [trigger \[\<command\> \<match\>\]](#trigger-command-match)
    - [untrigger \[all\] | \[\<command\> \<match\>\]](#untrigger-all--command-match)
  - [Chat Filters](#chat-filters)
    - [ignoretext \[match …\]](#ignoretext-match-)
    - [unignoretext \[all\] | \[match …\]](#unignoretext-all--match-)
    - [ignorenick \[nickname\]](#ignorenick-nickname)
    - [unignorenick \[all\] | \[nickname\]](#unignorenick-all--nickname)
  - [Draw Objects](#draw-objects)
    - [draw \<name\> \<x\> \<y\> \[color\]](#draw-name-x-y-color)
    - [undraw \[all\] | \<name\>](#undraw-all--name)
  - [Screenshots](#screenshots)
    - [screenshot \[format\]](#screenshot-format)
    - [screenshotpng \[filename\] \[compression\]](#screenshotpng-filename-compression)
    - [screenshotjpg \[filename\] \[quality\]](#screenshotjpg-filename-quality)
    - [screenshottga \[filename\]](#screenshottga-filename)
  - [Locations](#locations-1)
    - [loc \<add|del|set|list|save\>](#loc-adddelsetlistsave)
    - [add \<name\>](#add-name)
    - [del](#del)
    - [set \<name\>](#set-name)
    - [list](#list)
    - [save \[name\]](#save-name)
  - [Miscellaneous](#miscellaneous-1)
    - [vid\_restart](#vid_restart)
    - [fs\_restart](#fs_restart)
    - [r\_reload](#r_reload)
    - [passive](#passive)
    - [serverstatus \[address\]](#serverstatus-address)
    - [followip \[count\]](#followip-count)
    - [remotemode \<address\> \<password\>](#remotemode-address-password)
    - [ogg \<info|play|stop\>](#ogg-infoplaystop)
    - [info](#info)
    - [play \<track\>](#play-track)
    - [stop](#stop-1)
    - [whereis \<path\> \[all\]](#whereis-path-all)
    - [softlink \<name\> \<target\>](#softlink-name-target)
    - [softunlink \[-ah\] \<name\>](#softunlink--ah-name)
- [Incompatibilities](#incompatibilities)


---
# About

Q2PRO is an enhanced, multiplayer oriented Quake 2 client, compatible
with existing Quake 2 ports and licensed under GPLv2. This document
provides descriptions of console variables and commands added to or
modified by Q2PRO since the original Quake 2 release. Cvars and commands
inherited from original Quake 2 are not described here (yet).

# Variables

## Netcode

Q2PRO client supports separation of outgoing packet rate, physics frame
rate and rendering frame rate. Separation of physics and rendering frame
rates is accomplished in R1Q2 ‘cl\_async’ style and is enabled by
default.

In addition to this, Q2PRO network protocol is able to pack several
input commands into the single network packet for outgoing packet rate
reduction. This is very useful for some types of network links like
Wi-Fi that can’t deal with large number of small packets and cause
packet delay or loss. Q2PRO protocol is only in use when connected to a
Q2PRO server.

For the default Quake 2 protocol and R1Q2 protocol a hacky solution
exists, which exploits dropped packet recovery mechanism for the purpose
of packet rate reduction. This hack is disabled by default.

### cl\_protocol  
Specifies preferred network protocol version to use when connecting to
servers. If the server doesn’t support the specified protocol, client
will fall back to the previous supported version. Default value is 0.

-   0 — automatically select the highest protocol version supported

-   34 — use default Quake 2 protocol

-   35 — use enhanced R1Q2 protocol

-   36 — use enhanced Q2PRO protocol

### cl\_maxpackets  
Number of packets client sends per second. 0 means no particular limit.
Unless connected using Q2PRO protocol, this variable is ignored and
packets are sent in sync with client physics frame rate, controlled with
‘cl\_maxfps’ variable. Default value is 30.

### cl\_fuzzhack  
Enables ‘cl\_maxpackets’ limit even if Q2PRO protocol is not in use by
dropping packets. This is not a generally recommended thing to do, but
can be enabled if nothing else helps to reduce ping. Default value is 0
(disabled).

### cl\_packetdup  
Number of backup movement commands client includes in each new packet,
directly impacts upload rate. Unless connected using Q2PRO protocol,
hardcoded value of 2 backups per packet is used. Default value is 1.

### cl\_instantpacket  
Specifies if important events such as pressing ‘+attack’ or ‘+use’ are
sent to the server immediately, ignoring any rate limits. Default value
is 1 (enabled).

### cl\_async  
Controls rendering frame rate and physics frame rate separation. Default
value is 1. Influence of ‘cl\_async’ on client framerates is summarized
in the table below.

-   0 — run synchronous, like original Quake 2 does

-   1 — run asynchronous

<table>
<caption>Rate limits depending on ‘cl_async’ value</caption>
<colgroup>
<col style="width: 25%" />
<col style="width: 25%" />
<col style="width: 25%" />
<col style="width: 25%" />
</colgroup>
<thead>
<tr class="header">
<th style="text-align: left;">Value of ‘cl_async’</th>
<th style="text-align: left;">Rendering</th>
<th style="text-align: left;">Physics</th>
<th style="text-align: left;">Main loop</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td style="text-align: left;"><p>0</p></td>
<td style="text-align: left;"><p>cl_maxfps</p></td>
<td style="text-align: left;"><p>cl_maxfps</p></td>
<td style="text-align: left;"><p>cl_maxfps</p></td>
</tr>
<tr class="even">
<td style="text-align: left;"><p>1</p></td>
<td style="text-align: left;"><p>r_maxfps</p></td>
<td style="text-align: left;"><p>cl_maxfps</p></td>
<td style="text-align: left;"><p><em>unlimited</em></p></td>
</tr>
</tbody>
</table>

Rate limits depending on ‘cl\_async’ value

### r\_maxfps  
Specifies maximum rendering frame rate if ‘cl\_async’ is set to 1,
otherwise ignored. Default value is 0, which means no particular limit.

### cl\_maxfps  
Specifies client physics frame rate if ‘cl\_async’ 1 or 2 is used.
Otherwise, limits both rendering and physics frame rates. Default value
is 60.

### cl\_gibs  
Controls rendering of entities with ‘EF\_GIB’ flag set. When using Q2PRO
protocol, disabling this saves some bandwidth since the server stops
sending these entities at all. Default value is 1 (enabled).

### cl\_gun  
Controls rendering of the player’s own gun model. When using R1Q2 or
Q2PRO protocol, disabling this saves some bandwidth since the server
stops sending gun updates at all. Default value is 1.

-   0 — don’t draw gun

-   1 — draw gun depending on player handedness

-   2 — force right handed gun

-   3 — force left handed gun

### cl\_footsteps  
Controls footstep sounds. When using Q2PRO protocol, disabling this
saves some bandwidth since the server stops sending footstep events at
all. Default value is 1 (enabled).

### cl\_updaterate  
Specifies the perferred update rate requested from Q2PRO servers. Only
used when server is running in variable FPS mode, otherwise default rate
of 10 packets per second is used. Specified rate should evenly divide
native server frame rate. Default value is 0, which means to use the
highest update rate available (that is, native server frame rate).

### cl\_warn\_on\_fps\_rounding  
Print a warning if value specified for ‘cl\_maxfps’ or ‘r\_maxfps’ is
inexact due to rounding. Default value is 1 (enabled).

## Network

### net\_enable\_ipv6  
Enables IPv6 support. Default value is 1 on systems that support IPv6
and 0 otherwise.

-   0 — disable IPv6, use IPv4 only

-   1 — enable IPv6, but prefer IPv4 over IPv6 when resolving host names
    with multiple addresses

-   2 — enable IPv6, use normal address resolver priority configured by
    OS

### net\_ip  
Specifies network interface address client should use for outgoing UDP
connections using IPv4. Default value is empty, which means that default
network interface is used.

### net\_ip6  
Specifies network interface address client should use for outgoing UDP
connections using IPv6. Default value is empty, which means that default
network interface is used. Has no effect unless ‘net\_enable\_ipv6’ is
set to non-zero value.

### net\_clientport  
Specifies UDP port number client should use for outgoing connections
(using IPv4 or IPv6). Default value is -1, which means that random port
number is chosen at socket creation time.

### net\_maxmsglen  
Specifies maximum server to client packet size client will request from
servers. 0 means no hard limit. Default value is conservative 1390
bytes. It is nice to have this variable as close to your network link
MTU as possible (accounting for headers). Thus for normal Ethernet MTU
of 1500 bytes 1462 can be specified (10 bytes quake header, 8 bytes UDP
header, 20 bytes IPv4 header). Higher values may cause IP fragmentation
which is better to avoid. Servers will cap this variable to their own
maximum values. Don’t change this variable unless you know exactly what
you are doing.

### net\_chantype  
Specifies if enhanced Q2PRO network channel implementation is enabled
when connecting to Q2PRO servers. Q2PRO netchan supports
application-level fragmentation of datagrams that results is better
gamestate compression ratio and faster map load times. Default value is
1 (enabled).

## Triggers

### cl\_beginmapcmd  
Specifies command to be executed each time client enters a new map.
Default value is empty.

### cl\_changemapcmd  
Specifies command to be executed each time client begins loading a new
map. Default value is empty.

### cl\_disconnectcmd  
Specifies command to be executed each time client disconnects from the
server. Default value is empty.

See also ‘trigger’ client command description.

## Effects

Colors can be specified in one of the following formats:

-   \#RRGGBBAA, where R, G, B and A are hex digits

-   \#RRGGBB, which implies alpha value of FF

-   \#RGB, which is expanded to \#RRGGBB by duplicating digits

-   one of the predefined color names (black, red, green, yellow, blue,
    cyan, magenta, white)

### cl\_railtrail\_type  
Defines which type of rail trail effect to use. Default value is 0.

-   0 — use original effect

-   1 — use alternative effect, draw rail core only

-   2 — use alternative effect, draw rail core and spiral

Rail trail variables listed below apply to the alternative effect only.

### cl\_railtrail\_time  
Time, in seconds, for the rail trail to be visible. Default value is
1.0.

### cl\_railcore\_color  
Color of the rail core beam. Default value is "red".

### cl\_railcore\_width  
Width of the rail core beam. Default value is 3.

### cl\_railspiral\_color  
Color of the rail spiral. Default value is "blue".

### cl\_railspiral\_radius  
Radius of the rail spiral. Default value is 3.

### cl\_disable\_particles  
Disables rendering of particles for the following effects. This variable
is a bitmask. Default value is 0.

-   1 — grenade explosions

-   2 — grenade trails

-   4 — rocket explosions

-   8 — rocket trails

Bitmask cvars allow multiple features to be enabled. To enable the
needed set of features, their values need to be summed.

### cl\_disable\_explosions  
Disables rendering of animated models for the following effects. This
variable is a bitmask. Default value is 0.

-   1 — grenade explosions

-   2 — rocket explosions

### cl\_dlight\_hacks  
Toggles miscellaneous dynamic light effects options. This variable is a
bitmask. Default value is 0.

-   1 — make rocket projectile light red instead of yellow

-   2 — make rocket/grenade explosion light radius smaller

-   4 — disable muzzle flashes for machinegun and chaingun

### cl\_noglow  
Disables the glowing effect on bonus entities like ammo, health, etc.
Default value is 0 (glowing enabled).

### cl\_gunalpha  
Specifies opacity level of the player’s own gun model. Default value is
1 (fully opaque).

### cl\_gunfov  
Specifies custom FOV value for drawing player’s own gun model. Default
value is 90. Set to 0 to draw with current FOV value.

### cl\_gun\_x; cl\_gun\_y; cl\_gun\_z  
Specifies custom gun model offset. Default value is 0.

## Sound Subsystem

### s\_enable  
Specifies which sound engine to use. Default value is 1.

-   0 — sound is disabled

-   1 — use DMA sound engine

-   2 — use OpenAL sound engine

### s\_ambient  
Specifies if ambient sounds are played. Default value is 1.

-   0 — all ambient sounds are disabled

-   1 — all ambient sounds are enabled

-   2 — only ambient sounds from visible entities are enabled (rocket
    flybys, etc)

-   3 — only ambient sounds from player entity are enabled (railgun hum,
    hand grenade ticks, etc)

### s\_underwater  
Enables lowpass sound filter when underwater. Default value is 1
(enabled).

### s\_underwater\_gain\_hf  
Specifies HF gain value for lowpass sound filter. Default value is 0.25.

### s\_auto\_focus  
Specifies the minimum focus level main Q2PRO window should have for
sound to be activated. Default value is 0.

-   0 — sound is always activated

-   1 — sound is activated when main window is visible, and deactivated
    when it is iconified, or moved to another desktop

-   2 — sound is activated when main window has input focus, and
    deactivated when it loses it

### s\_swapstereo  
Swap left and right audio channels. Only effective when using DMA sound
engine. Default value is 0 (don’t swap).

### s\_driver  
Specifies which DMA sound driver to use. Default value is empty (detect
automatically). Possible sound drivers are (not all of them are
typically available at the same time, depending on how client was
compiled):

-   dsound — DirectSound

-   wave — Windows waveform audio

-   sdl — SDL2 audio

-   oss — OSS audio

### al\_device  
Specifies the name of OpenAL device to use. Format of this value depends
on your OpenAL implementation. Default value is empty, which means
default sound output device is used.

Using [OpenAL Soft](https://openal-soft.org/) implementation of OpenAL
is recommended.

## Ogg Vorbis music

### ogg\_enable  
Enables automatic playback of background music in Ogg Vorbis format.
Default value is 1.

### ogg\_volume  
Background music volume, relative to master volume. Default value is 1.

### ogg\_shuffle  
If disabled, play music tracks named ‘music/trackNN.ogg’, where NN is
current CD track number (typically in range 02-11). If enabled, play all
files with ‘.ogg’ extension from ‘music’ directory in random order.
Default value is 0 (disabled).

## Graphical Console

### con\_clock  
Toggles drawing of the digital clock at the lower right corner of
console. Default value is 0 (disabled).

### con\_height  
Fraction of the screen in-game console occupies. Default value is 0.5.

### con\_alpha  
Opacity of in-game console background. 0 is fully transparent, 1 is
opaque. Default value is 1.

### con\_scale  
Scaling factor of the console text. Default value is 0 (automatically
scale depending on current display resolution). Set to 1 to disable
scaling.

### con\_font  
Font used for drawing console text. Default value is "conchars".

### con\_background  
Image used as console background. Default value is "conback".

### con\_notifylines  
Number of the last console lines displayed in the notification area in
game. Default value is 4.

### con\_history  
Specifies how many lines to save into console history file before
exiting Q2PRO, to be reloaded on next startup. Maximum number of history
lines is 128. Default value is 0.

### con\_scroll  
Controls automatic scrolling of console text when some event occurs.
This variable is a bitmask. Default value is 0.

-   1 — when new command is entered

-   2 — when new lines are printed

### con\_timestamps  
Specifies if console lines are prefixed with a timestamp. Default value
is 0.

### con\_timestampsformat  
Format string for console timestamps. Default value is "%H:%M:%S ". See
strftime(3) for syntax description.

### con\_timestampscolor  
Text color used for console timestamps. Default value is "#aaa".

### con\_auto\_chat  
Specifies how console commands not starting with a slash or backslash
are handled while in game. Default value is 0.

-   0 — handle as regular commands

-   1 — forward as chat

-   2 — forward as team chat

## Game Screen

### scr\_draw2d  
Toggles drawing of 2D elements on the screen. Default value is 2.

-   0 — do not draw anything

-   1 — do not draw stats program

-   2 — draw everything

### scr\_showturtle  
Toggles drawing of various network error conditions at the lower left
corner of the screen. Default value is 1 (draw all errors except of
SUPPRESSED, CLIENTDROP and SERVERDROP). Values higher than 1 draw all
errors.

<table>
<colgroup>
<col style="width: 15%" />
<col style="width: 85%" />
</colgroup>
<tbody>
<tr class="odd">
<td><p>SERVERDROP</p></td>
<td><p>Packets from server to client were dropped by the
network.</p></td>
</tr>
<tr class="even">
<td><p>CLIENTDROP</p></td>
<td><p>A few packets from client to server were dropped by the network.
Server recovered player’s movement using backup commands.</p></td>
</tr>
<tr class="odd">
<td><p>CLIENTPRED</p></td>
<td><p>Many packets from client to server were dropped by the network.
Server ran out of backup commands and had to predict player’s
movement.</p></td>
</tr>
<tr class="even">
<td><p>NODELTA</p></td>
<td><p>Server sent an uncompressed frame. Typically occurs during a
heavy lag, when a lot of packets are dropped by the network.</p></td>
</tr>
<tr class="odd">
<td><p>SUPPRESSED</p></td>
<td><p>Server suppressed packets to client because rate limit was
exceeded.</p></td>
</tr>
<tr class="even">
<td><p>BADFRAME</p></td>
<td><p>Server sent an invalid delta compressed frame.</p></td>
</tr>
<tr class="odd">
<td><p>OLDFRAME</p></td>
<td><p>Server sent a delta compressed frame that is too old and can’t be
recovered.</p></td>
</tr>
<tr class="even">
<td><p>OLDENT</p></td>
<td><p>Server sent a delta compressed frame whose entities are too old
and can’t be recovered.</p></td>
</tr>
</tbody>
</table>

### scr\_demobar  
Toggles drawing of progress bar at the bottom of the screen during demo
playback. Default value is 1.

-   0 — do not draw demo bar

-   1 — draw demo bar and demo completion percentage

-   2 — draw demo bar, demo completion percentage and current demo time

### scr\_showpause  
Toggles drawing of pause indicator on the screen. Default value is 1.

-   0 — do not draw pause indicator

-   1 — draw pic in center of the screen

-   2 — draw text in demo bar (visible only during demo playback)

### scr\_scale  
Scaling factor of the HUD elements. Default value is 0 (automatically
scale depending on current display resolution). Set to 1 to disable
scaling.

### scr\_alpha  
Opacity of the HUD elements. 0 is fully transparent, 1 is opaque.
Default value is 1.

### scr\_font  
Font used for drawing HUD text. Default value is "conchars".

### scr\_lag\_draw  
Toggles drawing of small (48x48 pixels) ping graph on the screen.
Default value is 0.

-   0 — do not draw graph

-   1 — draw transparent graph

-   2 — overlay graph on gray background

### scr\_lag\_x  
Absolute value of this cvar specifies horizontal placement of the ping
graph, counted in pixels from the screen edge. Negative values align
graph to the right edge of the screen instead of the left edge. Default
value is -1.

### scr\_lag\_y  
Absolute value of this cvar specifies vertical placement of the ping
graph, counted in pixels from the screen edge. Negative values align
graph to the bottom edge of the screen intead of the top edge. Default
value is -1.

### scr\_lag\_min  
Specifies ping graph offset by defining the minimum value that can be
displayed. Default value is 0.

### scr\_lag\_max  
Specifies ping graph scale by defining the maximum value that can be
displayed. Default value is 200.

### scr\_chathud  
Toggles drawing of the last chat lines on the screen. Default value is
0.

-   0 — do not draw chat lines

-   1 — draw chat lines in normal color

-   2 — draw chat lines in alternative color

### scr\_chathud\_lines  
Specifies number of the last chat lines drawn on the screen. Default
value is 4. Maximum value is 32.

### scr\_chathud\_time  
Specifies visibility time of each chat line, counted in seconds. Default
value is 0 (lines never fade out).

### scr\_chathud\_x  
Absolute value of this cvar specifies horizontal placement of the chat
HUD, counted in pixels from the screen edge. Negative values align graph
to the right edge of the screen instead of the left edge. Default value
is 8.

### scr\_chathud\_y  
Absolute value of this cvar specifies vertical placement of the chat
HUD, counted in pixels from the screen edge. Negative values align graph
to the bottom edge of the screen intead of the top edge. Default value
is -64.

### ch\_health  
Enables dynamic crosshair coloring based on the health statistic seen in
the player’s HUD. Default value is 0 (use static color).

### ch\_red; ch\_green; ch\_blue  
These variables specify the color of crosshair image. Default values are
1 (draw in white color). Ignored if ‘ch\_health’ is enabled.

### ch\_alpha  
Opacity level of crosshair image. Default value is 1 (fully opaque).

### ch\_scale  
Scaling factor of the crosshair image. Default value is 1 (original
size).

### ch\_x; ch\_y  
These variables specify the crosshair image offset, counted in pixels
from the default position in center of the game screen. Default values
are 0 (draw in center).

### xhair_enabled
Enables new xhair system which replaces the classic crosshair.

### xhair_firing_error
Makes crosshair dynamic (move in and out) while firing.

### xhair_movement_error
Makes crosshair dynamic (move in and out) while moving.

### xhair_dot
Enables drawing of the dot in the center of the xhair.

### xhair_gap
Specifies the xhair gap, i.e. the amount of pixels from the center of the screen to the start of the xhair lines.

### xhair_length
Specifies the length of the xhair lines.

### xhair_deployed_weapon_gap
If enabled, the xhair gap will adapt to the weapon currently in use. For example, the shotguns will get a bigger gap due to their higher spread.

### xhair_thickness
Specifies the thickness of the xhair lines and/or dot.

### xhair_elasticity
Specifies how 'elastic' the xhair is, i.e. how easily it adapts. Lower values will result in a more stiff xhair, while higher values will have a more bouncy effect.

### xhair_x; xhair_y
These variables specify the crosshair image offset, counted in pixels from the default position in center of the game screen. Default values are 0 (draw in center).

## Video Modes

Hard coded list of the fullscreen video modes is gone from Q2PRO, you
can specify your own list in configuration files. Vertical refresh
frequency *freq* and bit depth *bpp* can be specified individually for
each mode.

Video mode change no longer requires ‘vid\_restart’ and is nearly
instant. In windowed mode, size as well as position of the main window
can be changed freely.

### vid\_modelist  
Space separated list of fullscreen video modes. Both *freq* and *bpp*
parameters are optional. Full syntax is: *WxH\[@freq\]\[:bpp\] \[…\]*.
Default value is "640x480 800x600 1024x768". On Linux, *freq* parameter
is currently ignored. Special keyword ‘desktop’ means to use default
desktop video mode.

### vid\_fullscreen  
If set to non zero *value*, run in the specified fullscreen mode. This
way, *value* acts as index into the list of video modes specified by
‘vid\_modelist’. Default value is 0, which means to run in windowed
mode.

### vid\_geometry  
Size and optional position of the main window on virtual desktop. Full
syntax is: `WxH[+X+Y]`. Default value is "640x480".

### vid\_flip\_on\_switch  
On Windows, specifies if original video mode is automatically restored
when switching from fullscreen Q2PRO to another application or desktop.
Default value is 0 (don’t switch video modes).

### vid\_hwgamma  
Instructs the video driver to use hardware gamma correction for
implementing ‘vid\_gamma’. Default value is 0 (use software gamma).

### vid\_driver  
Specifies which video driver to use. Default value is empty (detect
automatically). Possible video drivers are (not all of them are
typically available at the same time, depending on how client was
compiled):

-   win32wgl — standard Windows OpenGL

-   win32egl — OpenGL ES 3.0+ via third-party libEGL.dll (e.g. ANGLE)

-   wayland — native Wayland

-   x11 — native X11

-   sdl — SDL2 video driver

The following lines define 2 video modes: 640x480 and 800x600 at 75 Hz
vertical refresh and 32 bit framebuffer depth, and select the last
800x600 mode.

    /set vid_modelist "640x480@75:32 800x600@75:32"
    /set vid_fullscreen 2

## Windows Specific

The following variables are specific to the Windows port of Q2PRO.

### win\_noalttab  
Disables the Alt-Tab key combination to prevent it from interfering with
game when pressed. Default is 0 (don’t disable).

### win\_disablewinkey  
Disables the default Windows key action to prevent it from interfering
with game when pressed. Default is 0 (don’t disable).

### win\_noborder  
Hides the main window bar (borderless). Default is 0 (show window bar).

### win\_noresize  
Prevents the main window from resizing by dragging the border. Default
is 0 (allow resizing).

### win\_notitle  
Hides the main window title bar. Default is 0 (show title bar).

### win\_alwaysontop  
Puts the main window on top of other windows. Default is 0 (main window
can be obscured by other windows).

### sys\_viewlog  
Show system console window when running a client. Can be set from
command line only.

### sys\_disablecrashdump  
Disable crash dump generation. Can be set from command line only.

### sys\_exitonerror  
Exit on fatal error instead of showing error message. Can be set from
command line only.

## OpenGL Renderer

### gl\_gamma\_scale\_pics  
Apply software gamma scaling not only to textures and skins, but to HUD
pictures also. Default value is 0 (don’t apply to pics).

### gl\_noscrap  
By default, OpenGL renderer combines small HUD pictures into the single
texture called scrap. This usually speeds up rendering a bit, and allows
pixel precise rendering of non power of two sized images. If you don’t
like this optimization for some reason, this cvar can be used to disable
it. Default value is 0 (optimize).

### gl\_bilerp\_chars  
Enables bilinear filtering of charset images. Default value is 0
(disabled).

### gl\_bilerp\_pics  
Enables bilinear filtering of HUD pictures. Default value is 1.

-   0 — disabled for all pictures

-   1 — enabled for large pictures that don’t fit into the scrap

-   2 — enabled for all pictures, including the scrap texture itself

### gl\_upscale\_pcx  
Enables upscaling of PCX images using HQ2x and HQ4x filters. This
improves rendering quality when screen scaling is used. Default value is
0.

-   0 — don’t upscale

-   1 — upscale 2x (takes 5x more memory)

-   2 — upscale 4x (takes 21x more memory)

### gl\_downsample\_skins  
Specifies if skins are downsampled just like world textures are. When
disabled, ‘gl\_round\_down’, ‘gl\_picmip’ cvars have no effect on skins.
Default value is 1 (downsampling enabled).

### gl\_drawsky  
Enable skybox texturing. 0 means to draw sky box in solid black color.
Default value is 1 (enabled).

### gl\_waterwarp  
Enable screen warping effect when underwater. Only effective when using
GLSL backend. Default value is 0 (disabled).

### gl\_fontshadow  
Specifies font shadow width, in pixels, ranging from 0 to 2. Default
value is 0 (no shadow).

### gl\_partscale  
Specifies minimum size of particles. Default value is 2.

### gl\_partstyle  
Specifies drawing style of particles. Default value is 0.

-   0 — blend colors

-   1 — saturate colors

### gl\_partshape  
Specifies shape of particles. Default value is 0.

-   0 — faded circle

-   1 — square

-   2 — fuller, less faded circle

### gl\_celshading  
Enables drawing black contour lines around 3D models (aka ‘celshading’).
Value of this variable specifies thickness of the lines drawn. Default
value is 0 (celshading disabled).

### gl\_dotshading  
Enables dotshading effect when drawing 3D models, which helps them look
truly 3D-ish by simulating diffuse lighting from a fake light source.
Default value is 1 (enabled).

### gl\_saturation  
Enables grayscaling of world textures. 1 keeps original colors, 0
converts textures to grayscale format (this may save some video memory
and speed up rendering a bit since textures are uploaded at 8 bit per
pixel instead of 24), any value in between reduces colorfulness. Default
value is 1 (keep original colors).

### gl\_invert  
Inverts colors of world textures. In combination with ‘gl\_saturation 0’
effectively makes textures look like black and white photo negative.
Default value is 0 (do not invert colors).

### gl\_anisotropy  
When set to 2 and higher, enables anisotropic filtering of world
textures, if supported by your OpenGL implementation. Default value is 1
(anisotropic filtering disabled).

### gl\_brightness  
Specifies a brightness value that is added to each pixel of world
lightmaps. Positive values make lightmaps brighter, negative values make
lightmaps darker. Default value is 0 (keep original brightness).

### gl\_coloredlightmaps  
Enables grayscaling of world lightmaps. 1 keeps original colors, 0
converts lightmaps to grayscale format, any value in between reduces
colorfulness. Default value is 1 (keep original colors).

### gl\_modulate  
Specifies a primary modulation factor that each pixel of world lightmaps
is multiplied by. This cvar affects entity lighting as well. Default
value is 1 (identity).

### gl\_modulate\_world  
Specifies an secondary modulation factor that each pixel of world
lightmaps is multiplied by. This cvar does not affect entity lighting.
Default value is 1 (identity).

### gl\_modulate\_entities  
Specifies an secondary modulation factor that entity lighting is
multiplied by. This cvar does not affect world lightmaps. Default value
is 1 (identity).

An old trick to make entities look brighter in Quake 2 was setting
‘gl\_modulate’ to a high value without issuing ‘vid\_restart’
afterwards. This way it was possible to keep ‘gl\_modulate’ from
applying to world lightmaps, but only until the next map was loaded. In
Q2PRO this trick is no longer needed (and it won’t work, since
‘gl\_modulate’ is applied dynamically). To get the similar effect, set
the legacy ‘gl\_modulate’ variable to 1, and configure
‘gl\_modulate\_world’ and ‘gl\_modulate\_entities’ to suit your needs.

### gl\_doublelight\_entities  
Specifies if combined modulation factor is applied to entity lighting
one more time just before final lighting value is calculated, to
simulate a well-known bug in the original Quake 2 renderer. Default
value is 1 (apply twice).

Entity lighting is calculated based on the color of the lightmap sample
from the world surface directly beneath the entity. This means any cvar
affecting lightmaps affects entity lighting as well (with exception of
‘gl\_modulate\_world’). Cvars that have effect only on the entity
lighting are ‘gl\_modulate\_entities’ and ‘gl\_doublelight\_entities’.
Yet another cvar affecting entity lighting is ‘gl\_dotshading’, which
typically makes entities look a bit brighter. See also ‘cl\_noglow’ cvar
which removes the pulsing effect (glowing) on bonus entities.

### gl\_dynamic  
Controls dynamic lightmap updates. Default value is 1.

-   0 — all dynamic lighting is disabled

-   1 — all dynamic lighting is enabled

-   2 — most dynamic lights are disabled, but lightmap updates are still
    allowed for switchable lights to work

### gl\_dlight\_falloff  
Makes dynamic lights look a bit smoother, opposed to original jagged
Quake 2 style. Default value is 1 (enabled).

### gl\_shaders  
Enables GLSL rendering backend. This requires at least OpenGL 3.0 and
changes how ‘gl\_modulate’, ‘gl\_brightness’ and ‘intensity’ parameters
work to prevent ‘washed out’ colors. Default value is 1 (enabled) on
systems that support shaders, 0 otherwise.

### gl\_colorbits  
Specifies desired size of color buffer, in bits, requested from OpenGL
implementation (should be typically 0, 24 or 32). Default value is 0
(determine the best value automatically).

### gl\_depthbits  
Specifies desired size of depth buffer, in bits, requested from OpenGL
implementation (should be typically 0 or 24). Default value is 0
(determine the best value automatically).

### gl\_stencilbits  
Specifies desired size of stencil buffer, in bits, requested from OpenGL
implementation (should be typically 0 or 8). Currently stencil buffer is
used only for drawing projection shadows. Default value is 8. 0 means no
stencil buffer requested.

### gl\_multisamples  
Specifies number of samples per pixel used to implement multisample
anti-aliasing, if supported by OpenGL implementation. Values 0 and 1 are
equivalent and disable MSAA. Values from 2 to 32 enable MSAA. Default
value is 0.

### gl\_texturebits  
Specifies number of bits per texel used for internal texture storage
(should be typically 0, 8, 16 or 32). Default value is 0 (choose the
best internal format automatically).

### gl\_screenshot\_format  
Specifies image format ‘screenshot’ command uses. Possible values are
"png", "jpg" and "tga". Default value is "jpg".

### gl\_screenshot\_quality  
Specifies image quality of JPG screenshots. Values range from 0 (worst
quality) to 100 (best quality). Default value is 90.

### gl\_screenshot\_compression  
Specifies compression level of PNG screenshots. Values range from 0 (no
compression) to 9 (best compression). Default value is 6.

### gl\_screenshot\_async  
Specifies if screenshots are saved in background thread to avoid pausing
the client. Default value is 1.

-   0 — save screenshots synchronously

-   1 — save PNG screenshots in background thread

-   2 — save JPG and PNG screenshots in background thread

### gl\_screenshot\_template  
Specifies filename template in "fileXXX" format for ‘screenshot’
command. Template must contain at least 3 and at most 9 consecutive ‘X’
in the last component. Template may contain slashes to save under
subdirectory. Default value is "quakeXXX".

### r\_override\_textures  
Enables automatic overriding of palettized textures (in WAL or PCX
format) with truecolor replacements (in PNG, JPG or TGA format) by
stripping off original file extension and searching for alternative
filenames in the order specified by ‘r\_texture\_formats’ variable.
Default value is 1.

-   0 — don’t override textures

-   1 — override only palettized textures

-   2 — override all textures

### r\_texture\_formats  
Specifies the order in which truecolor texture replacements are
searched. Default value is "pjt", which means to try ‘.png’ extension
first, then ‘.jpg’, then ‘.tga’.

### r\_texture\_overrides  
Specifies what types of textures are affected by
‘r\_override\_textures’. This variable is a bitmask. Default value is -1
(all types).

-   1 — HUD pictures

-   2 — HUD fonts

-   4 — skins

-   8 — sprites

-   16 — wall textures

-   32 — sky textures

When Q2PRO attempts to load an alias model from disk, it determines
actual model format by file contents, rather than by filename extension.
Therefore, if you wish to override MD2 model with MD3 replacement,
simply rename the MD3 model to ‘tris.md2’ and place it in appropriate
packfile to make sure it gets loaded first.

## Downloads

These variables control automatic client downloads (both legacy UDP and
HTTP downloads).

### allow\_download  
Globally allows or disallows client downloads. Remaining variables
listed below are effective only when downloads are globally enabled.
Default value is 1.

-   -1 — downloads are permanently disabled (once this value is set, it
    can’t be modified)

-   0 — downloads are disabled

-   1 — downloads are enabled

### allow\_download\_maps  
Enables automatic downloading of maps. Default value is 1.

### allow\_download\_models  
Enables automatic downloading of non-player models, sprites and skins.
Default value is 1.

### allow\_download\_sounds  
Enables automatic downloading of non-player sounds. Default value is 1.

### allow\_download\_pics  
Enables automatic downloading of HUD pictures. Default value is 1.

### allow\_download\_players  
Enables automatic downloading of player models, skins, sounds and icons.
Default value is 1.

### allow\_download\_textures  
Enables automatic downloading of map textures. Default value is 1.

It is possible to specify a list of paths in ‘download-ignores.txt’ file
that are known to be non-existent and should never be downloaded from
server. This file accepts wildcard patterns one per line. Empty lines
and lines starting with ‘#’ or ‘/’ characters are ignored.

## HTTP Downloads

### cl\_http\_downloads  
Enables HTTP downloads, if server advertises download URL. Default value
is 1 (enabled).

### cl\_http\_filelists  
When a first file is about to be downloaded from HTTP server, send a
filelist request, and download any additional files specified in the
filelist. Filelists provide a ‘pushing’ mechanism for server operator to
make sure all clients download complete set of data for the particular
mod, instead of requesting files one-by-one. Default value is 1 (request
filelists).

### cl\_http\_max\_connections  
Maximum number of simultaneous connections to the HTTP server. Default
value is 2.

### cl\_http\_proxy  
HTTP proxy server to use for downloads. Default value is empty (direct
connection).

### cl\_http\_default\_url  
Default download URL to use when server doesn’t provide one. Client will
fall back to UDP downloading as soon as 404 is returned from default
repository. Default value is "".

### cl\_http\_insecure  
Disable checking of server certificate when using HTTPS. Default value
is 0.

## Locations

Client side location files provide a way to report a player’s position
on the map in team chat messages without depending on the game mod.
Locations are loaded from ‘locs/&lt;mapname&gt;.loc’ file. Once location
file is loaded, ‘loc\_here’ and ‘loc\_there’ macros will expand to the
name of location closest to the given position. Variables listed below
control some aspects of location selection.

### loc\_trace  
When enabled, location must be directly visible from the given position
(not obscured by solid map geometry) in order to be selected. Default
value is 0, which means any closest location will satisfy, even if it is
placed behind the wall.

### loc\_dist  
Maximum distance to the location, in world units, for it to be
considered by the location selection algorithm. Default value is 500.

### loc\_draw  
Enables visualization of location positions. Default value is 0
(disabled).

## Mouse Input

in\_grab  
Specifies mouse grabbing policy in windowed mode. Normally, mouse is
always grabbed in-game and released when console or menu is up. In
addition to that, smart policy mode automatically releases the mouse
when its input is not needed (playing a demo, or spectating a player).
Default value is 1.

-   0 — don’t grab mouse

-   1 — normal grabbing policy

-   2 — smart grabbing policy

### m\_autosens  
Enables automatic scaling of mouse sensitivity proportional to the
current player field of view. Values between 90 and 179 specify the
default FOV value to scale sensitivity from. Zero disables automatic
scaling. Any other value assumes default FOV of 90 degrees. Default
value is 0.

### m\_accel  
Specifies mouse acceleration factor. Default value is 0 (acceleration
disabled).

### m\_filter  
When enabled, mouse movement is averaged between current and previous
samples. Default value is 0 (filtering disabled).

## Miscellaneous

### cl\_chat\_notify  
Specifies whether to display chat lines in the notify area. Default
value is 1 (enabled).

### cl\_chat\_sound  
Specifies sound effect to play each time chat message is received.
Default value is 1.

-   0 — don’t play chat sound

-   1 — play normal sound (‘misc/talk.wav’)

-   2 — play alternative sound (‘misc/talk1.wav’)

### cl\_chat\_filter  
Specifies if unprintable characters are filtered from incoming chat
messages, to prevent common exploits like hiding player names. Default
value is 0 (don’t filter).

### cl\_noskins  
Restricts which models and skins players can use. Default value is 0.

-   0 — no restrictions, if skins exists, it will be loaded

-   1 — do not allow any skins except of ‘male/grunt’

-   2 — do not allow any skins except of ‘male/grunt’ and
    ‘female/athena’

With ‘cl\_noskins’ set to 2, it is possible to keep just 2 model/skin
pairs (‘male/grunt’ and ‘female/athena’) to save memory and reduce map
load times. This will not affect model-based TDM gameplay, since any
male skin will be replaced by ‘male/grunt’ and any female skin will be
replaced by ‘female/athena’.

### cl\_ignore\_stufftext  
Enable filtering of commands server is allowed to stuff into client
console. List of allowed wildcard patterns can be specified in
‘stufftext-whitelist.txt’ file. Commands are matched raw, before macro
expansion, but after splitting multi-line or semicolon separated
commands. Internal client commands are always allowed. If whitelist file
doesn’t exist or is empty, ‘cmd’ command (with arbitrary arguments) is
allowed. This allows the server to query any console variable on the
client. If there is at least one entry in whitelist, then ‘cmd’ needs to
be explicitly whitelisted. Q2PRO server will not allow the client in if
it can’t query version cvar, for example. When set to 2 and higher also
issues a warning when stufftext command is ignored. Default value is 0
(don’t filter stufftext commands).

Stufftext filtering is advanced feature and may create compatibility
problems with mods/servers.

### cl\_rollhack  
Default OpenGL renderer in Quake 2 contained a bug that caused ‘roll’
angle of 3D models to be inverted during rotation. Due to this bug,
player models did lean in the opposite direction when strafing. New
Q2PRO renderer doesn’t have this bug, but since many players got used to
it, Q2PRO is able to simulate original behavior. This cvar chooses in
which direction player models will lean. Default value is 1 (invert
‘roll’ angle).

### cl\_adjustfov  
Specifies if horizontal field of view is automatically adjusted for
screens with aspect ratio different from 4/3. Default value is 1
(enabled).

### cl\_demosnaps  
Specifies time interval, in seconds, between saving ‘snapshots’ in
memory during demo playback. Snapshots enable backward seeking in demo
(see ‘seek’ command description), and speed up repeated forward seeks.
Setting this variable to 0 disables snapshotting entirely. Default value
is 10.

### cl\_demomsglen  
Specifies default maximum message size used for demo recording. Default
value is 1390. See ‘record’ command description for more information on
demo packet sizes.

### cl\_demowait  
Specifies if demo playback is automatically paused at the last frame in
demo file. Default value is 0 (finish playback).

### cl\_demosuspendtoggle  
Specifies if ‘suspend’ both pauses and resumes demo recording or just
pauses if it was recoring. Default value is 1 (toggle between pause and
resume).

### cl\_autopause  
Specifies if single player game or demo playback is automatically paused
once client console or menu is opened. Default value is 1 (pause game).

### ui\_open  
Specifies if menu is automatically opened on startup, instead of full
screen console. Default value is 0 (don’t open menu).

### ui\_background  
Specifies image to use as menu background. Default value is empty, which
just fills the screen with solid black color.

### ui\_scale  
Scaling factor of the UI widgets. Default value is 0 (automatically
scale depending on current display resolution). Set to 1 to disable
scaling.

### ui\_sortdemos  
Specifies default sorting order of entries in demo browser. Default
value is 1. Negate the values for descending sorting order instead of
ascending.

-   0 — don’t sort

-   1 — sort by name

-   2 — sort by date

-   3 — sort by size

-   4 — sort by map

-   5 — sort by POV

### ui\_listalldemos  
List all demos, including demos in packs and demos in base directories.
Default value is 0 (limit the search to physical files within the
current game directory).

### ui\_sortservers  
Specifies default sorting order of entries in server browser. Default
value is 0. Negate the values for descending sorting order instead of
ascending.

-   0 — don’t sort

-   1 — sort by hostname

-   2 — sort by mod

-   3 — sort by map

-   4 — sort by players

-   5 — sort by RTT

### ui\_colorservers  
Enables highlighting of entries in server browser with different colors.
This option draws entries with low RTT in green and grays out password
protected and anticheat enforced servers. Default value is 0 (disabled).

### ui\_pingrate  
Specifies the server pinging rate used by server browser, in packets per
second. Default value is 0, which estimates the default pinging rate
based on ‘rate’ client variable.

### com\_time\_format  
Time format used by ‘com\_time’ macro. Default value is "%H.%M" on Win32
and "%H:%M" on UNIX. See strftime(3) for syntax description.

### com\_date\_format  
Date format used by ‘com\_date’ macro. Default value is "%Y-%m-%d". See
strftime(3) for syntax description.

uf  
User flags variable, automatically exported to game mod in userinfo.
Meaning and level of support of individual flags is game mod dependent.
Default value is empty. Commonly supported flags are reproduced below.
Flags 4 and 64 are supported during local demo playback. Flags 4-64 are
supported in MVD/GTV client mode.

-   1 — auto screenshot at end of match

-   2 — auto record demo at beginning of match

-   4 — prefer user FOV over chased player FOV

-   8 — mute player chat

-   16 — mute observer chat

-   32 — mute other messages

-   64 — prefer chased player FOV over user FOV

# Macros

Macros behave like automated console variables. When macro expansion is
performed, macros are searched first, then console variables.

Each of the following examples are valid and produce the same output:

    /echo $loc_here
    /echo $loc_here$
    /echo ${loc_here}
    /echo ${$loc_here}

<table>
<caption>List of client macros</caption>
<colgroup>
<col style="width: 15%" />
<col style="width: 85%" />
</colgroup>
<tbody>
<tr class="odd">
<td><p>cl_armor</p></td>
<td><p>armor statistic seen in the HUD</p></td>
</tr>
<tr class="even">
<td><p>cl_ammo</p></td>
<td><p>ammo statistic seen in the HUD</p></td>
</tr>
<tr class="odd">
<td><p>cl_health</p></td>
<td><p>health statistic seen in the HUD</p></td>
</tr>
<tr class="even">
<td><p>cl_weaponmodel</p></td>
<td><p>current weapon model</p></td>
</tr>
<tr class="odd">
<td><p>cl_timer</p></td>
<td><p>time since level load</p></td>
</tr>
<tr class="even">
<td><p>cl_demopos</p></td>
<td><p>current position in demo, in <em>timespec</em> syntax</p></td>
</tr>
<tr class="odd">
<td><p>cl_server</p></td>
<td><p>address of the server client is connected to</p></td>
</tr>
<tr class="even">
<td><p>cl_mapname</p></td>
<td><p>name of the current map</p></td>
</tr>
<tr class="odd">
<td><p>loc_there</p></td>
<td><p>name of the location player is looking at</p></td>
</tr>
<tr class="even">
<td><p>loc_here</p></td>
<td><p>name of the location player is standing at</p></td>
</tr>
<tr class="odd">
<td><p>cl_ping</p></td>
<td><p>average round trip time to the server</p></td>
</tr>
<tr class="even">
<td><p>cl_lag</p></td>
<td><p>incoming packet loss percentage</p></td>
</tr>
<tr class="odd">
<td><p>cl_fps</p></td>
<td><p>main client loop frame rate <a href="#fn1" class="footnote-ref"
id="fnref1" role="doc-noteref"><sup>1</sup></a></p></td>
</tr>
<tr class="even">
<td><p>cl_mps</p></td>
<td><p>movement commands generation rate in movements per second <a
href="#fn2" class="footnote-ref" id="fnref2"
role="doc-noteref"><sup>2</sup></a></p></td>
</tr>
<tr class="odd">
<td><p>cl_pps</p></td>
<td><p>movement packets transmission rate in packets per second</p></td>
</tr>
<tr class="even">
<td><p>cl_ups</p></td>
<td><p>player velocity in world units per second</p></td>
</tr>
<tr class="odd">
<td><p>r_fps</p></td>
<td><p>rendering frame rate</p></td>
</tr>
<tr class="even">
<td><p>com_time</p></td>
<td><p>current time formatted according to ‘com_time_format’</p></td>
</tr>
<tr class="odd">
<td><p>com_date</p></td>
<td><p>current date formatted according to ‘com_date_format’</p></td>
</tr>
<tr class="even">
<td><p>com_uptime</p></td>
<td><p>engine uptime in short format</p></td>
</tr>
<tr class="odd">
<td><p>net_dnrate</p></td>
<td><p>current download rate in bytes/sec</p></td>
</tr>
<tr class="even">
<td><p>net_uprate</p></td>
<td><p>current upload rate in bytes/sec</p></td>
</tr>
<tr class="odd">
<td><p>random</p></td>
<td><p>expands to the random decimal digit</p></td>
</tr>
</tbody>
</table>
<aside id="footnotes" class="footnotes footnotes-end-of-document"
role="doc-endnotes">
<hr />
<ol>
<li id="fn1"><p>This is not the framerate ‘cl_maxfps’ limits. Think of
it as an input polling frame rate, or a ‘master’ framerate.<a
href="#fnref1" class="footnote-back" role="doc-backlink">↩︎</a></p></li>
<li id="fn2"><p>Can be also called ‘physics’ frame rate. This is what
‘cl_maxfps’ limits.<a href="#fnref2" class="footnote-back"
role="doc-backlink">↩︎</a></p></li>
</ol>
</aside>

List of client macros

<table>
<caption>List of special macros</caption>
<colgroup>
<col style="width: 15%" />
<col style="width: 85%" />
</colgroup>
<tbody>
<tr class="odd">
<td><p>qt</p></td>
<td><p>expands to double quote</p></td>
</tr>
<tr class="even">
<td><p>sc</p></td>
<td><p>expands to semicolon</p></td>
</tr>
<tr class="odd">
<td><p>$</p></td>
<td><p>expands to dollar sign</p></td>
</tr>
</tbody>
</table>

List of special macros

# Commands

## Client Demos

### demo \[/\]&lt;filename\[.ext\]&gt;  
Begins demo playback. This command does not require file extension to be
specified and supports filename autocompletion on TAB. Loads file from
‘demos/’ unless slash is prepended to *filename*, otherwise loads from
the root of quake file system. Can be used to launch MVD playback as
well, if MVD file type is detected, it will be automatically passed to
the server subsystem. To stop demo playback, type ‘disconnect’.

By default, during demo playback, Q2PRO overrides FOV value stored in
demo file with value of local ‘fov’ variable, unless stored FOV value is
less than 90. This behavior can be changed with ‘uf’ variable (see
above).

### seek \[+-\]&lt;timespec|percent&gt;\[%\]  
Seeks the given amount of time during demo playback. Prepend with ‘+’ to
seek forward relative to current position, prepend with ‘-’ to seek
backward relative to current position. Without prefix, seeks to an
absolute frame position within the demo file. See below for *timespec*
syntax description. With ‘%’ suffix, seeks to specified file position
percentage. Initial forward seek may be slow, so be patient.

The ‘seek’ command actually operates on demo frame numbers, not pure
server time. Therefore, ‘seek +300’ does not exactly mean ‘skip 5
minutes of server time’, but just means ‘skip 3000 demo frames’, which
may account for **more** than 5 minutes if there were dropped frames.
For most demos, however, correspondence between frame numbers and server
time should be reasonably close.

Absolute or relative demo time can be specified in one of the following
formats:

-   .FF, where FF are frames

-   SS, where SS are seconds

-   SS.FF, where SS are seconds, FF are frames

-   MM:SS, where MM are minutes, SS are seconds

-   MM:SS.FF, where MM are minutes, SS are seconds, FF are frames

### record \[-hzes\] &lt;filename&gt;  
Begins demo recording into ‘demos/*filename*.dm2’, or prints some
statistics if already recording. If neither ‘--extended’ nor
‘--standard’ options are specified, this command uses maximum demo
message size defined by ‘cl\_demomsglen’ cvar.

-h | --help  
display help message

-z | --compress  
compress demo with gzip

-e | --extended  
use extended packet size (4086 bytes)

-s | --standard  
use standard packet size (1390 bytes)

With Q2PRO it is possible to record a demo while playing back another
one.

### stop  
Stops demo recording and prints some statistics about recorded demo.

### suspend  
Pauses and resumes demo recording if ‘cl\_demosuspendtoggle’ is set
to 1. Just pauses demo recording if ‘cl\_demosuspendtoggle’ is set to 0.

### resume  
Resumes demo recording.

When Q2PRO or R1Q2 protocols are in use, demo written to disk is
automatically downgraded to protocol 34. This can result in dropping of
large frames that don’t fit into standard protocol 34 limit. Demo packet
size can be extended to overcome this, but this may render demo
unplayable by other Quake 2 clients or demo editing tools. See the table
below for demo packet sizes supported by different clients. By default,
‘standard’ packet size (1390 bytes) is used. This default can be changed
using ‘cl\_demomsglen’ cvar, or can be overridden per demo by ‘record’
command options.

<table>
<caption>Maximum demo packet sizes supported by clients, bytes</caption>
<colgroup>
<col style="width: 50%" />
<col style="width: 50%" />
</colgroup>
<tbody>
<tr class="odd">
<td style="text-align: left;"><p>Quake 2</p></td>
<td style="text-align: left;"><p>1390</p></td>
</tr>
<tr class="even">
<td style="text-align: left;"><p>R1Q2</p></td>
<td style="text-align: left;"><p>4086</p></td>
</tr>
<tr class="odd">
<td style="text-align: left;"><p>Q2PRO</p></td>
<td style="text-align: left;"><p>32768</p></td>
</tr>
</tbody>
</table>

Maximum demo packet sizes supported by clients, bytes

## Cvar Operations

### toggle &lt;cvar&gt; \[value1 value2 …\]  
If *values* are omitted, toggle the specified *cvar* between 0 and 1. If
two or more *values* are specified, cycle through them.

### inc &lt;cvar&gt; \[value\]  
If *value* is omitted, add 1 to the value of *cvar*. Otherwise, add the
specified floating point *value*.

### dec &lt;cvar&gt; \[value\]  
If *value* is omitted, subtract 1 from the value of *cvar*. Otherwise,
subtract the specified floating point *value*.

### reset &lt;cvar&gt;  
Reset the specified *cvar* to its default value.

### resetall  
Resets all cvars to their default values.

### set &lt;cvar&gt; &lt;value&gt; \[u|s|…\]  
If 2 arguments are given, sets the specified *cvar* to *value*. If 3
arguments are given, and the last argument is ‘u’ or ‘s’, sets *cvar* to
*value* and marks the *cvar* with ‘userinfo’ or ‘serverinfo’ flags,
respectively. Otherwise, sets *cvar* to *value*, which is handled as
consisting from multiple tokens.

### setu &lt;cvar&gt; &lt;value&gt; \[…\]  
Sets the specified *cvar* to *value*, and marks the cvar with ‘userinfo’
flag. *Value* may be composed from multiple tokens.

### sets &lt;cvar&gt; &lt;value&gt; \[…\]  
Sets the specified *cvar* to *value*, and marks the cvar with
‘serverinfo’ flag. *Value* may be composed from multiple tokens.

### seta &lt;cvar&gt; &lt;value&gt; \[…\]  
Sets the specified *cvar* to *value*, and marks the cvar with ‘archive’
flag. *Value* may be composed from multiple tokens.

### cvarlist \[-achlmnrstuv\] \[wildcard\]  
Display the list of registered cvars and their current values with
filtering by cvar name or by cvar flags. If no options are given, all
cvars are listed. Optional *wildcard* argument filters cvars by name.
Supported options are reproduced below.

-a | --archive  
list archived cvars

-c | --cheat  
list cheat protected cvars

-h | --help  
display help message

-l | --latched  
list latched cvars

-m | --modified  
list modified cvars

-n | --noset  
list command line cvars

-r | --rom  
list read-only cvars

-s | --serverinfo  
list serverinfo cvars

-t | --custom  
list user-created cvars

-u | --userinfo  
list userinfo cvars

-v | --verbose  
display flags of each cvar

### macrolist  
Display the list of registered macros and their current values.

## Message Triggers

Message triggers provide a form of automatic command execution when some
game event occurs. Each trigger is composed from a *command* string to
execute and a *match* string. When a non-chat message is received from
server, a list of message triggers is examined. For each trigger,
*match* is macro expanded and wildcard compared with the message,
ignoring any unprintable characters. If the message matches, *command*
is stuffed into the command buffer and executed.

### trigger \[&lt;command&gt; &lt;match&gt;\]  
Adds new message trigger. When called without arguments, prints a list
of registered triggers.

### untrigger \[all\] | \[&lt;command&gt; &lt;match&gt;\]  
Removes the specified trigger. Specify *all* to remove all triggers.
When called without arguments, prints a list of registered triggers.

## Chat Filters

Chat filters allow messages from annoying players to be ignored. Each
chat filter is composed from a *match* string. When a chat message is
received from server, a list of chat filters is examined. For each
filter, *match* is wildcard compared with the message, ignoring any
unprintable characters. If the message matches, it is silently dropped.

There is also simpler form of chat filters: nickname filters that ignore
chat strings from specific *nickname*. They can be replicated with
generic filters and are supported for convenience. Unlike generic chat
filters that support wildcards, nicknames are matched as plain strings,
ignoring any unprintable characters.

### ignoretext \[match …\]  
Adds new generic chat filter. When called without arguments, prints a
list of registered generic filters.

### unignoretext \[all\] | \[match …\]  
Removes the specified generic chat filter. Specify *all* to remove all
filters. When called without arguments, prints a list of registered
generic filters.

### ignorenick \[nickname\]  
Adds new filter to ignore specific *nickname*. This command supports
nickname completion. When called without arguments, prints a list of
registered nickname filters.

### unignorenick \[all\] | \[nickname\]  
Removes filter to ignore specific *nickname*. This command supports
nickname completion. Specify *all* to remove all filters. To remove
literal nickname ‘all’, pass a second argument (can be any string). When
called without arguments, prints a list of registered nickname filters.

## Draw Objects

Draw objects provide a uniform way to display values of arbitrary cvars
and macros on the game screen. By default, text is positioned relative
to the top left corner of the screen, which has coordinates (0, 0). Use
negative values to align text to the opposite edge, e.g. point with
coordinates (-1, -1) is at the bottom right corner of the screen.
Absolute value of each coordinate specifies the distance from the
corresponding screen edge, counted in pixels.

### draw &lt;name&gt; &lt;x&gt; &lt;y&gt; \[color\]  
Add console variable or macro identified by *name* (without the ‘$’
prefix) to the list of objects drawn on the screen at position (*x*,
*y*), drawn in optional *color*.

### undraw \[all\] | &lt;name&gt;  
Remove object identified by *name* from the list of objects drawn on the
screen. Specify *all* to remove all objects.

<!-- -->

    /draw cl_fps -1 -1  // bottom right
    /draw com_time 0 -1 // bottom left

## Screenshots

### screenshot \[format\]  
Standard command to take a screenshot. If *format* argument is given,
takes the screenshot in this format. Otherwise, takes in the format
specified by ‘gl\_screenshot\_format’ variable. File name is picked up
automatically from template specified by ‘gl\_screenshot\_template’
variable.

### screenshotpng \[filename\] \[compression\]  
Takes the screenshot in PNG format. If *filename* argument is given,
saves the screenshot into ‘screenshots/*filename*.png’. Otherwise, file
name is picked up automatically. If *compression* argument is given,
saves with this compression level. Otherwise, saves with
‘gl\_screenshot\_compression’ level.

### screenshotjpg \[filename\] \[quality\]  
Takes the screenshot in JPG format. If *filename* argument is given,
saves the screenshot into ‘screenshots/*filename*.jpg’. Otherwise, file
name is picked up automatically. If *quality* argument is given, saves
with this quality level. Otherwise, saves with ‘gl\_screenshot\_quality’
level.

### screenshottga \[filename\]  
Takes the screenshot in TGA format. If *filename* argument is given,
saves the screenshot into ‘screenshots/*filename*.tga’. Otherwise, file
name is picked up automatically.

## Locations

### loc &lt;add|del|set|list|save&gt;  
Execute locations editing subcommand. Available subcommands:

### add &lt;name&gt;  
Adds new location with the specified *name* at current player position.

### del  
Deletes location closest to player position.

### set &lt;name&gt;  
Sets name of location closest to player position to *name*.

### list  
Lists all locations.

### save \[name\]  
Saves current location list into ‘locs/*name*.loc’ file. If *name* is
omitted, uses current map name.

Edit locations on a local server and don’t forget to execute ‘loc save’
command once you are finished. Otherwise all changes to location list
will be lost on map change or disconnect.

## Miscellaneous

### vid\_restart  
Perform complete shutdown and reinitialization of the renderer and video
subsystem. Rarely needed.

### fs\_restart  
Flush all media registered by the client (textures, models, sounds,
etc), restart the file system and reload the current level.

### r\_reload  
Flush and reload all media registered by the renderer (textures and
models). Weaker form of ‘fs\_restart’.

In Q2PRO, you don’t have to issue ‘vid\_restart’ after changing graphics
settings. Changes to console variables are detected, and appropriate
subsystem is restarted automatically.

### passive  
Toggle passive connection mode. When enabled, client waits for the first
‘passive\_connect’ packet from server and starts usual connection
procedure once this packet is received. This command is useful for
connecting to servers behind NATs or firewalls. See ‘pickclient’
<span id="server"></span> command for more details.

### serverstatus \[address\]  
Request the status string from the server at specified *address*,
display server info and list of players sorted by frags. If connected to
the server, *address* may be omitted, in this case current server is
queried.

### followip \[count\]  
Attempts to connect to the IP address recently seen in chat messages.
Optional *count* argument specifies how far to go back in message
history (it should be positive integer). If *count* is omitted, then the
most recent IP address is used.

### remotemode &lt;address&gt; &lt;password&gt;  
Put client console into rcon mode. All commands entered will be
forwarded to remove server. Press Ctrl+D or close console to exit this
mode.

### ogg &lt;info|play|stop&gt;  
Execute OGG subcommand. Available subcommands:

### info  
Display information about currently playing background music track.

### play &lt;track&gt;  
Start playing background music track ‘music/*track*.ogg’.

### stop  
Stop playing background music track.

### whereis &lt;path&gt; \[all\]  
Search for *path* and print the name of packfile or directory where it
is found. If *all* is specified, prints all found instances of path, not
just the first one.

### softlink &lt;name&gt; &lt;target&gt;  
Create soft symbolic link to *target* with the specified *name*. Soft
symbolic links are only effective when *name* was not found as regular
file.

### softunlink \[-ah\] &lt;name&gt;  
Deletes soft symbolic link with the specified *name*, or all soft
symbolic links. Supported options are reproduced below.

-a | --all  
delete all links

-h | --help  
display help message

# Incompatibilities

Q2PRO client tries to be compatible with other Quake 2 ports, including
original Quake 2 release. Compatibility, however, is defined in terms of
full file format and network protocol compatibility. Q2PRO is not meant
to be a direct replacement of your regular Quake 2 client. Some features
are implemented differently in Q2PRO, some may be not implemented at
all. You may need to review your config and adapt it for Q2PRO. This
section tries to document most of these incompatibilities so that when
something doesn’t work as it used to be you know where to look. The
following list may be incomplete.

-   Q2PRO has a built-in OpenGL renderer and doesn’t support dynamic
    loading of external renderers. Thus, ‘vid\_ref’ cvar has been made
    read-only and exists only for compatibility with tools like Q2Admin.

-   Q2PRO supports loading system OpenGL library only. Thus,
    ‘gl\_driver’ cvar has been made read-only and exists only for
    compatibility with tools like Q2Admin.

-   Changes to ‘gl\_modulate’ variable in Q2PRO take effect immediately.
    To set separate modulation factors for world lightmaps and entities
    please use ‘gl\_modulate\_world’ and ‘gl\_modulate\_entities’
    variables.

-   Default value of R1GL-specific ‘gl\_dlight\_falloff’ variable has
    been changed from 0 to 1.

-   ‘gl\_particle\_\*’ series of variables are gone, as well as
    ‘gl\_ext\_pointparameters’ and R1GL-specific
    ‘gl\_ext\_point\_sprite’. For controlling size of particles, which
    are always drawn as textured triangles, Q2PRO supports its own
    ‘gl\_partscale’ variable.

-   ‘ip’ variable has been renamed to ‘net\_ip’.

-   ‘clientport’ variable has been renamed to ‘net\_clientport’, and
    ‘ip\_clientport’ alias is no longer supported.

-   ‘demomap’ command has been removed in favor of ‘demo’ and ‘mvdplay’.

-   Q2PRO works only with virtual paths constrained to the quake file
    system. All paths are normalized before use so that it is impossible
    to go past virtual filesystem root using ‘../’ components. This
    means commands like these are equivalent and all reference the same
    file: ‘exec ../global.cfg’, ‘exec /global.cfg’, ‘exec global.cfg’.
    If you have any config files in your Quake 2 directory root, you
    should consider moving them into ‘baseq2/’ to make them accessible.

-   Likewise, ‘link’ command syntax has been changed to work with
    virtual paths constrained to the quake file system. All arguments to
    ‘link’ are normalized.

-   Joysticks are not supported.

-   Single player savegame format has been rewritten from scratch for
    better robustness and portability. Only the ‘baseq2’ game library
    included in Q2PRO distribution has been converted to use the new
    improved savegame format. Q2PRO will refuse to load and save games
    in old format for security reasons.
