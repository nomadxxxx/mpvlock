#include "Shape.hpp"
#include "../Renderer.hpp"
#include "../../config/ConfigDataValues.hpp"
#include "../../helpers/Log.hpp"
#include <hyprlang.hpp>
#include <GLES3/gl32.h>
#include <cmath>
#include <optional>

void CShape::registerSelf(const SP<CShape>& self) {
    m_self = self;
}

std::string CShape::type() const {
    return "shape";
}

void CShape::configure(const std::unordered_map<std::string, std::any>& props, const SP<COutput>& pOutput) {
    viewport = pOutput->getViewport();

    try {
        // Log all props for debugging
        Debug::log(LOG, "Shape configure: {} props received", props.size());
        for (const auto& [key, val] : props) {
            Debug::log(LOG, "Prop {}: type {}", key, val.type().name());
        }

        // Parse position
        pos = {0, 0};
        if (props.contains("position")) {
            auto val = props.at("position");
            if (val.type() == typeid(Hyprlang::VEC2)) {
                auto vec = std::any_cast<Hyprlang::VEC2>(val);
                pos.x = static_cast<double>(vec.x);
                pos.y = static_cast<double>(vec.y);
                Debug::log(LOG, "Parsed shape position: {}x{}", pos.x, pos.y);
            } else if (val.type() == typeid(CLayoutValueData*)) {
                auto layout = std::any_cast<CLayoutValueData*>(val);
                pos.x = layout->m_vValues.x;
                pos.y = layout->m_vValues.y;
                Debug::log(LOG, "Parsed shape position from CLayoutValueData: {}x{}", pos.x, pos.y);
            } else if (val.type() == typeid(std::string)) {
                try {
                    const auto POSSTR = std::any_cast<std::string>(val);
                    size_t sep = POSSTR.find_first_of(",x");
                    if (sep == std::string::npos) {
                        Debug::log(WARN, "Shape position invalid format: {}, defaulting to 0x0", POSSTR);
                    } else {
                        std::string xStr = POSSTR.substr(0, sep);
                        std::string yStr = POSSTR.substr(sep + 1);
                        xStr.erase(0, xStr.find_first_not_of(" \t"));
                        xStr.erase(xStr.find_last_not_of(" \t") + 1);
                        yStr.erase(0, yStr.find_first_not_of(" \t"));
                        yStr.erase(yStr.find_last_not_of(" \t") + 1);
                        double x = std::stod(xStr);
                        double y = std::stod(yStr);
                        pos.x = x;
                        pos.y = y;
                        Debug::log(LOG, "Parsed shape position from string: {}x{}", x, y);
                    }
                } catch (const std::exception& e) {
                    Debug::log(WARN, "Shape position parse failed: {}, defaulting to 0x0", e.what());
                }
            } else {
                Debug::log(WARN, "Shape position has unexpected type: {}, defaulting to 0x0", val.type().name());
            }
        } else {
            Debug::log(LOG, "No position prop found, defaulting to 0x0");
        }

        // Parse size
        size = {100, 100};
        if (props.contains("size")) {
            auto val = props.at("size");
            Debug::log(LOG, "Prop size: type {}", val.type().name());
            if (val.type() == typeid(Hyprlang::VEC2)) {
                auto vec = std::any_cast<Hyprlang::VEC2>(val);
                size.x = static_cast<double>(vec.x);
                size.y = static_cast<double>(vec.y);
                if (size.x <= 0 || size.y <= 0) {
                    Debug::log(WARN, "Shape size invalid values: {}x{}, defaulting to 100x100", size.x, size.y);
                    size = {100, 100};
                } else {
                    Debug::log(LOG, "Parsed shape size VEC2: {}x{}", size.x, size.y);
                }
            } else if (val.type() == typeid(CLayoutValueData*)) {
                auto layout = std::any_cast<CLayoutValueData*>(val);
                size.x = layout->m_vValues.x;
                size.y = layout->m_vValues.y;
                if (size.x <= 0 || size.y <= 0) {
                    Debug::log(WARN, "Shape size invalid values: {}x{}, defaulting to 100x100", size.x, size.y);
                    size = {100, 100};
                } else {
                    Debug::log(LOG, "Parsed shape size from CLayoutValueData: {}x{}", size.x, size.y);
                }
            } else if (val.type() == typeid(std::string)) {
                try {
                    const auto SIZESTR = std::any_cast<std::string>(val);
                    Debug::log(LOG, "Shape size string raw value: '{}'", SIZESTR);
                    size_t sep = SIZESTR.find_first_of(",");
                    if (sep == std::string::npos) {
                        Debug::log(WARN, "Shape size string invalid format: {}, defaulting to 100x100", SIZESTR);
                        size = {100, 100};
                    } else {
                        std::string xStr = SIZESTR.substr(0, sep);
                        std::string yStr = SIZESTR.substr(sep + 1);
                        xStr.erase(0, xStr.find_first_not_of(" \t"));
                        xStr.erase(xStr.find_last_not_of(" \t") + 1);
                        yStr.erase(0, yStr.find_first_not_of(" \t"));
                        yStr.erase(yStr.find_last_not_of(" \t") + 1);
                        double x = std::stod(xStr);
                        double y = std::stod(yStr);
                        if (x <= 0 || y <= 0) {
                            Debug::log(WARN, "Shape size string invalid values: {}x{}, defaulting to 100x100", x, y);
                            size = {100, 100};
                        } else {
                            size.x = x;
                            size.y = y;
                            Debug::log(LOG, "Parsed shape size from string: {}x{}", x, y);
                        }
                    }
                } catch (const std::exception& e) {
                    Debug::log(WARN, "Shape size string parse failed: {}, defaulting to 100x100", e.what());
                    size = {100, 100};
                }
            } else {
                Debug::log(WARN, "Shape size unexpected type: {}, defaulting to 100x100", val.type().name());
                size = {100, 100};
            }
        } else {
            Debug::log(WARN, "No size prop found, defaulting to 100x100");
        }

        // Parse color
        color = CHyprColor(1.0, 1.0, 1.0, 0.5);
        if (props.contains("color")) {
            auto colorVal = props.at("color");
            if (colorVal.type() == typeid(Hyprlang::STRING)) {
                std::string colorStr = std::any_cast<Hyprlang::STRING>(colorVal);
                if (colorStr.starts_with("0x") || colorStr.starts_with("#"))
                    colorStr = colorStr.substr(2);
                try {
                    uint64_t colorValue = std::stoull(colorStr, nullptr, 16);
                    color = CHyprColor(colorValue);
                    Debug::log(LOG, "Parsed shape color: r={}, g={}, b={}, a={}", color.r, color.g, color.b, color.a);
                } catch (const std::exception& e) {
                    Debug::log(WARN, "Failed to parse color string '{}', defaulting to semi-transparent white: {}", colorStr, e.what());
                }
            } else if (colorVal.type() == typeid(Hyprlang::INT)) {
                uint64_t colorValue = std::any_cast<Hyprlang::INT>(colorVal);
                color = CHyprColor(colorValue);
                Debug::log(LOG, "Parsed shape color: r={}, g={}, b={}, a={}", color.r, color.g, color.b, color.a);
            } else {
                Debug::log(WARN, "Shape color has unexpected type, defaulting to semi-transparent white");
            }
        }

        // Parse shape type
        shapeType = "rectangle";
        if (props.contains("shape")) {
            auto val = props.at("shape");
            if (val.type() == typeid(Hyprlang::STRING)) {
                shapeType = std::any_cast<Hyprlang::STRING>(val);
            } else {
                Debug::log(WARN, "Shape type has unexpected type, defaulting to rectangle");
            }
        }

        // Parse blur
        blurEnabled = false;
        blurParams = {.size = 0, .passes = 0};
        if (props.contains("blur")) {
            auto val = props.at("blur");
            if (val.type() == typeid(Hyprlang::INT) && std::any_cast<Hyprlang::INT>(val) > 0) {
                blurEnabled = true;
                blurParams.size = std::any_cast<Hyprlang::INT>(val);
                blurParams.passes = 3;
            } else if (val.type() == typeid(Hyprlang::FLOAT) && std::any_cast<Hyprlang::FLOAT>(val) > 0) {
                blurEnabled = true;
                blurParams.size = static_cast<int>(std::any_cast<Hyprlang::FLOAT>(val));
                blurParams.passes = 3;
            } else {
                Debug::log(WARN, "Shape blur has unexpected type or value, disabling blur");
            }
        }

        // Parse zindex
        if (props.contains("zindex")) {
            auto val = props.at("zindex");
            if (val.type() == typeid(Hyprlang::INT)) {
                setZindex(std::any_cast<Hyprlang::INT>(val));
                Debug::log(LOG, "Parsed shape zindex: {}", getZindex());
            } else {
                Debug::log(WARN, "Shape zindex has unexpected type: {}, defaulting to 10", val.type().name());
                setZindex(10);
            }
        } else {
            setZindex(10);
            Debug::log(LOG, "No zindex prop found, defaulting to 10");
        }

        // Parse rotation (angle in degrees)
        angle = 0.0;
        if (props.contains("rotate")) {
            auto val = props.at("rotate");
            if (val.type() == typeid(Hyprlang::FLOAT)) {
                angle = std::any_cast<Hyprlang::FLOAT>(val);
            } else if (val.type() == typeid(Hyprlang::INT)) {
                angle = static_cast<float>(std::any_cast<Hyprlang::INT>(val));
            } else {
                Debug::log(WARN, "Shape rotate has unexpected type, defaulting to 0");
            }
        }
        angle = angle * M_PI / 180.0;

        // Parse border
        border = 0;
        if (props.contains("border_size")) {
            auto val = props.at("border_size");
            if (val.type() == typeid(Hyprlang::INT)) {
                border = std::any_cast<Hyprlang::INT>(val);
            } else {
                Debug::log(WARN, "Shape border_size has unexpected type, defaulting to 0");
            }
        }

        // Parse border gradient
        borderGrad = CGradientValueData();
        if (props.contains("border_color")) {
            try {
                borderGrad = *CGradientValueData::fromAnyPv(props.at("border_color"));
            } catch (const std::exception& e) {
                Debug::log(WARN, "Failed to parse border_color, defaulting to empty gradient: {}", e.what());
            }
        }

        // Parse rounding
        rounding = 0;
        if (props.contains("rounding")) {
            auto val = props.at("rounding");
            if (val.type() == typeid(Hyprlang::INT)) {
                rounding = std::any_cast<Hyprlang::INT>(val);
                Debug::log(LOG, "Parsed shape rounding: {}", rounding);
            } else if (val.type() == typeid(Hyprlang::FLOAT)) {
                rounding = static_cast<int>(std::any_cast<Hyprlang::FLOAT>(val));
                Debug::log(LOG, "Parsed shape rounding: {}", rounding);
            } else {
                Debug::log(WARN, "Shape rounding has unexpected type: {}, defaulting to 0", val.type().name());
            }
        }

        // Parse halign and valign
        halign = "none";
        if (props.contains("halign")) {
            auto val = props.at("halign");
            if (val.type() == typeid(Hyprlang::STRING)) {
                halign = std::any_cast<Hyprlang::STRING>(val);
            }
        }
        valign = "none";
        if (props.contains("valign")) {
            auto val = props.at("valign");
            if (val.type() == typeid(Hyprlang::STRING)) {
                valign = std::any_cast<Hyprlang::STRING>(val);
            }
        }

        // Adjust position based on halign/valign
        Vector2D realSize = size + Vector2D{border * 2.0, border * 2.0};
        if (halign != "none" || valign != "none") {
            pos = posFromHVAlign(viewport, realSize, pos, halign, valign, angle);
            Debug::log(LOG, "Applied posFromHVAlign: pos={}x{}", pos.x, pos.y);
        } else {
            Debug::log(LOG, "Skipped posFromHVAlign, using raw position: {}x{}", pos.x, pos.y);
        }

        // Log final state
        Debug::log(LOG, "Shape configured: pos={}x{}, size={}x{}, zindex={}", pos.x, pos.y, size.x, size.y, getZindex());
    } catch (const std::exception& e) {
        Debug::log(ERR, "CShape::configure failed: {}", e.what());
        pos = {0, 0};
        size = {100, 100};
        color = CHyprColor(1.0, 1.0, 1.0, 0.5);
        shapeType = "rectangle";
        setZindex(10);
        angle = 0.0;
        border = 0;
        rounding = 0;
        blurEnabled = false;
    }
}

bool CShape::draw(const SRenderData& data) {
    try {
        CBox box = {pos.x, pos.y, size.x, size.y};
        box.round();
        box.rot = angle;

        Debug::log(LOG, "Drawing shape at {}x{} with size: {}x{}, color: r={}, g={}, b={}, a={}, zindex={}",
                   box.x, box.y, box.w, box.h, color.r, color.g, color.b, color.a, getZindex());

        if (blurEnabled) {
            if (!shapeFB.isAllocated()) {
                shapeFB.alloc((int)(size.x + border * 2), (int)(size.y + border * 2), true);
            }

            shapeFB.bind();
            glClearColor(0.0, 0.0, 0.0, 0.0);
            glClear(GL_COLOR_BUFFER_BIT);

            if (shapeType == "rectangle") {
                CBox shapeBox = {border, border, size.x, size.y};
                g_pRenderer->renderRect(shapeBox, color, rounding);
                if (border > 0 && !borderGrad.m_vColorsOkLabA.empty()) {
                    CBox borderBox = {0, 0, size.x + border * 2, size.y + border * 2};
                    g_pRenderer->renderBorder(borderBox, borderGrad, border, rounding, data.opacity);
                }
            } else {
                Debug::log(WARN, "Shape type {} not implemented, rendering rectangle", shapeType);
                g_pRenderer->renderRect(box, color, rounding);
            }

            CRenderer::SBlurParams rendererBlurParams = {
                .size = blurParams.size,
                .passes = blurParams.passes,
            };
            g_pRenderer->blurFB(shapeFB, rendererBlurParams);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

            CBox texBox = {pos.x - border, pos.y - border, size.x + border * 2, size.y + border * 2};
            texBox.round();
            texBox.rot = angle;
            g_pRenderer->renderTexture(texBox, shapeFB.m_cTex, data.opacity, rounding, HYPRUTILS_TRANSFORM_FLIPPED_180);
        } else {
            if (shapeType == "rectangle") {
                g_pRenderer->renderRect(box, color, rounding);
                if (border > 0 && !borderGrad.m_vColorsOkLabA.empty()) {
                    CBox borderBox = {pos.x - border, pos.y - border, size.x + border * 2, size.y + border * 2};
                    borderBox.round();
                    box.rot = angle;
                    g_pRenderer->renderBorder(borderBox, borderGrad, border, rounding, data.opacity);
                }
            } else {
                Debug::log(WARN, "Shape type {} not implemented, rendering rectangle", shapeType);
                g_pRenderer->renderRect(box, color, rounding);
            }
        }

        return data.opacity < 1.0;
    } catch (const std::exception& e) {
        Debug::log(ERR, "CShape::draw failed: {}", e.what());
        return false;
    }
}