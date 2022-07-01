#!/bin/bash

cd "$(realpath -L "$(dirname "$0")"/..)"

GOT=$( \
    timeout 2s \
    stdbuf -o0 -e0 \
    ./Simulator \
    --debugSections=ihfpmtdlsec \
    --assertsFile=dummy \
    ./tests_lab12/test01.prog 1 \
    ./tests_lab12/test01.prog 2 \
    ./tests_lab12/test01.prog 4 \
    ./tests_lab12/test01.prog 150 \
    not_existing.prog 270 \
)
EXPECTED="./tests_lab12/test01.out"

echo "Test exercises of the V3"
echo "(Assuming you have completed all the exercises)"

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