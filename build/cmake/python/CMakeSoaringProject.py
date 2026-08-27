#!/usr/bin/env python3

import os, sys, subprocess

Configuration = 'Multi'

# Which configuration to BUILD and RUN inside a multi-config build dir
# ('Multi').  Selectable without editing this file:
#     set XCSOAR_CONFIG=Release       (default: Debug)
# If Configuration above is set to 'Release'/'Debug' (legacy split
# build dirs msvc...release / msvc...debug), that choice wins.
_env_config = os.environ.get('XCSOAR_CONFIG', '').strip().capitalize()
if Configuration in ('Release', 'Debug'):
    BuildConfig = Configuration
elif _env_config in ('Release', 'Debug'):
    BuildConfig = _env_config
else:
    BuildConfig = 'Debug'


prev_batch = None
cmake_generator = None
program_dir = None
is_windows = False
build_system = 'unknown'
toolchain_file = None
num_proc=8

def gcc(toolchain, env):
  global cmake_generator
  global prev_batch
  global program_dir
  global toolchain_file
  src_dir = os.getcwd()
  if sys.platform.startswith('win'):
      if toolchain == 'mingw' or toolchain.startswith('mgw'):
         toolchain_file = src_dir.replace('\\','/') + '/build/cmake/toolchains/MinGW.toolchain'
      print('src_dir = ',src_dir)

      cmake_generator = 'MinGW Makefiles'
      return program_dir.replace('/', '\\') + '\\MinGW\\' + toolchain + '\\bin;' + env['PATH']
  else:
     cmake_generator ='Ninja'
     env_path = env['PATH']
  return env_path

def clang(toolchain, env):
  global cmake_generator
  global toolchain_file

  env_path = env['PATH']
  if sys.platform.startswith('win'):
     if toolchain == 'clangXX':
        env_path = program_dir.replace('/', '\\') + '\\LLVM\\' + toolchain + '-llvm\\bin;' + env_path['PATH']
     elif toolchain == 'clang12':
        env_path = program_dir.replace('/', '\\') + '\\LLVM\\' + toolchain + '\\bin;' + env_path
     else:
        env_path = program_dir.replace('/', '\\') + '\\LLVM\\' + toolchain + '\\bin;' + env_path
     toolchain_file = os.getcwd().replace('\\','/') + '/build/cmake/toolchains/WinClang.toolchain'
  else:
     env_path = env['PATH']
  cmake_generator ='Ninja'
  return env_path

generator = {
           'mgw73' : 'MinGW Makefiles',
           'mgw82' : 'MinGW Makefiles',
           'mgw103' : 'MinGW Makefiles',
           'mgw112' : 'MinGW Makefiles',
           'mgw122' : 'MinGW Makefiles',
           'mgw143' : 'MinGW Makefiles',
           'mgw152' : 'MinGW Makefiles',
           'ninja' : 'Ninja',
           'unix' : 'Unix Makefiles',
           'mingw' : 'MinGW Makefiles',
           'clang10' : 'Clang',
           'clang11' : 'Clang',
           'clang12' : 'Clang',
           'clang13' : 'Clang',
           'clang14' : 'Clang',
           'clang15' : 'Clang',
           'clang16' : 'Clang',
           'clang17' : 'Clang',
           'clang19' : 'Clang',
           'clang21' : 'Clang',
           'msvc2015' : 'Visual Studio 14',
           'msvc2017' : 'Visual Studio 15',
           'msvc2019' : 'Visual Studio 16',
           'msvc2022' : 'Visual Studio 17',
           'msvc2026' : 'Visual Studio 18',
}

def find_vcvars(toolchain):
  """Locate vcvars*.bat: first via vswhere (standard installs under
  C:/Program Files), then via the classic hardcoded paths (D:/Programs)."""
  import subprocess
  candidates = []
  vswhere = os.path.expandvars(
      r'%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe')
  if os.path.isfile(vswhere):
    try:
      out = subprocess.check_output(
          [vswhere, '-latest', '-prerelease', '-products', '*',
           '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
           '-property', 'installationPath'], text=True).strip()
      for inst in out.splitlines():
        candidates.append(inst + r'\VC\Auxiliary\Build\vcvars64.bat')
        candidates.append(inst + r'\VC\Auxiliary\Build\vcvarsx64.bat')
    except Exception as e:
      print('vswhere failed:', e)
  # standard installations under %ProgramFiles% (forward slashes work fine)
  pf = os.path.expandvars('%ProgramFiles%').replace('\\', '/')
  for edition in ('Insiders', 'Preview'):
    candidates.append(pf + '/Microsoft Visual Studio/18/' + edition + '/VC/Auxiliary/Build/vcvarsx64.bat')
    candidates.append(pf + '/Microsoft Visual Studio/18/' + edition + '/VC/Auxiliary/Build/vcvars64.bat')
  for edition in ('Community', 'Professional', 'Enterprise', 'Preview'):
    candidates.append(pf + '/Microsoft Visual Studio/2022/' + edition + '/VC/Auxiliary/Build/vcvars64.bat')
  # classic fallbacks (Flaps6 layout)
  if program_dir:
    if toolchain == 'msvc2022':
      candidates.append(program_dir + '/Microsoft Visual Studio/2022/Preview/VC/Auxiliary/Build/vcvars64.bat')
    elif toolchain == 'msvc2026':
      candidates.append(program_dir + '/Microsoft Visual Studio/18/Insiders/VC/Auxiliary/Build/vcvarsx64.bat')
  for c in candidates:
    if os.path.isfile(c):
      print('vcvars found:', c)
      return c
  print('vcvars NOT found; tried:')
  for c in candidates:
    print('  -', c)
  return None

def visual_studio(toolchain, env):
  global prev_batch, cmake_generator
  if toolchain not in ('msvc2015', 'msvc2017', 'msvc2019', 'msvc2022', 'msvc2026'):
    print('wrong toolchain: ', toolchain, '!')
    exit(1)
  prev_batch = find_vcvars(toolchain)  # informational; the VS generator finds VS itself
  cmake_generator = generator[toolchain]
  return env['PATH']

  # map the inputs to the function blocks
compiler_setup = {
           'mgw73' : gcc,
           'mgw82' : gcc,
           'mgw103' : gcc,
           'mgw112' : gcc,
           'mgw122' : gcc,
           'mgw143' : gcc,
           'mgw152' : gcc,
           'ninja' : clang,
           'unix' : gcc,
           'mingw' : gcc,
           'clang10' : clang,
           'clang11' : clang,
           'clang12' : clang,
           'clang13' : clang,
           'clang14' : clang,
           'clang15' : clang,
           'clang16' : clang,
           'clang17' : clang,
           'clang19' : clang,
           'clang21' : clang,
           'msvc2015' : visual_studio,
           'msvc2017' : visual_studio,
           'msvc2019' : visual_studio,
           'msvc2022' : visual_studio,
           'msvc2026' : visual_studio,
}

def create_xcsoar(args):
  global program_dir
  global is_windows
  is_windows = False

  filename = sys.argv[0]
  project_name = args[0]
  branch = args[1]
  toolchain = args[2]

  # the source root is three levels above this script
  # (<root>/build/cmake/python/) - derive it from the script location
  # with normalized slashes: the old string-replace missed the
  # backslash spelling ("python build\cmake\python\Start-...") and
  # handed CMake the python dir as source dir
  start_dir = os.path.dirname(os.path.abspath(filename)).replace('\\', '/')
  if start_dir.endswith('/build/cmake/python'):
    start_dir = start_dir[:-len('/build/cmake/python')]
  if len(start_dir) == 0:
     start_dir = os.getcwd();
  print('Start CMake Creation of ', project_name, ' / ', branch, ' / ', toolchain)
  print('BuildConfig (XCSOAR_CONFIG) = ', BuildConfig)
  print('====================================\n')
  print('CurrDir  :',os.getcwd())
  print('StartDir :',start_dir)
  # current git state ("Kontrollwert"): short hash + commit title +
  # branch - the same at-a-glance line as on Linux.  Printed here AND
  # again at the very end of the run, so nobody has to scroll back up.
  git_line = ''
  try:
    git_head = subprocess.run(
        ['git', '-C', start_dir, 'log', '-1', '--format=%h %s'],
        capture_output = True, text = True, timeout = 10).stdout.strip()
    git_branch = subprocess.run(
        ['git', '-C', start_dir, 'rev-parse', '--abbrev-ref', 'HEAD'],
        capture_output = True, text = True, timeout = 10).stdout.strip()
    if git_head:
      git_line = git_head + '  [' + git_branch + ']'
      print('Git      :', git_line)
  except Exception:
    pass  # no git, no line - never break the build over cosmetics

  my_env = os.environ.copy()
  # ImageMagick sometimes deadlocks on Windows when its OpenMP thread
  # pool initialises while MSBuild runs several magick.exe instances in
  # parallel ("Convert graphics!" hangs until Ctrl+C).  One worker
  # thread per process is plenty for our small bitmaps and makes the
  # conversions deterministic.
  my_env.setdefault('MAGICK_THREAD_LIMIT', '1')
  # and let every Python-based generator (bin2c, GenerateResourceLookup,
  # news_to_quickguide_md, ...) read/write UTF-8 regardless of the
  # Windows ANSI code page
  my_env.setdefault('PYTHONUTF8', '1')
  creation = 15
  if len(sys.argv) > 3:
    creation = int(sys.argv[3])
  verbose = creation & 0x100
  print('creation-flag = ', str(creation))
  if True:
    i = 0
    for arg in args:
      print(i, ': ', arg)
      i = i + 1
    if verbose:
      print('Debug-Stop')
      input("Press Enter to continue...")

  if sys.platform.startswith('win'):
    is_windows = True
    # configurable via environment, with existence-based fallbacks:
    #   XCSOAR_PROJECT_DIR  - projects root   (default: D:/Projects, C:/Projects, ~/Projects)
    #   XCSOAR_PROGRAM_DIR  - programs root   (default: D:/Programs, C:/Program Files)
    #   XCSOAR_LINK_LIBS    - prebuilt libs   (default: <project_dir>/link_libs)
    #   XCSOAR_THIRD_PARTY  - 3rd-party root  (default: D:/Libs, <project_dir>/Libs)
    def _first_existing(paths, default):
      for p in paths:
        if p and os.path.isdir(p):
          return p
      return default
    def _projects_ancestor(path):
      # if the source tree lives inside a .../Projects/... folder,
      # use that as the project root (e.g. C:/Projects/OpenSoaring/XCSoar
      # -> C:/Projects) - outputs then land on the same drive as the code
      p = os.path.abspath(path).replace('\\', '/')
      while True:
        if os.path.basename(p).lower() == 'projects':
          return p
        parent = os.path.dirname(p)
        if parent == p:
          return None
        p = parent
    project_dir = os.environ.get('XCSOAR_PROJECT_DIR') \
        or _projects_ancestor(start_dir) \
        or _first_existing(
        ['D:/Projects', 'C:/Projects'],
        os.path.expanduser('~/Projects').replace('\\', '/'))
    program_dir = os.environ.get('XCSOAR_PROGRAM_DIR') or _first_existing(
        ['D:/Programs', 'C:/Programs'], 'C:/Program Files')
    project_dir = project_dir.replace('\\', '/')
    program_dir = program_dir.replace('\\', '/')
    print('project_dir =', project_dir, ' program_dir =', program_dir)
    src_dir = start_dir
    if branch:
       binary_dir= project_dir + '/Binaries/' + project_name + '/' + branch
    else:
       binary_dir= project_dir + '/Binaries/' + project_name + '/build'
    link_libs = os.environ.get('XCSOAR_LINK_LIBS') or (project_dir + '/link_libs')
    build_dir = binary_dir + '/'+ toolchain


    third_party = os.environ.get('XCSOAR_THIRD_PARTY') or \
        ('D:/Libs' if os.path.isdir('D:/Libs') else project_dir + '/Libs')
    install_dir = program_dir + '/Install/' + project_name
  else:
    src_dir = start_dir
    root_dir = my_env['HOME']
    project_dir = root_dir + '/Projects'
    program_dir = root_dir + '/Programs'
    binary_dir= start_dir + '/_build'
    build_dir = binary_dir + '/'+ toolchain
    link_libs = project_dir + '/link_libs'
    third_party = 'D:/LibsX'
    install_dir = program_dir + '/Install/' + project_name

  toolset = None

  python_exe = None

  try:
    myprocess = subprocess.Popen(['python', '--version'], env = my_env)
    myprocess.wait()
    python_exe = 'python'
  except:
    print('"python" not callable')
  if not python_exe:
    try:
      myprocess = subprocess.Popen(['python3', '--version'], env = my_env)
      myprocess.wait()
      python_exe = 'python3'
    except:
      print('"python3" not callable')

  if sys.platform.startswith('win'):
    cmake_exe = (program_dir  + '/CMake/bin/') + 'cmake.exe'
    if os.path.isfile(cmake_exe):
      my_env['PATH'] =  (program_dir  + '/CMake/bin;').replace('/', '\\') + my_env['PATH']
    else:
      cmake_exe = 'cmake'  # standard install: use cmake from PATH
  else:
    cmake_exe = 'cmake'


  try:
    myprocess = subprocess.Popen([cmake_exe, '--version'], env = my_env)
    myprocess.wait()
  except:
    print('"cmake" not callable!')
    creation = 0

  if not os.path.exists(build_dir):
    os.makedirs(build_dir)
    creation = creation | 1

  my_env['PATH'] = compiler_setup[toolchain](toolchain, my_env)
  print(my_env['PATH'])


  print('Creation-Flag: ', creation)
  if prev_batch:
    print(prev_batch)

  if build_dir.endswith(('msvc2022', 'msvc2026')):
    if Configuration == 'Release':
      build_dir = build_dir + 'release'
    elif Configuration == 'Debug':
      build_dir = build_dir + 'debug'
  #========================================================================
  if creation & 1:
    print('Python Step 1 - Configure CMake')
    if os.path.isfile(build_dir+ '/CMakeCache.txt'):
      os.remove(build_dir+ '/CMakeCache.txt')
    arguments = []
    arguments.append(cmake_exe)
    arguments.append('-S')
    arguments.append(src_dir)
    arguments.append('-B')
    arguments.append(build_dir)
    arguments.append('-G')
    arguments.append(cmake_generator)

    if Configuration == 'Release' or Configuration == 'Debug':
      arguments.append('-DCMAKE_BUILD_TYPE=' + Configuration)
      arguments.append('--debug-trycompile')
    elif toolchain.startswith('msvc'):
      # multi-config dir ('Multi'): CMAKE_BUILD_TYPE steers only the
      # 3rd-party superbuild (which config gets built and installed to
      # link_libs); the app follows the IDE / --config choice
      arguments.append('-DCMAKE_BUILD_TYPE=' + BuildConfig)

    if is_windows and toolchain.startswith('clang'):
      compiler = toolchain
    if is_windows and toolchain == 'ninja':
      toolchain = 'clang17'
      compiler = toolchain

    print('---')
    if is_windows:
      print('!!! COMPUTERNAME = ', my_env['COMPUTERNAME'],  ', USERNAME = ', my_env['USERNAME'], '!!!')
      if toolchain_file:
        arguments.append('-DCMAKE_TOOLCHAIN_FILE:PATH=' + toolchain_file)
      if build_system.startswith('android'):
        arguments.append('-DCMAKE_TOOLCHAIN_FILE:PATH=\"' + program_dir + '/Android/android-ndk-r25b/build/cmake/android.toolchain.cmake\"')
    else:
      if toolchain == 'mingw':
        arguments.append('-DCMAKE_TOOLCHAIN_FILE:PATH=' + src_dir.replace('\\','/') + '/build/cmake/toolchains/MinGW.toolchain')
      else:
        arguments.append('-DCMAKE_TOOLCHAIN_FILE:PATH=' + src_dir.replace('\\','/') + '/build/cmake/toolchains/LinuxGCC.toolchain')
      print('!!! USER = ', my_env['USER'], '!!!')

    arguments.append('-DTOOLCHAIN=' + toolchain)
    if toolchain.startswith('msvc'):
      # all Windows msvc builds are the OpenGL flavor (GDI retired)
      arguments.append('-DXCSOAR_USE_OPENGL=ON')

    # flavor switch: XCSOAR_TESTING=ON -> red testing build,
    # XCSOAR_TESTING=OFF -> green release build; unset -> the
    # toolchain file's default (currently ON) applies.  CI derives it
    # from the tag: v7.45.25.t1 -> testing, v7.45.25 -> release.
    testing = os.environ.get('XCSOAR_TESTING')
    if testing:
      on = testing.upper() in ('1', 'ON', 'TRUE', 'YES')
      arguments.append('-DTARGET_TESTING=' + ('ON' if on else 'OFF'))

    arguments.append('-DTHIRD_PARTY=' + third_party)
    arguments.append('-DLINK_LIBS=' + link_libs)
    arguments.append('-Wno-dev')

    arguments.append('-DCMAKE_INSTALL_PREFIX=' + install_dir)
    if not toolset is None:
      arguments.append('-T' + toolset)


    myprocess = subprocess.Popen(arguments, env = my_env, shell = False)
    myprocess.wait()
    if myprocess.returncode != 0:
      creation = 0
      print('cmd with failure! (', myprocess, ')')
    else:
        if verbose:
            print('Debug-Stop')
            input("Press Enter to continue...")

  #========================================================================
  if creation & 2:
    print('Python Step 2 - Build with cmake')
    print('--------------------------------')
    arguments = []
    arguments.append(cmake_exe)
    arguments.append('--build')
    arguments.append(build_dir)
    if toolchain.startswith('msvc'): #  or toolchain.startswith('mgw'):
      arguments.append('--config')
      arguments.append(BuildConfig)
    else : # not msvc or mgw:
      print("no multiconfig toolchain: no '--Config Release'!")

    # parallel build: 'cmake --build --parallel N' hands /m:N to MSBuild
    # (project-level parallelism; without it MSBuild builds one project
    # at a time) resp. -jN to make/ninja
    jobs = os.cpu_count() or 4
    arguments.append('--parallel')
    arguments.append(str(jobs))

    if not toolchain.startswith('msvc'):
        arguments.append('--')  # nachfolgende Befehle werden zum Build tool durchgereicht
        arguments.append('-j')
        arguments.append('8')
    print("Arguments: ", arguments)
    myprocess = subprocess.Popen(arguments, env = my_env, shell = False)
    myprocess.wait()
    if myprocess.returncode != 0:
      creation = 0
      print('cmd: (', arguments, ')')
      print('cmd with failure! (', myprocess, ')')
    else:
        if verbose:
            print('Debug-Stop')
            input("Press Enter to continue...")

  #========================================================================
  if creation & 0x04:
    print('Python Step 3 - Install')
    arguments = []
    arguments.append(cmake_exe)
    arguments.append('--install')
    arguments.append(build_dir)
    myprocess = subprocess.Popen(arguments, env = my_env, shell = False)
    myprocess.wait()
    if myprocess.returncode != 0:
      creation = 0
      print('cmd with failure! (', myprocess, ')')
    else:
        if verbose:
            print('Debug-Stop')
            input("Press Enter to continue...")

  #========================================================================
  if creation & 0x08:   # ==>Run
    print('Python Step 4 - Execute OpenSoar')
    if toolchain.startswith('msvc'):
      # multi-config generator: the exe lands in the per-config subdir
      build_dir = build_dir + '/' + BuildConfig
      app_exe = project_name + '.exe'
    elif toolchain.startswith('mgw'):
      app_exe = project_name + '-MinGW.exe'
    elif toolchain.startswith('clang'):
      app_exe = project_name + '-Clang.exe'

    # each brand runs against its own data directory (they differ in
    # version check, info boxes, ...): <base>/XCSoarData resp.
    # <base>/OpenSoarData.  The base comes from the environment
    # (machine-specific, e.g.  set XCSOAR_BASEDATA=E:/Data); without it
    # no -datapath is passed and the app uses its own platform default.
    arguments = [build_dir + '/' + app_exe, '-1400x800', '-profile=August']
    basedata = os.environ.get('XCSOAR_BASEDATA')
    if basedata:
      arguments.append('-datapath=' + basedata.rstrip('/\\') + '/'
                       + project_name + 'Data')
    print('Step 4 command:', subprocess.list2cmdline(arguments))
    if not os.path.exists(arguments[0]):
        print("App '", arguments[0], "' doesn't exist!")
        creation = 0
    else:
        myprocess = subprocess.Popen(arguments, env = my_env, shell = False)
        myprocess.wait()
        if myprocess.returncode != 0:
            creation = 0
            print('cmd with failure! (', myprocess, ')')

  # end-of-run summary: repeat the key facts from the header so they
  # are visible without scrolling all the way up
  print('\n====================================')
  print('Project  :', project_name, '/', toolchain, '/', BuildConfig)
  if git_line:
    print('Git      :', git_line)
  if(not creation):
    print('Result   : FAILED')
    print('Error in cmake python script')
    # propagate the failure: callers (Compile-*.cmd, CI) check the
    # exit code - without this a broken build looked successful
    sys.exit(1)
  print('Result   : OK')
