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

namespace OpenXcom
{

class Base;
class Language;
class Mod;
class SavedGame;
struct CustomUiDataSource;

/**
 * Stable, copied record exposed to declarative custom UI tables.
 *
 * No C++ savegame pointers are retained by the UI. Record IDs and fields are
 * copied at refresh time so sorting/filtering cannot produce stale pointers.
 */
struct CustomUiDataRecord
{
	std::string id;
	std::map<std::string, std::string> fields;
	std::map<std::string, long long> numericFields;

	std::string get(const std::string &field) const;
};

/**
 * Converts selected game data into documented, stable records.
 */
class CustomUiDataProvider
{
public:
	static std::vector<CustomUiDataRecord> getRecords(
		const CustomUiDataSource &source,
		const Mod *mod,
		const SavedGame *save,
		Base *currentBase,
		Language *language);
};

}
