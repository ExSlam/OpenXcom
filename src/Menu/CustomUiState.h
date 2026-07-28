#pragma once
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
#include <map>
#include <string>
#include <vector>
#include "../Engine/State.h"
#include "CustomUiDataProvider.h"

namespace OpenXcom
{

class ArrowButton;
class Base;
class ComboBox;
class Game;
class InteractiveSurface;
class RuleCustomUi;
class SavedBattleGame;
class SavedGame;
class ScriptParserBase;
struct ScriptText;
class Text;
class TextButton;
class TextEdit;
class TextList;
class ToggleTextButton;
class Window;
struct CustomUiWidget;

/**
 * Generic state built from a RuleCustomUi definition.
 */
class CustomUiState : public State
{
public:
	static constexpr const char *ScriptName = "CustomUi";
	/// Factory boundary for native/engine-added screens.
	using NativeScreenFactory = State *(*)(Game *, Base *, SavedBattleGame *);

private:
	struct RuntimeValue
	{
		int type;
		int intValue;
		std::string stringValue;
	};

	struct ButtonStyle
	{
		TextButton *button;
		const CustomUiWidget *widget;
		int normalBackground;
		int normalText;
		int hoverBackground;
		int hoverText;
		int focusedBackground;
		int focusedText;
		int selectedBackground;
		int selectedText;
		bool selected;
	};

	struct LabelBinding
	{
		Text *label;
		const CustomUiWidget *widget;
	};

	struct ScrollLabelBinding
	{
		TextList *label;
		const CustomUiWidget *widget;
	};

	struct EditBinding
	{
		TextEdit *edit;
		const CustomUiWidget *widget;
		bool number;
		bool placeholderShown;
	};

	struct ToggleBinding
	{
		ToggleTextButton *toggle;
		const CustomUiWidget *widget;
	};

	struct ComboBinding
	{
		ComboBox *combo;
		const CustomUiWidget *widget;
	};

	struct NumberBinding
	{
		TextEdit *edit;
		ArrowButton *up;
		ArrowButton *down;
		const CustomUiWidget *widget;
	};

	struct ListBinding
	{
		TextList *list;
		const CustomUiWidget *widget;
		std::vector<CustomUiDataRecord> records;
		bool sortDescending;
	};

	struct SortHeaderBinding
	{
		ArrowButton *arrow;
		Text *label;
		const CustomUiWidget *widget;
		std::string field;
	};

	enum PendingNavigation
	{
		NAV_NONE,
		NAV_CLOSE,
		NAV_CUSTOM,
		NAV_NATIVE
	};

	const RuleCustomUi *_rule;
	SavedBattleGame *_battleGame;
	Base *_base;
	Window *_window;
	std::map<std::string, RuntimeValue> _values;
	std::map<std::string, std::string> _widgetTextOverrides;
	std::map<InteractiveSurface *, std::pair<std::string, std::string> > _actions;
	std::map<InteractiveSurface *, const CustomUiWidget *> _surfaceWidgets;
	std::map<std::string, TextEdit *> _editsById;
	std::vector<ButtonStyle> _buttonStyles;
	std::vector<LabelBinding> _labels;
	std::vector<ScrollLabelBinding> _scrollLabels;
	std::vector<EditBinding> _edits;
	std::vector<ToggleBinding> _toggles;
	std::vector<ComboBinding> _combos;
	std::vector<NumberBinding> _numbers;
	std::vector<ListBinding> _lists;
	std::vector<SortHeaderBinding> _sortHeaders;
	TextButton *_focusedButton;
	TextButton *_hoveredButton;
	TextEdit *_focusedEdit;
	std::string _selectedRecordId;
	PendingNavigation _pendingNavigation;
	std::string _pendingTarget;
	bool _pendingReplace;
	bool _syncing;

	int resolveColor(const std::string &value, int fallback) const;
	std::string formatRichText(const std::string &text, int defaultColor) const;
	std::string renderText(const CustomUiWidget &widget) const;
	std::string getValueText(const std::string &id) const;
	RuntimeValue *findValue(const std::string &id);
	const RuntimeValue *findValue(const std::string &id) const;
	ButtonStyle *findStyle(TextButton *button);
	const CustomUiWidget *findWidget(InteractiveSurface *surface) const;
	void applyButtonStyle(ButtonStyle &style);
	void focusButton(TextButton *button);
	void moveFocus(int direction);
	void activateButton(TextButton *button);
	void dispatchAction(const std::string &widgetId, const std::string &actionId);
	void executeScriptAction(const std::string &widgetId, const std::string &actionId);
	void applyPendingNavigation();
	void queueCustomUi(const std::string &target, bool replace);
	void queueNativeUi(const std::string &target, bool replace);
	State *createNativeState(const std::string &target);
	void syncBoundWidgets();
	void refreshLists(const std::string &widgetId = std::string());
	void refreshList(ListBinding &binding);
	void changeNumber(const CustomUiWidget &widget, int direction, bool large);
	std::vector<int> getListColumnWidths(const CustomUiWidget &widget) const;
	void setListColumns(TextList *list, const CustomUiWidget &widget);
	void addListRow(TextList *list, const CustomUiWidget &widget, const CustomUiDataRecord &record);
	void updateSortHeaders(const CustomUiWidget &widget);

	void buttonClick(Action *action);
	void buttonPress(Action *action);
	void buttonIn(Action *action);
	void buttonOut(Action *action);
	void toggleClick(Action *action);
	void editChange(Action *action);
	void editSubmit(Action *action);
	void editPress(Action *action);
	void numberUp(Action *action);
	void numberDown(Action *action);
	void numberWheel(Action *action);
	void comboChange(Action *action);
	void sortHeaderClick(Action *action);
	void listClick(Action *action);
public:
	/// Creates a state from a declarative custom UI.
	explicit CustomUiState(const RuleCustomUi *rule, SavedBattleGame *battleGame = nullptr, Base *base = nullptr);
	/// Cleans up the state.
	~CustomUiState() = default;
	/// Handles focus traversal and keyboard activation.
	void handle(Action *action) override;
	/// Draws an open dropdown after every ordinary widget.
	void blit() override;

	/// Registers a native screen factory for openNativeUi actions.
	static bool registerNativeScreen(const std::string &id, NativeScreenFactory factory);
	/// Registers the controlled functions available to customUiAction scripts.
	static void ScriptRegister(ScriptParserBase *parser);

	/// Script API: read an int or bool local value.
	int getIntScript(const std::string &id) const;
	/// Script API: read a string local value.
	void getTextScript(const std::string &id, ScriptText &result) const;
	/// Script API: assign an int or bool local value.
	void setIntScript(const std::string &id, int value);
	/// Script API: assign a string local value.
	void setTextScript(const std::string &id, const std::string &value);
	/// Script API: replace a label/button caption for this state instance.
	void setWidgetTextScript(const std::string &widgetId, const std::string &text);
	/// Script API: refresh one table, or every table when widgetId is empty.
	void refreshScript(const std::string &widgetId);
	/// Script API: close after the action script returns.
	void closeScript();
	/// Script API: open another schema screen after the action script returns.
	void openCustomUiScript(const std::string &screenId, int replace);
	/// Script API: open a registered native screen after the action script returns.
	void openNativeUiScript(const std::string &screenId, int replace);
};

}
