#pragma once

#include "IWidget.hpp"
#include "../../helpers/Color.hpp"
#include "../../helpers/Math.hpp"
#include "../../config/ConfigDataValues.hpp"
#include "../../core/Timer.hpp"
#include "../AsyncResourceGatherer.hpp"
#include "Shadowable.hpp"
#include <string>
#include <filesystem>
#include <unordered_map>
#include <any>

struct SPreloadedAsset;
class COutput;

class CImage : public IWidget {
  public:
    CImage() = default;
    ~CImage();

    void registerSelf(const SP<CImage>& self);

    virtual void configure(const std::unordered_map<std::string, std::any>& props, const SP<COutput>& pOutput) override;
    virtual bool draw(const SRenderData& data) override;
    virtual std::string type() const override; // Added for layered rendering
    virtual void setZindex(int zindex) override;
    virtual int getZindex() const override;
    virtual void onTimer(std::shared_ptr<CTimer> timer, void* data) override;

    void reset();
    void renderUpdate();
    void onTimerUpdate();
    void plantTimer();
    void startFade();

  private:
    WP<CImage> m_self;

    CFramebuffer imageFB;

    int size;
    int rounding;
    double border;
    double angle;
    CGradientValueData color;
    Vector2D pos;
    std::string halign, valign, path;
    bool firstRender = true;
    int reloadTime;
    std::string reloadCommand;
    std::filesystem::file_time_type modificationTime;
    std::shared_ptr<CTimer> imageTimer;
    CAsyncResourceGatherer::SPreloadRequest request;
    Vector2D viewport;
    std::string resourceID;
    std::string pendingResourceID; // if reloading image
    SPreloadedAsset* asset = nullptr;
    COutput* output = nullptr;
    CShadowable shadow;

    int zindex = 20; // Default: above shape, below input-field

    // Fading functionality
    struct {
        float opacity = 1.0f;
        bool enabled = false;
        bool fadingIn = true;
        uint64_t durationMs = 1000;
        std::shared_ptr<CTimer> fadeTimer = nullptr;
        std::chrono::system_clock::time_point startTime;
    } fade;
};