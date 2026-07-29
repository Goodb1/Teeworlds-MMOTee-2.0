#ifndef GAME_SERVER_CORE_ENTITIES_CASINO_DICE_DUEL_GAME_H
#define GAME_SERVER_CORE_ENTITIES_CASINO_DICE_DUEL_GAME_H

enum class EDiceBetType { HIGH = 0, LOW = 1, EQUAL = 2 };
enum class EDiceGameState { LOBBY, ROLLING, SHOW_RESULT };

class CGS;
class CEntityDiceLaser;
class CDiceDuelGame
{
	CGS* m_pGS{};
	vec2 m_Pos{};

public:
	struct SlotBet
	{
		int m_ClientID{};
		int m_AccountID{};
		int m_Currency{};
		int m_Bet{};
		EDiceBetType m_Type{};
		int m_Threshold{};
		bool m_Win{};
		int m_Payout{};
	};

	struct DiceResult
	{
		int m_Face1{};
		int m_Face2{};
		int m_Sum{};
		std::vector<SlotBet> m_vPlayers{};
	};

	using FOnEvent = std::function<void(int ClientID, const SlotBet&)>;
	using FOnStart = std::function<void()>;
	using FOnFinished = std::function<void(const DiceResult&)>;

private:
	int m_LobbyDurationTicks{};
	int m_ShowResultTicks{};
	CEntityDiceLaser* m_apDice[2]{};
	EDiceGameState m_State = EDiceGameState::LOBBY;
	int m_StateEndTick{};
	std::vector<SlotBet> m_vPlayers{};
	int m_aFaces[2]{};
	int m_FinishedDice{};

	FOnEvent m_pfnOnJoin;
	FOnEvent m_pfnOnLeave;
	FOnStart m_pfnOnStart;
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
	void SettleBet(SlotBet& Bet);
	bool IsPlayerJoined(int ClientID) const { return std::any_of(m_vPlayers.begin(), m_vPlayers.end(), [&](const SlotBet& b) { return b.m_ClientID == ClientID; }); }

	void SetOnJoin(FOnEvent pfCallback) { m_pfnOnJoin = std::move(pfCallback); }
	void SetOnLeave(FOnEvent pfCallback) { m_pfnOnLeave = std::move(pfCallback); }
	void SetOnStart(FOnStart pfCallback) { m_pfnOnStart = std::move(pfCallback); }
	void SetOnFinished(FOnFinished pfCallback) { m_pfnOnFinished = std::move(pfCallback); }

	EDiceGameState GetState() const { return m_State; }
	vec2 GetPos() const { return m_Pos; }
	int GetPlayersCount() const { return (int)m_vPlayers.size(); }
	int GetTicksLeft() const;

	static float GetWinProbability(EDiceBetType Type, int Threshold);
	static float GetMultiplier(EDiceBetType Type, int Threshold);
	static int CalcPayout(int Bet, EDiceBetType Type, int Threshold);
	static bool IsValidThreshold(EDiceBetType Type, int Threshold);
	static const char* BetTypeName(EDiceBetType Type);
};

#endif // GAME_SERVER_CORE_ENTITIES_CASINO_DICE_DUEL_GAME_H