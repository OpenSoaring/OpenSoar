import sys
import os
import io

debug = True   # False
res_id = 100
res_array = []

print('=========  CREATE_RESOURCE ===============')
print('CurrDir: ', os.getcwd())
print('create-resource:')

# def write_resource(outfile, line, macro, type, extension):
def create_line(name, res_name, path, file, ext):
        line = '{:30s}'.format(name) + ' ' + '{:12s}'.format(res_name)
        line = line + 'DISCARDABLE   "' + path + '/'
        line = line + file + '.' + ext + '"'
        outfile.write(line + '\n') # Copy of read line (with replaced field)
        if debug:
            print(line)
        ## return line

# MAKE_RESOURCE(IDI_XCSOAR, xcsoarswift, 100);
def create_header_line(name, res_name, path, file):
    global res_id
    line = 'MAKE_RESOURCE('
    line = line + name + ','
    line = line + file + ','
    line = line + str(res_id) + ');'
    outfile2.write(line + '\n') # Copy of read line (with replaced field)
    res_id = res_id + 1

def write_resource(outfile, line, resource):
         params = line.split(' ')
         # if debug:
         if len(params) >= 2:
             if resource[0] == 'bitmap_icon_scaled ':
                basename = params[2].strip(' \n').replace('"','')
                create_line(params[1],          resource[2], resource[3], basename + '_96',  resource[4])
                create_line(params[1] + '_HD',  resource[2], resource[3], basename + '_160', resource[4])
                create_line(params[1] + '_UHD', resource[2], resource[3], basename + '_300', resource[4])
             else:
                create_line(params[1], resource[2], resource[3], params[2].strip(' \n').replace('"',''), resource[4])
             # if len(line) > 0:
             # outfile.write(line.replace('/', '\\\\') +  '\n') # Copy of read line (with replaced field)
         else:
           outfile.write(line + '\n')

def write_header(outfile, line, resource):
         params = line.split(' ')
         # if debug:
         if len(params) >= 2:
             if resource[0] == 'bitmap_icon_scaled ':
                basename = params[2].strip(' \n').replace('"','')
                create_header_line(params[1],          resource[2], resource[3], basename + '_96')
                create_header_line(params[1] + '_HD',  resource[2], resource[3], basename + '_160')
                create_header_line(params[1] + '_UHD', resource[2], resource[3], basename + '_300')
                def_line = '#define '
                def_line = def_line + params[1] + '_ALL '
                def_line = def_line + params[1] + ','
                def_line = def_line + params[1] + '_HD,'
                def_line = def_line + params[1] + '_UHD'
                outfile2.write(def_line + '\n')
             else:
                create_header_line(params[1], resource[2], resource[3], params[2].strip(' \n').replace('"',''))
         # else:
         #  print(line)

def write_line(outfile, line):
      line = line.strip()
        
      if line.startswith('//'):
           print('!!\n')
      elif line.startswith('#include') or \
           line.startswith('ID') or \
           line.startswith('#if') or \
           line.startswith('#else') or \
           line.startswith('#elif') or \
           line.startswith('#endif'):
           outfile.write(line+ '\n') # Copy of read line (with replaced field)
           print(line + '\n')
      else:
        updated = False
        for resource in res_array:
           if line.startswith(resource[0]):
              write_resource(outfile, line, resource) # 'BITMAP_ICON', 'ICON', '.bmp')
              updated = True  # if one of this lines

        if not updated: 
           outfile.write('\n') # Copy of read line (with replaced field)

def write_hpp_line(outfile, line):  # , res_id):
      line = line.strip()
      
      if line.startswith('//'):
         line = ''
      if len(line) > 0:
          if line.startswith('#include') or \
               line.startswith('ID') or \
               line.startswith('#if') or \
               line.startswith('#else') or \
               line.startswith('#elif') or \
               line.startswith('#endif'):
               outfile.write(line+ '\n') # Copy of read line (with replaced field)
               print(line + '\n')
          else:
            updated = False
            for resource in res_array:
               if line.startswith(resource[0]):
                  write_header(outfile, line, resource)  # , res_id) # 'BITMAP_ICON', 'ICON', '.bmp')
                  updated = True  # if one of this lines
    
            # if not updated: 
            #   outfile.write('\n') # Copy of read line (with replaced field)
               # outfile.write(line+ '\n') # Copy of read line (with replaced field)
               # print(line + '\n')
#============================================================================

if debug:
  print('arguments: (', len(sys.argv), ') ' + str(sys.argv))
  count = 0
  for arg in sys.argv:
      print('argument ',count,': ', sys.argv[count])
      count = count + 1

src_location1 = sys.argv[3]
src_location2 = sys.argv[4]

res_array.append(['bitmap_icon_scaled ', 'BITMAP_ICON',   'BITMAP', src_location2 + '/icons', 'bmp'])
res_array.append(['bitmap_bitmap ',      'BITMAP_BITMAP', 'BITMAP', src_location1 + '/bitmaps', 'bmp'])
res_array.append(['bitmap_graphic ',     'BITMAP_GRAPHIC','BITMAP', src_location2 + '/graphics', 'bmp'])
res_array.append(['hatch_bitmap ',       'HATCH_BITMAP',  'BITMAP', src_location1 + '/bitmaps', 'bmp'])
res_array.append(['sound '               'SOUND',         'WAVE',   src_location1 + '/sound', 'wav'])
res_array.append(['app_icon ',           'ICON_ICON',     'ICON',   src_location1 + '/bitmaps', 'ico'])


if debug:
  count = 0
  for res in res_array:
      print('argument ',count,': ', res[0],': ', res[3])
      count = count + 1

include_loc = sys.argv[5]

if len(sys.argv) < 2:
   print('to less arguments: ', len(sys.argv))
else:
    infile = io.open(sys.argv[1])
    content = infile.readlines()
    outfile = io.open(sys.argv[2], 'w', newline='\n')
    ### outfile2 = io.open(sys.argv[5] + '/MakeResource.hpp', 'w', newline='\n')
    outfile2 = io.open(sys.argv[5], 'w', newline='\n')
    outfile2.write('// Create Make Resources:\n')
    print(sys.argv[1])
    print(infile)

    for line in content:  #infile:
       # print('line: ' + line)
       write_line(outfile, line)
       # outfile2.write('line: ' + line + '\n')
       write_hpp_line(outfile2, line)
       # write_hpp_line(outfile2, line, res_id)
       # res_id = res_id + 1

    infile.close()
    outfile.close()
    