#ifndef GAME_SERVER_CORE_CASINO_ENTITIES_ROULETTE_ARROW_H
#define GAME_SERVER_CORE_CASINO_ENTITIES_ROULETTE_ARROW_H

#include <game/server/entity.h>

class CEntityRouletteArrow : public CEntity
{
public:
	using FSpinFinishedCallback = std::function<void(int Number)>;

	CEntityRouletteArrow(CGameWorld* pGameWorld, int OwnerCID, vec2 Pos);

	void Tick() override;
	void Snap(int SnappingClient) override;

	void Spin(int DurationTicks, int FinalNumber, FSpinFinishedCallback pfnCallback);
	bool IsSpinning() const { return m_IsSpinning; }
	bool IsFading() const { return m_IsFading; }

	void Destroy() { GameWorld()->DestroyEntity(this); }

private:
	float m_Angle{};
	int m_DurationTicks{};
	int m_RemainingTicks{};
	int m_FinalNumber{};
	float m_StartAngle{};
	float m_TotalRotation{};
	bool m_IsSpinning{};
	bool m_IsFading{};
	int m_FadeTicks{};
	int m_FadeRemaining{};
	int m_TargetSlotIndex{};
	FSpinFinishedCallback m_pfnOnSpinFinished;

	static float EaseInOutSine(float t);
};

#endif