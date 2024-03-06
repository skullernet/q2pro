#!/usr/bin/python3

# Helper: Copy DLL built by game subproject to Meson output directory

import argparse
import os
import pathlib
import shutil
import stat

args = argparse.ArgumentParser()
args.add_argument('output_name', help="output file name")
args.add_argument('game_dll', help="game shared library path")
arg_values = args.parse_args()

game_dll_path = pathlib.Path(arg_values.game_dll)
dest_path = pathlib.Path(arg_values.output_name)
output_path = dest_path.parent

# Copy game DLL to output directory
shutil.copy2(game_dll_path, dest_path)

# Additional file extensions which are copied over as well, if existing.
# Currently, Windows debug symbols.
ADDITIONAL_EXTS = [".pdb"]
for ext in ADDITIONAL_EXTS:
    candidate = game_dll_path.with_suffix(ext)
    if candidate.exists():
        dest_path = output_path / candidate.name
        shutil.copy2(candidate, dest_path)
