#include "Auth.hpp"
#include "Pam.hpp"
#include "Fingerprint.hpp"
#include "../config/ConfigManager.hpp"
#include "../core/mpvlock.hpp"  // Updated from hyprlock.hpp
#include "src/helpers/Log.hpp"

#include <hyprlang.hpp>
#include <memory>

CAuth::CAuth() {
    static const auto ENABLEPAM = g_pConfigManager->getValue<Hyprlang::INT>("auth:pam:enabled");
    if (*ENABLEPAM)
        m_vImpls.emplace_back(makeShared<CPam>());
    static const auto ENABLEFINGERPRINT = g_pConfigManager->getValue<Hyprlang::INT>("auth:fingerprint:enabled");
    if (*ENABLEFINGERPRINT)
        m_vImpls.emplace_back(makeShared<CFingerprint>());

    RASSERT(!m_vImpls.empty(), "At least one authentication method must be enabled!");
}

void CAuth::start() {
    for (const auto& i : m_vImpls) {
        i->init();
    }
}

void CAuth::submitInput(const std::string& input) {
    for (const auto& i : m_vImpls) {
        i->handleInput(input);
    }

    g_pMpvlock->clearPasswordBuffer();  // Updated from g_pHyprlock
}

bool CAuth::checkWaiting() {
    return std::ranges::any_of(m_vImpls, [](const auto& i) { return i->checkWaiting(); });
}

const std::string& CAuth::getCurrentFailText() {
    return m_sCurrentFail.failText;
}

std::optional<std::string> CAuth::getFailText(eAuthImplementations implType) {
    for (const auto& i : m_vImpls) {
        if (i->getImplType() == implType)
            return i->getLastFailText();
    }
    return std::nullopt;
}

std::optional<std::string> CAuth::getPrompt(eAuthImplementations implType) {
    for (const auto& i : m_vImpls) {
        if (i->getImplType() == implType)
            return i->getLastPrompt();
    }
    return std::nullopt;
}

size_t CAuth::getFailedAttempts() {
    return m_sCurrentFail.failedAttempts;
}

SP<IAuthImplementation> CAuth::getImpl(eAuthImplementations implType) {
    for (const auto& i : m_vImpls) {
        if (i->getImplType() == implType)
            return i;
    }

    return nullptr;
}

void CAuth::terminate() {
    for (const auto& i : m_vImpls) {
        i->terminate();
    }
}

static void passwordUnlockCallback(std::shared_ptr<CTimer> self, void* data) {
    g_pMpvlock->unlock();  // Updated from g_pHyprlock
}

void CAuth::enqueueUnlock() {
    g_pMpvlock->addTimer(std::chrono::milliseconds(0), passwordUnlockCallback, nullptr);  // Updated from g_pHyprlock
}

static void passwordFailCallback(std::shared_ptr<CTimer> self, void* data) {
    g_pAuth->m_bDisplayFailText = true;

    g_pMpvlock->enqueueForceUpdateTimers();  // Updated from g_pHyprlock

    g_pMpvlock->renderAllOutputs();  // Updated from g_pHyprlock
}

static void displayFailTimeoutCallback(std::shared_ptr<CTimer> self, void* data) {
    if (g_pAuth->m_bDisplayFailText) {
        g_pAuth->m_bDisplayFailText = false;
        g_pMpvlock->renderAllOutputs();  // Updated from g_pHyprlock
    }
}

void CAuth::enqueueFail(const std::string& failText, eAuthImplementations implType) {
    const auto FAILTIMEOUT = g_pConfigManager->getValue<Hyprlang::INT>("general:fail_timeout");

    m_sCurrentFail.failText   = failText;
    m_sCurrentFail.failSource = implType;
    m_sCurrentFail.failedAttempts++;

    Debug::log(LOG, "Failed attempts: {}", m_sCurrentFail.failedAttempts);

    if (m_resetDisplayFailTimer) {
        m_resetDisplayFailTimer->cancel();
        m_resetDisplayFailTimer.reset();
    }

    g_pMpvlock->addTimer(std::chrono::milliseconds(0), passwordFailCallback, nullptr);  // Updated from g_pHyprlock
    m_resetDisplayFailTimer = g_pMpvlock->addTimer(std::chrono::milliseconds(*FAILTIMEOUT), displayFailTimeoutCallback, nullptr);  // Updated from g_pHyprlock
}

void CAuth::resetDisplayFail() {
    g_pAuth->m_bDisplayFailText = false;
    m_resetDisplayFailTimer->cancel();
    m_resetDisplayFailTimer.reset();
}