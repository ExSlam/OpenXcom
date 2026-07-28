/*
 * Copyright 2010-2026 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "CustomUiState.h"
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <utility>
#include <SDL.h>
#include "../Basescape/CraftsState.h"
#include "../Basescape/ManufactureState.h"
#include "../Basescape/ResearchState.h"
#include "../Basescape/SoldiersState.h"
#include "../Basescape/StoresState.h"
#include "../Basescape/TechTreeViewerState.h"
#include "../Engine/Action.h"
#include "../Engine/Exception.h"
#include "../Engine/Game.h"
#include "../Engine/Logger.h"
#include "../Engine/Options.h"
#include "../Engine/Screen.h"
#include "../Engine/ScriptBind.h"
#include "../Engine/Unicode.h"
#include "../Interface/ArrowButton.h"
#include "../Interface/ComboBox.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextEdit.h"
#include "../Interface/TextList.h"
#include "../Interface/ToggleTextButton.h"
#include "../Interface/Window.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleCustomUi.h"
#include "../Mod/RuleInterface.h"
#include "../Savegame/Base.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/SavedGame.h"
#include "../Ufopaedia/UfopaediaStartState.h"

namespace OpenXcom
{

namespace
{

int anchorX(CustomUiAnchor anchor, int width)
{
	switch (anchor)
	{
	case CUSTOM_UI_ANCHOR_TOP:
	case CUSTOM_UI_ANCHOR_CENTER:
	case CUSTOM_UI_ANCHOR_BOTTOM: return (320 - width) / 2;
	case CUSTOM_UI_ANCHOR_TOP_RIGHT:
	case CUSTOM_UI_ANCHOR_RIGHT:
	case CUSTOM_UI_ANCHOR_BOTTOM_RIGHT: return 320 - width;
	default: return 0;
	}
}

int anchorY(CustomUiAnchor anchor, int height)
{
	switch (anchor)
	{
	case CUSTOM_UI_ANCHOR_LEFT:
	case CUSTOM_UI_ANCHOR_CENTER:
	case CUSTOM_UI_ANCHOR_RIGHT: return (200 - height) / 2;
	case CUSTOM_UI_ANCHOR_BOTTOM_LEFT:
	case CUSTOM_UI_ANCHOR_BOTTOM:
	case CUSTOM_UI_ANCHOR_BOTTOM_RIGHT: return 200 - height;
	default: return 0;
	}
}

std::map<std::string, CustomUiState::NativeScreenFactory> &nativeFactories()
{
	static std::map<std::string, CustomUiState::NativeScreenFactory> factories;
	return factories;
}

void getTextScriptBridge(const CustomUiState *ui, const std::string &id, ScriptText &result)
{
	if (ui)
		ui->getTextScript(id, result);
	else
		result = ScriptText::empty;
}

}

CustomUiState::CustomUiState(const RuleCustomUi *rule, SavedBattleGame *battleGame, Base *base) :
	_rule(rule),
	_battleGame(battleGame),
	_base(base),
	_window(nullptr),
	_focusedButton(nullptr),
	_hoveredButton(nullptr),
	_focusedEdit(nullptr),
	_pendingNavigation(NAV_NONE),
	_pendingReplace(false),
	_syncing(false)
{
	_screen = false;
	if (!_base && _game->getSavedGame() && !_game->getSavedGame()->getBases()->empty())
		_base = _game->getSavedGame()->getSelectedBase();

	for (const auto &entry : _rule->getValues())
	{
		RuntimeValue value;
		value.type = entry.second.type;
		value.intValue = entry.second.type == CUSTOM_UI_VALUE_BOOL ? (entry.second.boolDefault ? 1 : 0) : entry.second.intDefault;
		value.stringValue = entry.second.stringDefault;
		_values[entry.first] = value;
	}

	const int originX = anchorX(_rule->getAnchor(), _rule->getWidth()) + _rule->getOffsetX();
	const int originY = anchorY(_rule->getAnchor(), _rule->getHeight()) + _rule->getOffsetY();
	_window = new Window(this, _rule->getWidth(), _rule->getHeight(), originX, originY, POPUP_BOTH);

	// A battle may remain available when one custom UI opens a screen from a
	// different context. Only battlescape screens should use its depth palette;
	// geoscape and basescape interfaces must retain their declared palettes.
	SavedBattleGame *paletteBattle = _rule->getContext() == CUSTOM_UI_BATTLESCAPE ? _battleGame : nullptr;
	setInterface(_rule->getInterface(), false, paletteBattle);
	add(_window, "window", _rule->getInterface());

	Text *title = new Text(_rule->getWidth() - 16, 17, originX + 8, originY + 7);
	add(title, "text", _rule->getInterface());
	title->setBig();
	title->setAlign(ALIGN_CENTER);
	title->setText(tr(_rule->getTitle()));

	for (const auto &widget : _rule->getWidgets())
	{
		const int x = originX + widget.x;
		const int y = originY + widget.y;
		if (widget.type == CUSTOM_UI_LABEL)
		{
			const bool wrapText = widget.wordWrap || widget.verticalOverflow;
			if (widget.autoScrollbar || (_rule->getAutoScrollbars() && (widget.wordWrap || widget.overflowExplicit)))
			{
				const int autoHeight = widget.height >= 16 ? widget.height - widget.height % 8 : widget.height;
				TextList *label = new TextList(widget.width, autoHeight, x, y);
				add(label, "list", _rule->getInterface());
				if (widget.big) label->setBig();
				label->setColumns(1, std::max(1, widget.width - 17));
				label->setWordWrap(wrapText);
				label->setSelectable(false);
				label->setBackground(_window);
				label->setMargin(2);
				label->setScrolling(true, -13);
				if (widget.align == CUSTOM_UI_ALIGN_CENTER) label->setAlign(ALIGN_CENTER);
				else if (widget.align == CUSTOM_UI_ALIGN_RIGHT) label->setAlign(ALIGN_RIGHT);
				_scrollLabels.push_back({ label, &widget });
			}
			else
			{
				Text *label = new Text(widget.width, widget.height, x, y);
				add(label, "text", _rule->getInterface());
				if (widget.big) label->setBig();
				label->setWordWrap(wrapText);
				if (widget.align == CUSTOM_UI_ALIGN_CENTER) label->setAlign(ALIGN_CENTER);
				else if (widget.align == CUSTOM_UI_ALIGN_RIGHT) label->setAlign(ALIGN_RIGHT);
				_labels.push_back({ label, &widget });
			}
		}
		else if (widget.type == CUSTOM_UI_BUTTON || widget.type == CUSTOM_UI_TOGGLE)
		{
			TextButton *button = widget.type == CUSTOM_UI_TOGGLE
				? static_cast<TextButton *>(new ToggleTextButton(widget.width, widget.height, x, y))
				: new TextButton(widget.width, widget.height, x, y);
			add(button, "button", _rule->getInterface());
			if (widget.big) button->setBig();
			button->onMousePress((ActionHandler)&CustomUiState::buttonPress, SDL_BUTTON_LEFT);
			button->onMouseIn((ActionHandler)&CustomUiState::buttonIn);
			button->onMouseOut((ActionHandler)&CustomUiState::buttonOut);
			_surfaceWidgets[button] = &widget;
			if (widget.type == CUSTOM_UI_TOGGLE)
			{
				ToggleTextButton *toggle = static_cast<ToggleTextButton *>(button);
				toggle->onMouseClick((ActionHandler)&CustomUiState::toggleClick);
				_toggles.push_back({ toggle, &widget });
			}
			else
			{
				button->onMouseClick((ActionHandler)&CustomUiState::buttonClick);
				_actions[button] = std::make_pair(widget.id, widget.action);
			}

			int interfaceTextColor = button->getColor();
			const Element *element = _game->getMod()->getInterface(_rule->getInterface())->getElementOptional("button");
			if (element && element->color2 != INT_MAX) interfaceTextColor = element->color2;
			ButtonStyle style {
				button, &widget,
				resolveColor(widget.colors.normal.background, button->getColor()),
				resolveColor(widget.colors.normal.text, interfaceTextColor),
				-1, -1, -1, -1, -1, -1,
				widget.selected
			};
			style.hoverBackground = resolveColor(widget.colors.hover.background, style.normalBackground);
			style.hoverText = resolveColor(widget.colors.hover.text, style.normalText);
			style.focusedBackground = resolveColor(widget.colors.focused.background, style.normalBackground);
			style.focusedText = resolveColor(widget.colors.focused.text, style.normalText);
			style.selectedBackground = resolveColor(widget.colors.selected.background, style.normalBackground);
			style.selectedText = resolveColor(widget.colors.selected.text, style.normalText);
			_buttonStyles.push_back(style);
			if (widget.focused) _focusedButton = button;
		}
		else if (widget.type == CUSTOM_UI_TEXT_INPUT || widget.type == CUSTOM_UI_SEARCH)
		{
			TextEdit *edit = new TextEdit(this, widget.width, widget.height, x, y);
			add(edit, "button", _rule->getInterface());
			edit->setDrawBackground(true);
			edit->onChange((ActionHandler)&CustomUiState::editChange);
			edit->onEnter((ActionHandler)&CustomUiState::editSubmit);
			edit->onMousePress((ActionHandler)&CustomUiState::editPress, SDL_BUTTON_LEFT);
			_surfaceWidgets[edit] = &widget;
			_editsById[widget.id] = edit;
			_edits.push_back({ edit, &widget, false, false });
			if (widget.focused)
			{
				edit->setFocus(true);
				_focusedEdit = edit;
			}
		}
		else if (widget.type == CUSTOM_UI_NUMBER)
		{
			const int arrowWidth = 11;
			const int editWidth = widget.width - arrowWidth - 1;
			const int upperHeight = widget.height / 2;
			TextEdit *edit = new TextEdit(this, editWidth, widget.height, x, y);
			ArrowButton *up = new ArrowButton(ARROW_SMALL_UP, arrowWidth, upperHeight, x + editWidth + 1, y);
			ArrowButton *down = new ArrowButton(ARROW_SMALL_DOWN, arrowWidth, widget.height - upperHeight, x + editWidth + 1, y + upperHeight);
			add(edit, "button", _rule->getInterface());
			add(up, "button", _rule->getInterface());
			add(down, "button", _rule->getInterface());
			edit->setDrawBackground(true);
			edit->setConstraint(TEC_NUMERIC);
			edit->onChange((ActionHandler)&CustomUiState::editChange);
			edit->onEnter((ActionHandler)&CustomUiState::editSubmit);
			if (widget.mouseWheel)
			{
				edit->onMousePress((ActionHandler)&CustomUiState::numberWheel, SDL_BUTTON_WHEELUP);
				edit->onMousePress((ActionHandler)&CustomUiState::numberWheel, SDL_BUTTON_WHEELDOWN);
			}
			up->onMouseClick((ActionHandler)&CustomUiState::numberUp);
			down->onMouseClick((ActionHandler)&CustomUiState::numberDown);
			_surfaceWidgets[edit] = &widget;
			_surfaceWidgets[up] = &widget;
			_surfaceWidgets[down] = &widget;
			_editsById[widget.id] = edit;
			_edits.push_back({ edit, &widget, true, false });
			_numbers.push_back({ edit, up, down, &widget });
			if (widget.focused)
			{
				edit->setFocus(true);
				_focusedEdit = edit;
			}
		}
		else if (widget.type == CUSTOM_UI_DROPDOWN)
		{
			ComboBox *combo = new ComboBox(this, widget.width, widget.height, x, y);
			add(combo, "button", _rule->getInterface());
			std::vector<std::string> options;
			for (const auto &option : widget.options)
				options.push_back(static_cast<const std::string &>(tr(option.text)));
			combo->setOptions(options);
			combo->onChange((ActionHandler)&CustomUiState::comboChange);
			_surfaceWidgets[combo] = &widget;
			_combos.push_back({ combo, &widget });
		}
		else if (widget.type == CUSTOM_UI_LIST)
		{
			const bool hasHeaders = std::any_of(widget.columns.begin(), widget.columns.end(), [](const CustomUiColumn &column) { return !column.header.empty(); });
			const int headerHeight = hasHeaders ? 10 : 0;
			const int contentHeight = widget.height - headerHeight;
			const int autoHeight = _rule->getAutoScrollbars() && contentHeight >= 16
				? contentHeight - contentHeight % 8
				: contentHeight;
			TextList *list = new TextList(widget.width, autoHeight, x, y + headerHeight);
			add(list, "list", _rule->getInterface());
			setListColumns(list, widget);
			list->setSelectable(widget.selectable);
			list->setBackground(_window);
			list->setMargin(2);
			if (_rule->getAutoScrollbars()) list->setScrolling(true, -13);
			list->onMouseClick((ActionHandler)&CustomUiState::listClick);
			_surfaceWidgets[list] = &widget;
			_lists.push_back({ list, &widget, {}, false });

			if (hasHeaders)
			{
				const std::vector<int> widths = getListColumnWidths(widget);
				int headerX = x + 2;
				for (size_t i = 0; i < widget.columns.size(); ++i)
				{
					const CustomUiColumn &column = widget.columns[i];
					if (!column.header.empty())
					{
						Text *label = new Text(std::max(1, widths[i] - 2), 9, headerX, y);
						add(label, "text", _rule->getInterface());
						label->setText(tr(column.header));

						ArrowButton *arrow = nullptr;
						if (!widget.sortValue.empty())
						{
							arrow = new ArrowButton(ARROW_NONE, 11, 8,
								std::min(headerX + widths[i] - 11, headerX + label->getTextWidth() + 4), y);
							add(arrow, "text", _rule->getInterface());
							arrow->onMouseClick((ActionHandler)&CustomUiState::sortHeaderClick);
						}
						_sortHeaders.push_back({ arrow, label, &widget, column.field });
					}
					headerX += widths[i];
				}
			}
		}
	}

	centerAllSurfaces();
	if (_rule->getContext() == CUSTOM_UI_BATTLESCAPE) applyBattlescapeTheme(_rule->getInterface());
	if (_rule->getBackgroundImage().empty())
	{
		if (_rule->getContext() != CUSTOM_UI_BATTLESCAPE) setWindowBackground(_window, _rule->getInterface());
	}
	else
	{
		setWindowBackgroundImage(_window, _rule->getBackgroundImage());
	}

	for (auto &entry : _labels)
	{
		const int textColor = resolveColor(entry.widget->colors.normal.text, entry.label->getColor());
		entry.label->setColor(textColor);
	}
	for (auto &entry : _scrollLabels)
	{
		const int textColor = resolveColor(entry.widget->colors.normal.text, entry.label->getColor());
		entry.label->setColor(textColor);
	}
	for (auto &entry : _sortHeaders)
	{
		const int textColor = resolveColor(entry.widget->colors.normal.text, entry.label->getColor());
		entry.label->setColor(textColor);
		if (entry.arrow) entry.arrow->setColor(textColor);
	}

	for (auto &entry : _edits)
	{
		entry.edit->setColor(resolveColor(entry.widget->colors.normal.text, entry.edit->getColor()));
	}
	for (auto &entry : _combos)
	{
		const std::string &color = entry.widget->colors.normal.text.empty()
			? entry.widget->colors.normal.background
			: entry.widget->colors.normal.text;
		entry.combo->setColor(resolveColor(color, entry.combo->getColor()));
	}
	for (auto &entry : _lists)
	{
		entry.list->setColor(resolveColor(entry.widget->colors.normal.text, entry.list->getColor()));
	}

	if (!_focusedButton && !_focusedEdit && !_buttonStyles.empty()) _focusedButton = _buttonStyles.front().button;
	for (auto &style : _buttonStyles)
	{
		const CustomUiWidget &widget = *style.widget;
		const int themedBackground = style.button->getColor();
		int themedText = themedBackground;
		const Element *element = _game->getMod()->getInterface(_rule->getInterface())->getElementOptional("button");
		if (_rule->getContext() != CUSTOM_UI_BATTLESCAPE && element && element->color2 != INT_MAX) themedText = element->color2;
		style.normalBackground = resolveColor(widget.colors.normal.background, themedBackground);
		style.normalText = resolveColor(widget.colors.normal.text, themedText);
		style.hoverBackground = resolveColor(widget.colors.hover.background, style.normalBackground);
		style.hoverText = resolveColor(widget.colors.hover.text, style.normalText);
		style.focusedBackground = resolveColor(widget.colors.focused.background, style.normalBackground);
		style.focusedText = resolveColor(widget.colors.focused.text, style.normalText);
		style.selectedBackground = resolveColor(widget.colors.selected.background, style.normalBackground);
		style.selectedText = resolveColor(widget.colors.selected.text, style.normalText);
		style.button->setFocus(style.button == _focusedButton);
	}

	syncBoundWidgets();
	refreshLists();
}

int CustomUiState::resolveColor(const std::string &value, int fallback) const
{
	if (value.empty()) return fallback;
	if (value[0] != '#') return std::atoi(value.c_str());

	const int r = std::strtol(value.substr(1, 2).c_str(), nullptr, 16);
	const int g = std::strtol(value.substr(3, 2).c_str(), nullptr, 16);
	const int b = std::strtol(value.substr(5, 2).c_str(), nullptr, 16);
	int best = 1;
	long bestDistance = LONG_MAX;
	for (int i = 1; i < 256; ++i)
	{
		const long dr = static_cast<long>(_palette[i].r) - r;
		const long dg = static_cast<long>(_palette[i].g) - g;
		const long db = static_cast<long>(_palette[i].b) - b;
		const long distance = dr * dr + dg * dg + db * db;
		if (distance < bestDistance)
		{
			best = i;
			bestDistance = distance;
		}
	}
	return best;
}

std::string CustomUiState::formatRichText(const std::string &text, int defaultColor) const
{
	std::string result;
	std::vector<int> colors(1, defaultColor);
	size_t pos = 0;
	while (pos < text.size())
	{
		if (text.compare(pos, 7, "[color=") == 0)
		{
			const size_t end = text.find(']', pos + 7);
			if (end != std::string::npos)
			{
				const std::string reference = text.substr(pos + 7, end - pos - 7);
				const bool isHex = reference.size() == 7 && reference[0] == '#' &&
					std::all_of(reference.begin() + 1, reference.end(), [](char c) {
						return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
					});
				const bool isIndex = !reference.empty() &&
					std::all_of(reference.begin(), reference.end(), [](char c) { return c >= '0' && c <= '9'; }) &&
					std::atoi(reference.c_str()) >= 1 && std::atoi(reference.c_str()) <= 255;
				if (isHex || isIndex)
				{
					const int color = resolveColor(reference, colors.back());
					colors.push_back(color);
					result.push_back(Unicode::TOK_CUSTOM_FORMAT);
					result.push_back('C');
					result += Unicode::convUtf32ToUtf8(UString(1, color));
					pos = end + 1;
					continue;
				}
			}
		}
		if (text.compare(pos, 8, "[/color]") == 0 && colors.size() > 1)
		{
			colors.pop_back();
			result.push_back(Unicode::TOK_CUSTOM_FORMAT);
			if (colors.size() == 1)
			{
				result.push_back('c');
				result.push_back('P');
			}
			else
			{
				result.push_back('C');
				result += Unicode::convUtf32ToUtf8(UString(1, colors.back()));
			}
			pos += 8;
			continue;
		}
		result.push_back(text[pos++]);
	}
	return result;
}

std::string CustomUiState::renderText(const CustomUiWidget &widget) const
{
	auto overrideText = _widgetTextOverrides.find(widget.id);
	std::string text = overrideText == _widgetTextOverrides.end()
		? static_cast<const std::string &>(tr(widget.text))
		: static_cast<const std::string &>(tr(overrideText->second));
	for (const auto &value : _values)
	{
		const std::string marker = "${" + value.first + "}";
		size_t position = 0;
		while ((position = text.find(marker, position)) != std::string::npos)
		{
			const std::string replacement = getValueText(value.first);
			text.replace(position, marker.size(), replacement);
			position += replacement.size();
		}
	}
	return text;
}

std::string CustomUiState::getValueText(const std::string &id) const
{
	const RuntimeValue *value = findValue(id);
	if (!value) return std::string();
	if (value->type == CUSTOM_UI_VALUE_STRING) return value->stringValue;
	return std::to_string(value->intValue);
}

CustomUiState::RuntimeValue *CustomUiState::findValue(const std::string &id)
{
	auto found = _values.find(id);
	return found == _values.end() ? nullptr : &found->second;
}

const CustomUiState::RuntimeValue *CustomUiState::findValue(const std::string &id) const
{
	auto found = _values.find(id);
	return found == _values.end() ? nullptr : &found->second;
}

CustomUiState::ButtonStyle *CustomUiState::findStyle(TextButton *button)
{
	for (auto &style : _buttonStyles)
		if (style.button == button) return &style;
	return nullptr;
}

const CustomUiWidget *CustomUiState::findWidget(InteractiveSurface *surface) const
{
	auto found = _surfaceWidgets.find(surface);
	return found == _surfaceWidgets.end() ? nullptr : found->second;
}

void CustomUiState::applyButtonStyle(ButtonStyle &style)
{
	int background = style.normalBackground;
	int text = style.normalText;
	if (style.selected)
	{
		background = style.selectedBackground;
		text = style.selectedText;
	}
	if (style.button == _focusedButton)
	{
		background = style.focusedBackground;
		text = style.focusedText;
	}
	if (style.button == _hoveredButton)
	{
		background = style.hoverBackground;
		text = style.hoverText;
	}
	style.button->setColor(background);
	style.button->setTextColor(text);
	if (ToggleTextButton *toggle = dynamic_cast<ToggleTextButton *>(style.button))
	{
		toggle->setInvertColor(style.selectedBackground);
		toggle->setPressed(style.selected);
	}
	style.button->setText(formatRichText(renderText(*style.widget), style.normalText));
	style.button->draw();
}

void CustomUiState::focusButton(TextButton *button)
{
	if (_focusedButton == button) return;
	TextButton *old = _focusedButton;
	_focusedButton = button;
	if (old)
	{
		old->setFocus(false);
		if (ButtonStyle *style = findStyle(old)) applyButtonStyle(*style);
	}
	if (_focusedButton)
	{
		_focusedButton->setFocus(true);
		if (ButtonStyle *style = findStyle(_focusedButton)) applyButtonStyle(*style);
	}
}

void CustomUiState::moveFocus(int direction)
{
	if (_buttonStyles.empty()) return;
	size_t index = 0;
	for (; index < _buttonStyles.size(); ++index)
		if (_buttonStyles[index].button == _focusedButton) break;
	if (index == _buttonStyles.size())
	{
		focusButton(direction > 0 ? _buttonStyles.front().button : _buttonStyles.back().button);
		return;
	}
	if (direction > 0) index = (index + 1) % _buttonStyles.size();
	else index = (index + _buttonStyles.size() - 1) % _buttonStyles.size();
	focusButton(_buttonStyles[index].button);
}

void CustomUiState::activateButton(TextButton *button)
{
	if (!button) return;
	if (ToggleTextButton *toggle = dynamic_cast<ToggleTextButton *>(button))
	{
		const CustomUiWidget *widget = findWidget(toggle);
		if (!widget) return;
		const bool next = !toggle->getPressed();
		toggle->setPressed(next);
		if (RuntimeValue *value = findValue(widget->bind)) value->intValue = next ? 1 : 0;
		if (ButtonStyle *style = findStyle(toggle))
		{
			style->selected = next;
			applyButtonStyle(*style);
		}
		dispatchAction(widget->id, widget->onChange.empty() ? widget->action : widget->onChange);
	}
	else
	{
		auto found = _actions.find(button);
		if (found != _actions.end()) dispatchAction(found->second.first, found->second.second);
	}
}

void CustomUiState::dispatchAction(const std::string &widgetId, const std::string &actionId)
{
	if (actionId.empty()) return;
	_pendingNavigation = NAV_NONE;
	_pendingTarget.clear();
	_pendingReplace = false;

	if (actionId == "close")
	{
		_pendingNavigation = NAV_CLOSE;
	}
	else if (const CustomUiActionDefinition *action = _rule->getAction(actionId))
	{
		switch (action->type)
		{
		case CUSTOM_UI_ACTION_CLOSE:
			_pendingNavigation = NAV_CLOSE;
			break;
		case CUSTOM_UI_ACTION_OPEN_CUSTOM_UI:
			queueCustomUi(action->target, action->replace);
			break;
		case CUSTOM_UI_ACTION_OPEN_NATIVE_UI:
			queueNativeUi(action->target, action->replace);
			break;
		case CUSTOM_UI_ACTION_SCRIPT:
			executeScriptAction(widgetId, actionId);
			break;
		}
	}
	applyPendingNavigation();
}

void CustomUiState::executeScriptAction(const std::string &widgetId, const std::string &actionId)
{
	ModScript::CustomUiAction::Output output {};
	ModScript::CustomUiAction::Worker worker {
		this,
		ScriptText { _rule->getId().c_str() },
		ScriptText { actionId.c_str() },
		ScriptText { widgetId.c_str() },
		ScriptText { _selectedRecordId.c_str() },
		_game->getSavedGame(),
		_battleGame
	};
	worker.execute(_rule->getScript<ModScript::CustomUiAction>(), output);
}

void CustomUiState::applyPendingNavigation()
{
	const PendingNavigation operation = _pendingNavigation;
	const std::string target = _pendingTarget;
	const bool replace = _pendingReplace;
	_pendingNavigation = NAV_NONE;
	_pendingTarget.clear();
	_pendingReplace = false;

	if (operation == NAV_CLOSE)
	{
		_game->popState();
		return;
	}
	if (operation == NAV_CUSTOM)
	{
		const RuleCustomUi *nextRule = _game->getMod()->getCustomUi(target, false);
		if (!nextRule)
		{
			Log(LOG_ERROR) << "Custom UI '" << _rule->getId() << "' tried to open unknown custom UI '" << target << "'";
			return;
		}
		if (nextRule->getContext() == CUSTOM_UI_BATTLESCAPE && !_battleGame)
		{
			Log(LOG_ERROR) << "Custom UI '" << _rule->getId() << "' cannot open battlescape UI '" << target << "' without a battle";
			return;
		}
		if (nextRule->getContext() == CUSTOM_UI_BASESCAPE && !_base)
		{
			Log(LOG_ERROR) << "Custom UI '" << _rule->getId() << "' cannot open basescape UI '" << target << "' without a base";
			return;
		}
		State *next = new CustomUiState(nextRule, _battleGame, _base);
		if (replace) _game->popState();
		_game->pushState(next);
		return;
	}
	if (operation == NAV_NATIVE)
	{
		State *next = createNativeState(target);
		if (!next)
		{
			Log(LOG_ERROR) << "Custom UI '" << _rule->getId() << "' tried to open unavailable native UI '" << target << "'";
			return;
		}
		if (replace) _game->popState();
		_game->pushState(next);
	}
}

void CustomUiState::queueCustomUi(const std::string &target, bool replace)
{
	_pendingNavigation = NAV_CUSTOM;
	_pendingTarget = target;
	_pendingReplace = replace;
}

void CustomUiState::queueNativeUi(const std::string &target, bool replace)
{
	_pendingNavigation = NAV_NATIVE;
	_pendingTarget = target;
	_pendingReplace = replace;
}

State *CustomUiState::createNativeState(const std::string &target)
{
	auto registered = nativeFactories().find(target);
	if (registered != nativeFactories().end()) return registered->second(_game, _base, _battleGame);
	if (target == "ufopaedia") return new UfopaediaStartState;
	if (target == "techTree") return new TechTreeViewerState();
	if (!_base) return nullptr;
	if (target == "research") return new ResearchState(_base);
	if (target == "manufacture") return new ManufactureState(_base);
	if (target == "soldiers") return new SoldiersState(_base);
	if (target == "crafts") return new CraftsState(_base);
	if (target == "stores") return new StoresState(_base);
	return nullptr;
}

void CustomUiState::syncBoundWidgets()
{
	if (_syncing) return;
	_syncing = true;
	for (auto &entry : _labels)
	{
		const int color = resolveColor(entry.widget->colors.normal.text, entry.label->getColor());
		entry.label->setText(formatRichText(renderText(*entry.widget), color));
	}
	for (auto &entry : _scrollLabels)
	{
		const int color = resolveColor(entry.widget->colors.normal.text, entry.label->getColor());
		entry.label->clearList();
		entry.label->addRow(1, formatRichText(renderText(*entry.widget), color).c_str());
	}
	for (auto &entry : _edits)
	{
		const RuntimeValue *value = findValue(entry.widget->bind);
		if (!value) continue;
		std::string text = entry.number ? std::to_string(value->intValue) : value->stringValue;
		entry.placeholderShown = false;
		if (!entry.number && text.empty() && !entry.widget->placeholder.empty() && entry.edit != _focusedEdit)
		{
			text = static_cast<const std::string &>(tr(entry.widget->placeholder));
			entry.placeholderShown = true;
		}
		entry.edit->setText(text);
	}
	for (auto &entry : _toggles)
	{
		const RuntimeValue *value = findValue(entry.widget->bind);
		const bool selected = value && value->intValue != 0;
		entry.toggle->setPressed(selected);
		if (ButtonStyle *style = findStyle(entry.toggle)) style->selected = selected;
	}
	for (auto &entry : _combos)
	{
		const RuntimeValue *value = findValue(entry.widget->bind);
		if (!value) continue;
		size_t selected = 0;
		for (size_t i = 0; i < entry.widget->options.size(); ++i)
			if (entry.widget->options[i].value == value->stringValue) { selected = i; break; }
		entry.combo->setSelected(selected);
	}
	for (auto &style : _buttonStyles) applyButtonStyle(style);
	_syncing = false;
}

void CustomUiState::refreshLists(const std::string &widgetId)
{
	for (auto &list : _lists)
		if (widgetId.empty() || list.widget->id == widgetId) refreshList(list);
}

void CustomUiState::refreshList(ListBinding &binding)
{
	binding.list->clearList();
	binding.records = CustomUiDataProvider::getRecords(
		binding.widget->source,
		_game->getMod(),
		_game->getSavedGame(),
		_base,
		_game->getLanguage());

	std::string query;
	if (!binding.widget->search.empty())
	{
		auto edit = _editsById.find(binding.widget->search);
		if (edit != _editsById.end())
		{
			query = edit->second->getText();
			for (const auto &entry : _edits)
				if (entry.edit == edit->second && entry.placeholderShown) query.clear();
		}
	}
	if (!query.empty())
	{
		binding.records.erase(
			std::remove_if(binding.records.begin(), binding.records.end(), [&](const CustomUiDataRecord &record)
			{
				if (Unicode::caseFind(record.id, query)) return false;
				for (const auto &field : record.fields)
					if (Unicode::caseFind(field.second, query)) return false;
				return true;
			}),
			binding.records.end());
	}

	std::string sortField = binding.widget->sortBy;
	if (!binding.widget->sortValue.empty())
	{
		const RuntimeValue *sort = findValue(binding.widget->sortValue);
		if (sort) sortField = sort->stringValue;
	}
	bool descending = binding.sortDescending;
	if (!binding.widget->descendingValue.empty())
	{
		const RuntimeValue *value = findValue(binding.widget->descendingValue);
		descending = value && value->intValue != 0;
	}
	binding.sortDescending = descending;
	if (!sortField.empty())
	{
		std::stable_sort(binding.records.begin(), binding.records.end(), [&](const CustomUiDataRecord &left, const CustomUiDataRecord &right)
		{
			auto leftNumber = left.numericFields.find(sortField);
			auto rightNumber = right.numericFields.find(sortField);
			if (leftNumber != left.numericFields.end() && rightNumber != right.numericFields.end())
				return descending ? rightNumber->second < leftNumber->second : leftNumber->second < rightNumber->second;
			return descending
				? Unicode::caseCompare(right.get(sortField), left.get(sortField))
				: Unicode::caseCompare(left.get(sortField), right.get(sortField));
		});
	}

	for (const auto &record : binding.records) addListRow(binding.list, *binding.widget, record);
	updateSortHeaders(*binding.widget);
}

void CustomUiState::changeNumber(const CustomUiWidget &widget, int direction, bool large)
{
	RuntimeValue *value = findValue(widget.bind);
	if (!value) return;
	const int amount = large ? widget.largeStep : widget.step;
	long long next = static_cast<long long>(value->intValue) + static_cast<long long>(direction) * amount;
	if (next > widget.maximum) next = widget.wrap ? widget.minimum : widget.maximum;
	if (next < widget.minimum) next = widget.wrap ? widget.maximum : widget.minimum;
	value->intValue = static_cast<int>(next);
	syncBoundWidgets();
	refreshLists();
	dispatchAction(widget.id, widget.onChange);
}

std::vector<int> CustomUiState::getListColumnWidths(const CustomUiWidget &widget) const
{
	std::vector<int> widths;
	int totalWidth = 0;
	for (const auto &column : widget.columns)
	{
		widths.push_back(column.width);
		totalWidth += column.width;
	}
	if (_rule->getAutoScrollbars() && totalWidth > 0)
	{
		const int availableWidth = std::max(1, widget.width - 17);
		if (totalWidth > availableWidth)
		{
			int assigned = 0;
			for (size_t i = 0; i < widths.size(); ++i)
			{
				widths[i] = std::max(1, widths[i] * availableWidth / totalWidth);
				assigned += widths[i];
			}
			for (size_t i = 0; assigned < availableWidth; ++i, ++assigned)
				++widths[i % widths.size()];
		}
	}
	return widths;
}

void CustomUiState::setListColumns(TextList *list, const CustomUiWidget &widget)
{
	const auto &c = widget.columns;
	const std::vector<int> widths = getListColumnWidths(widget);
	switch (c.size())
	{
	case 1: list->setColumns(1, widths[0]); break;
	case 2: list->setColumns(2, widths[0], widths[1]); break;
	case 3: list->setColumns(3, widths[0], widths[1], widths[2]); break;
	case 4: list->setColumns(4, widths[0], widths[1], widths[2], widths[3]); break;
	case 5: list->setColumns(5, widths[0], widths[1], widths[2], widths[3], widths[4]); break;
	case 6: list->setColumns(6, widths[0], widths[1], widths[2], widths[3], widths[4], widths[5]); break;
	case 7: list->setColumns(7, widths[0], widths[1], widths[2], widths[3], widths[4], widths[5], widths[6]); break;
	case 8: list->setColumns(8, widths[0], widths[1], widths[2], widths[3], widths[4], widths[5], widths[6], widths[7]); break;
	default: break;
	}
	for (size_t i = 0; i < c.size(); ++i)
	{
		if (c[i].align == CUSTOM_UI_ALIGN_CENTER) list->setAlign(ALIGN_CENTER, static_cast<int>(i));
		else if (c[i].align == CUSTOM_UI_ALIGN_RIGHT) list->setAlign(ALIGN_RIGHT, static_cast<int>(i));
	}
}

void CustomUiState::addListRow(TextList *list, const CustomUiWidget &widget, const CustomUiDataRecord &record)
{
	std::vector<std::string> values;
	for (const auto &column : widget.columns) values.push_back(record.get(column.field));
	switch (values.size())
	{
	case 1: list->addRow(1, values[0].c_str()); break;
	case 2: list->addRow(2, values[0].c_str(), values[1].c_str()); break;
	case 3: list->addRow(3, values[0].c_str(), values[1].c_str(), values[2].c_str()); break;
	case 4: list->addRow(4, values[0].c_str(), values[1].c_str(), values[2].c_str(), values[3].c_str()); break;
	case 5: list->addRow(5, values[0].c_str(), values[1].c_str(), values[2].c_str(), values[3].c_str(), values[4].c_str()); break;
	case 6: list->addRow(6, values[0].c_str(), values[1].c_str(), values[2].c_str(), values[3].c_str(), values[4].c_str(), values[5].c_str()); break;
	case 7: list->addRow(7, values[0].c_str(), values[1].c_str(), values[2].c_str(), values[3].c_str(), values[4].c_str(), values[5].c_str(), values[6].c_str()); break;
	case 8: list->addRow(8, values[0].c_str(), values[1].c_str(), values[2].c_str(), values[3].c_str(), values[4].c_str(), values[5].c_str(), values[6].c_str(), values[7].c_str()); break;
	default: break;
	}
}

void CustomUiState::updateSortHeaders(const CustomUiWidget &widget)
{
	std::string sortField = widget.sortBy;
	if (!widget.sortValue.empty())
	{
		const RuntimeValue *sort = findValue(widget.sortValue);
		if (sort) sortField = sort->stringValue;
	}
	bool descending = false;
	for (const auto &list : _lists)
	{
		if (list.widget != &widget) continue;
		descending = list.sortDescending;
		break;
	}
	if (!widget.descendingValue.empty())
	{
		const RuntimeValue *value = findValue(widget.descendingValue);
		descending = value && value->intValue != 0;
	}
	for (auto &header : _sortHeaders)
	{
		if (header.widget != &widget || !header.arrow) continue;
		header.arrow->setShape(header.field == sortField
			? (descending ? ARROW_SMALL_DOWN : ARROW_SMALL_UP)
			: ARROW_NONE);
	}
}

void CustomUiState::buttonClick(Action *action)
{
	activateButton(dynamic_cast<TextButton *>(action->getSender()));
}

void CustomUiState::buttonPress(Action *action)
{
	if (_focusedEdit)
	{
		_focusedEdit->setFocus(false);
		_focusedEdit = nullptr;
		syncBoundWidgets();
	}
	focusButton(dynamic_cast<TextButton *>(action->getSender()));
}

void CustomUiState::buttonIn(Action *action)
{
	_hoveredButton = dynamic_cast<TextButton *>(action->getSender());
	if (ButtonStyle *style = findStyle(_hoveredButton)) applyButtonStyle(*style);
}

void CustomUiState::buttonOut(Action *action)
{
	TextButton *button = dynamic_cast<TextButton *>(action->getSender());
	if (_hoveredButton == button) _hoveredButton = nullptr;
	if (ButtonStyle *style = findStyle(button)) applyButtonStyle(*style);
}

void CustomUiState::toggleClick(Action *action)
{
	ToggleTextButton *toggle = dynamic_cast<ToggleTextButton *>(action->getSender());
	const CustomUiWidget *widget = findWidget(toggle);
	if (!toggle || !widget) return;
	RuntimeValue *value = findValue(widget->bind);
	if (value) value->intValue = toggle->getPressed() ? 1 : 0;
	if (ButtonStyle *style = findStyle(toggle))
	{
		style->selected = toggle->getPressed();
		applyButtonStyle(*style);
	}
	refreshLists();
	dispatchAction(widget->id, widget->onChange.empty() ? widget->action : widget->onChange);
}

void CustomUiState::editChange(Action *action)
{
	if (_syncing) return;
	TextEdit *edit = dynamic_cast<TextEdit *>(action->getSender());
	const CustomUiWidget *widget = findWidget(edit);
	if (!edit || !widget) return;
	RuntimeValue *value = findValue(widget->bind);
	if (!value) return;
	for (auto &entry : _edits)
		if (entry.edit == edit) entry.placeholderShown = false;
	if (widget->type == CUSTOM_UI_NUMBER)
	{
		value->intValue = std::atoi(edit->getText().c_str());
	}
	else
	{
		std::string text = edit->getText();
		if (widget->maxLength > 0)
		{
			UString unicode = Unicode::convUtf8ToUtf32(text);
			if (unicode.size() > static_cast<size_t>(widget->maxLength))
			{
				unicode.resize(widget->maxLength);
				text = Unicode::convUtf32ToUtf8(unicode);
				_syncing = true;
				edit->setText(text);
				_syncing = false;
			}
		}
		value->stringValue = text;
	}
	for (auto &entry : _labels)
	{
		const int color = resolveColor(entry.widget->colors.normal.text, entry.label->getColor());
		entry.label->setText(formatRichText(renderText(*entry.widget), color));
	}
	for (auto &entry : _scrollLabels)
	{
		const int color = resolveColor(entry.widget->colors.normal.text, entry.label->getColor());
		entry.label->clearList();
		entry.label->addRow(1, formatRichText(renderText(*entry.widget), color).c_str());
	}
	refreshLists();
	dispatchAction(widget->id, widget->onChange);
}

void CustomUiState::editSubmit(Action *action)
{
	TextEdit *edit = dynamic_cast<TextEdit *>(action->getSender());
	const CustomUiWidget *widget = findWidget(edit);
	if (!edit || !widget) return;
	if (widget->type == CUSTOM_UI_NUMBER)
	{
		RuntimeValue *value = findValue(widget->bind);
		if (value)
		{
			value->intValue = std::max(widget->minimum, std::min(widget->maximum, value->intValue));
			syncBoundWidgets();
		}
	}
	dispatchAction(widget->id, widget->onSubmit);
}

void CustomUiState::editPress(Action *action)
{
	TextEdit *edit = dynamic_cast<TextEdit *>(action->getSender());
	if (!edit) return;
	if (_focusedEdit && _focusedEdit != edit) _focusedEdit->setFocus(false);
	_focusedEdit = edit;
	for (auto &entry : _edits)
	{
		if (entry.edit == edit && entry.placeholderShown)
		{
			entry.placeholderShown = false;
			_syncing = true;
			edit->setText("");
			_syncing = false;
			break;
		}
	}
	syncBoundWidgets();
}

void CustomUiState::numberUp(Action *action)
{
	if (const CustomUiWidget *widget = findWidget(dynamic_cast<InteractiveSurface *>(action->getSender())))
		changeNumber(*widget, 1, _game->isShiftPressed() || _game->isCtrlPressed());
}

void CustomUiState::numberDown(Action *action)
{
	if (const CustomUiWidget *widget = findWidget(dynamic_cast<InteractiveSurface *>(action->getSender())))
		changeNumber(*widget, -1, _game->isShiftPressed() || _game->isCtrlPressed());
}

void CustomUiState::numberWheel(Action *action)
{
	const CustomUiWidget *widget = findWidget(dynamic_cast<InteractiveSurface *>(action->getSender()));
	if (!widget) return;
	const int direction = action->getDetails()->button.button == SDL_BUTTON_WHEELUP ? 1 : -1;
	changeNumber(*widget, direction, _game->isShiftPressed() || _game->isCtrlPressed());
}

void CustomUiState::comboChange(Action *action)
{
	if (_syncing) return;
	ComboBox *combo = dynamic_cast<ComboBox *>(action->getSender());
	const CustomUiWidget *widget = findWidget(combo);
	if (!combo || !widget || combo->getSelected() >= widget->options.size()) return;
	RuntimeValue *value = findValue(widget->bind);
	if (value) value->stringValue = widget->options[combo->getSelected()].value;
	syncBoundWidgets();
	refreshLists();
	dispatchAction(widget->id, widget->onChange);
}

void CustomUiState::sortHeaderClick(Action *action)
{
	ArrowButton *arrow = dynamic_cast<ArrowButton *>(action->getSender());
	if (!arrow) return;
	for (const auto &header : _sortHeaders)
	{
		if (header.arrow != arrow) continue;
		RuntimeValue *sort = findValue(header.widget->sortValue);
		if (!sort) return;
		ListBinding *binding = nullptr;
		for (auto &list : _lists)
		{
			if (list.widget == header.widget)
			{
				binding = &list;
				break;
			}
		}
		if (!binding) return;

		const bool sameColumn = sort->stringValue == header.field;
		sort->stringValue = header.field;
		RuntimeValue *descending = header.widget->descendingValue.empty()
			? nullptr
			: findValue(header.widget->descendingValue);
		if (descending)
		{
			descending->intValue = sameColumn && !descending->intValue;
			binding->sortDescending = descending->intValue != 0;
		}
		else
		{
			binding->sortDescending = sameColumn && !binding->sortDescending;
		}
		refreshList(*binding);
		dispatchAction(header.widget->id, header.widget->onChange);
		return;
	}
}

void CustomUiState::listClick(Action *action)
{
	TextList *list = dynamic_cast<TextList *>(action->getSender());
	if (!list) return;
	for (auto &entry : _lists)
	{
		if (entry.list != list) continue;
		const size_t row = list->getSelectedRow();
		if (row >= entry.records.size()) return;
		_selectedRecordId = entry.records[row].id;
		if (!entry.widget->selectedValue.empty())
		{
			RuntimeValue *value = findValue(entry.widget->selectedValue);
			if (value) value->stringValue = _selectedRecordId;
			syncBoundWidgets();
		}
		dispatchAction(entry.widget->id, entry.widget->onSelect);
		return;
	}
}

void CustomUiState::handle(Action *action)
{
	if (action->getDetails()->type == SDL_KEYDOWN)
	{
		const SDLKey key = action->getDetails()->key.keysym.sym;
		if (_focusedEdit)
		{
			if (key == SDLK_TAB)
			{
				_focusedEdit->setFocus(false);
				_focusedEdit = nullptr;
				syncBoundWidgets();
				moveFocus((action->getDetails()->key.keysym.mod & KMOD_SHIFT) ? -1 : 1);
				return;
			}
			if (key == Options::keyCancel)
			{
				_game->popState();
				return;
			}
			State::handle(action);
			return;
		}
		if (key == SDLK_TAB)
		{
			moveFocus((action->getDetails()->key.keysym.mod & KMOD_SHIFT) ? -1 : 1);
			return;
		}
		if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE)
		{
			activateButton(_focusedButton);
			return;
		}
		if (key == Options::keyCancel)
		{
			_game->popState();
			return;
		}
	}
	State::handle(action);
}

void CustomUiState::blit()
{
	ComboBox *openCombo = dynamic_cast<ComboBox *>(_modal);
	SDL_Surface *screen = _game->getScreen()->getSurface();
	for (auto *surface : _surfaces)
	{
		if (surface != openCombo) surface->blit(screen);
	}
	if (openCombo) openCombo->blit(screen);
}

bool CustomUiState::registerNativeScreen(const std::string &id, NativeScreenFactory factory)
{
	if (id.empty() || !factory) return false;
	return nativeFactories().insert(std::make_pair(id, factory)).second;
}

int CustomUiState::getIntScript(const std::string &id) const
{
	const RuntimeValue *value = findValue(id);
	return value && value->type != CUSTOM_UI_VALUE_STRING ? value->intValue : 0;
}

void CustomUiState::getTextScript(const std::string &id, ScriptText &result) const
{
	const RuntimeValue *value = findValue(id);
	result = value && value->type == CUSTOM_UI_VALUE_STRING
		? ScriptText { value->stringValue.c_str() }
		: ScriptText::empty;
}

void CustomUiState::setIntScript(const std::string &id, int value)
{
	RuntimeValue *runtime = findValue(id);
	if (!runtime || runtime->type == CUSTOM_UI_VALUE_STRING) return;
	runtime->intValue = runtime->type == CUSTOM_UI_VALUE_BOOL ? (value ? 1 : 0) : value;
	syncBoundWidgets();
	refreshLists();
}

void CustomUiState::setTextScript(const std::string &id, const std::string &value)
{
	RuntimeValue *runtime = findValue(id);
	if (!runtime || runtime->type != CUSTOM_UI_VALUE_STRING) return;
	runtime->stringValue = value;
	syncBoundWidgets();
	refreshLists();
}

void CustomUiState::setWidgetTextScript(const std::string &widgetId, const std::string &text)
{
	_widgetTextOverrides[widgetId] = text;
	syncBoundWidgets();
}

void CustomUiState::refreshScript(const std::string &widgetId)
{
	refreshLists(widgetId);
}

void CustomUiState::closeScript()
{
	_pendingNavigation = NAV_CLOSE;
}

void CustomUiState::openCustomUiScript(const std::string &screenId, int replace)
{
	queueCustomUi(screenId, replace != 0);
}

void CustomUiState::openNativeUiScript(const std::string &screenId, int replace)
{
	queueNativeUi(screenId, replace != 0);
}

void CustomUiState::ScriptRegister(ScriptParserBase *parser)
{
	parser->registerPointerType<Mod>();
	parser->registerPointerType<SavedGame>();
	parser->registerPointerType<SavedBattleGame>();

	Bind<CustomUiState> ui = { parser, "ui" };
	ui.add<&CustomUiState::getIntScript>("getInt", "read a screen-local int or bool value");
	ui.add<&getTextScriptBridge>("getText", "read a screen-local string value");
	ui.add<&CustomUiState::setIntScript>("setInt", "assign a screen-local int or bool value");
	ui.add<&CustomUiState::setTextScript>("setText", "assign a screen-local string value");
	ui.add<&CustomUiState::setWidgetTextScript>("setWidgetText", "replace a label or button caption for this screen instance");
	ui.add<&CustomUiState::refreshScript>("refresh", "refresh one table by widget id, or every table for empty text");
	ui.add<&CustomUiState::closeScript>("close", "close the custom UI after this action returns");
	ui.add<&CustomUiState::openCustomUiScript>("open", "open a custom UI id; non-zero second argument replaces this screen");
	ui.add<&CustomUiState::openNativeUiScript>("openNative", "open a registered native UI id; non-zero second argument replaces this screen");
}

ModScript::CustomUiActionParser::CustomUiActionParser(ScriptGlobal *shared, const std::string &name, Mod *mod) :
	ScriptParserEvents { shared, name,
		"ui", "screen_id", "action_id", "widget_id", "selected_record_id", "geoscape_game", "battle_game" }
{
	BindBase bind { this };
	bind.addCustomPtr<const Mod>("rules", mod);
}

}
