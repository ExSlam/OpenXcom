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
#include "../Engine/Yaml.h"
#include "ModScript.h"

namespace OpenXcom
{

enum CustomUiContext
{
	CUSTOM_UI_GEOSCAPE,
	CUSTOM_UI_BASESCAPE,
	CUSTOM_UI_BATTLESCAPE
};

enum CustomUiAnchor
{
	CUSTOM_UI_ANCHOR_TOP_LEFT,
	CUSTOM_UI_ANCHOR_TOP,
	CUSTOM_UI_ANCHOR_TOP_RIGHT,
	CUSTOM_UI_ANCHOR_LEFT,
	CUSTOM_UI_ANCHOR_CENTER,
	CUSTOM_UI_ANCHOR_RIGHT,
	CUSTOM_UI_ANCHOR_BOTTOM_LEFT,
	CUSTOM_UI_ANCHOR_BOTTOM,
	CUSTOM_UI_ANCHOR_BOTTOM_RIGHT
};

enum CustomUiWidgetType
{
	CUSTOM_UI_LABEL,
	CUSTOM_UI_BUTTON,
	CUSTOM_UI_TOGGLE,
	CUSTOM_UI_TEXT_INPUT,
	CUSTOM_UI_NUMBER,
	CUSTOM_UI_DROPDOWN,
	CUSTOM_UI_SEARCH,
	CUSTOM_UI_LIST
};

enum CustomUiTextAlign
{
	CUSTOM_UI_ALIGN_LEFT,
	CUSTOM_UI_ALIGN_CENTER,
	CUSTOM_UI_ALIGN_RIGHT
};

enum CustomUiValueType
{
	CUSTOM_UI_VALUE_BOOL,
	CUSTOM_UI_VALUE_INT,
	CUSTOM_UI_VALUE_STRING
};

enum CustomUiActionType
{
	CUSTOM_UI_ACTION_CLOSE,
	CUSTOM_UI_ACTION_OPEN_CUSTOM_UI,
	CUSTOM_UI_ACTION_OPEN_NATIVE_UI,
	CUSTOM_UI_ACTION_SCRIPT
};

struct CustomUiWidgetColorState
{
	/// Palette index ("42") or RGB color ("#ffcc00"); empty inherits.
	std::string background;
	/// Palette index ("42") or RGB color ("#ffcc00"); empty inherits.
	std::string text;
};

struct CustomUiWidgetColors
{
	CustomUiWidgetColorState normal;
	CustomUiWidgetColorState hover;
	CustomUiWidgetColorState focused;
	CustomUiWidgetColorState selected;
};

struct CustomUiValueDefinition
{
	CustomUiValueType type = CUSTOM_UI_VALUE_STRING;
	bool boolDefault = false;
	int intDefault = 0;
	std::string stringDefault;
};

struct CustomUiOption
{
	std::string text;
	std::string value;
};

struct CustomUiColumn
{
	std::string field;
	/// Optional localized column heading. Enables a native sort arrow when the table has sortValue.
	std::string header;
	int width = 0;
	CustomUiTextAlign align = CUSTOM_UI_ALIGN_LEFT;
};

struct CustomUiDataSource
{
	std::string type;
	/// "currentBase" or "all"; providers choose a useful context default.
	std::string scope;
	bool includeZero = false;
};

struct CustomUiActionDefinition
{
	CustomUiActionType type = CUSTOM_UI_ACTION_SCRIPT;
	std::string target;
	bool replace = false;
};

struct CustomUiWidget
{
	std::string id;
	CustomUiWidgetType type = CUSTOM_UI_LABEL;
	std::string text;
	/// Button click action. Kept as "action" for schema 0.1 compatibility.
	std::string action;
	std::string onChange;
	std::string onSubmit;
	std::string onSelect;
	std::string bind;
	std::string placeholder;
	int maxLength = 0;
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
	bool big = false;
	bool wordWrap = false;
	/// Text overflow policy. Horizontal currently falls back to vertical wrapping.
	bool verticalOverflow = true;
	/// True when the ruleset explicitly declares overflow.
	bool overflowExplicit = false;
	/// Automatically provide a contained vertical scrollbar for an overflowing label.
	bool autoScrollbar = false;
	bool selected = false;
	bool focused = false;
	CustomUiTextAlign align = CUSTOM_UI_ALIGN_LEFT;
	CustomUiWidgetColors colors;

	/// Numeric input configuration.
	int minimum = 0;
	int maximum = 100;
	int step = 1;
	int largeStep = 10;
	bool wrap = false;
	bool mouseWheel = true;

	/// Dropdown configuration.
	std::vector<CustomUiOption> options;

	/// List/table configuration.
	CustomUiDataSource source;
	std::string search;
	std::string selectedValue;
	std::string sortBy;
	std::string sortValue;
	std::string descendingValue;
	bool selectable = true;
	std::vector<CustomUiColumn> columns;
};

/**
 * Declarative definition of a mod-provided user interface.
 */
class RuleCustomUi
{
private:
	std::string _id;
	std::string _title;
	std::string _interface;
	std::string _backgroundImage;
	CustomUiContext _context;
	CustomUiAnchor _anchor;
	int _offsetX;
	int _offsetY;
	int _width;
	int _height;
	bool _autoScrollbars;
	std::map<std::string, CustomUiValueDefinition> _values;
	std::map<std::string, CustomUiActionDefinition> _actions;
	std::vector<CustomUiWidget> _widgets;
	ModScript::CustomUiScripts::Container _customUiScripts;

	void validate() const;
public:
	/// Creates an empty custom UI rule.
	explicit RuleCustomUi(const std::string &id);
	/// Loads the custom UI from YAML.
	void load(const YAML::YamlNodeReader &reader, const ModScript &parsers);
	/// Gets the stable screen identifier.
	const std::string &getId() const { return _id; }
	/// Gets the localized screen title identifier.
	const std::string &getTitle() const { return _title; }
	/// Gets the interface theme used to style the screen.
	const std::string &getInterface() const { return _interface; }
	/// Gets an optional mod-provided background surface.
	const std::string &getBackgroundImage() const { return _backgroundImage; }
	/// Gets the game context in which the screen may be opened.
	CustomUiContext getContext() const { return _context; }
	/// Gets the screen anchor.
	CustomUiAnchor getAnchor() const { return _anchor; }
	/// Gets the horizontal offset from the anchor.
	int getOffsetX() const { return _offsetX; }
	/// Gets the vertical offset from the anchor.
	int getOffsetY() const { return _offsetY; }
	/// Gets the window width.
	int getWidth() const { return _width; }
	/// Gets the window height.
	int getHeight() const { return _height; }
	/// Gets whether overflowing content uses automatically placed scrollbars.
	bool getAutoScrollbars() const { return _autoScrollbars; }
	/// Gets the local value declarations.
	const std::map<std::string, CustomUiValueDefinition> &getValues() const { return _values; }
	/// Gets a declared action, or null if it is unknown.
	const CustomUiActionDefinition *getAction(const std::string &id) const;
	/// Gets the widgets in declaration order.
	const std::vector<CustomUiWidget> &getWidgets() const { return _widgets; }
	/// Gets this screen's custom UI action script.
	template<typename Script>
	const typename Script::Container &getScript() const { return _customUiScripts.get<Script>(); }
};

}
