#!/usr/bin/env bash

echo Test for invalid key-value pair strings

pairs=(
  ""
  "="
  "  =  "
  "1234"
  "=123456"
  "123456="
  "AABCA = 1234 "
  "  AABC=1234 "
  "ABC=1234"
  "ABCD=12345"
)

passed=true

for p in "${pairs[@]}" ; do
  err=`./pair_parser -c "$p"`
  if [ $? -ne 0 ]; then
    echo "  Invalid string: \"$p\""
    if [ -z "$err" ] ; then
      err="Something wrong with the test program ..."
      passed=false
    fi
    echo "    Error message: $err"
  else
    echo ERROR: Invalid string considered valid: \"$p\"
    passed=false
  fi
done

if $passed ; then
  echo Passed
else
  echo \*\*\* NOT PASSED \*\*\*
fi
