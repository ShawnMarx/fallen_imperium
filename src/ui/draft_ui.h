// Galaxy setup draft screen (0.15.0): place a system tile from your hand.
#pragma once
#include <memory>

#include "ui/decision_ui.h"

std::unique_ptr<DecisionUI> makeGalaxyDraftUI(const ti::Game& g, const ti::Decision& d, MapView& map);
