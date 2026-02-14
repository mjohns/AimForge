include(FetchContent)

# Dependencies

FetchContent_Declare(
  SDL3
  GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
  GIT_TAG release-3.2.30
)

FetchContent_Declare(
  SDL_mixer
  GIT_REPOSITORY "https://github.com/libsdl-org/SDL_mixer.git"
  GIT_TAG 269c637263b8c63b053d72b4dde4129b944d6e57
)

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.3
)

FetchContent_Declare(
  protobuf
  GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
  GIT_TAG        v33.2
)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.17.0
)

# Settings

set(SPDLOG_USE_STD_FORMAT ON)
set(ABSL_PROPAGATE_CXX_STD ON)
set(GLM_BUILD_LIBRARY ON)

set(protobuf_BUILD_TESTS OFF)
set(protobuf_BUILD_PROTOC_BINARIES  OFF)
set(protobuf_BUILD_LIBPROTOC OFF)
set(protobuf_BUILD_LIBUPB OFF)
set(protobuf_BUILD_EXAMPLES OFF)
set(protobuf_WITH_ZLIB OFF)

add_subdirectory(imgui)
add_subdirectory(sqlite3)
add_subdirectory(implot)
add_subdirectory(protobuf-matchers)
add_subdirectory(spdlog)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(SDL3 SDL_mixer glm protobuf googletest)

