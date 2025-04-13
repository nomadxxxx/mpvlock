# Maintainer: RAMA <rama@archpcx.one>
pkgname=mpvlock-git
pkgver=r123.4567890  # Updated by makepkg
pkgrel=1
pkgdesc="A screen locking utility based on hyprlock that supports video and audio"
arch=('x86_64')
url="https://github.com/nomadxxxx/mpvlock"
license=('GPL-3.0-or-later')
depends=(
  'wayland'
  'wayland-protocols>=1.35'
  'mesa'
  'hyprwayland-scanner>=0.4.4'
  'cairo'
  'libdrm'
  'pango'
  'libxkbcommon'
  'pam'
  'hyprlang>=0.6.0'
  'hyprutils>=0.5.0'
  'hyprgraphics'
  'libmagic'
  'mpvpaper'
  'libjpeg-turbo'
  'libwebp'
  'sdbus-cpp>=2.0.0'
)
makedepends=('git' 'cmake' 'ninja')
provides=('mpvlock')
conflicts=('hyprlock' 'hyprlock-git' 'mpvlock')
source=("git+$url.git")
sha256sums=('SKIP')

pkgver() {
  cd "${srcdir}/mpvlock"
  printf "r%s.%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

build() {
  cd "${srcdir}/mpvlock"
  cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr
  ninja -C build
}

package() {
  cd "${srcdir}/mpvlock"
  DESTDIR="${pkgdir}" ninja -C build install
  install -Dm644 LICENSE "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE"
  install -Dm644 pam/mpvlock "${pkgdir}/etc/pam.d/mpvlock"
}
