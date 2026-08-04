#ifndef GAME_SERVER_CORE_CASINO_ENTITIES_DICE_LASER_H
#define GAME_SERVER_CORE_CASINO_ENTITIES_DICE_LASER_H

#include <game/server/entity.h>

class CEntityDiceLaser : public CEntity
{
public:
	using FRollFinishedCallback = std::function<void(int Face)>;

private:
	int m_CurrentFace;
	int m_FinalFace;
	int m_RollTicks;
	bool m_IsRolling;
	float m_Scale;
	FRollFinishedCallback m_pfnOnRollFinished;
	static int RandomFace();

public:
	CEntityDiceLaser(CGameWorld* pGameWorld, int OwnerCID, vec2 Pos, int InitialFace = 1, float Scale = 1.0f);

	void Tick() override;
	void Snap(int SnappingClient) override;

	void SetFace(int Face);
	int GetFace() const { return m_CurrentFace; }
	void Roll(int DurationTicks, int FinalFace = -1);

	bool IsRolling() const { return m_IsRolling; }
	void SetOnRollFinished(FRollFinishedCallback pfnCallback) { m_pfnOnRollFinished = std::move(pfnCallback); }

	void Destroy() { GameWorld()->DestroyEntity(this); }
	void SetPos(vec2 Pos) { m_Pos = Pos; }
	void SetScale(float Scale) { m_Scale = maximum(0.1f, Scale); }
	float GetScale() const { return m_Scale; }
};

#endif // GAME_SERVER_CORE_CASINO_ENTITIES_DICE_LASER_H