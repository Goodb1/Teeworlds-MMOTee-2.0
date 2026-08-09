#ifndef GAME_SERVER_CORE_BALANCE_BALANCE_H
#define GAME_SERVER_CORE_BALANCE_BALANCE_H

#include <game/server/core/mmo_context.h>

class CPlayer;
class Balance
{
public:
	static Balance& Get();
	static void Init();

	float GetAttributeCap(AttributeIdentifier ID) const;
	int   GetAttributeBase(AttributeIdentifier ID) const;
	float GetBotGroupPercent(AttributeGroup Group) const;
	float GetBotBossDownscaleDivider() const;

	int CalculateScenarioMobPower(const std::vector<CPlayer*>& vpPlayers, int BasePower, bool IsGroupScenario) const;

private:
	static constexpr size_t kAttributeCount = static_cast<size_t>(AttributeIdentifier::ATTRIBUTES_NUM);

	// -------------------------------------------------------
	// Configuration structures
	// -------------------------------------------------------
	struct BotGroupScaling
	{
		float DamagePercent{};
		float DpsPercent{};
		float HealerPercent{};
		float BossDownscaleDivider{};
	};

	struct ScenarioMobPowerScaling
	{
		int   MinPower{};
		int   MaxPower{};
		float SoloStatBaseWeight{};
		float GroupStatBaseWeight{};
		float DiminishingKnee{};
	};

	// -------------------------------------------------------
	// Data
	// -------------------------------------------------------
	std::array<float, kAttributeCount> m_AttributeCaps{};
	std::array<int, kAttributeCount>   m_AttributeBase{};
	BotGroupScaling                    m_BotGroupScaling{};
	ScenarioMobPowerScaling            m_ScenarioMobPowerScaling{};

	// Cached list of attributes that contribute to mob power calculation
	std::vector<AttributeIdentifier>   m_RelevantMobPowerAttrs{};

	// -------------------------------------------------------
	// Private methods
	// -------------------------------------------------------
	void Initialize();
	void BuildRelevantAttrCache();

	bool IsAttributeRelevantForMobPower(AttributeIdentifier ID) const;
	static size_t ToIndex(AttributeIdentifier ID);
};

#endif // GAME_SERVER_CORE_BALANCE_BALANCE_H