#pragma once

#include "IWidget.hpp"
#include "../../helpers/Color.hpp"
#include "../../config/ConfigDataValues.hpp"
#include "Shadowable.hpp"
#include <hyprutils/math/Box.hpp>
#include <string>
#include <unordered_map>
#include <any>

class CShape : public IWidget {
  public:
    CShape() = default;
    virtual ~CShape() = default;

    void registerSelf(const SP<CShape>& self);

    virtual void configure(const std::unordered_map<std::string, std::any>& prop, const SP<COutput>& pOutput) override;
    virtual bool draw(const SRenderData& data) override;
    virtual std::string type() const override; // Added for layered rendering
    virtual void setZindex(int zindex) override;
    virtual int getZindex() const override;
    virtual void onTimer(std::shared_ptr<CTimer> timer, void* data) override;

  private:
    WP<CShape> m_self;

    CFramebuffer shapeFB;

    std::string shapeType; // e.g., "rectangle"
    bool blurEnabled = false;
    struct {
        int size = 0;
        int passes = 0;
    } blurParams;
    int zindex = 10; // Default: above background, below input-field

    int rounding;
    double border;
    double angle;
    CHyprColor color;
    CGradientValueData borderGrad;
    Vector2D size;
    Vector2D pos;

    std::string halign, valign;
    Vector2D viewport;
    CShadowable shadow;

    // Fading functionality
    struct {
        float opacity = 1.0f;
        bool enabled = false;
        bool fadingIn = true;
        uint64_t durationMs = 1000;
        std::shared_ptr<CTimer> fadeTimer = nullptr;
        std::chrono::system_clock::time_point startTime;
    } fade;

    std::string outputPort; // Added for renderOutput

    void startFade();
};