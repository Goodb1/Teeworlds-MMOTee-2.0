#ifndef GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_EQUIPMENT_PRESETS_H
#define GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_EQUIPMENT_PRESETS_H

#include <game/server/core/mmo_context.h>

class CGS;
class CPlayer;

class EquipmentPresets
{
	int m_ClientID{};

public:
	struct Preset
	{
		std::string Name;
		int ProfessionID {-1};
		std::map<ItemType, std::optional<int>> Equipment;
		std::vector<std::optional<int>> Modules;
	};

	static constexpr int MAX_PRESETS = 8;

	EquipmentPresets() = default;

	void initialize(const int clientID, const std::string& presetsJson);
	const std::vector<Preset>& getPresets() const { return m_Presets; }
	const Preset* getPreset(int SlotIndex) const;
	nlohmann::json dumpJson() const;

	bool Load(int SlotIndex);
	bool Save(int SlotIndex, const std::string& PresetName);
	bool Delete(int SlotIndex);
	bool Sanitize(int SlotIndex);

private:
	std::vector<Preset> m_Presets {};

	CGS* GS() const;
	CPlayer* GetPlayer() const;

	void initPresets();
	bool savePresetImpl(int SlotIndex, const std::string& Name, CPlayer* pPlayer);
	bool deletePresetImpl(int SlotIndex);
	bool loadPresetImpl(int SlotIndex, CPlayer* pPlayer);
};

#endif
