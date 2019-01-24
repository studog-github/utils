#
# Python-native implmentation of xxd
#

def printable(c):
    if type(c) is str:
        c = ord(c)
    if c < 0x20 or c > 0x7e:
        return '.'
    return chr(c)


LINE_LEN = 68
ASCII_OFFSET = 51

def xxd(data, length, printer):
    line = ' ' * (LINE_LEN - 1)
    line[LINE_LEN - 2] = '\n'

    i = 0;
    while i < length:
        line = "{0:08x}: ".format(i)
        printer(line)

    data2 = [('%02x' % ord(i)) for i in data]

    offset = 0
    while offset < length:
        print('{0}: {1:<39}  {2}'.format(('%08x' % counter),
                    ' '.join([''.join

# https://gist.github.com/puentesarrin/6567480
