class ZW_MainArsenalCatalogComponentClass : SCR_BaseGameModeComponentClass
{
}

class ZW_MainArsenalCatalogComponent : SCR_BaseGameModeComponent
{
	[Attribute(desc: "Каталог предметов арсенала НАТО", category: "Invasion")]
	protected ref SCR_EntityCatalog m_NatoItems;

	[Attribute(desc: "Каталог предметов арсенала РФ", category: "Invasion")]
	protected ref SCR_EntityCatalog m_RfItems;

	protected bool m_bCatalogsInitialized;
	protected ref map<SCR_ArsenalItem, ref ZW_MainArsenalPurchaseItem> m_PurchaseItems = new map<SCR_ArsenalItem, ref ZW_MainArsenalPurchaseItem>();

	//------------------------------------------------------------------------------------------------
	SCR_EntityCatalog GetItems(ZW_MainRoleComponent role, Faction faction)
	{
		if (!m_bCatalogsInitialized)
		{
			m_bCatalogsInitialized = true;
			InitItems(m_NatoItems, "НАТО");
			InitItems(m_RfItems, "РФ");
		}

		if (role.IsRfFaction(faction))
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
	protected void InitItems(SCR_EntityCatalog catalog, string side)
	{
		if (!catalog)
		{
			Print("[ZW_Invasion] Не загружен каталог ZARUBA Arsenal для " + side, LogLevel.ERROR);
			return;
		}

		catalog.InitCatalog();
		array<SCR_EntityCatalogEntry> entries = {};
		catalog.GetEntityListWithData(SCR_ArsenalItem, entries);
		Print(string.Format("[ZW_Invasion] ZARUBA Arsenal, %1: %2 записей арсенала", side, entries.Count()));
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
		ZW_MainRoleComponent role;
		ZW_MainArsenalCatalogComponent catalogs = ZW_GetMainArsenalCatalogs(role);
		if (!catalogs)
			return super.GetFilteredArsenalItems(filteredArsenalItems, requiresDisplayType);

		return ZW_FilterMainArsenalItems(catalogs, role, filteredArsenalItems, requiresDisplayType);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetFilteredOverwriteArsenalItems(out notnull array<SCR_ArsenalItem> filteredArsenalItems, EArsenalItemDisplayType requiresDisplayType = -1)
	{
		ZW_MainRoleComponent role;
		ZW_MainArsenalCatalogComponent catalogs = ZW_GetMainArsenalCatalogs(role);
		if (!catalogs)
			return super.GetFilteredOverwriteArsenalItems(filteredArsenalItems, requiresDisplayType);

		if (!ZW_FilterMainArsenalItems(catalogs, role, filteredArsenalItems, requiresDisplayType))
			return false;

		foreach (int i, SCR_ArsenalItem item : filteredArsenalItems)
			filteredArsenalItems[i] = catalogs.GetPurchaseItem(item);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected ZW_MainArsenalCatalogComponent ZW_GetMainArsenalCatalogs(out ZW_MainRoleComponent role)
	{
		role = ZW_MainRoleComponent.FindMainRole(GetOwner());
		if (!role)
			return null;

		return ZW_MainArsenalCatalogComponent.Cast(ZW_Invasion.GetGameMode().FindComponent(ZW_MainArsenalCatalogComponent));
	}

	//------------------------------------------------------------------------------------------------
	protected bool ZW_FilterMainArsenalItems(notnull ZW_MainArsenalCatalogComponent catalogs, notnull ZW_MainRoleComponent role, out notnull array<SCR_ArsenalItem> filteredArsenalItems, EArsenalItemDisplayType requiresDisplayType)
	{
		ZW_ObjectivesComponent objectives = ZW_Invasion.GetObjectives();
		Faction faction = objectives.GetDefenderFaction();
		if (role.GetRole() == ZW_EMainRole.ATTACK)
			faction = objectives.GetAttackerFaction();

		filteredArsenalItems.Clear();
		if (!faction)
			return false;

		SCR_EntityCatalog catalog = catalogs.GetItems(role, faction);
		SCR_EntityCatalogManagerComponent catalogManager = SCR_EntityCatalogManagerComponent.GetInstance();
		if (!catalog || !catalogManager)
			return false;

		return catalogManager.ZW_FilterMainArsenalItems(filteredArsenalItems, catalog, m_eSupportedArsenalItemTypes, m_eSupportedArsenalItemModes, requiresDisplayType);
	}
}

modded class SCR_EntityCatalogManagerComponent
{
	//------------------------------------------------------------------------------------------------
	bool ZW_FilterMainArsenalItems(out array<SCR_ArsenalItem> items, notnull SCR_EntityCatalog catalog, SCR_EArsenalItemType types, SCR_EArsenalItemMode modes, EArsenalItemDisplayType displayType)
	{
		return GetArsenalItems(items, catalog, types, modes, SCR_ArsenalManagerComponent.GetArsenalGameModeType_Static(), displayType);
	}
}
