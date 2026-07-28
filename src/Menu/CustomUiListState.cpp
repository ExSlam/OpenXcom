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
#include "CustomUiListState.h"
#include "CustomUiState.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleCustomUi.h"

namespace OpenXcom
{

CustomUiListState::CustomUiListState(CustomUiContext context, SavedBattleGame *battleGame, Base *base) :
	_context(context),
	_battleGame(battleGame),
	_base(base)
{
	_screen = false;

	_window = new Window(this, 300, 194, 10, 3, POPUP_BOTH);
	_title = new Text(268, 17, 26, 11);
	_hint = new Text(268, 9, 26, 28);
	// Keep the height aligned to the small font's 8-pixel row step.
	// TextList rounds partial rows up when calculating its scroll limit.
	_list = new TextList(252, 120, 26, 40);
	_open = new TextButton(122, 16, 26, 174);
	_cancel = new TextButton(122, 16, 162, 174);

	setInterface("oxceLinks", false, _battleGame);
	add(_window, "window", "oxceLinks");
	add(_title, "text", "oxceLinks");
	add(_hint, "text", "oxceLinks");
	add(_list, "button", "oxceLinks");
	add(_open, "button", "oxceLinks");
	add(_cancel, "button", "oxceLinks");
	centerAllSurfaces();
	setWindowBackground(_window, "oxceLinks");
	if (_context == CUSTOM_UI_BATTLESCAPE)
	{
		applyBattlescapeTheme("oxceLinks");
	}

	_title->setBig();
	_title->setAlign(ALIGN_CENTER);
	_title->setText(tr("STR_CUSTOM_UIS"));

	_hint->setAlign(ALIGN_CENTER);
	_hint->setText(tr("STR_CUSTOM_UI_PICKER_HINT"));

	_list->setColumns(1, 252);
	_list->setSelectable(true);
	_list->setBackground(_window);
	_list->setMargin(8);
	_list->setScrolling(true, 0);
	_list->onMouseClick((ActionHandler)&CustomUiListState::openClick, SDL_BUTTON_LEFT);

	for (const auto &id : _game->getMod()->getCustomUiList())
	{
		const RuleCustomUi *rule = _game->getMod()->getCustomUi(id);
		if (rule->getContext() == _context)
		{
			_rules.push_back(rule);
			_list->addRow(1, tr(rule->getTitle()).c_str());
		}
	}

	_open->setText(tr("STR_OPEN_CUSTOM_UI"));
	_open->onMouseClick((ActionHandler)&CustomUiListState::openClick);
	_open->onKeyboardPress((ActionHandler)&CustomUiListState::openClick, Options::keyOk);

	_cancel->setText(tr("STR_CANCEL"));
	_cancel->onMouseClick((ActionHandler)&CustomUiListState::cancelClick);
	_cancel->onKeyboardPress((ActionHandler)&CustomUiListState::cancelClick, Options::keyCancel);
}

void CustomUiListState::openClick(Action *)
{
	const size_t selected = _list->getSelectedRow();
	if (selected < _rules.size())
	{
		_game->pushState(new CustomUiState(_rules[selected], _battleGame, _base));
	}
}

void CustomUiListState::cancelClick(Action *)
{
	_game->popState();
}

}
