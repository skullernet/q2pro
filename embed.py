#!/usr/bin/python3

# loosely based on the following answer on SO: https://stackoverflow.com/a/61128721

import os, sys, shutil

# get absolute input and output paths
input_path = sys.argv[1]
output_path = sys.argv[2]

# make sure destination directory exists
dest_dir = os.path.dirname(output_path)
if len(dest_dir) > 0:
    os.makedirs(dest_dir, exist_ok = True)

# read in the file
in_file = open(input_path, "r")
in_contents = in_file.read()
in_file.close()

# convert
in_contents = in_contents.replace('\\', '\\\\')
in_contents = in_contents.replace('\n', '\\n')
in_contents = in_contents.replace('\r', '\\r')
in_contents = in_contents.replace('"', '\\"')

in_contents = 'const char *const ' + sys.argv[3] + ' = ' + '\"' + in_contents + '\";'

# write
out_file = open(output_path, "w")
out_contents = out_file.write(in_contents)
out_file.close()
