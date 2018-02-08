#!/usr/bin/env bash
mkdir -p build
docker run --rm --privileged -v $(realpath ../..):/src:ro -v $(realpath ./build):/build -v $(realpath ..):/test ofs-convert-testing
