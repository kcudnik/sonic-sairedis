#!/bin/bash

rm -rf aclocal.m4 autom4te.cache stamp-h1 libtool configure config.* Makefile.in Makefile config m4

find -type f -name "*~" -print0 | xargs -0 rm -f
find -type f -name "*.sw[po]" -print0 | xargs -0 rm -f
find -type f -name "Makefile" -print0 |grep -vzZ SAI | xargs -0 rm -f
find -type f -name "Makefile.in" -print0 | xargs -0 rm -f
find -type f -name "*.gcno" -print0 | xargs -0 rm -f
find -type f -name "*.gcda" -print0 | xargs -0 rm -f
find -type f -name "*.gcov" -print0 | xargs -0 rm -f
find -type f -name "*.o" -print0 | xargs -0 rm -f
find -type f -name "*.lo" -print0 | xargs -0 rm -f
find -type f -name "*.la" -print0 | xargs -0 rm -f
find -type f -name "*.a" -print0 | xargs -0 rm -f
find -type f -name "*.so" -print0 | xargs -0 rm -f
find -type f -name ".dirstamp" -print0 | xargs -0 rm -f
find -type f -name "*.rec" -print0 | xargs -0 rm -f

find -type d -name .deps -print0 | xargs -0 rm -rf
find -type d -name .libs -print0 | xargs -0 rm -rf

rm -f ./vslib/src/tests
rm -f ./meta/tests
rm -f ./syncd/syncd
rm -f ./lib/src/tests
rm -f ./syncd/tests
rm -f ./aminclude_static.am
rm -rf htmlcov


find -type f|\
         grep -vP "\.(cpp|h|c)$"|\
         grep -v /.git/|\
         grep -v Makefile.am|\
         grep -v .gitignore|\
         grep -v autoclean.sh|\
         grep -v autogen.sh|\
         grep -v configure.ac|\
         grep -v check.sh|\
         grep -v SAI/meta/Makefile

true
