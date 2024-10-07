---
name: 'Report a bug'
about: 'Create a bug report'
title: ''
labels: ''
assignees: ''

---

### Before reporting bugs

Make sure the bug is reproducible with latest Q2PRO version. If you compile
Q2PRO yourself, update to the latest version from git master. If you are using
prebuilt Windows binaries, update to the latest nightly build.

### Important information

Provide the following information:
- Q2PRO version
- OS version
- GPU driver and version

For Linux:
- Linux distribution and version
- Window manager version

### Reproduction steps

Steps to reproduce the behavior.

### Expected behavior

A clear and concise description of what you expected to happen.

### Actual behavior

A clear and concise description of what actually happened.

### Screenshots

If reporting graphics glitches, provide screenshot or video.

### Log file

Provide a link to the log file created by launching `q2pro +set developer 1
+set logfile 1`.

### Crash reports

If Q2PRO crashes, provide a crash report (Windows) or a backtrace (Linux). On
Linux, backtrace can be created by launching Q2PRO with `gdb q2pro --args
[...]` and typing `bt` after the crash.

### Compilation issues

If reporting a building / compilation issue, provide `meson setup` command
line and full console output.
