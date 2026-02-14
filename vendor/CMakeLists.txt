include(FetchContent)

# Dependencies

FetchContent_Declare(
  SDL3
  GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
  GIT_TAG release-3.2.30
)
# GIT_TAG release-3.4.0

FetchContent_Declare(
  SDL_mixer
  GIT_REPOSITORY "https://github.com/libsdl-org/SDL_mixer.git"
  GIT_TAG 269c637263b8c63b053d72b4dde4129b944d6e57
)

# Settings

set(SPDLOG_USE_STD_FORMAT ON)

set(ABSL_PROPAGATE_CXX_STD ON)
set(ABSL_ENABLE_INSTALL ON)
if (NOT BUILD_SHARED_LIBS) 
  set(ABSL_MSVC_STATIC_RUNTIME ON)
endif()

set(GLM_BUILD_LIBRARY ON)

set(SDLMIXER_FLAC OFF)
set(SDLMIXER_GME OFF)
set(SDLMIXER_MP3 OFF)
set(SDLMIXER_MIDI OFF)
set(SDLMIXER_WAVE OFF)
set(SDLMIXER_WAVPACK OFF)
set(SDLMIXER_OPUS OFF)
set(SDLMIXER_VORBIS_TREMOR OFF)
set(SDLMIXER_VORBIS_VORBISFILE OFF)
set(SDLMIXER_MOD OFF)

set(protobuf_LOCAL_DEPENDENCIES_ONLY ON)
set(protobuf_BUILD_TESTS OFF)
set(protobuf_BUILD_PROTOC_BINARIES  OFF)
set(protobuf_BUILD_LIBPROTOC OFF)
set(protobuf_BUILD_LIBUPB OFF)
set(protobuf_BUILD_EXAMPLES OFF)
set(protobuf_BUILD_CONFORMANCE OFF)
set(protobuf_WITH_ZLIB OFF)

add_subdirectory(absl)
add_subdirectory(imgui)
add_subdirectory(sqlite3)
add_subdirectory(glm)
add_subdirectory(implot)
add_subdirectory(spdlog)
add_subdirectory(protobuf)
add_subdirectory(googletest)
add_subdirectory(protobuf-matchers)

FetchContent_MakeAvailable(SDL3)

# https://github.com/libsdl-org/SDL_mixer/commit/269c637263b8c63b053d72b4dde4129b944d6e57
add_subdirectory(SDL_mixer)

