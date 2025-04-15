#pragma once

#include "IWidget.hpp"
#include "Shadowable.hpp"
#include "../../helpers/Math.hpp"
#include "../../core/Timer.hpp"
#include "../AsyncResourceGatherer.hpp"
#include <string>
#include <unordered_map>
#include <any>

struct SPreloadedAsset;
class CSessionLockSurface;

class CLabel : public IWidget {
  public:
    CLabel() = default;
    ~CLabel();

    void registerSelf(const SP<CLabel>& self);

    virtual void configure(const std::unordered_map<std::string, std::any>& prop, const SP<COutput>& pOutput) override;
    virtual bool draw(const SRenderData& data) override;
    virtual std::string type() const override;
    virtual void setZindex(int zindex) override;
    virtual int getZindex() const override;
    virtual void onTimer(std::shared_ptr<CTimer> timer, void* data) override;

    void reset();
    void renderUpdate();
    void onTimerUpdate();
    void plantTimer();

  private:
    int m_iZindex = 20; // Default for label
    WP<CLabel> m_self;

    std::string getUniqueResourceId();

    std::string labelPreFormat;
    IWidget::SFormatResult label;

    Vector2D viewport;
    Vector2D pos;
    Vector2D configPos;
    double angle;
    std::string resourceID;
    std::string pendingResourceID; // if dynamic label
    std::string halign, valign;
    std::string textOrientation = "horizontal"; // Added for vertical text support
    SPreloadedAsset* asset = nullptr;
    std::string outputStringPort;

    CAsyncResourceGatherer::SPreloadRequest request;
    std::shared_ptr<CTimer> labelTimer = nullptr;

    CShadowable shadow;
    bool updateShadow = true;

    // Fading functionality
    struct {
        float opacity = 1.0f;
        bool enabled = false;
        bool fadingIn = true;
        uint64_t durationMs = 1000;
        std::shared_ptr<CTimer> fadeTimer = nullptr;
        std::chrono::system_clock::time_point startTime;
    } fade;

    void startFade();
};