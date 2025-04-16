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

You can install these dependencies with:
```
sudo pacman -S hyprland mpv mpvpaper hypridle
```
### Installation via AUR

mpvlock is available via AUR:
```
yay -S mpvlock-git
```

Alternatively, you can manually clone and build the package:

```
git clone https://aur.archlinux.org/mpvlock-git.git
cd mpvlock-git
makepkg -si
```

### Configuration

mpvlock installs default configuration files to ~/.config/mpvlock/. You can customize the video background and other settings by editing ~/.config/mpvlock/mpvlock.conf

## Detailed Previews

| **cat-pool** | **cherry-blossom** |
|:--:|:--:|
| <img src="https://github.com/nomadxxxx/mpvlock-themes/blob/main/assets/cat-pool.png" width="500"> | <img src="https://github.com/nomadxxxx/mpvlock-themes/blob/main/assets/cherry-blossom.png" width="500"> |

| **elon** | **gothgirl** |
|:--:|:--:|
| <img src="https://github.com/nomadxxxx/mpvlock-themes/blob/main/assets/elon.png" width="500"> | <img src="https://github.com/nomadxxxx/mpvlock-themes/blob/main/assets/gothgirl.png" width="500"> |

| **solitude** | **Neon Jinx** |
|:--:|:--:|
| <img src="https://github.com/nomadxxxx/mpvlock-themes/blob/main/assets/solitude.png" width="500"> | <img src="https://github.com/nomadxxxx/hyprddm/raw/master/Previews/neon_jinx.png" width="500"> |

The themes are avaialble [here.](https://github.com/nomadxxxx/mpvlock-themes/tree/main) I will create an installer script soon, until then you need to manually move them into .config/mpvlock/themes. When you have selected the theme you want just replace your mpvlock.conf witht he one from your selected theme.

### Contributing

Contributions are welcome! Please submit issues or pull requests to the upstream repository (if available).
License

mpvlock is licensed under the BSD License. See the LICENSE file for details.

You can copy this text directly into a README.md file in your project directory, for example, by using a text editor or a command like echo or cat on the command line. Let me know if you need help saving it to a file or making any adjustments!
