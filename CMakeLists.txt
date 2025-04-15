cmake_minimum_required(VERSION 3.10)

file(READ "${CMAKE_SOURCE_DIR}/VERSION" VER_RAW)
string(STRIP ${VER_RAW} VERSION)

project(
  mpvlock
  DESCRIPTION "A screen locking utility based on hyprlock with video/audio via mpvpaper"
  VERSION ${VERSION})

set(CMAKE_MESSAGE_LOG_LEVEL "STATUS")
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

if(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)
  message(STATUS "Configuring mpvlock in Debug with CMake")
  add_compile_definitions(HYPRLAND_DEBUG)
else()
  add_compile_options(-O3)
  message(STATUS "Configuring mpvlock in Release with CMake")
endif()

include_directories(. "protocols/")

include(GNUInstallDirs)

# configure
set(CMAKE_CXX_STANDARD 23)
add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-value
                    -Wno-missing-field-initializers -Wno-narrowing)

add_compile_definitions(MPVLOCK_VERSION="${VERSION}")

if (DEFINED MPVLOCK_COMMIT)
  add_compile_definitions(MPVLOCK_COMMIT="${MPVLOCK_COMMIT}")
else()
  execute_process(
    OUTPUT_VARIABLE GIT_SHORT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND git rev-parse --short HEAD
    RESULT_VARIABLE GIT_RESULT
    ERROR_QUIET
  )
  if(GIT_RESULT EQUAL 0)
    add_compile_definitions(MPVLOCK_COMMIT="${GIT_SHORT_HASH}")
  else()
    add_compile_definitions(MPVLOCK_COMMIT="unknown")
  endif()
endif()

if (DEFINED MPVLOCK_VERSION_COMMIT)
  add_compile_definitions(MPVLOCK_VERSION_COMMIT="${MPVLOCK_VERSION_COMMIT}")
else()
  if (EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
      COMMAND git rev-parse --short v${VERSION}
      OUTPUT_VARIABLE GIT_TAG_SHORT_HASH
      OUTPUT_STRIP_TRAILING_WHITESPACE
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      RESULT_VARIABLE GIT_TAG_RESULT
      ERROR_QUIET
    )
    if(GIT_TAG_RESULT EQUAL 0)
      add_compile_definitions(MPVLOCK_VERSION_COMMIT="${GIT_TAG_SHORT_HASH}")
    else()
      add_compile_definitions(MPVLOCK_VERSION_COMMIT="unknown")
    endif()
  else()
    add_compile_definitions(MPVLOCK_VERSION_COMMIT="unknown")
  endif()
endif()

message(STATUS "VERSION COMMIT: ${MPVLOCK_VERSION_COMMIT}")
message(STATUS "COMMIT: ${MPVLOCK_COMMIT}")

# position independent build: __FILE__
add_compile_options(-fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=)

# dependencies
message(STATUS "Checking deps...")

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(OpenGL REQUIRED COMPONENTS EGL GLES3)
find_package(hyprwayland-scanner 0.4.4 REQUIRED)
pkg_check_modules(
  deps
  REQUIRED
  IMPORTED_TARGET
  wayland-client
  wayland-protocols>=1.35
  wayland-egl
  hyprlang>=0.6.0
  egl
  xkbcommon
  libjpeg
  libwebp
  libmagic
  cairo
  pangocairo
  libdrm
  gbm
  hyprutils>=0.5.0
  sdbus-c++>=2.0.0
  hyprgraphics
)

set(SRCFILES
    src/main.cpp
    src/core/Egl.cpp
    src/core/LockSurface.cpp
    src/core/Timer.cpp
    src/core/mpvlock.cpp
    src/core/CursorShape.cpp
    src/core/Output.cpp
    src/core/Seat.cpp
    src/core/AnimationManager.cpp
    src/config/ConfigManager.cpp
    src/renderer/AsyncResourceGatherer.cpp
    src/renderer/Shader.cpp
    src/renderer/widgets/Shape.cpp
    src/renderer/widgets/IWidget.cpp
    src/renderer/widgets/Image.cpp
    src/renderer/widgets/Background.cpp
    src/renderer/widgets/PasswordInputField.cpp
    src/renderer/widgets/Label.cpp
    src/renderer/widgets/Shadowable.cpp
    src/renderer/Screencopy.cpp
    src/renderer/Renderer.cpp
    src/renderer/Framebuffer.cpp
    src/renderer/Texture.cpp
    src/helpers/MiscFunctions.cpp
    src/helpers/Math.cpp
    src/helpers/Color.cpp
    src/auth/Pam.cpp
    src/auth/Fingerprint.cpp
    src/auth/Auth.cpp
)
add_executable(mpvlock ${SRCFILES})
target_link_libraries(mpvlock PRIVATE pam rt Threads::Threads PkgConfig::deps
                                     OpenGL::EGL OpenGL::GLES3)

# protocols
pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
message(STATUS "Found wayland-protocols at ${WAYLAND_PROTOCOLS_DIR}")
pkg_get_variable(WAYLAND_SCANNER_PKGDATA_DIR wayland-scanner pkgdatadir)
message(STATUS "Found wayland-scanner pkgdatadir at ${WAYLAND_SCANNER_PKGDATA_DIR}")

function(protocolnew protoPath protoName external)
  if(external)
    set(path ${CMAKE_SOURCE_DIR}/${protoPath})
  else()
    set(path ${WAYLAND_PROTOCOLS_DIR}/${protoPath})
  endif()
  add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/protocols/${protoName}.cpp
           ${CMAKE_SOURCE_DIR}/protocols/${protoName}.hpp
    COMMAND hyprwayland-scanner --client ${path}/${protoName}.xml
            ${CMAKE_SOURCE_DIR}/protocols/
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  target_sources(mpvlock PRIVATE protocols/${protoName}.cpp
                                protocols/${protoName}.hpp)
endfunction()
function(protocolWayland)
  add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/protocols/wayland.cpp
           ${CMAKE_SOURCE_DIR}/protocols/wayland.hpp
    COMMAND hyprwayland-scanner --wayland-enums --client
            ${WAYLAND_SCANNER_PKGDATA_DIR}/wayland.xml ${CMAKE_SOURCE_DIR}/protocols/
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  target_sources(mpvlock PRIVATE protocols/wayland.cpp protocols/wayland.hpp)
endfunction()

make_directory(${CMAKE_SOURCE_DIR}/protocols)

protocolwayland()

protocolnew("protocols" "wlr-screencopy-unstable-v1" true)
protocolnew("protocols" "wlr-layer-shell-unstable-v1" true)
protocolnew("staging/ext-session-lock" "ext-session-lock-v1" false)
protocolnew("stable/linux-dmabuf" "linux-dmabuf-v1" false)
protocolnew("staging/fractional-scale" "fractional-scale-v1" false)
protocolnew("stable/viewporter" "viewporter" false)
protocolnew("staging/cursor-shape" "cursor-shape-v1" false)
protocolnew("stable/tablet" "tablet-v2" false)
protocolnew("stable/xdg-shell" "xdg-shell" false)  # For xdg_popup_interface

# Installation
install(TARGETS mpvlock)

install(FILES ${CMAKE_SOURCE_DIR}/pam/mpvlock
        DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/pam.d)

install(
  FILES ${CMAKE_SOURCE_DIR}/assets/example.conf
  DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/mpvlock
  RENAME mpvlock.conf)