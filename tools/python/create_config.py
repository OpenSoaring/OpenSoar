import sys

debug = False
headerfile = None

if debug:
  count = 0
  for arg in sys.argv:
      print('argument ',count + 1,': ', sys.argv[count])
      count = count + 1

if len(sys.argv) < 2:
   print('to less arguments: ', len(sys.argv))
else:
    headerfile = open(sys.argv[2], 'w')

    repl = []  
    count = 0
    configfile = open(sys.argv[1])
    for line in configfile:
          if debug:
            print('argument ',count + 1,': ', line)
          config_pair = line.split('=')
          if len(config_pair) == 2:
            repl.append([config_pair[0].strip(), config_pair[1].strip()])
          count = count + 1
    configfile.close()
    
    for param in repl:
      if len(param) == 2:
        if debug:
           print('param0 = ',param[0],', param1 = ',param[1])
        if headerfile:
           headerfile.write('#define ' + '{:24s}'.format(param[0]) + ' "' + param[1] + '"\n')
    
    headerfile.close()

print('EOF replace.py')
exit(0)