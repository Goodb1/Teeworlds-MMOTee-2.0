#ifndef GAME_SERVER_CORE_CASINO_DICE_DUEL_GAME_H
#define GAME_SERVER_CORE_CASINO_DICE_DUEL_GAME_H

#include "casino_game.h"

enum class EDiceBetType { HIGH = 0, LOW = 1, EQUAL = 2 };
enum class EDiceGameState { LOBBY, ROLLING, SHOW_RESULT };

class CGS;
class CEntityDiceLaser;
class CDiceDuelGame : public CCasinoGame
{
	CGS* m_pGS{};
	vec2 m_Pos{};

public:
	using SlotBet = CCasinoGame::SlotBet;

	struct DiceResult
	{
		int m_Face1{};
		int m_Face2{};
		int m_Sum{};
		std::vector<SlotBet> m_vPlayers{};
	};

	using FOnEvent = CCasinoGame::FOnEvent;
	using FOnStart = CCasinoGame::FOnStart;
	using FOnFinished = std::function<void(const DiceResult&)>;

private:
	CEntityDiceLaser* m_apDice[2]{};
	EDiceGameState m_State = EDiceGameState::LOBBY;
	int m_aFaces[2]{};
	int m_FinishedDice{};

	FOnFinished m_pfnOnFinished;

	void EnterLobby();
	void EnterRolling();
	void EnterShowResult();
	void PurgeOutOfRange();

public:
	CDiceDuelGame(CGS* pGS, vec2 Pos, int LobbyDurationTicks = 15, int ShowResultTicks = 4);
	~CDiceDuelGame();

	void Tick();

	bool Join(int ClientID, int Bet, EDiceBetType Type, int Threshold);
	bool Leave(int ClientID, int* pRefund = nullptr);
	bool IsPlayerJoined(int ClientID) const { return CCasinoGame::IsPlayerJoined(ClientID); }

	void SetOnFinished(FOnFinished pfCallback) { m_pfnOnFinished = std::move(pfCallback); }

	EDiceGameState GetState() const { return m_State; }
	vec2 GetPos() const { return m_Pos; }
	int GetPlayersCount() const { return CCasinoGame::GetPlayersCount(); }
	int GetTicksLeft() const;

	static float GetWinProbability(EDiceBetType Type, int Threshold);
	static float GetMultiplier(EDiceBetType Type, int Threshold);
	static int CalcPayout(int Bet, EDiceBetType Type, int Threshold);
	static bool IsValidThreshold(EDiceBetType Type, int Threshold);
	static const char* BetTypeName(EDiceBetType Type);
};

#endif // GAME_SERVER_CORE_CASINO_DICE_DUEL_GAME_H