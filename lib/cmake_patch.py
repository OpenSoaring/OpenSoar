# CMake Patch for project SODIUM (sodium, libsodium)

import os, sys, subprocess, shutil

if len(sys.argv) < 2:
    print('No patch: ', sys.argv)
    exit(1)

patch_dir = os.path.dirname(sys.argv[0])
lib_name = sys.argv[1]

if not os.path.exists(patch_dir +'/' + lib_name):
    print('No patch for ', lib_name, ' available!')
    exit(0)
    # print('No patchdir exists for: ', lib_name)
    # exit(1)

patch_dir += '/' + lib_name
print('Patch-Dir: ', patch_dir, ' -> ' , os.getcwd(), ' =========================')

########################################
###  find xxx_CMakeList.txt.in #########
########################################
cmake_lists_in = patch_dir + '/cmake/' +lib_name+'_CMakeLists.txt.in'
if os.path.exists(cmake_lists_in):
    # configure CMakeLists.txt:
    print('configure CMakeLists.txt from: ', cmake_lists_in)
    

my_env = os.environ.copy()

patch_detect = 'patch_active'
do_patch = not os.path.exists(patch_detect)

########################################
###  hard reset the project ############
########################################
if not do_patch: # and False:
    os.remove(patch_detect)
    myprocess = subprocess.Popen(['git', 'reset', '--hard', 'HEAD'], env = my_env)
    myprocess.wait()
    do_patch = True
    print('git reset done')

########################################
###  patch the 'serial'  ###############
########################################
patch_dir += '/patches/'
try:
   file = open(patch_detect, 'r')
   print('patch is active')
except IOError:
   with open(patch_dir +'series') as file:
    for line in file:
        patch = line.rstrip().split('#')[0]
        if patch and len(patch) > 0:
            patch_file = patch_dir + patch
            if os.path.exists(patch_file):
                print(patch_file)
                myprocess = subprocess.Popen(['git', 'apply', '--verbose', patch_file], env = my_env)
                myprocess.wait()
            else:
                print(patch_file, 'doesn\'t exist')
   file = open(patch_detect, 'w')
   print('patch is finished')

########################################
###  Copy CMakeList.txt file ###########
########################################

try:
   src=cmake_lists_in
   if os.path.exists(src):
        dst='CMakeLists.txt'
        shutil.copy(src, dst)
        print('Copy: ', src, ' -> ', dst, ' successful')
        if os.path.exists(dst):
            file = open('patch_active', 'w')
   else:
        print('Copy: ', src, ' not found!')
except:
   print('Copy error: ', src, ' -> ', dst)
   exit(1)

print('patch was made')
exit(0)

