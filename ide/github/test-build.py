import os, sys, subprocess

print 'test-build.py'
print os.environ['HOME']
print 'test = ' + os.environ['HOME']

os.environ['TEST_ENV'] = 'myTest'