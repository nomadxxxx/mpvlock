#!/usr/bin/env bash

CACHE_DIR="$HOME/.config/mpvlock"
CACHE_FILE="$CACHE_DIR/gputemp"
mkdir -p "$CACHE_DIR"

# Try Nvidia
if command -v nvidia-smi &>/dev/null; then
    GPU_TEMP=$(nvidia-smi --query-gpu=temperature.gpu --format=csv,noheader,nounits | head -1)
    if [[ -n "$GPU_TEMP" ]]; then
        if ((GPU_TEMP >= 80)); then ICON="🔥"
        elif ((GPU_TEMP >= 60)); then ICON="🌡️"
        else ICON="❄️"; fi
        echo "GPU temp, ${GPU_TEMP}°C $ICON" | tee "$CACHE_FILE"
        exit 0
    fi
fi

# Try AMD SMI (ROCm/AMDGPU)
if command -v amdgpu-smi &>/dev/null; then
    GPU_TEMP=$(amdgpu-smi --showtemp | grep -m1 'Temperature' | grep -o '[0-9]\+')
    if [[ -n "$GPU_TEMP" ]]; then
        if ((GPU_TEMP >= 80)); then ICON="🔥"
        elif ((GPU_TEMP >= 60)); then ICON="🌡️"
        else ICON="❄️"; fi
        echo "GPU temp, ${GPU_TEMP}°C $ICON" | tee "$CACHE_FILE"
        exit 0
    fi
fi
if command -v amdsmi &>/dev/null; then
    GPU_TEMP=$(amdsmi --showtemp | grep -m1 'Temperature' | grep -o '[0-9]\+')
    if [[ -n "$GPU_TEMP" ]]; then
        if ((GPU_TEMP >= 80)); then ICON="🔥"
        elif ((GPU_TEMP >= 60)); then ICON="🌡️"
        else ICON="❄️"; fi
        echo "GPU temp, ${GPU_TEMP}°C $ICON" | tee "$CACHE_FILE"
        exit 0
    fi
fi

# Try lm-sensors (for Intel/others)
if command -v sensors &>/dev/null; then
    GPU_TEMP=$(sensors | awk '/edge:/ {print int($2)}; /temp1:/ {print int($2)}' | head -1)
    if [[ -n "$GPU_TEMP" ]]; then
        if ((GPU_TEMP >= 80)); then ICON="🔥"
        elif ((GPU_TEMP >= 60)); then ICON="🌡️"
        else ICON="❄️"; fi
        echo "GPU temp, ${GPU_TEMP}°C $ICON" | tee "$CACHE_FILE"
        exit 0
    fi
fi

echo "GPU temp, No Data" | tee "$CACHE_FILE"
