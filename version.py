#!/usr/bin/python3

import subprocess

try:
    with open('VERSION', encoding='utf-8') as f:
        print(f.readline().strip())
except:
    try:
        kwargs = { 'capture_output': True, 'encoding': 'utf-8', 'check': True }
        rev = subprocess.run(['git', 'rev-list',  '--count', 'HEAD'], **kwargs).stdout.strip()
        sha = subprocess.run(['git', 'rev-parse', '--short', 'HEAD'], **kwargs).stdout.strip()
        print(f'r{rev}~{sha}')
    except:
        print('r666~unknown')
