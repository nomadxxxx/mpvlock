#include "Image.hpp"
#include "../Renderer.hpp"
#include "../../core/mpvlock.hpp"
#include "../../helpers/Log.hpp"
#include "../../helpers/MiscFunctions.hpp"
#include "../../config/ConfigDataValues.hpp"
#include <cmath>
#include <hyprlang.hpp>

CImage::~CImage() {
    reset();
}

void CImage::registerSelf(const SP<CImage>& self) {
    m_self = self;
}

std::string CImage::type() const {
    return "image";
}

void CImage::setZindex(int zindex) {
    this->zindex = zindex;
}

int CImage::getZindex() const {
    return zindex;
}

void CImage::onTimer(std::shared_ptr<CTimer> timer, void* data) {
    WP<CImage> ref = *static_cast<WP<CImage>*>(data);
    if (auto PIMAGE = ref.lock(); PIMAGE) {
        // Check if this timer is for reloading or fading
        if (timer == PIMAGE->imageTimer) {
            PIMAGE->onTimerUpdate();
            PIMAGE->plantTimer();
        } else if (timer == PIMAGE->fade.fadeTimer) {
            auto now = std::chrono::system_clock::now();
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - PIMAGE->fade.startTime).count();
            float progress = static_cast<float>(elapsedMs) / PIMAGE->fade.durationMs;

            if (progress >= 1.0f) {
                // Fading complete
                PIMAGE->fade.opacity = PIMAGE->fade.fadingIn ? 1.0f : 0.0f;
                PIMAGE->fade.fadeTimer->cancel();
                PIMAGE->fade.fadeTimer.reset();
                PIMAGE->fade.fadingIn = !PIMAGE->fade.fadingIn; // Toggle direction for next fade
                Debug::log(TRACE, "CImage fading complete: opacity={}", PIMAGE->fade.opacity);
            } else {
                // Update opacity
                PIMAGE->fade.opacity = PIMAGE->fade.fadingIn ? progress : 1.0f - progress;
                Debug::log(TRACE, "CImage fading: progress={}, opacity={}", progress, PIMAGE->fade.opacity);
            }

            // Trigger a render to reflect the new opacity
            g_pMpvlock->renderOutput(PIMAGE->output->stringPort);
        }
    }
}

static void onAssetCallback(WP<CImage> ref) {
    if (auto PIMAGE = ref.lock(); PIMAGE)
        PIMAGE->renderUpdate();
}

void CImage::onTimerUpdate() {
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

    try {
        const auto MTIME = std::filesystem::last_write_time(absolutePath(path, ""));
        if (OLDPATH == path && MTIME == modificationTime)
            return;
        modificationTime = MTIME;
    } catch (std::exception& e) {
        path = OLDPATH;
        Debug::log(ERR, "{}", e.what());
        return;
    }

    if (!pendingResourceID.empty())
        return;

    request.id = std::string{"image:"} + path + ",time:" + std::to_string((uint64_t)modificationTime.time_since_epoch().count());
    pendingResourceID = request.id;
    request.asset = path;
    request.type = CAsyncResourceGatherer::eTargetType::TARGET_IMAGE;
    request.callback = [REF = m_self]() { onAssetCallback(REF); };

    g_pRenderer->asyncResourceGatherer->requestAsyncAssetPreload(request);
}

void CImage::plantTimer() {
    if (reloadTime == 0) {
        imageTimer = g_pMpvlock->addTimer(std::chrono::hours(1),
                                          [REF = m_self](std::shared_ptr<CTimer> timer, void*) { REF.lock()->onTimer(timer, (void*)&REF); }, nullptr, true);
    } else if (reloadTime > 0) {
        imageTimer = g_pMpvlock->addTimer(std::chrono::seconds(reloadTime),
                                          [REF = m_self](std::shared_ptr<CTimer> timer, void*) { REF.lock()->onTimer(timer, (void*)&REF); }, nullptr, false);
    }
}

void CImage::startFade() {
    if (!fade.enabled || fade.fadeTimer) {
        return; // Fading not enabled or already in progress
    }

    fade.startTime = std::chrono::system_clock::now();
    fade.fadeTimer = g_pMpvlock->addTimer(std::chrono::milliseconds(16), // ~60 FPS
                                          [REF = m_self](std::shared_ptr<CTimer> timer, void*) { REF.lock()->onTimer(timer, (void*)&REF); },
                                          nullptr, false);
    Debug::log(LOG, "CImage starting fade: fadingIn={}, duration={}ms", fade.fadingIn, fade.durationMs);
}

void CImage::configure(const std::unordered_map<std::string, std::any>& props, const SP<COutput>& pOutput) {
    reset();

    viewport = pOutput->getViewport();
    output = pOutput.get();

    shadow.configure(m_self.lock(), props, viewport);

    try {
        size = std::any_cast<Hyprlang::INT>(props.at("size"));
        rounding = std::any_cast<Hyprlang::INT>(props.at("rounding"));
        border = std::any_cast<Hyprlang::INT>(props.at("border_size"));
        color = *CGradientValueData::fromAnyPv(props.at("border_color"));
        pos = CLayoutValueData::fromAnyPv(props.at("position"))->getAbsolute(viewport);
        halign = std::any_cast<Hyprlang::STRING>(props.at("halign"));
        valign = std::any_cast<Hyprlang::STRING>(props.at("valign"));
        angle = std::any_cast<Hyprlang::FLOAT>(props.at("rotate"));
        path = std::any_cast<Hyprlang::STRING>(props.at("path"));
        reloadTime = std::any_cast<Hyprlang::INT>(props.at("reload_time"));
        reloadCommand = std::any_cast<Hyprlang::STRING>(props.at("reload_cmd"));

        if (props.contains("zindex")) {
            try {
                zindex = std::any_cast<Hyprlang::INT>(props.at("zindex"));
                Debug::log(LOG, "Image configured with zindex: {}", zindex);
            } catch (const std::exception& e) {
                Debug::log(WARN, "Failed to parse zindex for image: {}, defaulting to 20", e.what());
                zindex = 20;
            }
        } else {
            zindex = 20;
            Debug::log(LOG, "No zindex prop found for image, defaulting to 20");
        }

        // Parse fading options
        fade.enabled = false;
        if (props.contains("fade")) {
            auto val = props.at("fade");
            if (val.type() == typeid(Hyprlang::INT)) {
                fade.enabled = std::any_cast<Hyprlang::INT>(val) != 0;
            } else {
                Debug::log(WARN, "Image fade has unexpected type, defaulting to disabled");
            }
        }

        fade.durationMs = 1000; // Default 1 second
        if (props.contains("fade_duration")) {
            auto val = props.at("fade_duration");
            if (val.type() == typeid(Hyprlang::INT)) {
                fade.durationMs = std::any_cast<Hyprlang::INT>(val);
                if (fade.durationMs <= 0) {
                    Debug::log(WARN, "Image fade_duration invalid value: {}, defaulting to 1000ms", fade.durationMs);
                    fade.durationMs = 1000;
                }
            } else if (val.type() == typeid(Hyprlang::FLOAT)) {
                fade.durationMs = static_cast<uint64_t>(std::any_cast<Hyprlang::FLOAT>(val));
                if (fade.durationMs <= 0) {
                    Debug::log(WARN, "Image fade_duration invalid value: {}, defaulting to 1000ms", fade.durationMs);
                    fade.durationMs = 1000;
                }
            } else {
                Debug::log(WARN, "Image fade_duration has unexpected type, defaulting to 1000ms");
            }
        }
    } catch (const std::bad_any_cast& e) {
        RASSERT(false, "Failed to construct CImage: {}", e.what());
    } catch (const std::out_of_range& e) {
        RASSERT(false, "Missing property for CImage: {}", e.what());
    }

    resourceID = "image:" + path;
    angle = angle * M_PI / 180.0;

    if (reloadTime > -1) {
        try {
            modificationTime = std::filesystem::last_write_time(absolutePath(path, ""));
        } catch (std::exception& e) { Debug::log(ERR, "{}", e.what()); }
        plantTimer();
    }

    // Start fading if enabled
    if (fade.enabled) {
        startFade();
    }
}

void CImage::reset() {
    if (imageTimer) {
        imageTimer->cancel();
        imageTimer.reset();
    }
    if (fade.fadeTimer) {
        fade.fadeTimer->cancel();
        fade.fadeTimer.reset();
    }
    if (g_pMpvlock->m_bTerminate)
        return;
    imageFB.release();
    if (asset && reloadTime > -1)
        g_pRenderer->asyncResourceGatherer->unloadAsset(asset);
    asset = nullptr;
    pendingResourceID = "";
    resourceID = "";
}

bool CImage::draw(const SRenderData& data) {
    if (resourceID.empty())
        return false;

    if (!asset)
        asset = g_pRenderer->asyncResourceGatherer->getAssetByID(resourceID);

    if (!asset)
        return true;

    if (asset->texture.m_iType == TEXTURE_INVALID) {
        g_pRenderer->asyncResourceGatherer->unloadAsset(asset);
        resourceID = "";
        return false;
    }

    if (!imageFB.isAllocated()) {
        const Vector2D IMAGEPOS = {border, border};
        const Vector2D BORDERPOS = {0.0, 0.0};
        const Vector2D TEXSIZE = asset->texture.m_vSize;
        const float SCALEX = size / TEXSIZE.x;
        const float SCALEY = size / TEXSIZE.y;

        CBox texbox = {angle == 0 ? IMAGEPOS : IMAGEPOS + Vector2D{1.0, 1.0}, TEXSIZE};
        texbox.w *= std::max(SCALEX, SCALEY);
        texbox.h *= std::max(SCALEX, SCALEY);

        CBox borderBox = {angle == 0 ? BORDERPOS : BORDERPOS + Vector2D{1.0, 1.0}, texbox.size() + IMAGEPOS * 2.0};
        borderBox.round();

        const Vector2D FBSIZE = angle == 0 ? borderBox.size() : borderBox.size() + Vector2D{2.0, 2.0};
        const int ROUND = roundingForBox(texbox, rounding);
        const int BORDERROUND = roundingForBorderBox(borderBox, rounding, border);

        imageFB.alloc(FBSIZE.x, FBSIZE.y, true);
        g_pRenderer->pushFb(imageFB.m_iFb);
        glClearColor(0.0, 0.0, 0.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT);

        if (border > 0)
            g_pRenderer->renderBorder(borderBox, color, border, BORDERROUND, 1.0);

        texbox.round();
        g_pRenderer->renderTexture(texbox, asset->texture, 1.0, ROUND);
        g_pRenderer->popFb();
    }

    CTexture* tex = &imageFB.m_cTex;
    CBox texbox = {{}, tex->m_vSize};

    if (firstRender) {
        firstRender = false;
        shadow.markShadowDirty();
    }

    SRenderData shadowData = data;
    shadowData.opacity *= fade.opacity; // Adjust opacity for shadow
    shadow.draw(shadowData); // Render shadow with adjusted opacity

    const auto TEXPOS = posFromHVAlign(viewport, tex->m_vSize, pos, halign, valign, angle);
    texbox.x = TEXPOS.x;
    texbox.y = TEXPOS.y;

    texbox.round();
    texbox.rot = angle;
    float adjustedOpacity = data.opacity * fade.opacity;
    g_pRenderer->renderTexture(texbox, *tex, adjustedOpacity, 0);

    return adjustedOpacity < 1.0;
}

void CImage::renderUpdate() {
    auto newAsset = g_pRenderer->asyncResourceGatherer->getAssetByID(pendingResourceID);
    if (newAsset) {
        if (newAsset->texture.m_iType == TEXTURE_INVALID) {
            g_pRenderer->asyncResourceGatherer->unloadAsset(newAsset);
        } else if (resourceID != pendingResourceID) {
            g_pRenderer->asyncResourceGatherer->unloadAsset(asset);
            imageFB.release();
            asset = newAsset;
            resourceID = pendingResourceID;
            firstRender = true;
        }
        pendingResourceID = "";
    } else if (!pendingResourceID.empty()) {
        Debug::log(WARN, "Asset {} not available after the asyncResourceGatherer's callback!", pendingResourceID);
        pendingResourceID = "";
    }

    g_pMpvlock->renderOutput(output->stringPort);
}