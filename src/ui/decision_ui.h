// On-screen answers to the rules engine's questions, one widget per kind of
// decision (pick from a list, pick on the map, pay, move, produce, assign
// hits, land, command tokens). Each widget builds an Answer; the engine
// validates it and any error is shown back in the widget.
#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/input.h"
#include "engine/decision.h"
#include "engine/state.h"
#include "gfx/canvas.h"
#include "ui/map_view.h"

class DecisionUI {
public:
    DecisionUI(const ti::Game& g, const ti::Decision& d) : g_(g), d_(d) { d_.context = keyText(d_.context); }
    virtual ~DecisionUI() = default;
    // Returns true when an answer is ready (read it with answer()).
    virtual bool update(const Input& in, uint32_t now) = 0;
    virtual void draw(Canvas& c, uint32_t now) = 0;
    virtual bool mapMode() const { return false; }
    // The id of the option under the cursor (list choices), or -1.
    virtual int highlightedValue() const { return -1; }
    virtual std::vector<std::pair<std::string, std::string>> hints() const = 0;
    // Does B back out of something inside this question right now? When it
    // doesn't, B undoes the previous step instead (owner, 0.19.0: one button).
    virtual bool backAvailable() const { return true; }
    // Covers the whole screen (the research cards): the mouse wheel scrolls it, never the map (0.29.0).
    virtual bool fullScreen() const { return false; }
    // Widescreen (0.46.0): laid out for the whole 640-wide screen - drawn centred on a wider one.
    virtual bool centred() const { return fullScreen(); }
    // Left / right change values here (rows of quantities): with keyboard & mouse, WASD work as the arrow keys
    // instead of moving the map (0.43.0, owner P12).
    virtual bool stepsWithKeys() const { return false; }
    // Waypoint Mode (0.56.0, owner): the move editor plans routes on the map (the crosshair button / X)
    virtual bool waypointsAvailable() const { return false; }
    virtual bool waypointsOn() const { return false; }
    virtual void toggleWaypoints() {}
    void setError(const std::string& e) {
        error_ = e;
        errorFresh_ = true;
    }
    // Error messages disappear after 3 seconds.
    void ageError(uint32_t now) {
        if (errorFresh_) {
            errorSince_ = now;
            errorFresh_ = false;
        }
        if (!error_.empty() && now - errorSince_ > 3000) error_.clear();
    }
    const ti::Answer& answer() const { return answer_; }
    const ti::Decision& decision() const { return d_; }

protected:
    const ti::Game& g_;
    ti::Decision d_;
    ti::Answer answer_;
    std::string error_;
    bool errorFresh_ = false;
    uint32_t errorSince_ = 0;
};

// The panel area decisions draw into (over the map).
Rect decisionPanelRect();

std::unique_ptr<DecisionUI> makeDecisionUI(const ti::Game& g, const ti::Decision& d, MapView& map);
