#pragma once

#include "../../helpers/Math.hpp"
#include "../../defines.hpp"
#include "../../core/Timer.hpp"
#include <string>
#include <unordered_map>
#include <any>

class COutput;

class IWidget {
  public:
    struct SRenderData {
        float opacity = 1;
    };

    virtual ~IWidget() = default;

    virtual void configure(const std::unordered_map<std::string, std::any>& prop, const SP<COutput>& pOutput) = 0;
    virtual bool draw(const SRenderData& data) = 0;
    virtual std::string type() const = 0;
    virtual void onTimer(std::shared_ptr<CTimer> timer, void*) = 0;

    virtual int getZindex() const { return m_iZindex; }
    virtual void setZindex(int zindex) { m_iZindex = zindex; }

    static Vector2D posFromHVAlign(const Vector2D& viewport, const Vector2D& size, const Vector2D& offset, const std::string& halign, const std::string& valign,
                                   const double& ang = 0);
    static int roundingForBox(const CBox& box, int roundingConfig);
    static int roundingForBorderBox(const CBox& borderBox, int roundingConfig, int thickness);

    struct SFormatResult {
        std::string formatted;
        float updateEveryMs = 0;
        bool alwaysUpdate = false;
        bool cmd = false;
        bool allowForceUpdate = false;
    };

    static SFormatResult formatString(std::string in);

  protected:
    int m_iZindex = 0;
    std::shared_ptr<CTimer> m_pTimer;
};