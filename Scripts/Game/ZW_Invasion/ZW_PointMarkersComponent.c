// ZRBinvaga — точки Invasion на игровой карте (как в Squad).
//   - Оборона видит все точки: свои (ещё не взятые) — синим, потерянные — красным.
//   - Атака видит только текущую цель — красным. До начала штурма — первую цель.
// Метки локальные (у каждого игрока свои), пересобираются при смене стороны или текущей цели.

[ComponentEditorProps(category: "ZRB/Invasion", description: "Invasion: метки точек на карте по сторонам")]
class ZW_PointMarkersComponentClass : SCR_BaseGameModeComponentClass
{
}

class ZW_PointMarkersComponent : SCR_BaseGameModeComponent
{
	protected const int UPDATE_MS = 1000;

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

		string state = string.Format("%1|%2|%3|%4", isAttacker, isDefender, activeOrder, objectives.GetPointCount());
		if (state == m_sLastState)
			return;

		m_sLastState = state;
		ClearMarkers();

		if (!isAttacker && !isDefender)
			return;

		BaseWorld world = GetOwner().GetWorld();
		int count = objectives.GetPointCount();
		for (int order = 1; order <= count; order++)
		{
			ZW_InvasionPoint point = ZW_ObjectivesComponent.FindPointByOrder(world, order);
			if (!point)
				continue;

			EMilitarySymbolIdentity identity;
			string text = string.Format("%1. %2", order, point.GetPointName());

			if (isAttacker)
			{
				if (order != activeOrder)
					continue;

				identity = EMilitarySymbolIdentity.OPFOR;
				text = text + " — цель";
			}
			else
			{
				if (order < activeOrder)
				{
					identity = EMilitarySymbolIdentity.OPFOR;
					text = text + " — потеряна";
				}
				else
				{
					identity = EMilitarySymbolIdentity.BLUFOR;
					if (order == activeOrder)
						text = text + " — под атакой";
				}
			}

			AddMarker(markers, point.GetOrigin(), identity, text);
		}
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
