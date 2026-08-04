#include "dice_laser.h"

#include <game/server/gamecontext.h>

enum
{
	DICE_SNAP_GROUP = 0,
	MAX_DICE_FACE_LINES = 10,
	ROLL_UPDATE_RATE = 2,
};

struct SDiceLine 
{ 
	vec2 m_From;
	vec2 m_To; 
};

struct SDiceFace 
{ 
	int m_NumLines; 
	SDiceLine m_aLines[MAX_DICE_FACE_LINES]; 
};

static constexpr float DOT_LEN = 2.0f;
#define DOT(x, y) {{(x) - DOT_LEN * 0.5f, (y)}, {(x) + DOT_LEN * 0.5f, (y)}}
static const SDiceFace gs_aDiceFaces[6] =
{
	// 1
	{ 5, {
		{{-32.0f, -32.0f}, { 32.0f, -32.0f}},
		{{-32.0f,  32.0f}, { 32.0f,  32.0f}},
		{{-32.0f, -32.0f}, {-32.0f,  32.0f}},
		{{ 32.0f, -32.0f}, { 32.0f,  32.0f}},
		DOT(0.0f,   0.0f),
	}},
	// 2
	{ 6, {
		{{-32.0f, -32.0f}, { 32.0f, -32.0f}},
		{{-32.0f,  32.0f}, { 32.0f,  32.0f}},
		{{-32.0f, -32.0f}, {-32.0f,  32.0f}},
		{{ 32.0f, -32.0f}, { 32.0f,  32.0f}},
		DOT(16.0f, -16.0f),
		DOT(-16.0f,  16.0f),
	}},
	// 3
	{ 7, {
		{{-32.0f, -32.0f}, { 32.0f, -32.0f}},
		{{-32.0f,  32.0f}, { 32.0f,  32.0f}},
		{{-32.0f, -32.0f}, {-32.0f,  32.0f}},
		{{ 32.0f, -32.0f}, { 32.0f,  32.0f}},
		DOT(16.0f, -16.0f),
		DOT(0.0f,   0.0f),
		DOT(-16.0f,  16.0f),
	}},
	// 4
	{ 8, {
		{{-32.0f, -32.0f}, { 32.0f, -32.0f}},
		{{-32.0f,  32.0f}, { 32.0f,  32.0f}},
		{{-32.0f, -32.0f}, {-32.0f,  32.0f}},
		{{ 32.0f, -32.0f}, { 32.0f,  32.0f}},
		DOT(-16.0f, -16.0f),
		DOT(16.0f, -16.0f),
		DOT(-16.0f,  16.0f),
		DOT(16.0f,  16.0f),
	}},
	// 5
	{ 9, {
		{{-32.0f, -32.0f}, { 32.0f, -32.0f}},
		{{-32.0f,  32.0f}, { 32.0f,  32.0f}},
		{{-32.0f, -32.0f}, {-32.0f,  32.0f}},
		{{ 32.0f, -32.0f}, { 32.0f,  32.0f}},
		DOT(-16.0f, -16.0f),
		DOT(16.0f, -16.0f),
		DOT(0.0f,   0.0f),
		DOT(-16.0f,  16.0f),
		DOT(16.0f,  16.0f),
	}},
	// 6
	{ 10, {
		{{-32.0f, -32.0f}, { 32.0f, -32.0f}},
		{{-32.0f,  32.0f}, { 32.0f,  32.0f}},
		{{-32.0f, -32.0f}, {-32.0f,  32.0f}},
		{{ 32.0f, -32.0f}, { 32.0f,  32.0f}},
		DOT(-16.0f, -16.0f),
		DOT(16.0f, -16.0f),
		DOT(-16.0f,   0.0f),
		DOT(16.0f,   0.0f),
		DOT(-16.0f,  16.0f),
		DOT(16.0f,  16.0f),
	}},
};
#undef DOT

int CEntityDiceLaser::RandomFace()
{
	return clamp(1 + (int)(random_float() * 6.0f), 1, 6);
}

CEntityDiceLaser::CEntityDiceLaser(CGameWorld* pGameWorld, int OwnerCID, vec2 Pos, int InitialFace, float Scale)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_VISUAL, Pos, 0, OwnerCID), m_CurrentFace(clamp(InitialFace, 1, 6))
	, m_FinalFace(m_CurrentFace), m_RollTicks(0), m_IsRolling(false), m_Scale(maximum(0.1f, Scale)), m_pfnOnRollFinished(nullptr)
{
	AddSnappingGroupIds(DICE_SNAP_GROUP, MAX_DICE_FACE_LINES);
	GameWorld()->InsertEntity(this);
}

void CEntityDiceLaser::SetFace(int Face)
{
	m_CurrentFace = clamp(Face, 1, 6);
	m_FinalFace = m_CurrentFace;
	m_RollTicks = 0;
	m_IsRolling = false;
}

void CEntityDiceLaser::Roll(int DurationTicks, int FinalFace)
{
	const int Face = (FinalFace >= 1 && FinalFace <= 6) ? FinalFace : RandomFace();

	if (DurationTicks <= 0)
	{
		SetFace(Face);
		if (m_pfnOnRollFinished)
		{
			auto cb = std::move(m_pfnOnRollFinished);
			m_pfnOnRollFinished = nullptr;
			cb(m_CurrentFace);
		}
		return;
	}

	m_FinalFace = Face;
	m_RollTicks = DurationTicks;
	m_IsRolling = true;
	m_CurrentFace = RandomFace();
}

void CEntityDiceLaser::Tick()
{
	if (!m_IsRolling)
		return;

	if ((Server()->Tick() % ROLL_UPDATE_RATE) == 0)
	{
		int NewFace = RandomFace();
		if (NewFace == m_CurrentFace)
			NewFace = (NewFace % 6) + 1;
		m_CurrentFace = NewFace;
		GS()->CreateSound(m_Pos, SOUND_WEAPON_SWITCH);
	}

	if (--m_RollTicks <= 0)
	{
		m_CurrentFace = m_FinalFace;
		m_IsRolling = false;

		if (m_pfnOnRollFinished)
		{
			auto cb = std::move(m_pfnOnRollFinished);
			m_pfnOnRollFinished = nullptr;
			cb(m_CurrentFace);
			GS()->CreateFinishEffect(m_Pos);
		}
	}
}

void CEntityDiceLaser::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	if (m_CurrentFace < 1 || m_CurrentFace > 6)
		return;

	const auto* pGroupIds = FindSnappingGroupIds(DICE_SNAP_GROUP);
	if (!pGroupIds)
		return;

	const SDiceFace& Face = gs_aDiceFaces[m_CurrentFace - 1];
	for (int i = 0; i < Face.m_NumLines; i++)
	{
		const vec2 From = m_Pos + Face.m_aLines[i].m_From * m_Scale;
		const vec2 To = m_Pos + Face.m_aLines[i].m_To * m_Scale;
		GS()->SnapLaser(SnappingClient, (*pGroupIds)[i], From, To, Server()->Tick(), LASERTYPE_DOOR);
	}
}