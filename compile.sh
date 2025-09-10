#!/bin/bash
set -euo pipefail

gcc -rdynamic -g -o lalang *.c -ldl
