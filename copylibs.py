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

with open("/tmp/libs.txt", "w+") as f:
    code = subprocess.call(["ldd", BINARY], stdout=f)
    if code != 0:
        sys.exit(1)

    # Flush and reset to beginning
    f.flush()
    f.seek(0)

    for line in f.readlines():
        parts = line.strip().split(" => ")
        if len(parts) != 2:
            continue

        so_name = parts[0]
        path, _ = parts[1].split(" ")

        if not os.path.exists(path):
            print(f"shared object not found: {path}")
            sys.exit(1)

        abspath = os.path.join(DEST, os.path.basename(path))
        shutil.copy(path, abspath)
