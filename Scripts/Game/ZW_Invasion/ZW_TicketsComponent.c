// ZRBinvaga — тикеты атаки и обороны.
// Тикет списывается при респауне (первый спавн игрока в матче бесплатный).
// Ранение и поднятие — штатные: пока игрок лежит без сознания, тикет не тратится.

[ComponentEditorProps(category: "ZRB/Invasion", description: "Invasion: тикеты атаки и обороны")]
class ZW_TicketsComponentClass : SCR_BaseGameModeComponentClass
{
}

class ZW_TicketsComponent : SCR_BaseGameModeComponent
{
	[Attribute("300", UIWidgets.EditBox, "Тикеты атаки на старте", category: "Invasion")]
	protected int m_iAttackerStartTickets;

	[Attribute("800", UIWidgets.EditBox, "Тикеты обороны на старте", category: "Invasion")]
	protected int m_iDefenderStartTickets;

	[Attribute("100", UIWidgets.EditBox, "Бонус атаке за взятую точку", category: "Invasion")]
	protected int m_iCaptureBonus;

	[Attribute("1", UIWidgets.EditBox, "Цена респауна в тикетах", category: "Invasion")]
	protected int m_iRespawnCost;

	[RplProp(onRplName: "OnTicketsChanged")]
	protected int m_iAttackerTickets;

	[RplProp(onRplName: "OnTicketsChanged")]
	protected int m_iDefenderTickets;

	//! Игроки, которые уже спавнились в этом матче (сервер).
	protected ref set<int> m_SpawnedPlayers = new set<int>();

	//------------------------------------------------------------------------------------------------
	int GetAttackerTickets()
	{
		return m_iAttackerTickets;
	}

	//------------------------------------------------------------------------------------------------
	int GetDefenderTickets()
	{
		return m_iDefenderTickets;
	}

	//------------------------------------------------------------------------------------------------
	override void OnGameModeStart()
	{
		super.OnGameModeStart();

		if (!Replication.IsServer())
			return;

		m_iAttackerTickets = m_iAttackerStartTickets;
		m_iDefenderTickets = m_iDefenderStartTickets;
		m_SpawnedPlayers.Clear();
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerSpawnFinalize_S(SCR_SpawnRequestComponent requestComponent, SCR_SpawnHandlerComponent handlerComponent, SCR_SpawnData data, IEntity entity)
	{
		super.OnPlayerSpawnFinalize_S(requestComponent, handlerComponent, data, entity);

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		if (playerId <= 0)
			return;

		// Первый спавн в матче бесплатный.
		if (!m_SpawnedPlayers.Contains(playerId))
		{
			m_SpawnedPlayers.Insert(playerId);
			return;
		}

		Spend(ZW_Invasion.GetEntityFaction(entity), m_iRespawnCost);
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер: бонус атаке за взятую точку.
	void AddCaptureBonus()
	{
		if (!Replication.IsServer())
			return;

		m_iAttackerTickets += m_iCaptureBonus;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер: оборона потеряла последнюю точку.
	void ZeroDefenderTickets()
	{
		if (!Replication.IsServer())
			return;

		m_iDefenderTickets = 0;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер: списать тикеты у фракции (респаун, а на этапе 5 — потеря техники).
	void Spend(Faction faction, int amount)
	{
		if (!Replication.IsServer() || !faction || amount <= 0)
			return;

		ZW_ObjectivesComponent objectives = ZW_Invasion.GetObjectives();
		if (!objectives || objectives.IsEnded())
			return;

		if (faction == objectives.GetAttackerFaction())
		{
			m_iAttackerTickets = Math.Max(0, m_iAttackerTickets - amount);
			Replication.BumpMe();

			if (m_iAttackerTickets == 0)
				objectives.EndInvasion(false, EGameOverTypes.ENDREASON_SCORELIMIT);
		}
		else if (faction == objectives.GetDefenderFaction())
		{
			m_iDefenderTickets = Math.Max(0, m_iDefenderTickets - amount);
			Replication.BumpMe();

			if (m_iDefenderTickets == 0)
				objectives.EndInvasion(true, EGameOverTypes.ENDREASON_SCORELIMIT);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! На клиентах: тикеты обновились (HUD на этапе 3).
	protected void OnTicketsChanged()
	{
	}
}
