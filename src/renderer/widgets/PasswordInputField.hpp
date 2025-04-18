#pragma once

#include "IWidget.hpp"
#include "../../helpers/Color.hpp"
#include "../../helpers/Math.hpp"
#include "../../core/Timer.hpp"
#include "Shadowable.hpp"
#include "../../config/ConfigDataValues.hpp"
#include "../../helpers/AnimatedVariable.hpp"
#include <hyprutils/math/Vector2D.hpp>
#include <vector>
#include <any>
#include <unordered_map>

struct SPreloadedAsset;

class CPasswordInputField : public IWidget {
  public:
    CPasswordInputField() = default;
    virtual ~CPasswordInputField();

    void registerSelf(const SP<CPasswordInputField>& self);

    virtual void configure(const std::unordered_map<std::string, std::any>& prop, const SP<COutput>& pOutput) override;
    virtual bool draw(const SRenderData& data) override;
    virtual std::string type() const override; // Added for layered rendering
    virtual void setZindex(int zindex) override;
    virtual int getZindex() const override;
    virtual void onTimer(std::shared_ptr<CTimer> timer, void* data) override; // Add this declaration

    void reset();
    void onFadeOutTimer();

  private:
    int m_iZindex = 30; // Default for input-field
    WP<CPasswordInputField> m_self;

    void updateDots();
    void updateFade();
    void updatePlaceholder();
    void updateWidth();
    void updateHiddenInputState();
    void updateInputState();
    void updateColors();

    bool m_bDisplayFailText = false; // Added for failure text display
    bool firstRender = true;
    bool redrawShadow = false;
    bool checkWaiting = false;
    bool displayFail = false;

    size_t passwordLength = 0;

    PHLANIMVAR<Vector2D> size;
    Vector2D pos;
    Vector2D viewport;
    Vector2D configPos;
    Vector2D configSize;

    std::string halign, valign, configFailText, outputStringPort, configPlaceholderText, fontFamily;
    uint64_t configFailTimeoutMs = 2000;

    int outThick, rounding;

    struct {
        PHLANIMVAR<float> currentAmount;
        bool center = false;
        float size = 0;
        float spacing = 0;
        int rounding = 0;
        std::string textFormat = "";
        std::string textResourceID;
        SPreloadedAsset* textAsset = nullptr;
    } dots;

    struct {
        PHLANIMVAR<float> a;
        bool appearing = true;
        std::shared_ptr<CTimer> fadeOutTimer = nullptr;
        bool allowFadeOut = false;
    } fade;

    struct {
        std::string resourceID = "";
        SPreloadedAsset* asset = nullptr;
        std::string currentText = "";
        size_t failedAttempts = 0;
        std::vector<std::string> registeredResourceIDs;
    } placeholder;

    struct {
        CHyprColor lastColor;
        int lastQuadrant = 0;
        int lastPasswordLength = 0;
        bool enabled = false;
    } hiddenInputState;

    struct {
        CGradientValueData* outer = nullptr;
        CHyprColor inner;
        CHyprColor font;
        CGradientValueData* fail = nullptr;
        CGradientValueData* check = nullptr;
        CGradientValueData* caps = nullptr;
        CGradientValueData* num = nullptr;
        CGradientValueData* both = nullptr;
        CHyprColor hiddenBase;
        int transitionMs = 0;
        bool invertNum = false;
        bool swapFont = false;
    } colorConfig;

    struct {
        PHLANIMVAR<CGradientValueData> outer;
        PHLANIMVAR<CHyprColor> inner;
        CHyprColor font;
    } colorState;

    bool fadeOnEmpty;
    uint64_t fadeTimeoutMs;

    CShadowable shadow;
};