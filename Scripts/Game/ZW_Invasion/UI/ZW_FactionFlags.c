// ZRBinvaga — флаги сторон в режиме Invasion: герб РФ и звезда НАТО вместо штатных флагов США и СССР.
// Используются везде, где игра берёт флаг фракции: выбор стороны, меню возрождения, HUD точки, флаги на карте.
// Вне Invasion флаги штатные.

class ZW_FactionFlags
{
	static const ResourceName FLAG_RF = "{AF267116D5E40180}UI/Textures/ZW_Flag_RF.edds";
	static const ResourceName FLAG_NATO = "{422F9D34B9932A77}UI/Textures/ZW_Flag_NATO.edds";

	//! Ключи фракций по сторонам (с модовыми фракциями дополнить).
	static const ref array<string> RF_KEYS = {"USSR", "RF", "RU", "RUS"};
	static const ref array<string> NATO_KEYS = {"US", "USA", "NATO"};

	//------------------------------------------------------------------------------------------------
	//! Наш флаг для ключа фракции; пусто — оставить штатный.
	static ResourceName GetFlag(string factionKey)
	{
		if (!ZW_Invasion.GetObjectives())
			return ResourceName.Empty;

		if (RF_KEYS.Contains(factionKey))
			return FLAG_RF;

		if (NATO_KEYS.Contains(factionKey))
			return FLAG_NATO;

		return ResourceName.Empty;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_Faction
{
	override ResourceName GetFactionFlag()
	{
		ResourceName flag = ZW_FactionFlags.GetFlag(GetFactionKey());
		if (!flag.IsEmpty())
			return flag;

		return super.GetFactionFlag();
	}
}
