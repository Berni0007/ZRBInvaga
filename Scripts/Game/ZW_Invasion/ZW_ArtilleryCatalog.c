modded class SCR_Faction
{
	protected const ResourceName ZW_M777_DEPOT = "{3C204BB82C3A1147}Prefabs/Invasion/ZW_M777_Depot.et";
	protected const ResourceName ZW_MSTA_DEPOT = "{7D13ED5B62A94C80}Prefabs/Invasion/ZW_MstaB_Depot.et";
	protected const ResourceName ZW_T80U_DEPOT = "{6ACACBE7A01847C7}Prefabs/Vehicles/Tracked/T80U/T80u_Olive.et";
	protected const SCR_EEntitySpawnerSlotType ZW_ARTILLERY_SLOTS = SCR_EEntitySpawnerSlotType.VEHICLE_MEDIUM | SCR_EEntitySpawnerSlotType.VEHICLE_LARGE;

	//------------------------------------------------------------------------------------------------
	override SCR_EntityCatalog GetFactionEntityCatalogOfType(EEntityCatalogType catalogType, bool printNotFound = true)
	{
		SCR_EntityCatalog catalog = super.GetFactionEntityCatalogOfType(catalogType, printNotFound);
		if (!catalog || catalogType != EEntityCatalogType.VEHICLE || !ZW_Invasion.GetObjectives())
			return catalog;

		EEditableEntityLabel label = GetFactionLabel();
		if (label == EEditableEntityLabel.FACTION_US)
			catalog.ZW_RegisterDepotVehicle(ZW_M777_DEPOT, EEditableEntityLabel.VEHICLE_TRUCK, ZW_ARTILLERY_SLOTS);
		else if (label == EEditableEntityLabel.FACTION_USSR)
		{
			catalog.ZW_RegisterDepotVehicle(ZW_MSTA_DEPOT, EEditableEntityLabel.VEHICLE_TRUCK, ZW_ARTILLERY_SLOTS);
			// В исходном префабе Т-80У используется метка VEHICLE_APC.
			catalog.ZW_RegisterDepotVehicle(ZW_T80U_DEPOT, EEditableEntityLabel.VEHICLE_APC, SCR_EEntitySpawnerSlotType.VEHICLE_MEDIUM);
		}

		return catalog;
	}
}

modded class SCR_EntityCatalog
{
	protected ref set<ResourceName> m_ZWCheckedDepotVehicles = new set<ResourceName>();

	//------------------------------------------------------------------------------------------------
	void ZW_RegisterDepotVehicle(ResourceName prefab, EEditableEntityLabel vehicleLabel, SCR_EEntitySpawnerSlotType slots)
	{
		if (m_ZWCheckedDepotVehicles.Contains(prefab) || m_mPrefabIndexes.Contains(prefab))
			return;

		m_ZWCheckedDepotVehicles.Insert(prefab);
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
		{
			Print("[ZW_Invasion] Не загружен префаб техники для депо: " + prefab, LogLevel.ERROR);
			return;
		}

		ZW_DepotVehicleCatalogEntry entry = new ZW_DepotVehicleCatalogEntry(prefab, slots);
		int index = m_aEntityEntryList.Count();
		entry.InitEntry(this, index);
		if (!entry.GetEntityUiInfo() || !entry.HasEditableEntityLabel(vehicleLabel))
		{
			Print("[ZW_Invasion] Не пройдена проверка меток тяжёлого депо: " + prefab, LogLevel.ERROR);
			return;
		}

		m_aEntityEntryList.Insert(entry);
		m_mPrefabIndexes.Insert(prefab, index);
		entry.PostInitEntry(this);
	}
}

class ZW_DepotVehicleCatalogEntry : SCR_EntityCatalogEntry
{
	void ZW_DepotVehicleCatalogEntry(ResourceName prefab, SCR_EEntitySpawnerSlotType slots)
	{
		m_sEntityPrefab = prefab;
		m_bEnabled = true;
		m_aEntityDataList = { new ZW_DepotVehicleEditorData(), new ZW_DepotVehicleSpawnerData(slots) };
	}
}

class ZW_DepotVehicleEditorData : SCR_EntityCatalogEditorData
{
	void ZW_DepotVehicleEditorData()
	{
		m_bEnabled = true;
		m_eValidEditorModes = EEditorMode.BUILDING;
	}
}

class ZW_DepotVehicleSpawnerData : SCR_EntityCatalogSpawnerData
{
	void ZW_DepotVehicleSpawnerData(SCR_EEntitySpawnerSlotType slots)
	{
		m_bEnabled = true;
		m_eSlotTypes = slots;
		m_iEntityCount = 1;
		m_aVariantData = {};
	}
}

modded class SCR_CatalogEntitySpawnerComponent
{
	override protected void PerformSpawn(notnull SCR_EntityCatalogEntry entityEntry, IEntity user = null, SCR_EntitySpawnerSlotComponent preferredSlot = null)
	{
		IEntity previous = m_SpawnedEntity;
		super.PerformSpawn(entityEntry, user, preferredSlot);
		if (!Replication.IsServer() || !m_SpawnedEntity || m_SpawnedEntity == previous || !ZW_Invasion.GetTickets())
			return;

		SCR_VehicleDamageManagerComponent damage = SCR_VehicleDamageManagerComponent.Cast(m_SpawnedEntity.FindComponent(SCR_VehicleDamageManagerComponent));
		if (!damage)
			return;

		Faction faction = GetFaction();
		if (user)
			faction = GetRequesterFaction(user);
		damage.ZW_SetTicketOwner(faction);
	}

	//------------------------------------------------------------------------------------------------
	override Faction GetRequesterFaction(notnull IEntity user)
	{
		if (ZW_Invasion.GetObjectives())
		{
			int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(user);
			Faction faction = SCR_FactionManager.SGetPlayerFaction(playerId);
			if (faction)
				return faction;
		}

		return super.GetRequesterFaction(user);
	}
}

modded class SCR_EntitySpawnerSlotComponent
{
	//------------------------------------------------------------------------------------------------
	override protected bool TraceCallback(IEntity ent)
	{
		if (ZW_Invasion.GetObjectives() && (!ent || !ent.GetPhysics()))
			return false;

		return super.TraceCallback(ent);
	}
}
