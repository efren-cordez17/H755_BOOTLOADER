import zlib, struct 
 
app = open('application.bin', 'rb').read() 
hdr = struct.pack('<IIIII', 
                  0,                              # ota_flag: no update waiting 
                  0xABCDEFAB,                     # magic 
                  len(app),                       # size 
                  zlib.crc32(app) & 0xFFFFFFFF,   # crc 
                  1)                              # version 
hdr += b'\xFF' * 12                               # reserved, pads to 32 bytes 
open('app_header.bin', 'wb').write(hdr) 

 

import zlib, struct 
 
app = open('application.bin', 'rb').read() 
hdr = struct.pack('<IIIII', 
                  0,                              # ota_flag: no update waiting 
                  0xABCDEFAB,                     # magic 
                  len(app),                       # size 
                  zlib.crc32(app) & 0xFFFFFFFF,   # crc 
                  1)                              # version 
hdr += b'\xFF' * 12                               # reserved, pads to 32 bytes 
open('app_header.bin', 'wb').write(hdr) 