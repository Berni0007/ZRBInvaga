// ZRBinvaga — тикеты атаки и обороны.
// Тикет списывается при респауне (первый спавн игрока в матче бесплатный).
// Ранение и поднятие — штатные: пока игрок лежит без сознания, тикет не тратится.
// За уничтоженную технику платит сторона, которой она принадлежит.

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

	[Attribute("15", UIWidgets.EditBox, "Потеря танка", params: "0 inf", category: "Тикеты за технику")]
	protected int m_iTankLossCost;

	[Attribute("10", UIWidgets.EditBox, "Потеря вертолёта", params: "0 inf", category: "Тикеты за технику")]
	protected int m_iHelicopterLossCost;

	[Attribute("10", UIWidgets.EditBox, "Потеря БТР, БМП или Bradley", params: "0 inf", category: "Тикеты за технику")]
	protected int m_iMediumVehicleLossCost;

	[Attribute("20", UIWidgets.EditBox, "Потеря КШМ", params: "0 inf", category: "Тикеты за технику")]
	protected int m_iCommandVehicleLossCost;

	[Attribute("5", UIWidgets.EditBox, "Потеря транспорта снабжения, невооружённой техники, Тигра или MRAP", params: "0 inf", category: "Тикеты за технику")]
	protected int m_iLightVehicleLossCost;

	[RplProp(onRplName: "OnTicketsChanged")]
	protected int m_iAttackerTickets;

	[RplProp(onRplName: "OnTicketsChanged")]
	protected int m_iDefenderTickets;

	//! Игроки, которые уже спавнились в этом матче (сервер).
	protected ref set<int> m_SpawnedPlayers = new set<int>();
	protected bool m_bTicketsInitialized;

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
		m_bTicketsInitialized = true;
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
	//! Сервер: списать тикеты у фракции.
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

	void SpendVehicleLoss(notnull Faction faction, notnull Vehicle vehicle)
	{
		ZW_ObjectivesComponent objectives = ZW_Invasion.GetObjectives();
		if (!Replication.IsServer() || !objectives || objectives.IsEnded())
			return;

		ResourceName prefab;
		EntityPrefabData prefabData = vehicle.GetPrefabData();
		if (prefabData)
			prefab = prefabData.GetPrefabName();

		int cost = GetVehicleLossCost(vehicle, prefab);
		if (cost <= 0)
			return;

		Spend(faction, cost);
		PrintFormat("[ZW_Invasion] Потеря техники %1, сторона %2, тикеты -%3", prefab, faction.GetFactionKey(), cost);
	}

	//------------------------------------------------------------------------------------------------
	protected int GetVehicleLossCost(notnull Vehicle vehicle, ResourceName prefabName)
	{
		if (vehicle.FindComponent(ZW_KshmComponent) || vehicle.m_eVehicleType == EVehicleType.COMM_TRUCK)
			return m_iCommandVehicleLossCost;

		if (HelicopterControllerComponent.Cast(vehicle.GetVehicleController()))
			return m_iHelicopterLossCost;

		string prefab = FilePath.StripPath(prefabName);
		prefab.ToLower();
		if (prefab.StartsWith("type59") || prefab.StartsWith("m1a1") || prefab.StartsWith("m1a2") || prefab.Contains("abrams") || prefab.StartsWith("t90") || prefab.StartsWith("t72") || prefab.StartsWith("t80") || prefab.StartsWith("leopard"))
			return m_iTankLossCost;

		if (vehicle.m_eVehicleType == EVehicleType.SUPPLY_TRUCK || prefab.Contains("tigr") || prefab.Contains("gaz233") || prefab.Contains("oshkosh") || prefab.StartsWith("m-atv") || prefab.StartsWith("matv") || prefab.Contains("mrap") || prefab.Contains("cougar") || prefab.Contains("maxxpro") || prefab.StartsWith("m122") || prefab.StartsWith("m1240"))
			return m_iLightVehicleLossCost;

		SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.GetEditableEntity(vehicle);
		SCR_EditableEntityUIInfo info;
		if (editable)
			info = SCR_EditableEntityUIInfo.Cast(editable.GetInfo());

		if (info && info.HasEntityLabel(EEditableEntityLabel.VEHICLE_HELICOPTER))
			return m_iHelicopterLossCost;

		if (vehicle.m_eVehicleType == EVehicleType.APC || (info && info.HasEntityLabel(EEditableEntityLabel.VEHICLE_APC)) || prefab.StartsWith("btr") || prefab.StartsWith("bmp") || prefab.StartsWith("m2a") || prefab.StartsWith("m3a") || prefab.Contains("bradley"))
			return m_iMediumVehicleLossCost;

		if (info && info.HasEntityLabel(EEditableEntityLabel.TRAIT_UNARMED))
			return m_iLightVehicleLossCost;

		if (info && info.HasEntityLabel(EEditableEntityLabel.TRAIT_ARMED))
			return 0;

		SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!compartments)
			return 0;

		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (TurretCompartmentSlot.Cast(slot))
				return 0;
		}

		if (!slots.IsEmpty())
			return m_iLightVehicleLossCost;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! На клиентах: тикеты обновились (HUD на этапе 3).
	protected void OnTicketsChanged()
	{
	}
}
