class ZW_MainArsenalCatalogComponentClass : SCR_BaseGameModeComponentClass
{
}

class ZW_MainArsenalCatalogComponent : SCR_BaseGameModeComponent
{

	[Attribute("{3BFC57A750823093}Configs/EntityCatalog/NATO/ZRB_Items_NATO.conf", UIWidgets.ResourceNamePicker, "Каталог предметов арсенала НАТО", "conf", category: "Invasion")]
	protected ResourceName m_NatoCatalogResource;

	[Attribute("{32BAB1B847BFDB81}Configs/EntityCatalog/RUSSIA/ZRB_InventoryItems_RU.conf", UIWidgets.ResourceNamePicker, "Каталог предметов арсенала РФ", "conf", category: "Invasion")]
	protected ResourceName m_RfCatalogResource;

	protected ref SCR_EntityCatalog m_NatoItems;
	protected ref SCR_EntityCatalog m_RfItems;

	protected bool m_bCatalogsInitialized;
	protected ref map<SCR_ArsenalItem, ref ZW_MainArsenalPurchaseItem> m_PurchaseItems = new map<SCR_ArsenalItem, ref ZW_MainArsenalPurchaseItem>();

	//------------------------------------------------------------------------------------------------
	SCR_EntityCatalog GetItems(bool useRfCatalog)
	{
		if (!m_bCatalogsInitialized)
		{
			m_bCatalogsInitialized = true;
			m_NatoItems = LoadItems(m_NatoCatalogResource, "НАТО");
			m_RfItems = LoadItems(m_RfCatalogResource, "РФ");
		}

		if (useRfCatalog)
			return m_RfItems;

		return m_NatoItems;
	}

	//------------------------------------------------------------------------------------------------
	SCR_ArsenalItemStandalone GetPurchaseItem(notnull SCR_ArsenalItem item)
	{
		ZW_MainArsenalPurchaseItem purchase = m_PurchaseItems.Get(item);
		if (!purchase)
		{
			purchase = new ZW_MainArsenalPurchaseItem(item);
			m_PurchaseItems.Set(item, purchase);
		}

		return purchase;
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_EntityCatalog LoadItems(ResourceName resource, string side)
	{
		SCR_EntityCatalog catalog = SCR_ConfigHelperT<SCR_EntityCatalog>.GetConfigObject(resource);
		if (!catalog)
		{
			Print("[ZW_Invasion] Не загружен каталог ZARUBA Arsenal для " + side + ": " + resource, LogLevel.ERROR);
			return null;
		}

		catalog.InitCatalog();
		catalog.ZW_AddAceMedicalItems();
		array<SCR_EntityCatalogEntry> entries = {};
		catalog.GetEntityListWithData(SCR_ArsenalItem, entries);
		Print(string.Format("[ZW_Invasion] ZARUBA Arsenal, %1: %2 записей арсенала", side, entries.Count()));
		return catalog;
	}
}

[ComponentEditorProps(category: "ZRB/Invasion", description: "Каталог ZARUBA Arsenal для КШМ")]
class ZW_KshmArsenalComponentClass : ScriptComponentClass
{
}

class ZW_KshmArsenalComponent : ScriptComponent
{
	[Attribute("0", UIWidgets.CheckBox, "Использовать каталог РФ")]
	protected bool m_bUseRfCatalog;

	bool UsesRfCatalog()
	{
		return m_bUseRfCatalog;
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		// Контейнер встроенного арсенала может появиться после инициализации машины.
		GetGame().GetCallqueue().CallLater(EnableFullCatalog, 0);
	}

	protected void EnableFullCatalog()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		SCR_ArsenalComponent arsenal = SCR_ArsenalComponent.FindArsenalComponent(owner);
		if (!arsenal)
			return;

		arsenal.SetSupportedArsenalItemTypes(-1);
		arsenal.SetSupportedArsenalItemModes(-1);
	}
}

// Штатная выдача принимает записи из отдельного списка в формате Standalone.
class ZW_MainArsenalPurchaseItem : SCR_ArsenalItemStandalone
{
	protected SCR_ArsenalItem m_Source;

	void ZW_MainArsenalPurchaseItem(notnull SCR_ArsenalItem source)
	{
		m_Source = source;
		m_ItemResourceName = source.GetItemResourceName();
	}

	//------------------------------------------------------------------------------------------------
	override int GetSupplyCost(SCR_EArsenalSupplyCostType supplyCostType, bool addAdditionalCosts = true)
	{
		return m_Source.GetSupplyCost(supplyCostType, addAdditionalCosts);
	}
}

modded class SCR_ArsenalComponent
{
	//------------------------------------------------------------------------------------------------
	override bool GetFilteredArsenalItems(out notnull array<SCR_ArsenalItem> filteredArsenalItems, EArsenalItemDisplayType requiresDisplayType = -1)
	{
		ZW_MainArsenalCatalogComponent catalogs;
		SCR_EntityCatalog catalog;
		if (!ZW_TryResolveZarubaCatalog(catalogs, catalog))
			return super.GetFilteredArsenalItems(filteredArsenalItems, requiresDisplayType);

		return ZW_FilterZarubaArsenalItems(catalog, filteredArsenalItems, requiresDisplayType);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetFilteredOverwriteArsenalItems(out notnull array<SCR_ArsenalItem> filteredArsenalItems, EArsenalItemDisplayType requiresDisplayType = -1)
	{
		ZW_MainArsenalCatalogComponent catalogs;
		SCR_EntityCatalog catalog;
		if (!ZW_TryResolveZarubaCatalog(catalogs, catalog))
			return super.GetFilteredOverwriteArsenalItems(filteredArsenalItems, requiresDisplayType);

		if (!ZW_FilterZarubaArsenalItems(catalog, filteredArsenalItems, requiresDisplayType))
			return false;

		foreach (int i, SCR_ArsenalItem item : filteredArsenalItems)
			filteredArsenalItems[i] = catalogs.GetPurchaseItem(item);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ZW_TryResolveZarubaCatalog(out ZW_MainArsenalCatalogComponent catalogs, out SCR_EntityCatalog catalog)
	{
		ZW_ObjectivesComponent objectives = ZW_Invasion.GetObjectives();
		if (!objectives)
			return false;

		catalogs = ZW_MainArsenalCatalogComponent.Cast(ZW_Invasion.GetGameMode().FindComponent(ZW_MainArsenalCatalogComponent));
		ZW_KshmArsenalComponent kshm = ZW_FindKshmArsenal();
		if (kshm)
		{
			if (catalogs)
				catalog = catalogs.GetItems(kshm.UsesRfCatalog());
			return true;
		}

		if (!catalogs)
			return false;

		ZW_MainRoleComponent role = ZW_MainRoleComponent.FindMainRole(GetOwner());
		if (!role)
			return false;

		Faction faction = objectives.GetDefenderFaction();
		if (role.GetRole() == ZW_EMainRole.ATTACK)
			faction = objectives.GetAttackerFaction();
		if (faction)
			catalog = catalogs.GetItems(role.IsRfFaction(faction));
		return true;
	}

	protected ZW_KshmArsenalComponent ZW_FindKshmArsenal()
	{
		IEntity owner = GetOwner();
		for (IEntity entity = owner; entity; entity = entity.GetParent())
		{
			ZW_KshmArsenalComponent kshm = ZW_KshmArsenalComponent.Cast(entity.FindComponent(ZW_KshmArsenalComponent));
			if (kshm && (entity == owner || SCR_ArsenalComponent.FindArsenalComponent(entity) == this))
				return kshm;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ZW_FilterZarubaArsenalItems(SCR_EntityCatalog catalog, out notnull array<SCR_ArsenalItem> filteredArsenalItems, EArsenalItemDisplayType requiresDisplayType)
	{
		filteredArsenalItems.Clear();
		SCR_EntityCatalogManagerComponent catalogManager = SCR_EntityCatalogManagerComponent.GetInstance();
		if (!catalog || !catalogManager)
			return false;

		return catalogManager.ZW_FilterZarubaArsenalItems(filteredArsenalItems, catalog, m_eSupportedArsenalItemTypes, m_eSupportedArsenalItemModes, requiresDisplayType);
	}
}

modded class SCR_EntityCatalogManagerComponent
{
	//------------------------------------------------------------------------------------------------
	bool ZW_FilterZarubaArsenalItems(out array<SCR_ArsenalItem> items, notnull SCR_EntityCatalog catalog, SCR_EArsenalItemType types, SCR_EArsenalItemMode modes, EArsenalItemDisplayType displayType)
	{
		return GetArsenalItems(items, catalog, types, modes, SCR_ArsenalManagerComponent.GetArsenalGameModeType_Static(), displayType);
	}
}

modded class SCR_EntityCatalog
{
	//------------------------------------------------------------------------------------------------
	void ZW_AddAceMedicalItems()
	{
		ResourceName config = "{A5E3827C546B8210}Configs/EntityCatalog/ACE_InventoryItems_EntityCatalog_Medical.conf";
		ACE_EntityCatalogMultiListEntry medicine = SCR_ConfigHelperT<ACE_EntityCatalogMultiListEntry>.GetConfigObject(config);
		if (!medicine || !medicine.m_aEntities)
		{
			Print("[ZW_Invasion] Не загружен список предметов ACE Medical: " + config, LogLevel.ERROR);
			return;
		}

		foreach (SCR_EntityCatalogEntry entry : medicine.m_aEntities)
		{
			if (!entry || !entry.IsEnabled())
				continue;

			ResourceName prefab = entry.GetPrefab();
			if (m_mPrefabIndexes.Contains(prefab))
				continue;

			int index = m_aEntityEntryList.Count();
			entry.InitEntry(this, index);
			m_aEntityEntryList.Insert(entry);
			m_mPrefabIndexes.Insert(prefab, index);
			entry.PostInitEntry(this);
		}
	}
}
