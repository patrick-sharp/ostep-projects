#!/bin/bash

# NOTE: THESE TESTS MUST BE PERFORMED WITH num_reducers SET TO 1 IN main.c 
# OTHERWISE, NON-DETERMINISM FROM MULTITHREADING WILL CAUSE THEM TO FAIL.

t1() {
  ./mapreduce test_files/1/in/*.txt > test_files/1/1-out-actual.txt
  expected="test_files/1/1-out-expected.txt"
  actual="test_files/1/1-out-actual.txt"

  if cmp -s "$expected" "$actual"; then
      echo "Test 1 PASS"
  else
      echo "TEST 1 FAIL"
  fi
}


t2() {
  ./mapreduce test_files/2/in/*.txt > test_files/2/2-out-actual.txt
  expected="test_files/2/2-out-expected.txt"
  actual="test_files/2/2-out-actual.txt"

  if cmp -s "$expected" "$actual"; then
      echo "Test 2 PASS"
  else
      echo "TEST 2 FAIL"
  fi
}

t3 () {
  ./mapreduce test_files/3/in/*.txt > test_files/3/3-out-actual.txt
  expected="test_files/3/3-out-expected.txt"
  actual="test_files/3/3-out-actual.txt"

  if cmp -s "$expected" "$actual"; then
      echo "Test 3 PASS"
  else
      echo "TEST 3 FAIL"
  fi
}

t4 () {
  ./mapreduce test_files/4/in/*.txt > test_files/4/4-out-actual.txt
  expected="test_files/4/4-out-expected.txt"
  actual="test_files/4/4-out-actual.txt"

  if cmp -s "$expected" "$actual"; then
      echo "Test 4 PASS"
  else
      echo "TEST 4 FAIL"
  fi
}

t1
t2
t3
t4

