modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	override bool ZW_CanDeployParachute(SCR_ChimeraCharacter pilot)
	{
		ZW_TimerComponent timer = ZW_Invasion.GetTimer();
		return (!timer || !timer.IsPrepPhase()) && super.ZW_CanDeployParachute(pilot);
	}
}

[BaseContainerProps(configRoot: true)]
class ZW_ParachuteArsenalConfig
{
	[Attribute(desc: "Запись ранца для арсеналов обоих мейнов")]
	ref SCR_EntityCatalogInventoryItem m_Entry;
}

modded class ZW_MainArsenalCatalogComponent
{
	//------------------------------------------------------------------------------------------------
	override protected SCR_EntityCatalog LoadItems(ResourceName resource, string side)
	{
		SCR_EntityCatalog catalog = super.LoadItems(resource, side);
		if (catalog)
			catalog.ZW_AddParachutePack();
		return catalog;
	}
}

modded class SCR_EntityCatalog
{
	//------------------------------------------------------------------------------------------------
	void ZW_AddParachutePack()
	{
		ResourceName config = "{D0543DC6DC3A4489}Configs/ZW_ParachuteArsenal.conf";
		ZW_ParachuteArsenalConfig data = SCR_ConfigHelperT<ZW_ParachuteArsenalConfig>.GetConfigObject(config);
		if (!data || !data.m_Entry)
		{
			Print("[ZW_Invasion] Не загружена запись арсенала парашюта " + config, LogLevel.ERROR);
			return;
		}

		SCR_EntityCatalogInventoryItem entry = data.m_Entry;
		ResourceName prefab = entry.GetPrefab();
		if (m_mPrefabIndexes.Contains(prefab))
			return;

		int index = m_aEntityEntryList.Count();
		entry.InitEntry(this, index);
		m_aEntityEntryList.Insert(entry);
		m_mPrefabIndexes.Insert(prefab, index);
		entry.PostInitEntry(this);
	}
}
