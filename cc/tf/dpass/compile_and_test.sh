#!/bin/bash

# This script for compiling dynamic pass for tensorflow.

set -x

curdir=$(pwd)
ccdir=${curdir}/../../
builddir=${ccdir}/../build/
libpath=${builddir}/lib
mkdir -p ${libpath}

TF_INC=$(python3 -c 'import tensorflow as tf; print(tf.sysconfig.get_include())')
TF_LIB=$(python3 -c 'import tensorflow as tf; print(tf.sysconfig.get_lib())')
TF_CFLG=$(python3 -c 'import tensorflow as tf; print(" ".join(tf.sysconfig.get_compile_flags()))')
TF_LFLG=$(python3 -c 'import tensorflow as tf; print(" ".join(tf.sysconfig.get_link_flags()))')



#########################
## compile libtf-pass.so
#########################
g++ -O2 -std=c++11 -shared *.cc -o ${libpath}/libtf-dpass.so ${TF_CFLG[@]} \
    -I. -I ${TF_INC} -L ${TF_LIB} ${TF_LFLG[@]} -fPIC -Wl,--rpath='$ORIGIN/..:$ORIGIN'
cd ${curdir}


