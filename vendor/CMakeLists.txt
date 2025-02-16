include(FetchContent)

# Dependencies

FetchContent_Declare(
  fmt
  GIT_REPOSITORY "https://github.com/fmtlib/fmt.git"
  GIT_TAG 11.0.2
)

FetchContent_Declare(
  SDL3
  GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
  GIT_TAG release-3.2.0
)

FetchContent_Declare(
  SDL_mixer
  GIT_REPOSITORY "https://github.com/libsdl-org/SDL_mixer.git"
  GIT_TAG 269c637263b8c63b053d72b4dde4129b944d6e57
)

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY "https://github.com/gabime/spdlog.git"
  GIT_TAG v1.15.0
)

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.1
)

FetchContent_Declare(
  protobuf
  GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
  GIT_TAG        v29.3
)

# Settings

# For SDL2 to be able to override options
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(FMT_INSTALL OFF)
set(SPDLOG_FMT_EXTERNAL ON)
set(protobuf_BUILD_TESTS OFF)
set(ABSL_PROPAGATE_CXX_STD ON)
set(GLM_BUILD_LIBRARY ON)

add_subdirectory(imgui)
add_subdirectory(glad)
add_subdirectory(sqlite3)
add_subdirectory(implot)
add_subdirectory(json)
add_subdirectory(stb)

FetchContent_MakeAvailable(fmt SDL3 SDL_mixer spdlog glm protobuf)

