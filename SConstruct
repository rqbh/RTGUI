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
TARGETS = [['demo', 'demo'],
	       ['realtouch', 'realtouch']]

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

# build for testcases 
list = os.listdir(os.path.join(str(Dir('#')), 'test_cases'))
for d in list:
    src_path = os.path.join(str(Dir('#')), 'test_cases', d)
    if os.path.isfile(os.path.join(src_path, 'SConscript')):
        exe_name = os.path.basename(src_path)
        objs = base_objs + SConscript(dirs=[src_path],
                                       variant_dir=path_join('build', src_path),
                                       duplicate=0)
        # build program
        env.Program(path_join(exe_dir, exe_name), objs)

        # end building
        EndBuilding(exe_name)
