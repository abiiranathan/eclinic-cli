"""
ldd ./bin/eclinic
linux-vdso.so.1 (0x000070cd4f915000)
libm.so.6 => /usr/lib/libm.so.6 (0x000070cd4f80b000)
libpq.so.5 => /usr/lib/libpq.so.5 (0x000070cd4f7bb000)
libc.so.6 => /usr/lib/libc.so.6 (0x000070cd4f5cf000)
/lib64/ld-linux-x86-64.so.2 => /usr/lib64/ld-linux-x86-64.so.2 (0x000070cd4f917000)
libssl.so.3 => /usr/lib/libssl.so.3 (0x000070cd4f4f5000)
libcrypto.so.3 => /usr/lib/libcrypto.so.3 (0x000070cd4f000000)
libgssapi_krb5.so.2 => /usr/lib/libgssapi_krb5.so.2 (0x000070cd4efac000)
libldap.so.2 => /usr/lib/libldap.so.2 (0x000070cd4ef4d000)
libkrb5.so.3 => /usr/lib/libkrb5.so.3 (0x000070cd4ee75000)
libk5crypto.so.3 => /usr/lib/libk5crypto.so.3 (0x000070cd4ee47000)
libcom_err.so.2 => /usr/lib/libcom_err.so.2 (0x000070cd4f4ed000)
libkrb5support.so.0 => /usr/lib/libkrb5support.so.0 (0x000070cd4f4dd000)
libkeyutils.so.1 => /usr/lib/libkeyutils.so.1 (0x000070cd4f4d6000)
libresolv.so.2 => /usr/lib/libresolv.so.2 (0x000070cd4ee35000)
liblber.so.2 => /usr/lib/liblber.so.2 (0x000070cd4ee25000)
libsasl2.so.3 => /usr/lib/libsasl2.so.3 (0x000070cd4ee06000)
"""

import os
import sys
import subprocess
import shutil

if len(sys.argv) != 3:
    print(f"Usage: python3 {sys.argv[0]} binary dest_dir")
    sys.exit(1)

BINARY = sys.argv[1]
DEST = sys.argv[2]

os.makedirs(DEST, exist_ok=True)

if not os.path.exists(BINARY):
    print(f"File not found: {BINARY}")
    sys.exit(1)

with open("/tmp/libs.txt", "w") as f:
    code = subprocess.call(["ldd", BINARY], stdout=f)
    if code != 0:
        sys.exit(1)

with open("/tmp/libs.txt", "r") as f:
    for line in f.readlines():
        parts = line.strip().split(" => ")
        if len(parts) != 2:
            continue

        so_name = parts[0]
        path, _ = parts[1].split(" ")

        if not os.path.exists(path):
            print("shared object not found: {path}")
            sys.exit(1)

        abspath = os.path.join(DEST, os.path.basename(path))
        shutil.copy(path, abspath)
