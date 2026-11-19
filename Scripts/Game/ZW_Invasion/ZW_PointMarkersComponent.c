// ZRBinvaga — точки Invasion на игровой карте (как в Squad).
//   - Оборона видит все точки, атака — только текущую цель (до начала штурма — первую).
//   - Цвет точки — цвет стороны, которая её держит (как метка её мейна): НАТО — синий, РФ — красный.
//     Невзятые точки — цвет обороны, взятые атакой — цвет атаки.
// Метки локальные (у каждого игрока свои), пересобираются при смене стороны или текущей цели.

[ComponentEditorProps(category: "ZRB/Invasion", description: "Invasion: метки точек на карте по сторонам")]
class ZW_PointMarkersComponentClass : SCR_BaseGameModeComponentClass
{
}

class ZW_PointMarkersComponent : SCR_BaseGameModeComponent
{
	protected const int UPDATE_MS = 1000;

	[Attribute("USSR,RF,RU,RUS", UIWidgets.EditBox, "Ключи фракций РФ (красный цвет), остальные — синий")]
	protected string m_sRfFactionKeys;

	protected ref array<ref SCR_MapMarkerBase> m_aMarkers = {};
	protected string m_sLastState;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (System.IsConsoleApp())
			return; // выделенный сервер — карты нет

		GetGame().GetCallqueue().CallLater(UpdateMarkers, UPDATE_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(UpdateMarkers);
		ClearMarkers();
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateMarkers()
	{
		if (!GetGame().GetPlayerController())
			return;

		ZW_ObjectivesComponent objectives = ZW_Invasion.GetObjectives();
		SCR_MapMarkerManagerComponent markers = SCR_MapMarkerManagerComponent.GetInstance();
		if (!objectives || !markers)
			return;

		Faction local = SCR_FactionManager.SGetLocalPlayerFaction();
		bool isAttacker = local && local == objectives.GetAttackerFaction();
		bool isDefender = local && local == objectives.GetDefenderFaction();

		int activeOrder = objectives.GetActiveIndex() + 1; // 0 — штурм ещё не начался
		if (activeOrder < 1)
			activeOrder = 1;

		string state = string.Format("%1|%2|%3|%4|%5", isAttacker, isDefender, activeOrder, objectives.GetPointCount(), objectives.GetAttackerFaction());
		if (state == m_sLastState)
			return;

		m_sLastState = state;
		ClearMarkers();

		BaseWorld world = GetOwner().GetWorld();
		AddMainMarkers(markers, world, objectives);

		if (!isAttacker && !isDefender)
			return;

		int count = objectives.GetPointCount();
		for (int order = 1; order <= count; order++)
		{
			ZW_InvasionPoint point = ZW_ObjectivesComponent.FindPointByOrder(world, order);
			if (!point)
				continue;

			string text = string.Format("%1. %2", order, point.GetPointName());
			bool lost = order < activeOrder;

			if (isAttacker)
			{
				if (order != activeOrder)
					continue;

				text = text + " — цель";
			}
			else if (lost)
				text = text + " — потеряна";
			else if (order == activeOrder)
				text = text + " — под атакой";

			Faction holder = objectives.GetDefenderFaction();
			if (lost)
				holder = objectives.GetAttackerFaction();

			EMilitarySymbolIdentity identity = EMilitarySymbolIdentity.BLUFOR;
			if (holder && IsRfFaction(holder.GetFactionKey()))
				identity = EMilitarySymbolIdentity.OPFOR;

			AddMarker(markers, point.GetOrigin(), identity, text);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Метки мейнов: видны всем, цвет фракции, которой мейн принадлежит в этом матче.
	protected void AddMainMarkers(SCR_MapMarkerManagerComponent markers, BaseWorld world, ZW_ObjectivesComponent objectives)
	{
		foreach (ZW_MainRoleComponent role : ZW_MainRoleComponent.GetAll())
		{
			if (!role || !role.HasMapMarker() || !role.GetOwner() || role.GetOwner().GetWorld() != world)
				continue;

			Faction faction = objectives.GetDefenderFaction();
			if (role.GetRole() == ZW_EMainRole.ATTACK)
				faction = objectives.GetAttackerFaction();

			if (!faction)
				continue;

			EMilitarySymbolIdentity identity = EMilitarySymbolIdentity.BLUFOR;
			string text = "Мейн НАТО";
			if (IsRfFaction(faction.GetFactionKey()))
			{
				identity = EMilitarySymbolIdentity.OPFOR;
				text = "Мейн РФ";
			}

			AddMarker(markers, role.GetOwner().GetOrigin(), identity, text);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsRfFaction(string factionKey)
	{
		array<string> keys = {};
		m_sRfFactionKeys.Split(",", keys, true);
		foreach (string key : keys)
		{
			key.Trim();
			if (key == factionKey)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void AddMarker(SCR_MapMarkerManagerComponent markers, vector pos, EMilitarySymbolIdentity identity, string text)
	{
		SCR_MapMarkerBase marker = markers.PrepareMilitaryMarker(identity, EMilitarySymbolDimension.INSTALLATION, 0);
		if (!marker)
			marker = markers.PrepareMilitaryMarker(identity, EMilitarySymbolDimension.LAND, 0);

		if (!marker)
		{
			Print("[ZW_Invasion] Метки точек: не удалось создать метку", LogLevel.WARNING);
			return;
		}

		marker.SetWorldPos(Math.Round(pos[0]), Math.Round(pos[2]));
		marker.SetCustomText(text);
		marker.SetCanBeRemovedByOwner(false);
		markers.InsertStaticMarker(marker, true);
		m_aMarkers.Insert(marker);
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearMarkers()
	{
		SCR_MapMarkerManagerComponent markers = SCR_MapMarkerManagerComponent.GetInstance();
		if (markers)
		{
			foreach (SCR_MapMarkerBase marker : m_aMarkers)
			{
				if (marker)
					markers.RemoveStaticMarker(marker);
			}
		}

		m_aMarkers.Clear();
	}
}
