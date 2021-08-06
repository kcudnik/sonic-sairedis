#!/bin/bash

if [ ! -e configure ];
then
    echo autoclean
    ./autoclean.sh
    echo autogen
    ./autogen.sh
    echo configure

    set -e

    ./configure CXXFLAGS="" --enable-code-coverage
fi

echo make
make

echo mkdir
mkdir -p htmlcov

echo start gcovr

set +e

# error with 'SAI/meta/.libs/saimetadata.c'
# this file is in SAI/meta/ directory not .libs
gcovr -r ./ --html --html-details  -o htmlcov/index.html

# error with 'lib/src/.libs/SaiInterface.cpp' directory sources
# this file is in lib/src directory not .libs
gcovr -r ./ -e SAI/meta/.libs/saimetadata.c --html --html-details  -o htmlcov/index.html

# error with 'lib/src/Notification.h' headers in src directories
# this file is in lib/inc directory not lib/src
gcovr -r ./ -e SAI/meta/.libs/saimetadata.c -e ".+/.libs/.+" --html --html-details  -o htmlcov/index.html
