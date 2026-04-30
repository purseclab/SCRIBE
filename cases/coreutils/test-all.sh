#!/bin/bash

for cve_dir in $(find . -type d -name "CVE-*"); do
    cve_name=$(basename $cve_dir)
    pushd $cve_dir &> /dev/null
    ./crash.sh &> /dev/null
    crash_status=$?
    ./patched.sh &> /dev/null
    patched_status=$?
    test_result=$(./test.sh 2> /dev/null | grep -oP "#.*:.*")
    echo "======== $cve_name ========"
    echo "crash status: $crash_status"
    echo "patched status: $patched_status"
    echo "test result:"
    echo "$test_result"
    echo "==========================="
    echo
    echo
    popd &> /dev/null
done
