import sys, shutil


if len(sys.argv) > 2:
   print('src = ', sys.argv[1], ' ==> ', sys.argv[2])
   src = sys.argv[1]
   dst = sys.argv[2]
   shutil.copyfile(src + '/szip_CMakeLists.txt.in', dst + '/CMakeLists.txt')
   shutil.copyfile(src + '/SZConfig.h', dst + '/src/SZConfig.h')
   print('finished copy')
else:
   print('no path defined')
   exit(-1)

