{
  lib,
  stdenv,
  cmake,
  pkg-config,
  cairo,
  file,
  libdrm,
  libGL,
  libjpeg,
  libwebp,
  libxkbcommon,
  libgbm,
  hyprgraphics,
  hyprlang,
  hyprutils,
  hyprwayland-scanner,
  pam,
  pango,
  sdbus-cpp,
  systemdLibs,
  wayland,
  wayland-protocols,
  wayland-scanner,
  libmpv, # Added for mpvpaper
  date-tz ? null, # Optional, confirm need
  version ? "git",
  shortRev ? "",
}:
stdenv.mkDerivation {
  pname = "mpvlock";
  inherit version;

  src = ../.;

  nativeBuildInputs = [
    cmake
    pkg-config
    hyprwayland-scanner
    wayland-scanner
  ];

  buildInputs = [
    cairo
    file
    libdrm
    libGL
    libjpeg
    libwebp
    libxkbcommon
    libgbm
    hyprgraphics
    hyprlang
    hyprutils
    pam
    pango
    sdbus-cpp
    systemdLibs
    wayland
    wayland-protocols
    libmpv # Added
  ] ++ lib.optionals (date-tz != null) [ date-tz ]; # Optional

  cmakeFlags = lib.mapAttrsToList lib.cmakeFeature {
    MPVLOCK_COMMIT = shortRev;
    MPVLOCK_VERSION_COMMIT = ""; # Intentionally left empty (mpvlock --version will always print the commit)
  };

  meta = {
    homepage = "https://github.com/nomadxxxx/mpvlock"; 
    description = "A gpu-accelerated screen lock based on Hyprland’s hyprlock with video and audio support";
    license = lib.licenses.bsd3;
    platforms = lib.platforms.linux;
    mainProgram = "mpvlock";
  };
}