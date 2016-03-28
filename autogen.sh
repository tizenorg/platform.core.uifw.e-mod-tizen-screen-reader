#!/bin/sh

set  -x
aclocal
autoconf
libtoolize --copy --force
autoheader
automake --foreign --add-missing --copy
