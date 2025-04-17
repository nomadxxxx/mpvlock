# mpvlock

A comprehensive fork of hyprlock that provides video and audio support and additional themeing options such as vertical labels and shapes.

![All Themes Preview](https://github.com/nomadxxxx/mpvlock-themes/blob/main/assets/solitude.png)

### Features:
1. Locks your screen with a customizable video background using mpv.
2. Designed for Wayland and Hyprland from hyprlock to provide more flexible theming options.
3. Integrates with hypridle for idle timeout locking.

### Prerequisites

Before installing mpvlock, ensure you have the following dependencies:

    mpvpaper (for video backgrounds)
    hypridle (optional, for idle timeout integration)

### Installation via AUR

mpvlock is available via AUR:
```
yay -S mpvlock-git
```

### Installation via script (testing)
```bash
git clone https://github.com/nomadxxxx/mpvlock-themes.git
cd mpvlock-themes
chmod +x mpvlock_theme_installer.sh
./mpvlock_theme_installer.sh
```
**Note I'm still developing this script and have only tested on arch although ubuntu and fedora are supported.
### Alternatively, you can manually clone and build the package:
```
git clone https://aur.archlinux.org/mpvlock-git.git
cd mpvlock-git
makepkg -si
```

### Configuration

mpvlock installs default configuration files to ~/.config/mpvlock/. You can customize the video background and other settings by editing ~/.config/mpvlock/mpvlock.conf

Scripts are located in ~/.config/mpvlock/scripts and you will need to update your location in weather.sh to get the weather widget working.

## Detailed Previews

| **cat-pool** | **cherry-blossom** |
|:--:|:--:|
| <img src="https://github.com/nomadxxxx/mpvlock-themes/blob/main/assets/cat-pool.png" width="500"> | <img src="https://github.com/nomadxxxx/mpvlock-themes/blob/main/assets/cherry-blossom.png" width="500"> |

| **elon** | **gothgirl** |
|:--:|:--:|
| <img src="https://github.com/nomadxxxx/mpvlock-themes/blob/main/assets/elon.png" width="500"> | <img src="https://github.com/nomadxxxx/mpvlock-themes/blob/main/assets/gothgirl.png" width="500"> |

| **solitude** | **Neon Jinx** |
|:--:|:--:|
| <img src="https://github.com/nomadxxxx/mpvlock-themes/blob/main/assets/solitude.png" width="500"> | <img src="https://github.com/nomadxxxx/mpvlock-themes/blob/main/assets/neon_jinx.png" width="500"> |

The themes are avaialble [here.](https://github.com/nomadxxxx/mpvlock-themes/tree/main). You can also install them using below:

### Install theme

Install a specific theme directly without cloning the repository:

- **Install a specific theme** (e.g., `cherry-blossom`):
  ```bash
  curl -L https://raw.githubusercontent.com/nomadxxxx/mpvlock-themes/main/mpvlock_theme_installer.sh | bash -s - -t cherry-blossom
  ```
  Replace `cherry-blossom` with any available theme name (see below).
### Installation (Manual Method)

1. **Prerequisites**:
   - `curl` must be installed on your system.
   - Git, CMake, Ninja, and various development libraries will be installed automatically by the script if missing (hopfully).

2. **Clone the Repository** (optional for advanced users):
   ```bash
   git clone https://github.com/nomadxxxx/mpvlock-themes.git
   cd mpvlock-themes
   ```

3. **Run the Installer**:
   ```bash
   chmod +x mpvlock_theme_installer.sh
   ./mpvlock_theme_installer.sh
   ```
   - Follow the on-screen menu to install mpvlock (option 1) or select a theme (options 2-7).
   - Themes are installed to `~/.config/mpvlock/themes/`, and the configuration is updated accordingly.

### Contributing

Contributions are welcome! Please submit issues or pull requests to the upstream repository (if available).
License

mpvlock is licensed under the BSD License. See the LICENSE file for details.
