#!/bin/bash
find . -type f | grep "\.cc" | grep -v ".pb.cc"
find . -type f | grep "\.h" | grep -v ".pb.h"