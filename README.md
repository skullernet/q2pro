# Q2JUMP-PRO

This is a remake of the [q2pro-jump](https://github.com/TotallyMehis/q2pro-jump)
client for Quake 2 with some additional features and improvements specifically
for playing the [q2jump](http://q2jump.net) mod. The q2pro-jump was forked
from the [q2pro-speed](https://github.com/kugelrund/q2pro-speed), a custom client
focused on single-player speed runs, which itself was forked from
[q2pro](https://github.com/skullernet/q2pro), an enhanced multiplayer-oriented
Quake 2 client and server. While q2pro is still actively maintained, the
q2pro-jump and q2pro-speed clients have been unmaintained for some time and
thus have become out of date and hard to keep synchronised with the latest
upstream q2pro changes.

The main goal of this project is to reimplement the essential q2pro-jump 
features and try to keep it in sync with q2pro.

## Current status

### Implemented features

- Added [strafe_helper](https://github.com/kugelrund/strafe_helper) acceleration
  HUD
- Added the dynamic colorization of `cl_ups` when using
  `draw cl_ups <x> <y> dynamic`
- Added the additional console auto-completions for commands available in the
  q2jump mod.

These have been almost directly copied from the q2pro-jump client, with some
minor modifications to make them work with the latest q2pro codebase.

### Missing features

- Server code is currently vanilla q2pro, not q2jump.
- An introduction to the q2jump mod with a tutorial and video links would be
  nice.

## Building

Follow the instructions in the
[q2pro INSTALL.md](https://github.com/skullernet/q2pro/blob/master/INSTALL.md)

It should be possible to compile a version for at least Windows, Linux, and
macOS.
