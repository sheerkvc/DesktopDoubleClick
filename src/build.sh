#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
resource_object="$(mktemp --suffix=.o)"
trap 'rm -f -- "$resource_object"' EXIT
x86_64-w64-mingw32-windres -I . app.rc -O coff -o "$resource_object"
x86_64-w64-mingw32-g++ -std=c++11 -Os -ffunction-sections -fdata-sections -Wall -Wextra -Werror \
  -municode -mwindows -static -static-libgcc -static-libstdc++ -fno-exceptions -fno-rtti \
  main.cpp "$resource_object" -o ../DesktopDoubleClick.exe \
  -luuid -loleacc -loleaut32 -lole32 -lshell32 -luser32 -ladvapi32 -lpsapi \
  -Wl,--gc-sections,--dynamicbase,--nxcompat,--high-entropy-va,--no-insert-timestamp -s
