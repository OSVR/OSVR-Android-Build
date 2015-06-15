#!/bin/sh -e
BUILDROOT=build
if [ -e ${BUILDROOT}/src ]; then
  (
  cd ${BUILDROOT}/src
  # Delete all the projects except the big OpenCV download.
  ls | grep prefix | grep -v OpenCV | xargs rm -rf
  )
fi

rm -rf ${BUILDROOT}/install
rm -rf ${BUILDROOT}/host-install

mkdir -p ${BUILDROOT}
(
  cd ${BUILDROOT} && cmake .. $@
)
