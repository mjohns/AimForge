add_library(protobuf-matchers protobuf-matchers/protocol-buffer-matchers.cc)

target_link_libraries(protobuf-matchers protobuf::libprotobuf GTest::gmock)
target_include_directories(protobuf-matchers PUBLIC .)
