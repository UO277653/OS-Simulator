#!/bin/bash

cd "$(realpath -L "$(dirname "$0")"/..)"

GOT=$( \
    timeout 2s \
    stdbuf -o0 -e0 \
    ./Simulator \
    --debugSections=ihfpmtdlsec \
    --assertsFile=dummy \
    ./tests_lab09/test09_1.prog \
    ./tests_lab09/test09_2.prog \
)
EXPECTED="./tests_lab09/test09.out"
echo "(Assuming you have done all the exercises)"

echo "Test: Complex test with Interruptions Masking, Yield and Sleep"

CLEAN_GOT=$(echo "$GOT" | tr -d '\r' | sed 's/^[ \t]*//;s/[ \t]*$//')
CLEAN_EXPECTED=$(cat "$EXPECTED" | tr -d '\r' | sed 's/^[ \t]*//;s/[ \t]*$//')
if [ "$CLEAN_GOT" == "$CLEAN_EXPECTED" ];
then
    echo "    PASS"
    exit 0
else
    echo "    FAIL"
    icdiff -H -W -N -u --label="GOT" --label="EXPECTED" <(echo "$CLEAN_GOT") <(echo "$CLEAN_EXPECTED")
    exit 1
fi