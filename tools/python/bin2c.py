import sys, os


if len(sys.argv) > 1:
    filename = sys.argv[1]

outdir = ''  # without this argument 2 write in the same directory like input file!
if len(sys.argv) > 2:
    outdir = sys.argv[2]
    outdir = outdir + '/'


if not os.path.exists(filename):
    print(filename, ' not exists!')
    exit(1)

bin_name_file = os.path.basename(filename)
bin_name = bin_name_file.replace('.', '_').replace('-', '_')

if not bin_name:
    bin_name = 'de'

with open(filename, 'rb') as readfile:
    bytes_read = readfile.read()
    i = 0
    filelength = '0x'+ ''.join('{:08X}'.format(len(bytes_read)))  # Format 0x00000000
    # LANGUAGE = bin_name.upper()

    # C Source file :
    with open(outdir + os.path.basename(filename) +'.c', 'wt', newline='') as writefile:
        writefile.write('#include <stddef.h>\n')
        writefile.write('#include <stdint.h>\n')
        writefile.write('const uint8_t ' + bin_name + '[] = {\n')

        for b in bytes_read:
            i = i + 1
            writefile.write('0x' + ''.join('{:02X}'.format(b)) + ', ')  # Format 0x00
            if i >= 16 :
                writefile.write('\n')
                i = 0
        writefile.write('};\n\n')
        # writefile.write('\n')
        writefile.write('const size_t ' + bin_name + '_size = ' + filelength + ';\n')
        writefile.write('const uint8_t *' + bin_name + '_end       = ' + bin_name + ' + ' + filelength + ';\n')
        writefile.close()
readfile.close()


