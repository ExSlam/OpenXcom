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
#include "CustomUiDataProvider.h"
#include <iomanip>
#include <sstream>
#include "../Engine/Language.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleCraft.h"
#include "../Mod/RuleCustomUi.h"
#include "../Mod/RuleItem.h"
#include "../Mod/RuleResearch.h"
#include "../Savegame/Base.h"
#include "../Savegame/Craft.h"
#include "../Savegame/ItemContainer.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Soldier.h"

namespace OpenXcom
{

namespace
{

std::string number(long long value)
{
	return std::to_string(value);
}

std::string decimal(double value)
{
	std::ostringstream out;
	out << std::fixed << std::setprecision(1) << value;
	return out.str();
}

std::string translated(Language *language, const std::string &id)
{
	if (!language) return id;
	return static_cast<const std::string &>(language->getString(id));
}

void putNumber(CustomUiDataRecord &record, const std::string &field, long long value)
{
	record.numericFields[field] = value;
	record.fields[field] = number(value);
}

std::vector<Base *> selectedBases(const SavedGame *save, Base *currentBase, const CustomUiDataSource &source)
{
	std::vector<Base *> result;
	const bool all = source.scope == "all" || (!currentBase && source.scope.empty());
	if (!all && currentBase)
	{
		result.push_back(currentBase);
	}
	else if (save)
	{
		for (Base *base : *save->getBases()) result.push_back(base);
	}
	return result;
}

}

std::string CustomUiDataRecord::get(const std::string &field) const
{
	if (field == "id") return id;
	auto found = fields.find(field);
	return found == fields.end() ? std::string() : found->second;
}

std::vector<CustomUiDataRecord> CustomUiDataProvider::getRecords(
	const CustomUiDataSource &source,
	const Mod *mod,
	const SavedGame *save,
	Base *currentBase,
	Language *language)
{
	std::vector<CustomUiDataRecord> records;
	if (!mod) return records;

	if (source.type == "research")
	{
		for (const std::string &id : mod->getResearchList())
		{
			const RuleResearch *rule = mod->getResearch(id);
			if (!rule) continue;
			CustomUiDataRecord record;
			record.id = id;
			record.fields["name"] = translated(language, rule->getName());
			putNumber(record, "cost", rule->getCost());
			putNumber(record, "points", rule->getPoints());
			const bool discovered = save && save->isResearched(rule, false);
			const int statusId = save ? save->getResearchRuleStatus(id) : RuleResearch::RESEARCH_STATUS_NEW;
			record.fields["discovered"] = discovered ? "true" : "false";
			putNumber(record, "discoveredValue", discovered ? 1 : 0);
			putNumber(record, "statusId", statusId);
			if (discovered)
				record.fields["status"] = "researched";
			else if (statusId == RuleResearch::RESEARCH_STATUS_DISABLED)
				record.fields["status"] = "disabled";
			else if (statusId == RuleResearch::RESEARCH_STATUS_HIDDEN)
				record.fields["status"] = "hidden";
			else if (statusId == RuleResearch::RESEARCH_STATUS_NORMAL)
				record.fields["status"] = "available";
			else
				record.fields["status"] = "locked";
			records.push_back(record);
		}
		return records;
	}

	const std::vector<Base *> bases = selectedBases(save, currentBase, source);

	if (source.type == "items")
	{
		std::map<const RuleItem *, long long> quantities;
		for (Base *base : bases)
		{
			for (const auto &entry : *base->getStorageItems()->getContents())
				quantities[entry.first] += entry.second;
		}
		for (const std::string &id : mod->getItemsList())
		{
			const RuleItem *rule = mod->getItem(id);
			if (!rule) continue;
			const long long quantity = quantities[rule];
			if (!source.includeZero && quantity == 0) continue;
			CustomUiDataRecord record;
			record.id = id;
			record.fields["name"] = translated(language, rule->getName());
			putNumber(record, "quantity", quantity);
			putNumber(record, "buyCost", rule->getBuyCost());
			putNumber(record, "sellCost", rule->getSellCost());
			record.fields["size"] = decimal(rule->getSize());
			records.push_back(record);
		}
		return records;
	}

	if (source.type == "soldiers")
	{
		for (Base *base : bases)
		{
			for (const Soldier *soldier : *base->getSoldiers())
			{
				CustomUiDataRecord record;
				record.id = number(soldier->getId());
				record.fields["name"] = soldier->getName();
				record.fields["rank"] = translated(language, soldier->getRankString());
				record.fields["base"] = base->getName(language);
				record.fields["craft"] = soldier->getCraft() ? soldier->getCraft()->getDefaultName(language) : std::string();
				putNumber(record, "missions", soldier->getMissions());
				putNumber(record, "kills", soldier->getKills());
				putNumber(record, "woundRecovery", soldier->getWoundRecoveryInt());
				records.push_back(record);
			}
		}
		return records;
	}

	if (source.type == "bases")
	{
		if (!save) return records;
		for (Base *base : *save->getBases())
		{
			CustomUiDataRecord record;
			record.id = number(base->getId());
			record.fields["name"] = base->getName(language);
			putNumber(record, "soldiers", base->getTotalSoldiers());
			putNumber(record, "crafts", static_cast<long long>(base->getCrafts()->size()));
			putNumber(record, "scientists", base->getScientists());
			putNumber(record, "engineers", base->getEngineers());
			record.fields["usedStores"] = decimal(base->getUsedStores());
			putNumber(record, "availableStores", base->getAvailableStores());
			records.push_back(record);
		}
		return records;
	}

	if (source.type == "crafts")
	{
		for (Base *base : bases)
		{
			for (const Craft *craft : *base->getCrafts())
			{
				const CraftId unique = craft->getUniqueId();
				CustomUiDataRecord record;
				record.id = unique.first + ":" + number(unique.second);
				record.fields["name"] = craft->getDefaultName(language);
				record.fields["type"] = translated(language, craft->getRules()->getType());
				record.fields["status"] = translated(language, craft->getStatus());
				record.fields["base"] = base->getName(language);
				putNumber(record, "fuel", craft->getFuelPercentage());
				putNumber(record, "damage", craft->getDamagePercentage());
				int soldiers = 0;
				for (const Soldier *soldier : *base->getSoldiers())
					if (soldier->getCraft() == craft) ++soldiers;
				putNumber(record, "soldiers", soldiers);
				records.push_back(record);
			}
		}
	}
	return records;
}

}
