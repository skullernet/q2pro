#!/usr/bin/python3

# Helper: Copy DLL built by game subproject to Meson output directory

import argparse
import os
import pathlib
import shutil
import stat

args = argparse.ArgumentParser()
args.add_argument('stamp_file', help="timestamp file")
args.add_argument('game_dll', help="game shared library path")
args.add_argument('output_dir', help="output directory")
arg_values = args.parse_args()

game_dll_path = pathlib.Path(arg_values.game_dll)
output_path = pathlib.Path(arg_values.output_dir)

# Copy game DLL to output directory
dest_path = output_path / game_dll_path.name
shutil.copy2(game_dll_path, dest_path)

# Additional file extensions which are copied over as well, if existing.
# Currently, Windows debug symbols.
ADDITIONAL_EXTS = [".pdb"]
for ext in ADDITIONAL_EXTS:
    candidate = game_dll_path.with_suffix(ext)
    if candidate.exists():
        dest_path = output_path / candidate.name
        shutil.copy2(candidate, dest_path)

# Finally, copy metadata to stamp file.
open(arg_values.stamp_file, "w") # create, if not yet existing
shutil.copystat(arg_values.game_dll, arg_values.stamp_file)
# Reset executable bits
os.chmod(arg_values.stamp_file, os.stat(arg_values.stamp_file).st_mode & ~(stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH))
