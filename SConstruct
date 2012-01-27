import os
import sys
import shutil
from os.path import join as path_join

RTT_ROOT = os.path.normpath(os.getcwd())
sys.path = sys.path + [path_join(RTT_ROOT, 'win32')]
from building import *
import rtconfig

exe_dir = 'executables'

# list of targets, list item format in:
#     ['executable_name', 'path_to_SConscript']
TARGETS = [#['demo', 'demo'],
           ['swin', path_join('test_cases', 'simple_win')]
          ]

env = Environment()

Export('RTT_ROOT')
Export('rtconfig')

# prepare building environment
base_objs = PrepareBuilding(env, RTT_ROOT)

for exe_name, src_path in TARGETS:
    work_objs = base_objs + SConscript(dirs=[src_path],
                                       variant_dir=path_join('build', src_path),
                                       duplicate=0)
    # build program
    env.Program(path_join(exe_dir, exe_name), work_objs)

    # end building
    EndBuilding(exe_name)
