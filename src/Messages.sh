#!/usr/bin/env bash

$XGETTEXT `find . -name \*.cpp -o -name \*.h` -o $podir/kio5_s3.pot
