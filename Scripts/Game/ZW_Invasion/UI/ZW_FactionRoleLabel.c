// ZRBinvaga — на экране выбора фракции к названию добавляем роль в этом матче:
// «US Army — ATTACK» / «Soviet Army — DEFENSE». Стороны меняются каждый матч, без подписи игроки не поймут.
// Faction.GetFactionName() — proto, его не переопределить, поэтому правим сами виджеты меню.

class ZW_FactionRoleLabel
{
	//------------------------------------------------------------------------------------------------
	//! Название фракции с ролью; если роли ещё не назначены — просто название.
	static string Decorate(Faction faction, string name)
	{
		if (!faction)
			return name;

		ZW_ObjectivesComponent objectives = ZW_Invasion.GetObjectives();
		if (!objectives)
			return name;

		if (faction == objectives.GetAttackerFaction())
			return name + " — ATTACK";

		if (faction == objectives.GetDefenderFaction())
			return name + " — DEFENSE";

		return name;
	}
}

//------------------------------------------------------------------------------------------------
//! Строки фракций в списке и большая карточка справа (Deployment Setup).
modded class SCR_FactionPlayerList
{
	override void SetFaction(Faction faction)
	{
		super.SetFaction(faction);

		if (m_wName && faction)
			m_wName.SetText(ZW_FactionRoleLabel.Decorate(faction, faction.GetFactionName()));
	}
}

//------------------------------------------------------------------------------------------------
//! Кнопки фракций в меню возрождения.
modded class SCR_FactionButton
{
	override void SetFactionName(string name)
	{
		super.SetFactionName(ZW_FactionRoleLabel.Decorate(m_Faction, name));
	}
}
