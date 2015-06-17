#!/bin/sh -e
BUILDROOT=$(cd $(dirname $0) && pwd)/build
cmake --build "${BUILDROOT}" "$@"
