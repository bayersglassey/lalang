#!/bin/bash
set euo -pipefail

gcc -shared -fPIC -o nlist.so extensions/nlist.c -ldl
