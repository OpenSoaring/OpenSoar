import sys
import os
import io

debug = False

print('=========  CREATE_RESOURCE ===============')
print('CurrDir: ', os.getcwd())

res_array = []


# def write_resource(outfile, line, macro, type, extension):
def create_line(name, res_name, path, file, ext):
        line = '{:30s}'.format(name) + ' ' + '{:12s}'.format(res_name)
        line = line + 'DISCARDABLE   L"' + path + '/'
        line = line + file + '.' + ext + '"'
        # line = '{:30s}'.format(params[1]) + ' ' + '{:12s}'.format(resource[2])
        # line = line + 'DISCARDABLE   L"' + resource[3] + '/'
        # line = line + params[2].strip(' \n').replace('"','') + resource[4] + '"'
        outfile.write(line + '\n') # Copy of read line (with replaced field)
        if debug:
            print(line)
        ## return line

def write_resource(outfile, line, resource):
         # line = line.replace(resource[0]+'(','')
         # line = line.replace(')','')
         # params = line.split(',')
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
           # outfile.write(line+ '\n') # Copy of read line (with replaced field)
           # print(line + '\n')
#============================================================================

if debug:
  count = 0
  for arg in sys.argv:
      print('argument ',count + 1,': ', sys.argv[count])
      count = count + 1

src_location1 = sys.argv[3]
src_location2 = sys.argv[4]

### res_array.append(['BITMAP_ICON',   'BITMAP', src_location2 + '/icons', '.bmp'])
### res_array.append(['BITMAP_BITMAP', 'BITMAP', src_location1 + '/bitmaps', '.bmp'])
### res_array.append(['BITMAP_GRAPHIC','BITMAP', src_location2 + '/graphics', '.bmp'])
### res_array.append(['HATCH_BITMAP',  'BITMAP', src_location1 + '/bitmaps', '.bmp'])
### res_array.append(['SOUND',         'WAVE',   src_location1 + '/sound', '.wav'])
### res_array.append(['ICON_ICON',     'ICON',   src_location1 + '/bitmaps', '.ico'])

# res_array.append(['bitmap_icon ', 'BITMAP', src_location2 + '/icons', '.bmp'])
res_array.append(['bitmap_icon_scaled ', 'BITMAP_ICON',   'BITMAP', src_location2 + '/icons', 'bmp'])
res_array.append(['bitmap_bitmap ',      'BITMAP_BITMAP', 'BITMAP', src_location1 + '/bitmaps', 'bmp'])
res_array.append(['bitmap_graphic ',     'BITMAP_GRAPHIC','BITMAP', src_location2 + '/graphics', 'bmp'])
res_array.append(['hatch_bitmap ',       'HATCH_BITMAP',  'BITMAP', src_location1 + '/bitmaps', 'bmp'])
res_array.append(['sound '               'SOUND',         'WAVE',   src_location1 + '/sound', 'wav'])
res_array.append(['app_icon ',           'ICON_ICON',     'ICON',   src_location1 + '/bitmaps', 'ico'])

if len(sys.argv) < 2:
   print('to less arguments: ', len(sys.argv))
else:
    infile = io.open(sys.argv[1])
    outfile = io.open(sys.argv[2], 'w', newline='\n')
    outfile2 = io.open(sys.argv[2]+'.h', 'w', newline='\n')
    
    for line in infile:
       write_line(outfile, line)
    infile.close()
    outfile.close()
    