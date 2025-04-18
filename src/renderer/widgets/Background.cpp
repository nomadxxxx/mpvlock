#include "Background.hpp"
#include "../Renderer.hpp"
#include "../AsyncResourceGatherer.hpp"
#include "../../core/mpvlock.hpp"
#include "../../helpers/Log.hpp"
#include "../../helpers/MiscFunctions.hpp"
#include <chrono>
#include <hyprlang.hpp>
#include <filesystem>
#include <memory>
#include <cstring>
#include <GLES3/gl32.h>
#include <magic.h>

extern UP<CRenderer> g_pRenderer;

CBackground::~CBackground() {
    reset();
    if (m_bIsVideoBackground && !monitor.empty()) {
        Debug::log(LOG, "Stopping mpvpaper for monitor {}", monitor);
        g_pRenderer->stopMpvpaper(monitor);
        m_bIsVideoBackground = false;
    }
}

void CBackground::registerSelf(const SP<CBackground>& self) {
    m_self = self;
}

std::string CBackground::type() const {
    return "background";
}

void CBackground::setZindex(int zindex) {
    m_iZindex = zindex;
}

int CBackground::getZindex() const {
    return m_iZindex;
}

bool CBackground::isVideoBackground() const {
    return m_bIsVideoBackground;
}

void CBackground::onTimer(std::shared_ptr<CTimer> timer, void* data) {
    WP<CBackground> ref = *static_cast<WP<CBackground>*>(data);
    if (auto PBACKGROUND = ref.lock(); PBACKGROUND) {
        if (timer == PBACKGROUND->reloadTimer) {
            PBACKGROUND->onReloadTimerUpdate();
            PBACKGROUND->plantReloadTimer();
        } else if (timer == PBACKGROUND->fadeAnimation.fadeTimer) {
            auto now = std::chrono::system_clock::now();
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - PBACKGROUND->fadeAnimation.startTime).count();
            float progress = static_cast<float>(elapsedMs) / PBACKGROUND->fadeAnimation.durationMs;

            if (progress >= 1.0f) {
                // Fading complete
                PBACKGROUND->fadeAnimation.opacity = PBACKGROUND->fadeAnimation.fadingIn ? 1.0f : 0.0f;
                PBACKGROUND->fadeAnimation.fadeTimer->cancel();
                PBACKGROUND->fadeAnimation.fadeTimer.reset();
                PBACKGROUND->fadeAnimation.fadingIn = !PBACKGROUND->fadeAnimation.fadingIn;
                Debug::log(TRACE, "CBackground fading complete: opacity={}", PBACKGROUND->fadeAnimation.opacity);
            } else {
                // Update opacity
                PBACKGROUND->fadeAnimation.opacity = PBACKGROUND->fadeAnimation.fadingIn ? progress : 1.0f - progress;
                Debug::log(TRACE, "CBackground fading: progress={}, opacity={}", progress, PBACKGROUND->fadeAnimation.opacity);
            }

            // Trigger a render
            g_pMpvlock->renderOutput(PBACKGROUND->outputPort);
        } else if (timer == PBACKGROUND->fade->crossFadeTimer) {
            PBACKGROUND->onCrossFadeTimerUpdate();
        }
    }
}

void CBackground::configure(const std::unordered_map<std::string, std::any>& props, const SP<COutput>& pOutput) {
    try {
        reset();

        // Parse zindex
        if (props.contains("zindex")) {
            try {
                m_iZindex = std::any_cast<Hyprlang::INT>(props.at("zindex"));
                Debug::log(LOG, "Background configured with zindex: {}", m_iZindex);
            } catch (const std::exception& e) {
                Debug::log(WARN, "Failed to parse zindex for background: {}, defaulting to -1", e.what());
                m_iZindex = -1;
            }
        } else {
            m_iZindex = -1;
        }

        // Parse color
        if (props.contains("color")) {
            try {
                const auto& colorVal = props.at("color");
                if (colorVal.type() == typeid(Hyprlang::STRING)) {
                    std::string colorStr = std::any_cast<Hyprlang::STRING>(colorVal);
                    if (colorStr.starts_with("0x") || colorStr.starts_with("#"))
                        colorStr = colorStr.substr(2);
                    uint64_t colorValue = std::stoull(colorStr, nullptr, 16);
                    color = CHyprColor(colorValue);
                } else if (colorVal.type() == typeid(Hyprlang::INT)) {
                    uint64_t colorValue = std::any_cast<Hyprlang::INT>(colorVal);
                    color = CHyprColor(colorValue);
                } else {
                    throw std::bad_any_cast();
                }
            } catch (const std::exception& e) {
                Debug::log(ERR, "Failed to parse color: {}", e.what());
                color = CHyprColor(0, 0, 0, 0);
            }
        } else {
            color = CHyprColor(0, 0, 0, 0);
        }

        blurPasses = 3;
        if (props.contains("blur_passes")) {
            try {
                const auto& val = props.at("blur_passes");
                if (val.type() == typeid(Hyprlang::INT)) {
                    blurPasses = std::any_cast<Hyprlang::INT>(val);
                } else {
                    Debug::log(WARN, "blur_passes has unexpected type, using default: 3");
                }
            } catch (const std::exception& e) {
                Debug::log(ERR, "Failed to parse blur_passes: {}", e.what());
            }
        }

        blurSize = 10;
        if (props.contains("blur_size")) {
            try {
                const auto& val = props.at("blur_size");
                if (val.type() == typeid(Hyprlang::INT)) {
                    blurSize = std::any_cast<Hyprlang::INT>(val);
                } else {
                    Debug::log(WARN, "blur_size has unexpected type, using default: 10");
                }
            } catch (const std::exception& e) {
                Debug::log(ERR, "Failed to parse blur_size: {}", e.what());
            }
        }

        vibrancy = 0.1696f;
        if (props.contains("vibrancy")) {
            try {
                const auto& val = props.at("vibrancy");
                if (val.type() == typeid(Hyprlang::FLOAT)) {
                    vibrancy = std::any_cast<Hyprlang::FLOAT>(val);
                } else if (val.type() == typeid(Hyprlang::INT)) {
                    vibrancy = static_cast<float>(std::any_cast<Hyprlang::INT>(val));
                } else {
                    Debug::log(WARN, "vibrancy has unexpected type, using default: 0.1696");
                }
            } catch (const std::exception& e) {
                Debug::log(ERR, "Failed to parse vibrancy: {}", e.what());
            }
        }

        vibrancy_darkness = 0.f;
        if (props.contains("vibrancy_darkness")) {
            try {
                const auto& val = props.at("vibrancy_darkness");
                if (val.type() == typeid(Hyprlang::FLOAT)) {
                    vibrancy_darkness = std::any_cast<Hyprlang::FLOAT>(val);
                } else if (val.type() == typeid(Hyprlang::INT)) {
                    vibrancy_darkness = static_cast<float>(std::any_cast<Hyprlang::INT>(val));
                } else {
                    Debug::log(WARN, "vibrancy_darkness has unexpected type, using default: 0");
                }
            } catch (const std::exception& e) {
                Debug::log(ERR, "Failed to parse vibrancy_darkness: {}", e.what());
            }
        }

        noise = 0.0117f;
        if (props.contains("noise")) {
            try {
                const auto& val = props.at("noise");
                if (val.type() == typeid(Hyprlang::FLOAT)) {
                    noise = std::any_cast<Hyprlang::FLOAT>(val);
                } else if (val.type() == typeid(Hyprlang::INT)) {
                    noise = static_cast<float>(std::any_cast<Hyprlang::INT>(val));
                } else {
                    Debug::log(WARN, "noise has unexpected type, using default: 0.0117");
                }
            } catch (const std::exception& e) {
                Debug::log(ERR, "Failed to parse noise: {}", e.what());
            }
        }

        brightness = 0.8172f;
        if (props.contains("brightness")) {
            try {
                const auto& val = props.at("brightness");
                if (val.type() == typeid(Hyprlang::FLOAT)) {
                    brightness = std::any_cast<Hyprlang::FLOAT>(val);
                } else if (val.type() == typeid(Hyprlang::INT)) {
                    brightness = static_cast<float>(std::any_cast<Hyprlang::INT>(val));
                } else {
                    Debug::log(WARN, "brightness has unexpected type, using default: 0.8172");
                }
            } catch (const std::exception& e) {
                Debug::log(ERR, "Failed to parse brightness: {}", e.what());
            }
        }

        contrast = 0.8916f;
        if (props.contains("contrast")) {
            try {
                const auto& val = props.at("contrast");
                if (val.type() == typeid(Hyprlang::FLOAT)) {
                    contrast = std::any_cast<Hyprlang::FLOAT>(val);
                } else if (val.type() == typeid(Hyprlang::INT)) {
                    contrast = static_cast<float>(std::any_cast<Hyprlang::INT>(val));
                } else {
                    Debug::log(WARN, "contrast has unexpected type, using default: 0.8916");
                }
            } catch (const std::exception& e) {
                Debug::log(ERR, "Failed to parse contrast: {}", e.what());
            }
        }

        path = "";
        if (props.contains("path")) {
            try {
                const auto& val = props.at("path");
                if (val.type() == typeid(Hyprlang::STRING)) {
                    path = std::any_cast<Hyprlang::STRING>(val);
                } else {
                    Debug::log(WARN, "path has unexpected type, using default: empty");
                }
            } catch (const std::exception& e) {
                Debug::log(ERR, "Failed to parse path: {}", e.what());
            }
        }

        reloadCommand = "";
        if (props.contains("reload_cmd")) {
            try {
                const auto& val = props.at("reload_cmd");
                if (val.type() == typeid(Hyprlang::STRING)) {
                    reloadCommand = std::any_cast<Hyprlang::STRING>(val);
                } else {
                    Debug::log(WARN, "reload_cmd has unexpected type, using default: empty");
                }
            } catch (const std::exception& e) {
                Debug::log(ERR, "Failed to parse reload_cmd: {}", e.what());
            }
        }

        reloadTime = -1;
        if (props.contains("reload_time")) {
            try {
                const auto& val = props.at("reload_time");
                if (val.type() == typeid(Hyprlang::INT)) {
                    reloadTime = std::any_cast<Hyprlang::INT>(val);
                } else {
                    Debug::log(WARN, "reload_time has unexpected type, using default: -1");
                }
            } catch (const std::exception& e) {
                Debug::log(ERR, "Failed to parse reload_time: {}", e.what());
            }
        }

        crossFadeTime = -1.f;
        if (props.contains("crossfade_time")) {
            try {
                const auto& val = props.at("crossfade_time");
                if (val.type() == typeid(Hyprlang::FLOAT)) {
                    crossFadeTime = std::any_cast<Hyprlang::FLOAT>(val);
                } else if (val.type() == typeid(Hyprlang::INT)) {
                    crossFadeTime = static_cast<float>(std::any_cast<Hyprlang::INT>(val));
                } else {
                    Debug::log(WARN, "crossfade_time has unexpected type, using default: -1");
                }
            } catch (const std::exception& e) {
                Debug::log(ERR, "Failed to parse crossfade_time: {}", e.what());
            }
        }

        fallbackPath = "";
        if (props.contains("fallback_path")) {
            try {
                const auto& val = props.at("fallback_path");
                if (val.type() == typeid(Hyprlang::STRING)) {
                    fallbackPath = std::any_cast<Hyprlang::STRING>(val);
                } else {
                    Debug::log(WARN, "fallback_path has unexpected type, using default: empty");
                }
            } catch (const std::exception& e) {
                Debug::log(ERR, "Failed to parse fallback_path: {}", e.what());
            }
        }

        // Parse new fading options
        fadeAnimation.enabled = false;
        if (props.contains("fade")) {
            auto val = props.at("fade");
            if (val.type() == typeid(Hyprlang::INT)) {
                fadeAnimation.enabled = std::any_cast<Hyprlang::INT>(val) != 0;
            } else {
                Debug::log(WARN, "Background fade has unexpected type, defaulting to disabled");
            }
        }

        fadeAnimation.durationMs = 1000;
        if (props.contains("fade_duration")) {
            auto val = props.at("fade_duration");
            if (val.type() == typeid(Hyprlang::INT)) {
                fadeAnimation.durationMs = std::any_cast<Hyprlang::INT>(val);
                if (fadeAnimation.durationMs <= 0) {
                    Debug::log(WARN, "Background fade_duration invalid value: {}, defaulting to 1000ms", fadeAnimation.durationMs);
                    fadeAnimation.durationMs = 1000;
                }
            } else if (val.type() == typeid(Hyprlang::FLOAT)) {
                fadeAnimation.durationMs = static_cast<uint64_t>(std::any_cast<Hyprlang::FLOAT>(val));
                if (fadeAnimation.durationMs <= 0) {
                    Debug::log(WARN, "Background fade_duration invalid value: {}, defaulting to 1000ms", fadeAnimation.durationMs);
                    fadeAnimation.durationMs = 1000;
                }
            } else {
                Debug::log(WARN, "Background fade_duration has unexpected type, defaulting to 1000ms");
            }
        }

        isScreenshot = path == "screenshot";
        monitor = pOutput->stringPort;
        viewport = pOutput->getViewport();
        outputPort = pOutput->stringPort;
        transform = isScreenshot ? wlTransformToHyprutils(invertTransform(pOutput->transform)) : HYPRUTILS_TRANSFORM_NORMAL;

        m_bIsVideoBackground = false;
        videoPath = "";
        resourceID = "";

        // Check if path is a video using libmagic
        if (!path.empty()) {
            magic_t magic = magic_open(MAGIC_MIME_TYPE | MAGIC_SYMLINK);
            if (!magic) {
                Debug::log(ERR, "Failed to initialize libmagic for path: {}", path);
            } else if (magic_load(magic, nullptr) != 0) {
                Debug::log(ERR, "Failed to load magic database: {}", magic_error(magic));
                magic_close(magic);
            } else {
                const char* mime = magic_file(magic, path.c_str());
                bool isVideo = mime && strncmp(mime, "video/", 6) == 0;
                Debug::log(LOG, "File {} MIME type: {}", path, mime ? mime : "unknown");

                if (isVideo && std::filesystem::exists(path)) {
                    Debug::log(LOG, "Detected video background: {}", path);
                    videoPath = path;
                    m_bIsVideoBackground = true;

                    // Build mpvpaper options
                    std::string mpvOptions = "loop";
                    if (props.contains("mpvpaper_fps")) {
                        try {
                            mpvOptions += " --vf=fps=" + std::to_string(std::any_cast<Hyprlang::INT>(props.at("mpvpaper_fps")));
                        } catch (const std::exception& e) {
                            Debug::log(ERR, "Failed to parse mpvpaper_fps: {}", e.what());
                        }
                    }
                    if (props.contains("mpvpaper_panscan")) {
                        try {
                            mpvOptions += " panscan=" + std::to_string(std::any_cast<Hyprlang::FLOAT>(props.at("mpvpaper_panscan")));
                        } catch (const std::exception& e) {
                            Debug::log(ERR, "Failed to parse mpvpaper_panscan: {}", e.what());
                        }
                    }
                    if (props.contains("mpvpaper_hwdec")) {
                        try {
                            mpvOptions += " --hwdec=" + std::string(std::any_cast<Hyprlang::STRING>(props.at("mpvpaper_hwdec")));
                        } catch (const std::exception& e) {
                            Debug::log(ERR, "Failed to parse mpvpaper_hwdec: {}", e.what());
                        }
                    }
                    if (props.contains("mpvpaper_mute")) {
                        try {
                            if (std::any_cast<Hyprlang::INT>(props.at("mpvpaper_mute")) == 0) {
                                mpvOptions += " --mute=no";
                            } else {
                                mpvOptions += " --mute=yes";
                            }
                        } catch (const std::exception& e) {
                            Debug::log(ERR, "Failed to parse mpvpaper_mute: {}, defaulting to mute", e.what());
                            mpvOptions += " --mute=yes";
                        }
                    } else {
                        mpvOptions += " --mute=yes";
                    }
                    std::string mpvpaperLayer = "overlay";
                    if (props.contains("mpvpaper_layer")) {
                        try {
                            mpvpaperLayer = std::any_cast<Hyprlang::STRING>(props.at("mpvpaper_layer"));
                            Debug::log(LOG, "Parsed mpvpaper_layer: {}", mpvpaperLayer);
                        } catch (const std::exception& e) {
                            Debug::log(ERR, "Failed to parse mpvpaper_layer: {}, defaulting to overlay", e.what());
                        }
                    }

                    Debug::log(LOG, "Attempting to start mpvpaper for monitor {} with video {}", monitor, path);
                    bool mpvSuccess = g_pRenderer->startMpvpaper(monitor, path, mpvpaperLayer, mpvOptions);
                    if (!mpvSuccess) {
                        Debug::log(ERR, "startMpvpaper failed for monitor {}, video {}", monitor, path);
                        m_bIsVideoBackground = false;
                    }
                }
                magic_close(magic);
            }
        }

        // Handle video failure or non-video path
        if (!m_bIsVideoBackground) {
            std::string targetPath = path;
            if (!path.empty() && !std::filesystem::exists(path) && !fallbackPath.empty()) {
                Debug::log(LOG, "Path {} does not exist, attempting fallback: {}", path, fallbackPath);
                targetPath = fallbackPath;
            }

            if (!targetPath.empty() && !targetPath.ends_with(".mp4") && !targetPath.ends_with(".mkv")) {
                resourceID = isScreenshot ? CScreencopyFrame::getResourceId(pOutput) : "background:" + targetPath;
                if (!isScreenshot) {
                    CAsyncResourceGatherer::SPreloadRequest request;
                    request.id = resourceID;
                    request.asset = targetPath;
                    request.type = CAsyncResourceGatherer::eTargetType::TARGET_IMAGE;
                    request.callback = [REF = m_self]() { REF.lock()->startCrossFadeOrUpdateRender(); };
                    g_pRenderer->asyncResourceGatherer->requestAsyncAssetPreload(request);
                    Debug::log(LOG, "Requested async preload for resource: {}", resourceID);
                }
            } else if (!resourceID.empty() && resourceID != CScreencopyFrame::getResourceId(pOutput)) {
                Debug::log(ERR, "No valid background or fallback path, falling back to transparent");
                resourceID = "";
            }

            if (isScreenshot && !resourceID.empty() && g_pMpvlock->getScreencopy()) {
                if (g_pRenderer->asyncResourceGatherer->gathered) {
                    if (!g_pRenderer->asyncResourceGatherer->getAssetByID(resourceID)) {
                        Debug::log(ERR, "Screencopy resource {} not found", resourceID);
                        resourceID = "";
                    }
                }
            } else if (isScreenshot) {
                Debug::log(ERR, "No screencopy support! path=screenshot won't work. Falling back to transparent.");
                resourceID = "";
            }
        }

        if (!isScreenshot && !m_bIsVideoBackground && reloadTime > -1) {
            try {
                modificationTime = std::filesystem::last_write_time(absolutePath(path.empty() ? fallbackPath : path, ""));
            } catch (std::exception& e) {
                Debug::log(ERR, "Failed to get modification time for {}: {}", path.empty() ? fallbackPath : path, e.what());
            }
            plantReloadTimer();
        }

        // Start fading if enabled
        if (fadeAnimation.enabled) {
            startFade();
        }
    } catch (const std::exception& e) {
        Debug::log(ERR, "Exception in CBackground::configure: {}", e.what());
        m_bIsVideoBackground = false;
        resourceID = "";
    }
}

void CBackground::reset() {
    if (reloadTimer) {
        reloadTimer->cancel();
        reloadTimer.reset();
    }
    if (fade) {
        if (fade->crossFadeTimer) {
            fade->crossFadeTimer->cancel();
            fade->crossFadeTimer.reset();
        }
        fade.reset();
    }
    if (fadeAnimation.fadeTimer) {
        fadeAnimation.fadeTimer->cancel();
        fadeAnimation.fadeTimer.reset();
    }
}

void CBackground::renderRect(CHyprColor color) {
    CBox monbox = {0, 0, (int)viewport.x, (int)viewport.y};
    g_pRenderer->renderRect(monbox, color, 0);
}

bool CBackground::draw(const SRenderData& data) {
    if (m_bIsVideoBackground) {
        static int skipBackgroundCount = 0;
        if (skipBackgroundCount++ % 100 == 0)
            Debug::log(TRACE, "Skipping static background rendering; using video background via mpvpaper");
        return false;
    }

    float adjustedOpacity = data.opacity * fadeAnimation.opacity;

    if (resourceID.empty()) {
        CHyprColor col = color;
        col.a *= adjustedOpacity;
        renderRect(col);
        return adjustedOpacity < 1.0;
    }

    if (!asset)
        asset = g_pRenderer->asyncResourceGatherer->getAssetByID(resourceID);

    if (!asset) {
        CHyprColor col = color;
        col.a *= adjustedOpacity;
        renderRect(col);
        return true;
    }

    if (asset->texture.m_iType == TEXTURE_INVALID) {
        g_pRenderer->asyncResourceGatherer->unloadAsset(asset);
        resourceID = "";
        return true;
    }

    if (fade || ((blurPasses > 0 || isScreenshot) && (!blurredFB.isAllocated() || firstRender))) {
        if (firstRender)
            firstRender = false;

        Vector2D size = asset->texture.m_vSize;
        if (transform % 2 == 1 && isScreenshot) {
            size.x = asset->texture.m_vSize.y;
            size.y = asset->texture.m_vSize.x;
        }

        CBox texbox = {{}, size};
        float scaleX = viewport.x / size.x;
        float scaleY = viewport.y / size.y;

        texbox.w *= std::max(scaleX, scaleY);
        texbox.h *= std::max(scaleX, scaleY);

        if (scaleX > scaleY)
            texbox.y = -(texbox.h - viewport.y) / 2.f;
        else
            texbox.x = -(texbox.w - viewport.x) / 2.f;
        texbox.round();

        if (!blurredFB.isAllocated())
            blurredFB.alloc((int)viewport.x, (int)viewport.y);

        blurredFB.bind();

        if (fade)
            g_pRenderer->renderTextureMix(texbox, asset->texture, pendingAsset->texture, 1.0,
                                          std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - fade->start).count() / (1000 * crossFadeTime), 0,
                                          transform);
        else
            g_pRenderer->renderTexture(texbox, asset->texture, 1.0, 0, transform);

        if (blurPasses > 0)
            g_pRenderer->blurFB(blurredFB,
                                CRenderer::SBlurParams{.size = blurSize,
                                                       .passes = blurPasses,
                                                       .noise = noise,
                                                       .contrast = contrast,
                                                       .brightness = brightness,
                                                       .vibrancy = vibrancy,
                                                       .vibrancy_darkness = vibrancy_darkness});
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    }

    CTexture* tex = blurredFB.isAllocated() ? &blurredFB.m_cTex : &asset->texture;

    CBox      texbox = {{}, tex->m_vSize};
    Vector2D  size   = tex->m_vSize;
    float     scaleX = viewport.x / tex->m_vSize.x;
    float     scaleY = viewport.y / tex->m_vSize.y;

    texbox.w *= std::max(scaleX, scaleY);
    texbox.h *= std::max(scaleX, scaleY);

    if (scaleX > scaleY)
        texbox.y = -(texbox.h - viewport.y) / 2.f;
    else
        texbox.x = -(texbox.w - viewport.x) / 2.f;
    texbox.round();
    g_pRenderer->renderTexture(texbox, *tex, adjustedOpacity, 0, HYPRUTILS_TRANSFORM_FLIPPED_180);

    return fade || adjustedOpacity < 1.0;
}

void CBackground::plantReloadTimer() {
    if (reloadTime == 0)
        reloadTimer = g_pMpvlock->addTimer(std::chrono::hours(1),
            [REF = m_self](std::shared_ptr<CTimer> timer, void*) { REF.lock()->onTimer(timer, (void*)&REF); }, nullptr, true);
    else if (reloadTime > -1)
        reloadTimer = g_pMpvlock->addTimer(std::chrono::seconds(reloadTime),
            [REF = m_self](std::shared_ptr<CTimer> timer, void*) { REF.lock()->onTimer(timer, (void*)&REF); }, nullptr, true);
}

void CBackground::onReloadTimerUpdate() {
    const std::string OLDPATH = path;

    if (!reloadCommand.empty()) {
        path = g_pMpvlock->spawnSync(reloadCommand);
        if (path.ends_with('\n'))
            path.pop_back();
        if (path.starts_with("file://"))
            path = path.substr(7);
        if (path.empty())
            return;
    }

    if (m_bIsVideoBackground)
        return;

    try {
        const auto MTIME = std::filesystem::last_write_time(absolutePath(path.empty() ? fallbackPath : path, ""));
        if (OLDPATH == path && MTIME == modificationTime)
            return;
        modificationTime = MTIME;
    } catch (std::exception& e) {
        path = OLDPATH;
        Debug::log(ERR, "Failed to update modification time: {}", e.what());
        return;
    }

    if (!pendingResourceID.empty())
        return;

    request.id = std::string{"background:"} + path + ",time:" + std::to_string((uint64_t)modificationTime.time_since_epoch().count());
    pendingResourceID = request.id;
    request.asset = path;
    request.type = CAsyncResourceGatherer::eTargetType::TARGET_IMAGE;

    request.callback = [REF = m_self]() { REF.lock()->startCrossFadeOrUpdateRender(); };
    g_pRenderer->asyncResourceGatherer->requestAsyncAssetPreload(request);
}

void CBackground::onCrossFadeTimerUpdate() {
    if (fade) {
        fade->crossFadeTimer.reset();
        fade.reset();
    }

    if (blurPasses <= 0 && !isScreenshot)
        blurredFB.release();

    asset = pendingAsset;
    resourceID = pendingResourceID;
    pendingResourceID = "";
    pendingAsset = nullptr;
    firstRender = true;

    g_pMpvlock->renderOutput(outputPort);
}

void CBackground::startCrossFadeOrUpdateRender() {
    auto newAsset = g_pRenderer->asyncResourceGatherer->getAssetByID(pendingResourceID);
    if (newAsset) {
        if (newAsset->texture.m_iType == TEXTURE_INVALID) {
            g_pRenderer->asyncResourceGatherer->unloadAsset(newAsset);
            Debug::log(ERR, "New asset had an invalid texture!");
        } else if (resourceID != pendingResourceID) {
            pendingAsset = newAsset;
            if (crossFadeTime > 0) {
                if (!fade)
                    fade = makeUnique<SFade>();
                else {
                    if (fade->crossFadeTimer) {
                        fade->crossFadeTimer->cancel();
                        fade->crossFadeTimer.reset();
                    }
                }
                fade->start = std::chrono::system_clock::now();
                fade->a = 0;
                fade->crossFadeTimer =
                    g_pMpvlock->addTimer(std::chrono::milliseconds((int)(1000.0 * crossFadeTime)),
                        [REF = m_self](std::shared_ptr<CTimer> timer, void*) { REF.lock()->onTimer(timer, (void*)&REF); }, nullptr, true);
            } else {
                onCrossFadeTimerUpdate();
            }
        }
    } else if (!pendingResourceID.empty()) {
        Debug::log(WARN, "Asset {} not available after the asyncResourceGatherer's callback!", pendingResourceID);
        g_pMpvlock->addTimer(std::chrono::milliseconds(100),
            [REF = m_self](std::shared_ptr<CTimer> timer, void*) { REF.lock()->startCrossFadeOrUpdateRender(); }, nullptr, true);
    }

    g_pMpvlock->renderOutput(outputPort);
}

void CBackground::startFade() {
    if (!fadeAnimation.enabled || fadeAnimation.fadeTimer) {
        return;
    }

    fadeAnimation.startTime = std::chrono::system_clock::now();
    fadeAnimation.fadeTimer = g_pMpvlock->addTimer(std::chrono::milliseconds(16),
                                                   [REF = m_self](std::shared_ptr<CTimer> timer, void*) { REF.lock()->onTimer(timer, (void*)&REF); },
                                                   nullptr, false);
    Debug::log(LOG, "CBackground starting fade: fadingIn={}, duration={}ms", fadeAnimation.fadingIn, fadeAnimation.durationMs);
}