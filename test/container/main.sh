#!/usr/bin/env bash
cd /build
cmake /src
cmake --build .
cd /test
python3 run.py /build/ofs-convert tests
