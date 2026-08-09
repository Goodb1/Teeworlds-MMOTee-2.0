#include "balance.h"

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

// -------------------------------------------------------
// Utilities
// -------------------------------------------------------
namespace
{
	// Soft-caps a raw value using logarithmic diminishing returns.
	// Values well below `Knee` grow almost linearly;
	// values far above it are heavily compressed.
	float DiminishingReturns(float RawValue, float Knee)
	{
		if (RawValue <= 0.0f || Knee <= 0.0f)
			return 0.0f;
		return Knee * std::log1pf(RawValue / Knee);
	}
} // namespace

// -------------------------------------------------------
// Singleton access
// -------------------------------------------------------
Balance& Balance::Get()
{
	static Balance s_Instance;
	return s_Instance;
}

void Balance::Init()
{
	static std::once_flag s_Once;
	std::call_once(s_Once, [] { Get().Initialize(); });
}

// -------------------------------------------------------
// Index helper
// -------------------------------------------------------
size_t Balance::ToIndex(AttributeIdentifier ID)
{
	const auto Index = static_cast<size_t>(ID);
	assert(Index < kAttributeCount && "AttributeIdentifier out of range");
	return (Index < kAttributeCount) ? Index : 0u;
}

// -------------------------------------------------------
// Public getters
// -------------------------------------------------------
float Balance::GetAttributeCap(AttributeIdentifier ID) const
{
	return m_AttributeCaps[ToIndex(ID)];
}

int Balance::GetAttributeBase(AttributeIdentifier ID) const
{
	return m_AttributeBase[ToIndex(ID)];
}

float Balance::GetBotGroupPercent(AttributeGroup Group) const
{
	switch (Group)
	{
	case AttributeGroup::DamageType: return m_BotGroupScaling.DamagePercent;
	case AttributeGroup::Dps:        return m_BotGroupScaling.DpsPercent;
	case AttributeGroup::Healer:     return m_BotGroupScaling.HealerPercent;
	default:                         return 100.0f;
	}
}

float Balance::GetBotBossDownscaleDivider() const
{
	return m_BotGroupScaling.BossDownscaleDivider;
}

// -------------------------------------------------------
// Attribute relevance filter
// -------------------------------------------------------
bool Balance::IsAttributeRelevantForMobPower(AttributeIdentifier ID) const
{
	switch (ID)
	{
	case AttributeIdentifier::Crit:
	case AttributeIdentifier::Vampirism:
	case AttributeIdentifier::Lucky:
	case AttributeIdentifier::LuckyDropItem:
	case AttributeIdentifier::AttackSPD:
	case AttributeIdentifier::AmmoRegen:
		return false;
	default:
		return true;
	}
}

// -------------------------------------------------------
// Relevant-attribute cache (lazy initialization)
// -------------------------------------------------------
// Called from Initialize() as a placeholder.
// The actual population happens lazily in CalculateScenarioMobPower
// because we need a valid GameServer context to query attribute metadata.
void Balance::BuildRelevantAttrCache()
{
	m_RelevantMobPowerAttrs.clear();
}

// -------------------------------------------------------
// Scenario mob power calculation
// -------------------------------------------------------
int Balance::CalculateScenarioMobPower(
	const std::vector<CPlayer*>& vpPlayers,
	int BasePower,
	bool IsGroupScenario) const
{
	const auto& Cfg = m_ScenarioMobPowerScaling;
	const int ClampedBase = std::clamp(BasePower, Cfg.MinPower, Cfg.MaxPower);

	if (vpPlayers.empty())
		return ClampedBase;

	// -------------------------------------------------------
	// Lazily build the relevant-attribute list on first call.
	// We need a valid player pointer to access GameServer metadata.
	// const_cast is acceptable here: the cache is logically immutable
	// data (memoization), not observable state.
	// -------------------------------------------------------
	auto& RelevantAttrs = const_cast<Balance*>(this)->m_RelevantMobPowerAttrs;

	if (RelevantAttrs.empty())
	{
		// Find the first non-null player to access GameServer
		const CPlayer* pFirst = nullptr;
		for (const auto* p : vpPlayers)
		{
			if (p) { pFirst = p; break; }
		}

		if (pFirst)
		{
			RelevantAttrs.reserve(kAttributeCount);
			for (int i = static_cast<int>(AttributeIdentifier::DMG);
				i < static_cast<int>(AttributeIdentifier::ATTRIBUTES_NUM);
				++i)
			{
				const auto AttrID = static_cast<AttributeIdentifier>(i);
				if (!IsAttributeRelevantForMobPower(AttrID))
					continue;

				const auto* pInfo = pFirst->GS()->GetAttributeInfo(AttrID);
				if (pInfo &&
					(pInfo->IsGroup(AttributeGroup::Tank) ||
						pInfo->IsGroup(AttributeGroup::Healer) ||
						pInfo->IsGroup(AttributeGroup::Dps)))
				{
					RelevantAttrs.push_back(AttrID);
				}
			}
		}
	}

	// -------------------------------------------------------
	// Gather per-player stat budgets
	// -------------------------------------------------------
	float TotalBudget = 0.0f;
	float MaxBudget = 0.0f;
	int   ValidCount = 0;

	for (const auto* pPlayer : vpPlayers)
	{
		if (!pPlayer)
			continue;

		int64_t Budget = 0;
		for (const auto AttrID : RelevantAttrs)
			Budget += pPlayer->GetTotalRawAttributeValue(AttrID);

		const float fBudget = static_cast<float>(Budget);
		TotalBudget += fBudget;
		MaxBudget = std::max(MaxBudget, fBudget);
		++ValidCount;
	}

	if (ValidCount <= 0)
		return ClampedBase;

	// -------------------------------------------------------
	// Anti-carry formula
	//
	// 1. Blend average and maximum budgets (70/30) so one
	//    overpowered player can't trivialize content.
	// 2. Apply diminishing returns to compress extreme values.
	// 3. Scale the base power by a bonus percentage that
	//    accounts for solo vs group context.
	// -------------------------------------------------------
	const float AvgBudget = TotalBudget / static_cast<float>(ValidCount);
	const float BlendedBudget = AvgBudget * 0.7f + MaxBudget * 0.3f;
	const float Effective = DiminishingReturns(BlendedBudget, Cfg.DiminishingKnee);

	const float Weight = IsGroupScenario
		? Cfg.GroupStatBaseWeight
		: Cfg.SoloStatBaseWeight;

	const float GroupMultiplier = IsGroupScenario
		? std::sqrt(static_cast<float>(ValidCount))
		: 1.0f;

	// BonusPercent represents how much stronger the mob becomes
	const float BonusPercent = (Effective / Cfg.DiminishingKnee) * Weight * GroupMultiplier;
	const float ClampedBonusPercent = std::clamp(BonusPercent, 0.0f, 3.0f);

	const int FinalPower = static_cast<int>(
		std::round(static_cast<float>(ClampedBase) * (1.0f + ClampedBonusPercent)));

	return std::clamp(FinalPower, ClampedBase, Cfg.MaxPower);
}

// -------------------------------------------------------
// Balance data initialization
// -------------------------------------------------------
void Balance::Initialize()
{
	// --- Attribute caps (0 = uncapped) ---
	m_AttributeCaps.fill(0.0f);
	m_AttributeCaps[ToIndex(AttributeIdentifier::AttackSPD)] = 600.0f;
	m_AttributeCaps[ToIndex(AttributeIdentifier::AmmoRegen)] = 800.0f;
	m_AttributeCaps[ToIndex(AttributeIdentifier::Vampirism)] = 30.0f;
	m_AttributeCaps[ToIndex(AttributeIdentifier::Crit)] = 30.0f;
	m_AttributeCaps[ToIndex(AttributeIdentifier::Lucky)] = 20.0f;
	m_AttributeCaps[ToIndex(AttributeIdentifier::LuckyDropItem)] = 30.0f;

	// --- Base attribute values ---
	m_AttributeBase.fill(0);
	m_AttributeBase[ToIndex(AttributeIdentifier::HP)] = 10;
	m_AttributeBase[ToIndex(AttributeIdentifier::MP)] = 10;

	// --- Bot group scaling ---
	m_BotGroupScaling = {
		/*.DamagePercent=*/        2.5f,
		/*.DpsPercent=*/          10.0f,
		/*.HealerPercent=*/       20.0f,
		/*.BossDownscaleDivider=*/30.0f,
	};

	// --- Scenario mob power scaling ---
	m_ScenarioMobPowerScaling = {
		/*.MinPower=*/             1,
		/*.MaxPower=*/         10000,
		/*.SoloStatBaseWeight=*/   0.35f,
		/*.GroupStatBaseWeight=*/  0.25f,
		/*.DiminishingKnee=*/   1500.0f,
	};

	BuildRelevantAttrCache();
}