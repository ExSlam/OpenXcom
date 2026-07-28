#pragma once
/*
 * Copyright 2010-2026 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <vector>
#include "../Engine/State.h"
#include "../Mod/RuleCustomUi.h"

namespace OpenXcom
{

class Base;
class RuleCustomUi;
class SavedBattleGame;
class Text;
class TextButton;
class TextList;
class Window;

/**
 * Lists mod-provided custom UIs available from the geoscape.
 */
class CustomUiListState : public State
{
private:
	Window *_window;
	Text *_title;
	Text *_hint;
	TextList *_list;
	TextButton *_open;
	TextButton *_cancel;
	std::vector<const RuleCustomUi *> _rules;
	CustomUiContext _context;
	SavedBattleGame *_battleGame;
	Base *_base;
public:
	/// Creates the custom UI list.
	explicit CustomUiListState(CustomUiContext context = CUSTOM_UI_GEOSCAPE, SavedBattleGame *battleGame = nullptr, Base *base = nullptr);
	/// Cleans up the state.
	~CustomUiListState() = default;
	/// Opens the selected custom UI.
	void openClick(Action *action);
	/// Closes the list.
	void cancelClick(Action *action);
};

}
