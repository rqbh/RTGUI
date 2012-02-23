import os
import shutil
import string
from SCons.Script import *

BuildOptions = {}
Projects = []
Rtt_Root = ''
Env = None

def _get_filetype(fn):
    if fn.rfind('.c') != -1 or fn.rfind('.C') != -1 or fn.rfind('.cpp') != -1:
        return 1

    # assimble file type
    if fn.rfind('.s') != -1 or fn.rfind('.S') != -1:
        return 2

    # header type
    if fn.rfind('.h') != -1:
        return 5

    # other filetype
    return 5

def splitall(loc):
    """
    Return a list of the path components in loc. (Used by relpath_).

    The first item in the list will be  either ``os.curdir``, ``os.pardir``, empty,
    or the root directory of loc (for example, ``/`` or ``C:\\).

    The other items in the list will be strings.

    Adapted from *path.py* by Jason Orendorff.
    """
    parts = []
    while loc != os.curdir and loc != os.pardir:
        prev = loc
        loc, child = os.path.split(prev)
        if loc == prev:
            break
        parts.append(child)
    parts.append(loc)
    parts.reverse()
    return parts

def _make_path_relative(origin, dest):
    """
    Return the relative path between origin and dest.

    If it's not possible return dest.


    If they are identical return ``os.curdir``

    Adapted from `path.py <http://www.jorendorff.com/articles/python/path/>`_ by Jason Orendorff.
    """
    origin = os.path.abspath(origin).replace('\\', '/')
    dest = os.path.abspath(dest).replace('\\', '/')
    #
    orig_list = splitall(os.path.normcase(origin))
    # Don't normcase dest!  We want to preserve the case.
    dest_list = splitall(dest)
    #
    if orig_list[0] != os.path.normcase(dest_list[0]):
        # Can't get here from there.
        return dest
    #
    # Find the location where the two paths start to differ.
    i = 0
    for start_seg, dest_seg in zip(orig_list, dest_list):
        if start_seg != os.path.normcase(dest_seg):
            break
        i += 1
    #
    # Now i is the point where the two paths diverge.
    # Need a certain number of "os.pardir"s to work up
    # from the origin to the point of divergence.
    segments = [os.pardir] * (len(orig_list) - i)
    # Need to add the diverging part of dest_list.
    segments += dest_list[i:]
    if len(segments) == 0:
        # If they happen to be identical, use os.curdir.
        return os.curdir
    else:
        # return os.path.join(*segments).replace('\\', '/')
        return os.path.join(*segments)

def PrepareBuilding(env, root_directory):
    import SCons.cpp
    import rtconfig

    global BuildOptions
    global Projects
    global Env
    global Rtt_Root

    Env = env
    Rtt_Root = root_directory

    Repository(Rtt_Root)
    # include components
    objs = SConscript('components/SConscript', variant_dir='build/components', duplicate=0)
    objs.append(SConscript('win32/SConscript', variant_dir='build/win32', duplicate=0))
    Env['LIBS'] = ['SDL', 'SDLmain', 'msvcrt', 'user32', 'kernel32', 'gdi32.lib']
    if Env['MSVC_VERSION'] == '6.0':
        print 'Visual C++ 6.0'
        Env['SDL_LIBPATH'] = Rtt_Root + '/win32/SDL/lib_vc6'
    else:
        Env['SDL_LIBPATH'] = Rtt_Root + '/win32/SDL/lib'
    Env.Append(LIBPATH=Env['SDL_LIBPATH'])
    Env.Append(CCFLAGS=['/MT', '/ZI', '/Od', '/W 3', '/WL'])
    Env.Append(LINKFLAGS='/SUBSYSTEM:WINDOWS /NODEFAULTLIB /MACHINE:X86 /DEBUG')
    Env.Append(ENV = os.environ)

    return objs

def GetDepend(depend):
    no_support = ['RT_USING_FINSH', 'RT_USING_FTK', 'RT_USING_LUA', 'RT_USING_CAIRO']
    if depend == 'RT_USING_FTK' or depend == ['RT_USING_FTK']:
        return False
    
    if type(depend) == type('str'):
        if no_support.count(depend):
                return False

    # for list type depend 
    for item in depend:
        if item != '':
            if no_support.count(item):
                return False

    return True 

def RemoveObjs(objs, item):
    for obj in objs:
        if type(obj) == type(objs):
            if RemoveObjs(obj, item) == True:
                return True
        else:
            if obj.rstr().find(item) != -1:
                objs.remove(obj)
                return True

    return False

def AddDepend(option):
    BuildOptions[option] = 1

def DefineGroup(name, src, depend, **parameters):
    global Env
    if not GetDepend(depend):
        return []

    group = parameters
    group['name'] = name
    if type(src) == type(['src1', 'str2']):
        group['src'] = File(src)
    else:
        group['src'] = src

    Projects.append(group)

    if group.has_key('CCFLAGS'):
        Env.Append(CCFLAGS = group['CCFLAGS'])
    if group.has_key('CPPPATH'):
        Env.Append(CPPPATH = group['CPPPATH'])
    if group.has_key('CPPDEFINES'):
        Env.Append(CPPDEFINES = group['CPPDEFINES'])
    if group.has_key('LINKFLAGS'):
        Env.Append(LINKFLAGS = group['LINKFLAGS'])

    objs = Env.Object(group['src'])
    return objs

def EndBuilding(target):
    pass

def GetPackage(url):
    import urllib
 
    fn = url.rfind("/")
    fn = url[fn:]
    print "\nTry to download: " + fn + " From: " + url + "\n"
    url2 = urllib.urlopen(url)
    size = url2.info()['Content-Length']
    print url, "->", fn, "- Size:", size, "\r"
    urllib.urlretrieve(url, fn)

def GetCurrentDir():
    conscript = File('SConscript')
    fn = conscript.rfile()
    name = fn.name
    path = os.path.dirname(fn.abspath)
    return path
