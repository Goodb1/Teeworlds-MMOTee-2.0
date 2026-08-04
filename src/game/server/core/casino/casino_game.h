#ifndef GAME_SERVER_CORE_CASINO_CASINO_GAME_H
#define GAME_SERVER_CORE_CASINO_CASINO_GAME_H

class CGS;
class CPlayer;
class CCasinoGame
{
protected:
	CGS* m_pGS{};

public:
	struct SlotBet
	{
		int m_ClientID{};
		int m_AccountID{};
		int m_Currency{};
		int m_Bet{};
		int m_Type{};
		int m_Threshold{};
		bool m_Win{};
		int m_Payout{};
	};

	struct GameResult
	{
		std::vector<SlotBet> m_vPlayers{};
	};

	using FOnEvent = std::function<void(int ClientID, const SlotBet&)>;
	using FOnStart = std::function<void()>;
	using FOnFinished = std::function<void(const GameResult&)>;

protected:
	int m_StateEndTick{};
	int m_LobbyDurationTicks{};
	int m_ShowResultTicks{};

private:
	std::vector<SlotBet> m_vPlayers{};
	FOnEvent m_pfnOnJoin;
	FOnEvent m_pfnOnLeave;
	FOnStart m_pfnOnStart;
	FOnFinished m_pfnOnFinished;

public:
	CCasinoGame(CGS* pGS, int LobbyDurationTicks = 15, int ShowResultTicks = 4);
	virtual ~CCasinoGame();

	virtual void Tick() { }
	virtual vec2 GetPos() const { return vec2(0,0); }

	bool Join(int ClientID, int Bet, int Type, int Threshold, int CurrencyItemID);
	bool Leave(int ClientID, int* pRefund = nullptr);
	void SettleBet(SlotBet& Bet);
	void PurgeOutOfRange();

	bool IsPlayerJoined(int ClientID) const { return std::any_of(m_vPlayers.begin(), m_vPlayers.end(), [&](const SlotBet& b) { return b.m_ClientID == ClientID; }); }
	int GetPlayersCount() const { return (int)m_vPlayers.size(); }
	int GetTicksLeft() const;

	const std::vector<SlotBet>& GetPlayers() const { return m_vPlayers; }
	std::vector<SlotBet>& GetPlayers() { return m_vPlayers; }
	void ClearPlayers() { m_vPlayers.clear(); }

	// callbacks
	void SetOnJoin(FOnEvent pfCallback) { m_pfnOnJoin = std::move(pfCallback); }
	void SetOnLeave(FOnEvent pfCallback) { m_pfnOnLeave = std::move(pfCallback); }
	void SetOnStart(FOnStart pfCallback) { m_pfnOnStart = std::move(pfCallback); }
	void SetOnFinished(FOnFinished pfCallback) { m_pfnOnFinished = std::move(pfCallback); }

protected:
	// helpers for game impl to invoke
	void NotifyStart() { if (m_pfnOnStart) m_pfnOnStart(); }
	void NotifyFinished(const GameResult& Result) { if (m_pfnOnFinished) m_pfnOnFinished(Result); }
	void NotifyJoin(int ClientID, const SlotBet& Bet) { if (m_pfnOnJoin) m_pfnOnJoin(ClientID, Bet); }
	void NotifyLeave(int ClientID, const SlotBet& Bet) { if (m_pfnOnLeave) m_pfnOnLeave(ClientID, Bet); }
	void AwardToAccount(int AccountID, int CurrencyItemID, int Amount);
};

#endif // GAME_SERVER_CORE_CASINO_CASINO_GAME_H
