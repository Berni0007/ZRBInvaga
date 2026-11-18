// ZRBinvaga — общие помощники режима Invasion.
// Все классы мода имеют префикс ZW_ (соглашение BI: тег против конфликтов имён).

class ZW_Invasion
{
	//------------------------------------------------------------------------------------------------
	static SCR_BaseGameMode GetGameMode()
	{
		return SCR_BaseGameMode.Cast(GetGame().GetGameMode());
	}

	//------------------------------------------------------------------------------------------------
	static ZW_ObjectivesComponent GetObjectives()
	{
		SCR_BaseGameMode gameMode = GetGameMode();
		if (!gameMode)
			return null;

		return ZW_ObjectivesComponent.Cast(gameMode.FindComponent(ZW_ObjectivesComponent));
	}

	//------------------------------------------------------------------------------------------------
	static ZW_TicketsComponent GetTickets()
	{
		SCR_BaseGameMode gameMode = GetGameMode();
		if (!gameMode)
			return null;

		return ZW_TicketsComponent.Cast(gameMode.FindComponent(ZW_TicketsComponent));
	}

	//------------------------------------------------------------------------------------------------
	static ZW_TimerComponent GetTimer()
	{
		SCR_BaseGameMode gameMode = GetGameMode();
		if (!gameMode)
			return null;

		return ZW_TimerComponent.Cast(gameMode.FindComponent(ZW_TimerComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Фракция сущности (персонаж, техника) через FactionAffiliationComponent.
	static Faction GetEntityFaction(IEntity entity)
	{
		if (!entity)
			return null;

		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (!affiliation)
			return null;

		return affiliation.GetAffiliatedFaction();
	}
}
