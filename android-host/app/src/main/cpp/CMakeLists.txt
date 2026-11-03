cmake_minimum_required(VERSION 3.22)
project(tcldroid C)

# Tcl is built by tools/build-tcl-android.sh, not by CMake. Link it by *name*
# with a library directory: linking by full path would record that build-machine
# path in DT_NEEDED (libtcl9.0.so has no SONAME).
set(TCL_PREFIX "${TCL_INSTALL_ROOT}/android-${ANDROID_ABI}")

add_library(tcldroid SHARED tcldroid_jni.c)
target_include_directories(tcldroid PRIVATE "${TCL_PREFIX}/include")
target_link_directories(tcldroid PRIVATE "${TCL_PREFIX}/lib")
target_link_libraries(tcldroid PRIVATE tcl9.0 log)
target_compile_options(tcldroid PRIVATE -Wall -Wextra)
target_link_options(tcldroid PRIVATE -Wl,-z,max-page-size=16384)
