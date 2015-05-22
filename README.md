# OSVR-Android-Build
Super-build and submodules for building OSVR for Android.

This repository should be cloned with `--recursive`, or you need to run

```sh
git submodule update --init --recursive
```

in the root directory. (You may want to do the latter after updating this repo, as well, to make sure the submodules are all up to date - make sure that any changes you've made in those repos has been committed on a named branch first.)

## Requirements
- The latest [CMake][] (3.2.2 verified to work 22-May-2015)
- The latest [CrystaX NDK][] (10.1.0 verified to work 22-May-2015)
  - There is some code to theoretically support the upstream Android NDK, but it does not work at this time and is not recommended unless you want to help hack on the build system. Anyway, the CrystaX NDK appears to provide other benefits besides reducing dependencies (it comes with Boost already compiled), so it will most likely remain the targeted NDK.
- Python 2.x

[CMake]: http://cmake.org
[CrystaX NDK]: https://www.crystax.net/android/ndk

## Build Overview
This repository's primary contents, besides the submodules, is a CMake build system using the [ExternalProject][] functionality to control nested configuration and build processes - the so-called "super-build". You should configure it using your **standard, native platform build platform** (Visual Studio, MinGW, Unix Makefiles, whatever suits you). With the correct setting of things like the `ANDROID_NDK` variable in the super-build's CMake configuration (command line or GUI), **the build will automatically set up the cross-compilation toolchain of nested builds** as required.

Essentially, the super-build is a sort of meta-project or outer shell around the nested builds, with configuration, build, and install of the nested builds as targets in the super-build.

> A bit more details: The OSVR build process requires a host-format (that is, not cross-compiled for Android) tool binary (`osvr_json_to_c`). On non-cross-compilation builds, it's just built as part of the standard build process then used, but this of course does not work when cross-compiling. This super-project includes a nested build just for that tool and its dependency, which uses whatever CMake generator you use for the super-build which of course assumes that it can build binaries that your machine can run. The Android nested builds automatically have their `CMAKE_TOOLCHAIN_FILE` and related options set appropriately by the super-build, as well as having this native tool substituted in.

**Important development note:** As with all Git repos with submodules, if you are going to make changes to a submodule's contents, be sure to check out/create a named branch first, as the submodule process checks out a specific commit (a "detached HEAD").

[ExternalProject]: http://www.cmake.org/cmake/help/v3.0/module/ExternalProject.html


## Build/usage instructions
- Download and unpack the NDK somewhere convenient.
- Clone the repository and submodules.
- Run CMake/CMake GUI on the top-level directory:
  - setting the build/binary directory to a different directory
  - specifying the required variables:
    - `ANDROID_NDK` - the location of the NDK (the root of it)
    - `CMAKE_BUILD_TYPE` - Only for single-configuration CMake generators (so, not for Visual Studio or XCode) - either `Release` or `Debug`
  - optionally specifying these to override defaults:
    - `ANDROID_ABI`
    - any of the `BUILD_` options: these get passed through to the OSVR-Core cross-build.
  - If using the GUI, "Configure" and "Generate".
- In the build directory chosen, open the solution/project and build it (the default target), or run `make` or your other build tool (depending on the CMake generator you chose)
  - Build products are installed into your binary directory, in a subdirectory called `install`
- Either:
  - Copy the desired files directly and individually to your Android device, or
  - Run `cpack` in the binary dir/build the "package" target to get a `.tar.gz` file containing just the runtime files, copy that to your Android device and unpack it.
- Optional (but recommended) step: from a shell (`ssh`, for instance) on the device or over `adb`, run the command `sh setup.sh` in the root directory of the OSVR tree. See below for details. You'll then have a number of executable files (actually symlinks, but that's not important) in the root directory of the OSVR tree on the device for starting bundled apps/tools.

## Convenience scripts
The build includes some simple scripts intended for running on the device that are optional but make testing/usage easier. They require Busybox to be installed and in the path. If you can't/don't want to use them, you can just read them to see what they're doing.

- `setup.sh` in the root directory of the tree takes care of setting executable permissions on the binaries and scripts (in case you built on Windows or otherwise couldn't preserve the desired permissions during file creation/transfer), and also creates symlinks in the tree root to `bin/launcher.sh` for simple starting of various applications.
- `launcher.sh` located under `bin` is a wrapper/launcher script, designed to be used by creating a symlink in the root of the tree (which `setup.sh` does). It uses the name that it's invoked with (that is, the symlink name) to specify which binary to run, after it sets up library paths appropriately and sets the current working directory to be the `bin` directory.
