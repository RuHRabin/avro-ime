// Fcitx5 frontend for the avro core engine.
//
// All the linguistics live in core/avro_core.*. This file only translates
// between Fcitx5 events and avro::Session:
//
//   printable key  -> Session::feed()      (preedit shown underlined)
//   BackSpace      -> Session::backspace() (removes one Latin key)
//   Escape         -> Session::cancel()
//   Space/Enter/Tab, arrows, Ctrl/Alt/Super shortcuts
//                  -> commit the word, then let the key through to the app
//
// The typed Latin letters are also shown in the aux area above the cursor,
// so you can see what you typed while the Bengali preedit is in the text.

#include <fcitx-config/configuration.h>
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <fcitx-utils/log.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/text.h>

#include "avro_core.hpp"

namespace {

FCITX_CONFIGURATION(
    AvroConfig,
    fcitx::Option<bool> finalOKar{this, "FinalOKar",
                                  "Word-final 'o' after a consonant gives ো (bhalo -> ভালো)", false};
    fcitx::Option<bool> banglaDigits{this, "BanglaDigits", "Type Bengali digits (০-৯)", true};);

constexpr char kConfigPath[] = "conf/avroime.conf";

// One avro::Session per input context, so typing in two windows never mixes.
class AvroState : public fcitx::InputContextProperty {
public:
    avro::Session session;
};

class AvroEngine : public fcitx::InputMethodEngineV2 {
public:
    explicit AvroEngine(fcitx::Instance *instance)
        : instance_(instance),
          factory_([](fcitx::InputContext &) { return new AvroState(); }) {
        instance_->inputContextManager().registerProperty("avroimeState", &factory_);
        reloadConfig();
    }

    void keyEvent(const fcitx::InputMethodEntry &, fcitx::KeyEvent &event) override {
        if (event.isRelease()) return;

        fcitx::InputContext *ic = event.inputContext();
        AvroState *st = ic->propertyFor(&factory_);
        st->session.set_options(options());
        const fcitx::Key key = event.key();

        // Shortcuts (Ctrl+C, Alt+Tab, Super+...): finish the word, don't touch the key.
        if (key.states().testAny(fcitx::KeyStates{fcitx::KeyState::Ctrl, fcitx::KeyState::Alt,
                                                  fcitx::KeyState::Super})) {
            apply(ic, st->session.flush(), st);
            return;
        }

        switch (key.sym()) {
        case FcitxKey_BackSpace: {
            auto r = st->session.backspace();
            apply(ic, r, st);
            if (r.consumed) event.filterAndAccept();
            return;
        }
        case FcitxKey_Escape: {
            auto r = st->session.cancel();
            apply(ic, r, st);
            if (r.consumed) event.filterAndAccept();
            return;
        }
        default:
            break;
        }

        const uint32_t ch = fcitx::Key::keySymToUnicode(key.sym());
        if (ch >= 0x20 && ch < 0x7f) {
            auto r = st->session.feed(static_cast<char>(ch));
            apply(ic, r, st);
            if (r.consumed) event.filterAndAccept();
            return;
        }

        // Enter, Tab, arrows, Home/End, Delete, F-keys, ...: commit, then pass through.
        apply(ic, st->session.flush(), st);
    }

    // Called on focus change, cursor jumps by the application, etc.
    void reset(const fcitx::InputMethodEntry &, fcitx::InputContextEvent &event) override {
        clear(event.inputContext());
    }
    void deactivate(const fcitx::InputMethodEntry &, fcitx::InputContextEvent &event) override {
        clear(event.inputContext());
    }

    void reloadConfig() override { fcitx::readAsIni(config_, kConfigPath); }
    const fcitx::Configuration *getConfig() const override { return &config_; }
    void setConfig(const fcitx::RawConfig &raw) override {
        config_.load(raw, true);
        fcitx::safeSaveAsIni(config_, kConfigPath);
    }

private:
    avro::Options options() const {
        avro::Options o;
        o.final_o_kar = *config_.finalOKar;
        o.bangla_digits = *config_.banglaDigits;
        return o;
    }

    void apply(fcitx::InputContext *ic, const avro::Session::Result &r, AvroState *st) {
        if (!r.commit.empty()) ic->commitString(r.commit);
        showPreedit(ic, r.preedit, st->session.raw());
    }

    void clear(fcitx::InputContext *ic) {
        AvroState *st = ic->propertyFor(&factory_);
        st->session.cancel();
        showPreedit(ic, "", "");
    }

    void showPreedit(fcitx::InputContext *ic, const std::string &bengali, const std::string &latin) {
        auto &panel = ic->inputPanel();
        panel.reset();
        if (!bengali.empty()) {
            fcitx::Text text;
            text.append(bengali, fcitx::TextFormatFlag::Underline);
            text.setCursor(static_cast<int>(bengali.size()));
            if (ic->capabilityFlags().test(fcitx::CapabilityFlag::Preedit)) {
                panel.setClientPreedit(text);
            } else {
                panel.setPreedit(text);
            }
            panel.setAuxUp(fcitx::Text(latin));
        }
        ic->updatePreedit();
        ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    }

    fcitx::Instance *instance_;
    fcitx::FactoryFor<AvroState> factory_;
    AvroConfig config_;
};

class AvroEngineFactory : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance *create(fcitx::AddonManager *manager) override {
        return new AvroEngine(manager->instance());
    }
};

}  // namespace

FCITX_ADDON_FACTORY(AvroEngineFactory);
