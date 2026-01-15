# CMake Patch for project SODIUM (sodium, libsodium)

import os, sys, subprocess, shutil

if len(sys.argv) < 1:
    print('No patch: ', sys.argv)
    exit(1)

patch_dir = os.path.dirname(sys.argv[0])
## os.chdir(sys.argv[1])              ## batch: cd /D %~dp0
print('Patch-Dir: ', patch_dir, ' -> ' , os.getcwd(), ' =========================')
my_env = os.environ.copy()

# myprocess = subprocess.Popen(['python', '--version'], env = my_env)
# myprocess.wait()

with open(patch_dir +'/patches/series') as file:
    for line in file:
        patch = line.rstrip().split('#')[0]
        if patch and len(patch) > 0:
            patch_file = patch_dir +'/patches/' + patch
            if os.path.exists(patch_file):
                print(patch_file)
            else:
                print(patch_file, 'doesn\'t exist')
        else:
            print('no patch: ',line.rstrip())

if False:
    try:
        file = open('patch_active', 'r')
        print('patch is active')
    except IOError:
        print('patch is to do')
        # myprocess = subprocess.Popen(['git', 'apply', patch_dir +'/patches/disable_db.patch'], env = my_env)
        # myprocess.wait()
try:
   src=patch_dir + '/cmake/sodium_CMakeLists.txt.in'
   if os.path.exists(src):
        dst='CMakeLists.txt'
        shutil.copy(src, dst)
        print('Copy: ', src, ' -> ', dst, ' successful')
        if os.path.exists(dst):
            file = open('patch_active', 'w')
        # test only: shutil.copy(src, '_CMakeLists.txt')
   else:
        print('Copy: ', src, ' not found!')
except:
   print('Copy error: ', src, ' -> ', dst)
   exit(1)

exit(0)

