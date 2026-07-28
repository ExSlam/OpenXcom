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
#include "RuleCustomUi.h"
#include <cctype>
#include <climits>
#include <set>
#include "../Engine/Exception.h"

namespace OpenXcom
{

namespace
{

CustomUiContext readContext(const std::string &value, const std::string &screenId)
{
	if (value == "geoscape") return CUSTOM_UI_GEOSCAPE;
	if (value == "basescape") return CUSTOM_UI_BASESCAPE;
	if (value == "battlescape") return CUSTOM_UI_BATTLESCAPE;
	throw Exception("Custom UI '" + screenId + "' has unsupported context '" + value + "'");
}

CustomUiAnchor readAnchor(const std::string &value, const std::string &screenId)
{
	if (value == "topLeft") return CUSTOM_UI_ANCHOR_TOP_LEFT;
	if (value == "top") return CUSTOM_UI_ANCHOR_TOP;
	if (value == "topRight") return CUSTOM_UI_ANCHOR_TOP_RIGHT;
	if (value == "left") return CUSTOM_UI_ANCHOR_LEFT;
	if (value == "center") return CUSTOM_UI_ANCHOR_CENTER;
	if (value == "right") return CUSTOM_UI_ANCHOR_RIGHT;
	if (value == "bottomLeft") return CUSTOM_UI_ANCHOR_BOTTOM_LEFT;
	if (value == "bottom") return CUSTOM_UI_ANCHOR_BOTTOM;
	if (value == "bottomRight") return CUSTOM_UI_ANCHOR_BOTTOM_RIGHT;
	throw Exception("Custom UI '" + screenId + "' has unsupported anchor '" + value + "'");
}

CustomUiWidgetType readWidgetType(const std::string &value, const std::string &screenId)
{
	if (value == "label") return CUSTOM_UI_LABEL;
	if (value == "button") return CUSTOM_UI_BUTTON;
	if (value == "toggle") return CUSTOM_UI_TOGGLE;
	if (value == "textInput") return CUSTOM_UI_TEXT_INPUT;
	if (value == "number") return CUSTOM_UI_NUMBER;
	if (value == "dropdown") return CUSTOM_UI_DROPDOWN;
	if (value == "search") return CUSTOM_UI_SEARCH;
	if (value == "list" || value == "table") return CUSTOM_UI_LIST;
	throw Exception("Custom UI '" + screenId + "' has unsupported widget type '" + value + "'");
}

CustomUiTextAlign readTextAlign(const std::string &value, const std::string &screenId, const std::string &widgetId)
{
	if (value == "left") return CUSTOM_UI_ALIGN_LEFT;
	if (value == "center") return CUSTOM_UI_ALIGN_CENTER;
	if (value == "right") return CUSTOM_UI_ALIGN_RIGHT;
	throw Exception("Custom UI '" + screenId + "' widget '" + widgetId + "' has unsupported alignment '" + value + "'");
}

CustomUiValueType readValueType(const std::string &value, const std::string &screenId, const std::string &valueId)
{
	if (value == "bool") return CUSTOM_UI_VALUE_BOOL;
	if (value == "int") return CUSTOM_UI_VALUE_INT;
	if (value == "string") return CUSTOM_UI_VALUE_STRING;
	throw Exception("Custom UI '" + screenId + "' value '" + valueId + "' has unsupported type '" + value + "'");
}

CustomUiActionType readActionType(const std::string &value, const std::string &screenId, const std::string &actionId)
{
	if (value == "close") return CUSTOM_UI_ACTION_CLOSE;
	if (value == "openCustomUi") return CUSTOM_UI_ACTION_OPEN_CUSTOM_UI;
	if (value == "openNativeUi") return CUSTOM_UI_ACTION_OPEN_NATIVE_UI;
	if (value == "script") return CUSTOM_UI_ACTION_SCRIPT;
	throw Exception("Custom UI '" + screenId + "' action '" + actionId + "' has unsupported type '" + value + "'");
}

void readColorState(const YAML::YamlNodeReader &reader, CustomUiWidgetColorState &state)
{
	reader.tryRead("background", state.background);
	reader.tryRead("text", state.text);
}

bool isValidColorReference(const std::string &value)
{
	if (value.empty()) return true;
	if (value.size() == 7 && value[0] == '#')
	{
		for (size_t i = 1; i < value.size(); ++i)
		{
			if (!std::isxdigit(static_cast<unsigned char>(value[i]))) return false;
		}
		return true;
	}
	for (char c : value)
	{
		if (!std::isdigit(static_cast<unsigned char>(c))) return false;
	}
	try
	{
		const int index = std::stoi(value);
		return index >= 1 && index <= 255;
	}
	catch (...)
	{
		return false;
	}
}

void validateColorState(const CustomUiWidgetColorState &state, const std::string &screenId, const std::string &widgetId, const std::string &stateName)
{
	if (!isValidColorReference(state.background))
		throw Exception("Custom UI '" + screenId + "' widget '" + widgetId + "' has invalid " + stateName + " background color '" + state.background + "'");
	if (!isValidColorReference(state.text))
		throw Exception("Custom UI '" + screenId + "' widget '" + widgetId + "' has invalid " + stateName + " text color '" + state.text + "'");
}

bool requiresText(CustomUiWidgetType type)
{
	return type == CUSTOM_UI_LABEL || type == CUSTOM_UI_BUTTON || type == CUSTOM_UI_TOGGLE;
}

}

RuleCustomUi::RuleCustomUi(const std::string &id) :
	_id(id),
	_title(id),
	_interface("geoscape"),
	_context(CUSTOM_UI_GEOSCAPE),
	_anchor(CUSTOM_UI_ANCHOR_CENTER),
	_offsetX(0),
	_offsetY(0),
	_width(256),
	_height(180),
	_autoScrollbars(false)
{
}

void RuleCustomUi::load(const YAML::YamlNodeReader &reader, const ModScript &parsers)
{
	if (const auto &parent = reader["refNode"])
	{
		load(parent, parsers);
	}

	reader.tryRead("title", _title);
	reader.tryRead("interface", _interface);
	reader.tryRead("backgroundImage", _backgroundImage);
	reader.tryRead("width", _width);
	reader.tryRead("height", _height);
	reader.tryRead("autoScrollbars", _autoScrollbars);

	std::string context = "geoscape";
	if (reader.tryRead("context", context)) _context = readContext(context, _id);

	if (const auto &position = reader["position"])
	{
		std::string anchor = "center";
		if (position.tryRead("anchor", anchor)) _anchor = readAnchor(anchor, _id);
		position.tryRead("x", _offsetX);
		position.tryRead("y", _offsetY);
	}

	if (const auto &values = reader["values"])
	{
		_values.clear();
		for (const auto &valueReader : values.children())
		{
			const std::string valueId = valueReader.readKey<std::string>();
			CustomUiValueDefinition value;
			value.type = readValueType(valueReader["type"].readVal<std::string>(), _id, valueId);
			if (value.type == CUSTOM_UI_VALUE_BOOL)
				valueReader.tryRead("default", value.boolDefault);
			else if (value.type == CUSTOM_UI_VALUE_INT)
				valueReader.tryRead("default", value.intDefault);
			else
				valueReader.tryRead("default", value.stringDefault);
			_values[valueId] = value;
		}
	}

	if (const auto &actions = reader["actions"])
	{
		_actions.clear();
		for (const auto &actionReader : actions.children())
		{
			const std::string actionId = actionReader.readKey<std::string>();
			CustomUiActionDefinition action;
			action.type = readActionType(actionReader["type"].readVal<std::string>("script"), _id, actionId);
			actionReader.tryRead("target", action.target);
			std::string mode = "push";
			actionReader.tryRead("mode", mode);
			if (mode != "push" && mode != "replace")
				throw Exception("Custom UI '" + _id + "' action '" + actionId + "' has unsupported mode '" + mode + "'");
			action.replace = mode == "replace";
			_actions[actionId] = action;
		}
	}

	if (const auto &widgets = reader["widgets"])
	{
		_widgets.clear();
		for (const auto &widgetReader : widgets.children())
		{
			CustomUiWidget widget;
			widget.id = widgetReader["id"].readVal<std::string>();
			widget.type = readWidgetType(widgetReader["type"].readVal<std::string>(), _id);
			widgetReader.tryRead("text", widget.text);
			widgetReader.tryRead("action", widget.action);
			widgetReader.tryRead("onClick", widget.action);
			widgetReader.tryRead("onChange", widget.onChange);
			widgetReader.tryRead("onSubmit", widget.onSubmit);
			widgetReader.tryRead("onSelect", widget.onSelect);
			widgetReader.tryRead("bind", widget.bind);
			widgetReader.tryRead("placeholder", widget.placeholder);
			widgetReader.tryRead("maxLength", widget.maxLength);
			widgetReader.tryRead("x", widget.x);
			widgetReader.tryRead("y", widget.y);
			widgetReader.tryRead("width", widget.width);
			widgetReader.tryRead("height", widget.height);
			widgetReader.tryRead("big", widget.big);
			widgetReader.tryRead("wordWrap", widget.wordWrap);
			std::string overflow = "vertical";
			if (widgetReader.tryRead("overflow", overflow))
			{
				widget.overflowExplicit = true;
				if (overflow != "vertical" && overflow != "horizontal")
					throw Exception("Custom UI '" + _id + "' widget '" + widget.id + "' has unsupported overflow policy '" + overflow + "'");
				// OXCE has no horizontal text viewport/scrollbar. Use the
				// documented vertical fallback for either requested policy.
				widget.verticalOverflow = true;
			}
			widgetReader.tryRead("autoScrollbar", widget.autoScrollbar);
			widgetReader.tryRead("selected", widget.selected);
			widgetReader.tryRead("focused", widget.focused);

			widgetReader.tryRead("minimum", widget.minimum);
			widgetReader.tryRead("maximum", widget.maximum);
			widgetReader.tryRead("step", widget.step);
			widgetReader.tryRead("largeStep", widget.largeStep);
			widgetReader.tryRead("wrap", widget.wrap);
			widgetReader.tryRead("mouseWheel", widget.mouseWheel);

			if (const auto &colors = widgetReader["colors"])
			{
				if (const auto &normal = colors["default"]) readColorState(normal, widget.colors.normal);
				if (const auto &hover = colors["hover"]) readColorState(hover, widget.colors.hover);
				if (const auto &focused = colors["focused"]) readColorState(focused, widget.colors.focused);
				if (const auto &selected = colors["selected"]) readColorState(selected, widget.colors.selected);
			}

			std::string align = (widget.type == CUSTOM_UI_BUTTON || widget.type == CUSTOM_UI_TOGGLE) ? "center" : "left";
			if (widgetReader.tryRead("align", align))
				widget.align = readTextAlign(align, _id, widget.id);
			else
				widget.align = (widget.type == CUSTOM_UI_BUTTON || widget.type == CUSTOM_UI_TOGGLE) ? CUSTOM_UI_ALIGN_CENTER : CUSTOM_UI_ALIGN_LEFT;

			if (const auto &options = widgetReader["options"])
			{
				for (const auto &optionReader : options.children())
				{
					CustomUiOption option;
					if (optionReader.isMap())
					{
						option.text = optionReader["text"].readVal<std::string>();
						option.value = optionReader["value"].readVal<std::string>();
					}
					else
					{
						option.text = optionReader.readVal<std::string>();
						option.value = option.text;
					}
					widget.options.push_back(option);
				}
			}

			if (const auto &source = widgetReader["source"])
			{
				if (source.isMap())
				{
					source.tryRead("type", widget.source.type);
					source.tryRead("scope", widget.source.scope);
					source.tryRead("includeZero", widget.source.includeZero);
				}
				else
				{
					source.tryReadVal(widget.source.type);
				}
			}
			widgetReader.tryRead("search", widget.search);
			widgetReader.tryRead("selectedValue", widget.selectedValue);
			widgetReader.tryRead("sortBy", widget.sortBy);
			widgetReader.tryRead("sortValue", widget.sortValue);
			widgetReader.tryRead("descendingValue", widget.descendingValue);
			widgetReader.tryRead("selectable", widget.selectable);

			if (const auto &columns = widgetReader["columns"])
			{
				for (const auto &columnReader : columns.children())
				{
					CustomUiColumn column;
					column.field = columnReader["field"].readVal<std::string>();
					columnReader.tryRead("header", column.header);
					columnReader.tryRead("width", column.width);
					std::string columnAlign = "left";
					if (columnReader.tryRead("align", columnAlign))
						column.align = readTextAlign(columnAlign, _id, widget.id);
					widget.columns.push_back(column);
				}
			}

			_widgets.push_back(widget);
		}
	}

	_customUiScripts.load(_id, reader, parsers.customUiScripts);
	validate();
}

const CustomUiActionDefinition *RuleCustomUi::getAction(const std::string &id) const
{
	auto found = _actions.find(id);
	return found == _actions.end() ? nullptr : &found->second;
}

void RuleCustomUi::validate() const
{
	if (_id.empty()) throw Exception("Custom UI id must not be empty");
	if (_title.empty()) throw Exception("Custom UI '" + _id + "' title must not be empty");
	if (_interface.empty()) throw Exception("Custom UI '" + _id + "' interface must not be empty");
	if (_width < 64 || _width > 320 || _height < 40 || _height > 200)
		throw Exception("Custom UI '" + _id + "' dimensions must fit within 64..320 by 40..200");

	int originX = 0;
	int originY = 0;
	switch (_anchor)
	{
	case CUSTOM_UI_ANCHOR_TOP:
	case CUSTOM_UI_ANCHOR_CENTER:
	case CUSTOM_UI_ANCHOR_BOTTOM: originX = (320 - _width) / 2; break;
	case CUSTOM_UI_ANCHOR_TOP_RIGHT:
	case CUSTOM_UI_ANCHOR_RIGHT:
	case CUSTOM_UI_ANCHOR_BOTTOM_RIGHT: originX = 320 - _width; break;
	default: break;
	}
	switch (_anchor)
	{
	case CUSTOM_UI_ANCHOR_LEFT:
	case CUSTOM_UI_ANCHOR_CENTER:
	case CUSTOM_UI_ANCHOR_RIGHT: originY = (200 - _height) / 2; break;
	case CUSTOM_UI_ANCHOR_BOTTOM_LEFT:
	case CUSTOM_UI_ANCHOR_BOTTOM:
	case CUSTOM_UI_ANCHOR_BOTTOM_RIGHT: originY = 200 - _height; break;
	default: break;
	}
	originX += _offsetX;
	originY += _offsetY;
	if (originX < 0 || originY < 0 || originX + _width > 320 || originY + _height > 200)
		throw Exception("Custom UI '" + _id + "' anchored position lies outside the 320x200 UI area");
	if (_widgets.empty()) throw Exception("Custom UI '" + _id + "' must declare at least one widget");

	for (const auto &value : _values)
	{
		if (value.first.empty()) throw Exception("Custom UI '" + _id + "' has an empty local value id");
	}
	for (const auto &action : _actions)
	{
		if (action.first.empty()) throw Exception("Custom UI '" + _id + "' has an empty action id");
		if ((action.second.type == CUSTOM_UI_ACTION_OPEN_CUSTOM_UI || action.second.type == CUSTOM_UI_ACTION_OPEN_NATIVE_UI) && action.second.target.empty())
			throw Exception("Custom UI '" + _id + "' action '" + action.first + "' requires a target");
	}

	std::set<std::string> ids;
	std::map<std::string, CustomUiWidgetType> widgetTypes;
	bool hasFocusedWidget = false;
	for (const auto &widget : _widgets)
	{
		if (widget.id.empty()) throw Exception("Custom UI '" + _id + "' contains a widget with an empty id");
		if (!ids.insert(widget.id).second) throw Exception("Custom UI '" + _id + "' contains duplicate widget id '" + widget.id + "'");
		widgetTypes[widget.id] = widget.type;
		if (requiresText(widget.type) && widget.text.empty())
			throw Exception("Custom UI '" + _id + "' widget '" + widget.id + "' text must not be empty");
		if (widget.width <= 0 || widget.height <= 0)
			throw Exception("Custom UI '" + _id + "' widget '" + widget.id + "' dimensions must be positive");
		if (widget.x < 0 || widget.y < 0 || widget.x + widget.width > _width || widget.y + widget.height > _height)
			throw Exception("Custom UI '" + _id + "' widget '" + widget.id + "' lies outside the window");

		auto requireBinding = [&](CustomUiValueType type, const std::string &property)
		{
			if (widget.bind.empty())
				throw Exception("Custom UI '" + _id + "' widget '" + widget.id + "' requires a " + property + " binding");
			auto found = _values.find(widget.bind);
			if (found == _values.end())
				throw Exception("Custom UI '" + _id + "' widget '" + widget.id + "' references unknown value '" + widget.bind + "'");
			if (found->second.type != type)
				throw Exception("Custom UI '" + _id + "' widget '" + widget.id + "' has an incompatible binding type");
		};

		if (widget.type == CUSTOM_UI_TOGGLE) requireBinding(CUSTOM_UI_VALUE_BOOL, "boolean");
		if (widget.type == CUSTOM_UI_TEXT_INPUT || widget.type == CUSTOM_UI_SEARCH) requireBinding(CUSTOM_UI_VALUE_STRING, "string");
		if (widget.type == CUSTOM_UI_NUMBER)
		{
			requireBinding(CUSTOM_UI_VALUE_INT, "integer");
			if (widget.minimum > widget.maximum || widget.step <= 0 || widget.largeStep <= 0)
				throw Exception("Custom UI '" + _id + "' numeric widget '" + widget.id + "' has an invalid range or step");
			if (widget.width < 24 || widget.height < 12)
				throw Exception("Custom UI '" + _id + "' numeric widget '" + widget.id + "' is too small");
		}
		if (widget.type == CUSTOM_UI_DROPDOWN)
		{
			requireBinding(CUSTOM_UI_VALUE_STRING, "string");
			if (widget.options.empty())
				throw Exception("Custom UI '" + _id + "' dropdown '" + widget.id + "' must declare options");
			std::set<std::string> optionValues;
			for (const auto &option : widget.options)
			{
				if (option.text.empty() || option.value.empty())
					throw Exception("Custom UI '" + _id + "' dropdown '" + widget.id + "' has an empty option");
				if (!optionValues.insert(option.value).second)
					throw Exception("Custom UI '" + _id + "' dropdown '" + widget.id + "' has duplicate option value '" + option.value + "'");
			}
		}
		if (widget.type == CUSTOM_UI_LIST)
		{
			if (widget.source.type.empty())
				throw Exception("Custom UI '" + _id + "' list '" + widget.id + "' requires a source");
			if (widget.source.type != "research" && widget.source.type != "items" && widget.source.type != "soldiers" && widget.source.type != "bases" && widget.source.type != "crafts")
				throw Exception("Custom UI '" + _id + "' list '" + widget.id + "' has unsupported source '" + widget.source.type + "'");
			if (!widget.source.scope.empty() && widget.source.scope != "currentBase" && widget.source.scope != "all")
				throw Exception("Custom UI '" + _id + "' list '" + widget.id + "' has unsupported source scope '" + widget.source.scope + "'");
			if (widget.columns.empty() || widget.columns.size() > 8)
				throw Exception("Custom UI '" + _id + "' list '" + widget.id + "' must declare 1..8 columns");
			int columnWidth = 0;
			for (const auto &column : widget.columns)
			{
				if (column.field.empty() || column.width <= 0)
					throw Exception("Custom UI '" + _id + "' list '" + widget.id + "' has an invalid column");
				if (!column.header.empty() && !widget.sortValue.empty() && column.width < 14)
					throw Exception("Custom UI '" + _id + "' list '" + widget.id + "' has a sortable column too narrow for its arrow");
				columnWidth += column.width;
			}
			const bool hasHeaders = std::any_of(widget.columns.begin(), widget.columns.end(), [](const CustomUiColumn &column) { return !column.header.empty(); });
			if (hasHeaders && widget.height < 20)
				throw Exception("Custom UI '" + _id + "' list '" + widget.id + "' is too short for column headers");
			if (columnWidth > widget.width)
				throw Exception("Custom UI '" + _id + "' list '" + widget.id + "' columns exceed the widget width");
			if (!widget.selectedValue.empty())
			{
				auto found = _values.find(widget.selectedValue);
				if (found == _values.end() || found->second.type != CUSTOM_UI_VALUE_STRING)
					throw Exception("Custom UI '" + _id + "' list '" + widget.id + "' selectedValue must reference a string value");
			}
			if (!widget.sortValue.empty())
			{
				auto found = _values.find(widget.sortValue);
				if (found == _values.end() || found->second.type != CUSTOM_UI_VALUE_STRING)
					throw Exception("Custom UI '" + _id + "' list '" + widget.id + "' sortValue must reference a string value");
			}
			if (!widget.descendingValue.empty())
			{
				auto found = _values.find(widget.descendingValue);
				if (found == _values.end() || found->second.type != CUSTOM_UI_VALUE_BOOL)
					throw Exception("Custom UI '" + _id + "' list '" + widget.id + "' descendingValue must reference a bool value");
			}
		}

		auto validateAction = [&](const std::string &actionId, const std::string &property)
		{
			if (!actionId.empty() && actionId != "close" && _actions.find(actionId) == _actions.end())
				throw Exception("Custom UI '" + _id + "' widget '" + widget.id + "' " + property + " references unknown action '" + actionId + "'");
		};
		validateAction(widget.action, "onClick");
		validateAction(widget.onChange, "onChange");
		validateAction(widget.onSubmit, "onSubmit");
		validateAction(widget.onSelect, "onSelect");

		if (widget.type == CUSTOM_UI_LABEL && (!widget.action.empty() || !widget.onChange.empty() || !widget.onSubmit.empty() || !widget.onSelect.empty()))
			throw Exception("Custom UI '" + _id + "' label '" + widget.id + "' cannot declare interaction actions");
		if (widget.focused && (widget.type == CUSTOM_UI_BUTTON || widget.type == CUSTOM_UI_TOGGLE ||
			widget.type == CUSTOM_UI_TEXT_INPUT || widget.type == CUSTOM_UI_SEARCH || widget.type == CUSTOM_UI_NUMBER))
		{
			if (hasFocusedWidget) throw Exception("Custom UI '" + _id + "' must not declare more than one focused widget");
			hasFocusedWidget = true;
		}
		else if (widget.focused && widget.type != CUSTOM_UI_TEXT_INPUT && widget.type != CUSTOM_UI_SEARCH && widget.type != CUSTOM_UI_NUMBER)
		{
			throw Exception("Custom UI '" + _id + "' widget '" + widget.id + "' cannot receive initial focus");
		}
		if (widget.selected && widget.type != CUSTOM_UI_BUTTON && widget.type != CUSTOM_UI_TOGGLE)
			throw Exception("Custom UI '" + _id + "' widget '" + widget.id + "' cannot be selected");
		if (widget.overflowExplicit && widget.type != CUSTOM_UI_LABEL)
			throw Exception("Custom UI '" + _id + "' widget '" + widget.id + "' can only declare overflow on a label");
		if (widget.autoScrollbar && widget.type != CUSTOM_UI_LABEL)
			throw Exception("Custom UI '" + _id + "' widget '" + widget.id + "' can only declare autoScrollbar on a label");
		if (widget.maxLength < 0)
			throw Exception("Custom UI '" + _id + "' widget '" + widget.id + "' has a negative maxLength");

		validateColorState(widget.colors.normal, _id, widget.id, "default");
		validateColorState(widget.colors.hover, _id, widget.id, "hover");
		validateColorState(widget.colors.focused, _id, widget.id, "focused");
		validateColorState(widget.colors.selected, _id, widget.id, "selected");
	}

	for (const auto &widget : _widgets)
	{
		if (!widget.search.empty())
		{
			auto found = widgetTypes.find(widget.search);
			if (found == widgetTypes.end() || (found->second != CUSTOM_UI_SEARCH && found->second != CUSTOM_UI_TEXT_INPUT))
				throw Exception("Custom UI '" + _id + "' list '" + widget.id + "' search must reference a search or textInput widget");
		}
	}
}

}
