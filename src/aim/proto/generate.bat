protoc --proto_path=. --cpp_out=. common.proto replay.proto scenario.proto settings.proto
clang-format -i *.proto