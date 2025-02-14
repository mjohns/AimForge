#!/bin/bash
protoc --proto_path=. --cpp_out=. *.proto
clang-format -i *.proto
