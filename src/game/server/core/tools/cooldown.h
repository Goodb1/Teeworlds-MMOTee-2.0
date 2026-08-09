#ifndef GAME_SERVER_CORE_UTILITIES_COOLDOWN_H
#define GAME_SERVER_CORE_UTILITIES_COOLDOWN_H

class CGS;
class IServer;
class CPlayer;
class CCharacter;
using CCooldownCallback = std::function<void()>;

class CCooldown
{
public:
	CCooldown() = default;

	void Init(int ClientID);
	void Start(int DurationTicks, std::string_view Name, CCooldownCallback fnCallback);
	void Reset();
	void Tick();

	[[nodiscard]] bool IsActive() const { return m_Active; }

private:
	static constexpr float kInterruptDistance = 48.0f;
	static constexpr int kProgressUpdatesPerSecond = 25;
	static constexpr int kProgressBarSegments = 10;

	int m_ClientID{ NOPE };
	std::string m_Name{};
	vec2 m_StartPos{};
	int m_Tick{};
	int m_StartedTick{};
	CCooldownCallback m_Callback{};
	bool m_Active{};

	[[nodiscard]] bool IsClientIDValid() const;
	[[nodiscard]] CGS* GetGameServer() const;
	[[nodiscard]] bool HasPlayerMoved(const CCharacter* pChar) const;

	void Finish();
	void Interrupt(const char* pReason);
	void BroadcastCooldownInfo(const char* pMessage = "") const;
	void BroadcastCooldownProgress(IServer* pServer) const;
};

#endif // GAME_SERVER_CORE_UTILITIES_COOLDOWN_H