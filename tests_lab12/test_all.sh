#!/bin/bash

cd "$(dirname "$0")"
me=`basename "$0"`

find . -name '*.sh' ! -name $me  -type f \( -exec bash {} \; -o -quit \)