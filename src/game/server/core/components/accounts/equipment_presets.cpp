#include "equipment_presets.h"

#include <game/server/gamecontext.h>

CGS* EquipmentPresets::GS() const
{
	return (CGS*)Instance::GameServerPlayer(m_ClientID);
}

CPlayer* EquipmentPresets::GetPlayer() const
{
	return GS()->GetPlayer(m_ClientID);
}

void EquipmentPresets::initPresets()
{
	m_Presets.clear();
	for(int i = 0; i < MAX_PRESETS; i++)
	{
		m_Presets.emplace_back(Preset{"", {}});
	}
}

void EquipmentPresets::initialize(const int clientID, const std::string& presetsJson)
{
	m_ClientID = clientID;
	initPresets();

	mystd::json::parse(presetsJson, [this](nlohmann::json& jsonData)
	{
		if(!jsonData.is_array())
			return;

		for(size_t i = 0; i < jsonData.size() && i < MAX_PRESETS; i++)
		{
			const auto& jsonPreset = jsonData[i];
			if(!jsonPreset.is_object())
				continue;

			auto& preset = m_Presets[i];

			// load preset name
			if(jsonPreset.contains("Name") && jsonPreset["Name"].is_string())
			{
				preset.Name = jsonPreset["Name"].get<std::string>();
			}

			// load profession ID
			if(jsonPreset.contains("ProfessionID") && jsonPreset["ProfessionID"].is_number_integer())
			{
				preset.ProfessionID = jsonPreset["ProfessionID"].get<int>();
			}

			// load equipment items
			if(jsonPreset.contains("Equipment") && jsonPreset["Equipment"].is_object())
			{
				for(auto& [strType, jsonItemID] : jsonPreset["Equipment"].items())
				{
					try
					{
						const auto itemType = (ItemType)std::stoi(strType);

						if(jsonItemID.is_number_integer())
						{
							const int itemID = jsonItemID.get<int>();
							if(CItemDescription::Data().contains(itemID))
							{
								CItem item(itemID, 1);
								if(item.IsValid() && item.Info()->IsType(itemType))
								{
									preset.Equipment[itemType] = itemID;
								}
							}
						}
						else if(jsonItemID.is_null())
						{
							preset.Equipment[itemType] = std::nullopt;
						}
					}
					catch(const std::exception&)
					{
						// Skip invalid entries
					}
				}
			}

			// load modules
			if(jsonPreset.contains("Modules") && jsonPreset["Modules"].is_array())
			{
				for(const auto& jsonModuleID : jsonPreset["Modules"])
				{
					if(jsonModuleID.is_number_integer())
					{
						const int moduleID = jsonModuleID.get<int>();
						if(CItemDescription::Data().contains(moduleID))
						{
							preset.Modules.push_back(moduleID);
						}
					}
					else if(jsonModuleID.is_null())
					{
						preset.Modules.push_back(std::nullopt);
					}
				}
			}
		}
	});
}

nlohmann::json EquipmentPresets::dumpJson() const
{
	nlohmann::json jsonArray = nlohmann::json::array();

	for(const auto& preset : m_Presets)
	{
		nlohmann::json jsonPreset = nlohmann::json::object();
		jsonPreset["Name"] = preset.Name;
		jsonPreset["ProfessionID"] = preset.ProfessionID;

		nlohmann::json jsonEquipment = nlohmann::json::object();
		for(const auto& [itemType, itemIDOpt] : preset.Equipment)
		{
			jsonEquipment[std::to_string((int)itemType)] = itemIDOpt.value_or(NOPE);
		}
		jsonPreset["Equipment"] = jsonEquipment;

		nlohmann::json jsonModules = nlohmann::json::array();
		for(const auto& moduleIDOpt : preset.Modules)
		{
			jsonModules.push_back(moduleIDOpt.value_or(NOPE));
		}
		jsonPreset["Modules"] = jsonModules;

		jsonArray.push_back(jsonPreset);
	}

	return jsonArray;
}

bool EquipmentPresets::Sanitize(int SlotIndex)
{
	auto* pPlayer = GetPlayer();
	if (SlotIndex < 0 || SlotIndex >= MAX_PRESETS || !pPlayer)
		return false;

	auto& preset = m_Presets[SlotIndex];
	if (preset.Name.empty() && preset.Equipment.empty() && preset.Modules.empty())
		return false;

	bool changed = false;
	for (auto& [itemType, itemIDOpt] : preset.Equipment)
	{
		if (!itemIDOpt.has_value())
			continue;

		const int itemID = *itemIDOpt;
		auto* pPlayerItem = pPlayer->GetItem(itemID);
		const bool valid = CItemDescription::Data().contains(itemID) && pPlayerItem && pPlayerItem->HasItem();

		if (!valid)
		{
			itemIDOpt = std::nullopt;
			changed = true;
		}
	}

	for (auto& moduleIDOpt : preset.Modules)
	{
		if (!moduleIDOpt.has_value())
			continue;

		const int moduleID = *moduleIDOpt;
		auto* pPlayerItem = pPlayer->GetItem(moduleID);
		const bool valid = CItemDescription::Data().contains(moduleID) && pPlayerItem && pPlayerItem->HasItem();

		if (!valid)
		{
			moduleIDOpt = std::nullopt;
			changed = true;
		}
	}

	return changed;
}

const EquipmentPresets::Preset* EquipmentPresets::getPreset(int SlotIndex) const
{
	if(SlotIndex < 0 || SlotIndex >= MAX_PRESETS)
		return nullptr;

	return &m_Presets[SlotIndex];
}

bool EquipmentPresets::Delete(int SlotIndex)
{
	const auto pPlayer = GetPlayer();
	if (SlotIndex < 0 || SlotIndex >= MAX_PRESETS || !pPlayer)
		return false;

	const auto* pPreset = getPreset(SlotIndex);
	if (!pPreset || pPreset->Name.empty())
	{
		GS()->Chat(m_ClientID, "Preset slot {} is already empty", SlotIndex + 1);
		return false;
	}

	if (deletePresetImpl(SlotIndex))
	{
		GS()->Chat(m_ClientID, "Equipment preset '{}' deleted from slot {}", pPreset->Name, SlotIndex + 1);
		pPlayer->Account()->SaveEquipmentPresets();
		pPlayer->m_VotesData.UpdateCurrentVotes();
	}
	return true;
}

bool EquipmentPresets::Load(int SlotIndex)
{
	const auto pPlayer = GetPlayer();
	if (SlotIndex < 0 || SlotIndex >= EquipmentPresets::MAX_PRESETS || !pPlayer)
		return false;

	const int CurrentProfessionID = (int)pPlayer->Account()->GetActiveProfessionID();
	const auto* pPreset = getPreset(SlotIndex);
	if (!pPreset || pPreset->Name.empty())
	{
		GS()->Chat(m_ClientID, "Preset slot {} is empty", SlotIndex + 1);
		return true;
	}

	// check spam
	if ((pPlayer->m_aPlayerTick[LastDamage] + GS()->Server()->TickSpeed() * 5) > GS()->Server()->Tick())
	{
		GS()->Chat(m_ClientID, "Wait a couple of seconds, your player is currently in combat or taking damage");
		return true;
	}

	// can't change profession in dungeon
	if (GS()->IsWorldType(WorldType::Dungeon) && CurrentProfessionID != pPreset->ProfessionID)
	{
		GS()->Chat(m_ClientID, "You can change only equipment presets with the same profession in dungeons");
		return true;
	}

	// sanitize preset and remove lost items
	if (Sanitize(SlotIndex))
	{
		GS()->Chat(m_ClientID, "Some items from preset '{}' were lost or removed!", pPreset->Name);
		pPlayer->Account()->SaveEquipmentPresets();
	}

	// try load preset
	if (!loadPresetImpl(SlotIndex, pPlayer))
	{
		GS()->Chat(m_ClientID, "Failed to load preset '{}'", pPreset->Name);
		return true;
	}

	GS()->Chat(m_ClientID, "Loaded equipment preset '{}'", pPreset->Name);
	return true;
}

bool EquipmentPresets::Save(int SlotIndex, const std::string& PresetName)
{
	const auto pPlayer = GetPlayer();
	if (SlotIndex < 0 || SlotIndex >= MAX_PRESETS || !pPlayer)
		return false;

	if (savePresetImpl(SlotIndex, PresetName, pPlayer))
	{
		GS()->Chat(m_ClientID, "Equipment preset '{}' saved in slot {}!", PresetName, SlotIndex + 1);
		pPlayer->Account()->SaveEquipmentPresets();
		pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}
	return false;
}


//
// Implements
//
bool EquipmentPresets::savePresetImpl(int SlotIndex, const std::string& Name, CPlayer* pPlayer)
{
	auto* pAccount = pPlayer->Account();
	int CurrentProfessionID = (int)pAccount->GetActiveProfessionID();

	// collect current active equipped items
	std::map<ItemType, std::optional<int>> Equipment;
	for (const auto& [SlotType, ItemIDOpt] : pAccount->GetActiveProfession()->GetEquippedSlots().getSlots())
	{
		if (ItemIDOpt.has_value())
		{
			auto* pItem = pPlayer->GetItem(ItemIDOpt.value());
			if (pItem && pItem->HasItem())
				Equipment[SlotType] = ItemIDOpt.value();
			else
				Equipment[SlotType] = std::nullopt;
		}
		else
		{
			Equipment[SlotType] = std::nullopt;
		}
	}

	// collect current equipped items
	for (const auto& [SlotType, ItemIDOpt] : pAccount->GetEquippedSlots().getSlots())
	{
		if (ItemIDOpt.has_value())
		{
			auto* pItem = pPlayer->GetItem(ItemIDOpt.value());
			if (pItem && pItem->HasItem())
				Equipment[SlotType] = ItemIDOpt.value();
			else
				Equipment[SlotType] = std::nullopt;
		}
		else
		{
			Equipment[SlotType] = std::nullopt;
		}
	}

	// collect all equipped modules
	std::vector<std::optional<int>> Modules;
	for (auto& [ID, Item] : CPlayerItem::Data()[pPlayer->GetCID()])
	{
		if (!Item.HasItem())
			continue;

		if (Item.Info()->IsEquipmentModules() && Item.IsEquipped())
		{
			Modules.push_back(std::make_optional<int>(Item.GetID()));
		}
	}

	m_Presets[SlotIndex].Name = Name;
	m_Presets[SlotIndex].ProfessionID = CurrentProfessionID;
	m_Presets[SlotIndex].Equipment = Equipment;
	m_Presets[SlotIndex].Modules = Modules;
	return true;
}

bool EquipmentPresets::deletePresetImpl(int SlotIndex)
{
	m_Presets[SlotIndex] = Preset{ "", -1, {}, {} };
	return true;
}

bool EquipmentPresets::loadPresetImpl(int SlotIndex, CPlayer* pPlayer)
{
	const auto& preset = m_Presets[SlotIndex];
	if (preset.Name.empty() && preset.Equipment.empty() && preset.Modules.empty())
		return false;

	// check if all items are still available
	for (auto& [itemType, itemIDOpt] : preset.Equipment)
	{
		if (!itemIDOpt.has_value())
			continue;

		const int itemID = itemIDOpt.value();
		if (!CItemDescription::Data().contains(itemID))
			return false;

		auto* pPlayerItem = pPlayer->GetItem(itemID);
		if (!pPlayerItem || !pPlayerItem->HasItem())
			return false;
	}

	// check if all modules are still available
	for (const auto& moduleIDOpt : preset.Modules)
	{
		if (!moduleIDOpt.has_value())
			continue;

		const int moduleID = moduleIDOpt.value();
		if (!CItemDescription::Data().contains(moduleID))
			return false;

		auto* pPlayerItem = pPlayer->GetItem(moduleID);
		if (!pPlayerItem || !pPlayerItem->HasItem())
			return false;
	}

	// change profession if needed
	auto* pAccount = pPlayer->Account();
	if (preset.ProfessionID >= 0 && (int)pAccount->GetActiveProfessionID() != preset.ProfessionID)
	{
		pAccount->SetProfessionRaw((ProfessionIdentifier)preset.ProfessionID);
		GS()->Core()->SaveAccount(pPlayer, SAVE_PROFESSION);
	}

	// collect all equipment slots by player
	std::vector<EquippedSlots::SlotEntry> vEquipmentProfession = pAccount->GetActiveProfession()->GetEquippedSlots().getSlots();
	const auto& accountSlots = pAccount->GetEquippedSlots().getSlots();
	vEquipmentProfession.insert(vEquipmentProfession.end(), accountSlots.begin(), accountSlots.end());

	// unequip only items that differ from preset
	for (const auto& [SlotType, ItemIDOpt] : vEquipmentProfession)
	{
		if (!ItemIDOpt.has_value())
			continue;

		std::optional<int> PresetItemID = std::nullopt;
		if (const auto it = preset.Equipment.find(SlotType); it != preset.Equipment.end())
			PresetItemID = it->second;

		// If the same item is already equipped in this slot, keep it equipped
		if (PresetItemID.has_value() && PresetItemID.value() == ItemIDOpt.value())
			continue;

		auto* pItem = pPlayer->GetItem(ItemIDOpt.value());
		if (pItem && pItem->IsEquipped())
			pItem->UnEquip();
	}

	// build set of preset modules
	std::unordered_set<int> PresetModules;
	for (const auto& moduleIDOpt : preset.Modules)
	{
		if (moduleIDOpt.has_value())
			PresetModules.insert(moduleIDOpt.value());
	}

	// unequip only modules that are not in preset
	for (auto& [ID, Item] : CPlayerItem::Data()[pPlayer->GetCID()])
	{
		if (!Item.HasItem())
			continue;

		if (Item.Info()->IsEquipmentModules() && Item.IsEquipped())
		{
			if (PresetModules.find(Item.GetID()) == PresetModules.end())
				Item.UnEquip();
		}
	}

	// equip preset items only if they are not already equipped
	for (const auto& [itemType, itemIDOpt] : preset.Equipment)
	{
		if (!itemIDOpt.has_value())
			continue;

		auto* pItem = pPlayer->GetItem(itemIDOpt.value());
		if (pItem && pItem->HasItem() && !pItem->IsEquipped())
			pItem->Equip();
	}

	// equip preset modules only if they are not already equipped
	for (const int moduleID : PresetModules)
	{
		auto* pItem = pPlayer->GetItem(moduleID);
		if (pItem && pItem->HasItem() && !pItem->IsEquipped())
			pItem->Equip();
	}

	return true;
}
