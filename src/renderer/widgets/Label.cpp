#include "Label.hpp"
#include "../Renderer.hpp"
#include "../../helpers/Log.hpp"
#include "../../core/mpvlock.hpp"
#include "../../auth/Auth.hpp"
#include "../../helpers/Color.hpp"
#include "../../config/ConfigDataValues.hpp"
#include <hyprlang.hpp>
#include <stdexcept>
#include <pango/pangocairo.h>

CLabel::~CLabel() {
    reset();
}

void CLabel::registerSelf(const SP<CLabel>& self) {
    m_self = self;
}

std::string CLabel::type() const {
    return "label";
}

void CLabel::setZindex(int zindex) {
    m_iZindex = zindex;
}

int CLabel::getZindex() const {
    return m_iZindex;
}

void CLabel::onTimer(std::shared_ptr<CTimer> timer, void* data) {
    WP<CLabel> ref = *static_cast<WP<CLabel>*>(data);
    if (auto PLABEL = ref.lock(); PLABEL) {
        // Check if this timer is for label updates or fading
        if (timer == PLABEL->labelTimer) {
            PLABEL->onTimerUpdate();
            PLABEL->plantTimer();
        } else if (timer == PLABEL->fade.fadeTimer) {
            auto now = std::chrono::system_clock::now();
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - PLABEL->fade.startTime).count();
            float progress = static_cast<float>(elapsedMs) / PLABEL->fade.durationMs;

            if (progress >= 1.0f) {
                // Fading complete
                PLABEL->fade.opacity = PLABEL->fade.fadingIn ? 1.0f : 0.0f;
                PLABEL->fade.fadeTimer->cancel();
                PLABEL->fade.fadeTimer.reset();
                PLABEL->fade.fadingIn = !PLABEL->fade.fadingIn; // Toggle direction for next fade
                Debug::log(TRACE, "CLabel fading complete: opacity={}", PLABEL->fade.opacity);
            } else {
                // Update opacity
                PLABEL->fade.opacity = PLABEL->fade.fadingIn ? progress : 1.0f - progress;
                Debug::log(TRACE, "CLabel fading: progress={}, opacity={}", progress, PLABEL->fade.opacity);
            }

            // Trigger a render to reflect the new opacity
            g_pMpvlock->renderOutput(PLABEL->outputStringPort);
        }
    }
}

static void onAssetCallback(WP<CLabel> ref) {
    if (auto PLABEL = ref.lock())
        PLABEL->renderUpdate();
}

std::string CLabel::getUniqueResourceId() {
    return std::string{"label:"} + std::to_string((uintptr_t)this) + ",time:" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
}

void CLabel::onTimerUpdate() {
    std::string oldFormatted = label.formatted;

    // Check auth state explicitly for $FAIL or $PAMPROMPT
    bool authWaiting = g_pAuth->checkWaiting();
    auto failText = g_pAuth->getCurrentFailText();
    bool authChanged = authWaiting || !failText.empty();

    label = formatString(labelPreFormat);

    // Update only if text changed or auth state requires it
    if (label.formatted == oldFormatted && !label.alwaysUpdate && !authChanged)
        return;

    if (!pendingResourceID.empty()) {
        Debug::log(WARN, "Trying to update label, but resource {} is still pending! Skipping update.", pendingResourceID);
        return;
    }

    request.id        = getUniqueResourceId();
    pendingResourceID = request.id;
    request.asset     = label.formatted;

    request.callback = [REF = m_self]() { onAssetCallback(REF); };

    g_pRenderer->asyncResourceGatherer->requestAsyncAssetPreload(request);
}

void CLabel::plantTimer() {
    if (label.updateEveryMs != 0)
        labelTimer = g_pMpvlock->addTimer(std::chrono::milliseconds((int)label.updateEveryMs), 
                                          [REF = m_self](std::shared_ptr<CTimer> timer, void*) { REF.lock()->onTimer(timer, (void*)&REF); }, 
                                          nullptr, label.allowForceUpdate);
    else if (label.updateEveryMs == 0 && label.allowForceUpdate)
        labelTimer = g_pMpvlock->addTimer(std::chrono::hours(1), 
                                          [REF = m_self](std::shared_ptr<CTimer> timer, void*) { REF.lock()->onTimer(timer, (void*)&REF); }, 
                                          nullptr, true);
}

void CLabel::startFade() {
    if (!fade.enabled || fade.fadeTimer) {
        return; // Fading not enabled or already in progress
    }

    fade.startTime = std::chrono::system_clock::now();
    fade.fadeTimer = g_pMpvlock->addTimer(std::chrono::milliseconds(16), // ~60 FPS
                                          [REF = m_self](std::shared_ptr<CTimer> timer, void*) { REF.lock()->onTimer(timer, (void*)&REF); },
                                          nullptr, false);
    Debug::log(LOG, "CLabel starting fade: fadingIn={}, duration={}ms", fade.fadingIn, fade.durationMs);
}

void CLabel::configure(const std::unordered_map<std::string, std::any>& props, const SP<COutput>& pOutput) {
    reset();

    outputStringPort = pOutput->stringPort;
    viewport         = pOutput->getViewport();

    shadow.configure(m_self.lock(), props, viewport);

    try {
        Debug::log(LOG, "Label configure: {} props received", props.size());
        for (const auto& [key, val] : props) {
            Debug::log(LOG, "Label prop: {} (type: {})", key, val.type().name());
        }

        configPos      = CLayoutValueData::fromAnyPv(props.at("position"))->getAbsolute(viewport);
        Debug::log(LOG, "Parsed label position: {}x{}", configPos.x, configPos.y);
        labelPreFormat = std::any_cast<Hyprlang::STRING>(props.at("text"));
        halign         = std::any_cast<Hyprlang::STRING>(props.at("halign"));
        valign         = std::any_cast<Hyprlang::STRING>(props.at("valign"));
        angle          = std::any_cast<Hyprlang::FLOAT>(props.at("rotate"));
        angle          = angle * M_PI / 180.0;
        Debug::log(LOG, "Parsed rotate: {} radians", angle);

        textOrientation = "horizontal";
        if (props.contains("text_orientation")) {
            Debug::log(LOG, "text_orientation found in props");
            auto val = props.at("text_orientation");
            if (val.type() == typeid(Hyprlang::STRING)) {
                textOrientation = std::any_cast<Hyprlang::STRING>(val);
                Debug::log(LOG, "Parsed text_orientation: {}", textOrientation);
            } else {
                Debug::log(WARN, "text_orientation has unexpected type: {}, defaulting to horizontal", val.type().name());
            }
        } else {
            Debug::log(WARN, "text_orientation NOT found in props, defaulting to horizontal");
        }

        std::string textAlign  = std::any_cast<Hyprlang::STRING>(props.at("text_align"));
        std::string fontFamily = std::any_cast<Hyprlang::STRING>(props.at("font_family"));
        CHyprColor  labelColor = std::any_cast<Hyprlang::INT>(props.at("color"));
        int         fontSize   = std::any_cast<Hyprlang::INT>(props.at("font_size"));

        if (props.contains("zindex")) {
            try {
                m_iZindex = std::any_cast<Hyprlang::INT>(props.at("zindex"));
                Debug::log(LOG, "Label configured with zindex: {}", m_iZindex);
            } catch (const std::exception& e) {
                Debug::log(WARN, "Failed to parse zindex for label: {}, defaulting to 20", e.what());
                m_iZindex = 20;
            }
        } else {
            m_iZindex = 20;
        }

        // Parse fading options
        fade.enabled = false;
        if (props.contains("fade")) {
            auto val = props.at("fade");
            if (val.type() == typeid(Hyprlang::INT)) {
                fade.enabled = std::any_cast<Hyprlang::INT>(val) != 0;
            } else {
                Debug::log(WARN, "Label fade has unexpected type, defaulting to disabled");
            }
        }

        fade.durationMs = 1000; // Default 1 second
        if (props.contains("fade_duration")) {
            auto val = props.at("fade_duration");
            if (val.type() == typeid(Hyprlang::INT)) {
                fade.durationMs = std::any_cast<Hyprlang::INT>(val);
                if (fade.durationMs <= 0) {
                    Debug::log(WARN, "Label fade_duration invalid value: {}, defaulting to 1000ms", fade.durationMs);
                    fade.durationMs = 1000;
                }
            } else if (val.type() == typeid(Hyprlang::FLOAT)) {
                fade.durationMs = static_cast<uint64_t>(std::any_cast<Hyprlang::FLOAT>(val));
                if (fade.durationMs <= 0) {
                    Debug::log(WARN, "Label fade_duration invalid value: {}, defaulting to 1000ms", fade.durationMs);
                    fade.durationMs = 1000;
                }
            } else {
                Debug::log(WARN, "Label fade_duration has unexpected type, defaulting to 1000ms");
            }
        }

        label = formatString(labelPreFormat);

        request.id                   = getUniqueResourceId();
        resourceID                   = request.id;
        request.asset                = label.formatted;
        request.type                 = CAsyncResourceGatherer::eTargetType::TARGET_TEXT;
        request.props["font_family"] = fontFamily;
        request.props["color"]       = labelColor;
        request.props["font_size"]   = fontSize;
        request.props["cmd"]         = label.cmd;

        if (textOrientation == "vertical") {
            PangoAttrList* attrList = pango_attr_list_new();
            PangoAttribute* gravityAttr = pango_attr_gravity_new(PANGO_GRAVITY_EAST);
            pango_attr_list_insert(attrList, gravityAttr);
            request.props["pango_attributes"] = attrList;
            Debug::log(TRACE, "Set PangoGravity::East for vertical text");
        }

        if (!textAlign.empty())
            request.props["text_align"] = textAlign;

    } catch (const std::bad_any_cast& e) {
        RASSERT(false, "Failed to construct CLabel: {}", e.what());
    } catch (const std::out_of_range& e) {
        RASSERT(false, "Missing property for CLabel: {}", e.what());
    }

    pos = configPos;

    g_pRenderer->asyncResourceGatherer->requestAsyncAssetPreload(request);

    plantTimer();

    // Start fading if enabled
    if (fade.enabled) {
        startFade();
    }
}

void CLabel::reset() {
    if (labelTimer) {
        labelTimer->cancel();
        labelTimer.reset();
    }

    if (fade.fadeTimer) {
        fade.fadeTimer->cancel();
        fade.fadeTimer.reset();
    }

    if (g_pMpvlock->m_bTerminate)
        return;

    if (asset)
        g_pRenderer->asyncResourceGatherer->unloadAsset(asset);

    asset = nullptr;
    pendingResourceID.clear();
    resourceID.clear();
}

bool CLabel::draw(const SRenderData& data) {
    if (!asset) {
        asset = g_pRenderer->asyncResourceGatherer->getAssetByID(resourceID);
        if (!asset) {
            Debug::log(WARN, "No asset for label, resourceID: {}", resourceID);
            return true;
        }
    }

    if (updateShadow) {
        updateShadow = false;
        shadow.markShadowDirty();
    }

    SRenderData shadowData = data;
    shadowData.opacity *= fade.opacity; // Adjust opacity for shadow
    shadow.draw(shadowData);

    Vector2D size = asset->texture.m_vSize;
    if (size.x <= 0 || size.y <= 0) {
        Debug::log(WARN, "Invalid texture size {}x{} for label: {}, skipping render", size.x, size.y, label.formatted);
        return true;
    }

    Debug::log(TRACE, "Original texture size: {}x{}", size.x, size.y);
    pos = posFromHVAlign(viewport, size, configPos, halign, valign, angle);

    double finalAngle = angle;
    Vector2D adjustedPos = pos;
    if (textOrientation == "vertical") {
        finalAngle += M_PI / 2.0; // Keep rotation for texture alignment
        adjustedPos = posFromHVAlign(viewport, size, configPos, halign, valign, finalAngle);
        Debug::log(TRACE, "Vertical orientation: angle={} radians, size={}x{}, pos={}x{}", 
                   finalAngle, size.x, size.y, adjustedPos.x, adjustedPos.y);
    }

    CBox box = {adjustedPos.x, adjustedPos.y, size.x, size.y};
    box.rot = finalAngle;

    float adjustedOpacity = data.opacity * fade.opacity;
    g_pRenderer->renderTexture(box, asset->texture, adjustedOpacity);

    Debug::log(TRACE, "Drawing label at {}x{} with size: {}x{}, text: {}, orientation: {}", 
               box.x, box.y, box.w, box.h, label.formatted, textOrientation);
    return adjustedOpacity < 1.0;
}

void CLabel::renderUpdate() {
    auto newAsset = g_pRenderer->asyncResourceGatherer->getAssetByID(pendingResourceID);
    if (newAsset) {
        g_pRenderer->asyncResourceGatherer->unloadAsset(asset);
        asset             = newAsset;
        resourceID        = pendingResourceID;
        pendingResourceID = "";
        updateShadow      = true;
    } else {
        Debug::log(WARN, "Asset {} not available after the asyncResourceGatherer’s callback!", pendingResourceID);
        g_pMpvlock->addTimer(std::chrono::milliseconds(100), [REF = m_self](auto, auto) { onAssetCallback(REF); }, nullptr);
        return;
    }

    g_pMpvlock->renderOutput(outputStringPort);
}