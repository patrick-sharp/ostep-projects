#! /bin/bash

if ! [[ -x xcheck ]]; then
    echo "xcheck executable does not exist"
    exit 1
fi

../tester/run-tests.sh $*



