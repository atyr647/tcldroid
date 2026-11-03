cmake_minimum_required(VERSION 3.22)
project(tcldroid C)

# Tcl and Tk are built by tools/build-tcl-android.sh and tools/build-tk.sh, not by CMake.
# Link them by *name* with a library directory: linking by full path would record that
# build-machine path in DT_NEEDED (neither library has a SONAME).
set(TCL_PREFIX "${TCL_INSTALL_ROOT}/android-${ANDROID_ABI}")
set(TK_LIBDIR  "${TK_BUILD_ROOT}/tk-${ANDROID_ABI}")

add_library(tcldroid SHARED tcldroid_jni.c tcldroid_core.c)
target_include_directories(tcldroid PRIVATE "${TCL_PREFIX}/include" "${TK_ANDROID_DIR}")
target_link_directories(tcldroid PRIVATE "${TCL_PREFIX}/lib" "${TK_LIBDIR}")
target_link_libraries(tcldroid PRIVATE tcl9.0 tk9.0 log)
target_compile_options(tcldroid PRIVATE -Wall -Wextra)
target_link_options(tcldroid PRIVATE -Wl,-z,max-page-size=16384)
