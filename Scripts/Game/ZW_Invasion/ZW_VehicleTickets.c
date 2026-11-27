modded class SCR_VehicleDamageManagerComponent
{
	protected Faction m_ZWTicketOwner;
	protected bool m_bZWVehicleLossCounted;

	// При заказе в депо сторона известна даже у префабов без своей фракции.
	void ZW_SetTicketOwner(Faction faction)
	{
		m_ZWTicketOwner = ZW_GetMatchFaction(faction);
	}

	// Нейтральная машина относится к стороне первого водителя или пилота.
	void ZW_ClaimNeutralVehicle(IEntity driver)
	{
		if (!ZW_Invasion.GetTickets() || ZW_GetTicketOwner())
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(driver);
		Faction faction;
		if (playerId > 0)
			faction = SCR_FactionManager.SGetPlayerFaction(playerId);
		if (!faction)
			faction = ZW_Invasion.GetEntityFaction(driver);

		ZW_SetTicketOwner(faction);
	}

	protected Faction ZW_GetTicketOwner()
	{
		if (m_ZWTicketOwner)
			return m_ZWTicketOwner;

		IEntity vehicle = GetOwner();
		ZW_KshmComponent kshm = ZW_KshmComponent.Cast(vehicle.FindComponent(ZW_KshmComponent));
		if (kshm)
			return ZW_GetMatchFaction(GetGame().GetFactionManager().GetFactionByKey(kshm.GetFactionKey()));

		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(vehicle.FindComponent(FactionAffiliationComponent));
		if (affiliation)
		{
			Faction faction = ZW_GetMatchFaction(affiliation.GetDefaultAffiliatedFaction());
			if (faction)
				return faction;
		}

		SCR_EditableVehicleComponent editable = SCR_EditableVehicleComponent.Cast(vehicle.FindComponent(SCR_EditableVehicleComponent));
		if (editable)
		{
			SCR_EditableEntityUIInfo info = SCR_EditableEntityUIInfo.Cast(editable.GetInfo());
			if (info)
			{
				Faction faction = ZW_GetMatchFaction(info.GetFaction());
				if (faction)
					return faction;
			}
		}

		return ZW_GetMatchFaction(ZW_Invasion.GetEntityFaction(vehicle));
	}

	protected Faction ZW_GetMatchFaction(Faction faction)
	{
		if (!faction)
			return null;

		ZW_ObjectivesComponent objectives = ZW_Invasion.GetObjectives();
		FactionManager factions = GetGame().GetFactionManager();
		if (!objectives || !factions)
			return null;

		string key = faction.GetFactionKey();
		if (key == "RHS_USAF" || key == "RHS_USMC")
			faction = factions.GetFactionByKey("US");
		else if (key == "RHS_AFRF")
			faction = factions.GetFactionByKey("USSR");

		if (faction == objectives.GetAttackerFaction() || faction == objectives.GetDefenderFaction())
			return faction;

		return null;
	}

	override protected void OnDamageStateChanged(EDamageState newState, EDamageState previousDamageState, bool isJIP)
	{
		ZW_TicketsComponent tickets;
		Faction faction;
		Vehicle vehicle = Vehicle.Cast(GetOwner());
		if (Replication.IsServer() && !isJIP && !m_bZWVehicleLossCounted && newState == EDamageState.DESTROYED && previousDamageState != EDamageState.DESTROYED && vehicle)
		{
			tickets = ZW_Invasion.GetTickets();
			if (tickets)
			{
				m_bZWVehicleLossCounted = true;
				faction = ZW_GetTicketOwner();
			}
		}

		super.OnDamageStateChanged(newState, previousDamageState, isJIP);
		if (tickets && faction && vehicle)
			tickets.SpendVehicleLoss(faction, vehicle);
	}
}

modded class SCR_VehicleFactionAffiliationComponent
{
	override protected void OnCompartmentEntered(IEntity vehicle, IEntity occupant, BaseCompartmentSlot compartment, bool move)
	{
		// Запоминаем сторону до того, как штатная логика сменит текущую фракцию машины.
		if (Replication.IsServer() && vehicle && occupant && compartment && compartment.GetType() == ECompartmentType.PILOT)
		{
			SCR_VehicleDamageManagerComponent damage = SCR_VehicleDamageManagerComponent.Cast(vehicle.FindComponent(SCR_VehicleDamageManagerComponent));
			if (damage)
				damage.ZW_ClaimNeutralVehicle(occupant);
		}
		super.OnCompartmentEntered(vehicle, occupant, compartment, move);
	}
}
