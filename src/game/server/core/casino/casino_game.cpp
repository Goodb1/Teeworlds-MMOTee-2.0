#include "casino_game.h"

#include <components/mails/mail_wrapper.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

CCasinoGame::CCasinoGame(CGS* pGS, int LobbyDurationTicks, int ShowResultTicks)
	: m_pGS(pGS), m_LobbyDurationTicks(LobbyDurationTicks), m_ShowResultTicks(ShowResultTicks)
{
}

CCasinoGame::~CCasinoGame()
{
}

bool CCasinoGame::Join(int ClientID, int Bet, int Type, int Threshold, int CurrencyItemID)
{
	if (Bet <= 0)
		return false;

	auto* pPlayer = m_pGS->GetPlayer(ClientID, true);
	if (!pPlayer || !pPlayer->GetCharacter())
		return false;

	if (std::any_of(m_vPlayers.begin(), m_vPlayers.end(), [&](const SlotBet& bet) { return bet.m_ClientID == ClientID; }))
		return false;

	if (!pPlayer->Account()->SpendCurrency(Bet, CurrencyItemID))
		return false;

	SlotBet slotBet{ ClientID, pPlayer->Account()->GetID(), CurrencyItemID, Bet, Type, Threshold };
	m_vPlayers.push_back(slotBet);
	NotifyJoin(ClientID, slotBet);
	return true;
}

bool CCasinoGame::Leave(int ClientID, int* pRefund)
{
	auto it = std::find_if(m_vPlayers.begin(), m_vPlayers.end(), [&](const SlotBet& b) { return b.m_ClientID == ClientID; });
	if (it == m_vPlayers.end())
		return false;

	const int Refund = it->m_Bet;
	SettleBet(*it);

	SlotBet Copy = *it;
	m_vPlayers.erase(it);
	NotifyLeave(ClientID, Copy);
	if (pRefund)
		*pRefund = Refund;
	return true;
}

void CCasinoGame::SettleBet(SlotBet& Bet)
{
	const int Value = Bet.m_Win ? Bet.m_Payout : Bet.m_Bet;
	if (Value <= 0)
		return;

	if (auto* pPlayer = m_pGS->GetPlayerByUserID(Bet.m_AccountID))
	{
		pPlayer->GetItem(Bet.m_Currency)->Add(Value);
		return;
	}

	// player offline - send by mail
	MailWrapper Mail("System", Bet.m_AccountID, "[Casino] Payout");
	Mail.AddDescLine("Item was not received by you personally.");
	Mail.AttachItem(CItem(Bet.m_Currency, Value));
	Mail.Send();
}

void CCasinoGame::PurgeOutOfRange()
{
	for (auto it = m_vPlayers.begin(); it != m_vPlayers.end(); )
	{
		auto* pPlayer = m_pGS->GetPlayer(it->m_ClientID, true);
		const bool Alive = pPlayer && pPlayer->GetCharacter();

		if (!Alive)
		{
			SettleBet(*it);
			NotifyLeave(it->m_ClientID, *it);
			it = m_vPlayers.erase(it);
			continue;
		}
		++it;
	}
}

int CCasinoGame::GetTicksLeft() const
{
	return maximum(0, m_StateEndTick - m_pGS->Server()->Tick());
}

void CCasinoGame::AwardToAccount(int AccountID, int CurrencyItemID, int Amount)
{
	if (Amount <= 0)
		return;

	if (auto* pPlayer = m_pGS->GetPlayerByUserID(AccountID))
	{
		pPlayer->GetItem(CurrencyItemID)->Add(Amount);
		return;
	}

	MailWrapper Mail("System", AccountID, "[Casino] Reward");
	Mail.AddDescLine("Item was not received by you personally.");
	Mail.AttachItem(CItem(CurrencyItemID, Amount));
	Mail.Send();
}
