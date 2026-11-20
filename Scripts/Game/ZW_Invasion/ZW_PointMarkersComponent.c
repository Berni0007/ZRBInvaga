// ZRBinvaga — точки Invasion на игровой карте (как в Squad).
//   - Оборона видит все точки, атака — только текущую цель (до начала штурма — первую).
//   - Цвет точки — цвет стороны, которая её держит (как метка её мейна): НАТО — синий, РФ — красный.
//     Невзятые точки — цвет обороны, взятые атакой — цвет атаки.
// Точки рисуются флагом стороны, которая их держит (взятая — флаг захвативших, нейтральная — белый),
// под флагом — номер и название. Флаги — свои виджеты поверх карты, мейны — штатные метки.

[ComponentEditorProps(category: "ZRB/Invasion", description: "Invasion: метки точек на карте по сторонам")]
class ZW_PointMarkersComponentClass : SCR_BaseGameModeComponentClass
{
}

class ZW_PointMarkersComponent : SCR_BaseGameModeComponent
{
	protected const int UPDATE_MS = 1000;

	[Attribute("USSR,RF,RU,RUS", UIWidgets.EditBox, "Ключи фракций РФ (красный цвет), остальные — синий")]
	protected string m_sRfFactionKeys;

	[Attribute("{6421CCFA3BB34F5F}UI/layouts/ZW_Invasion/ZW_MapFlags.layout", UIWidgets.ResourceNamePicker, "Слой флагов поверх карты", "layout")]
	protected ResourceName m_sFlagsLayout;

	[Attribute("{D290B8B7D065D109}UI/layouts/ZW_Invasion/ZW_MapFlag.layout", UIWidgets.ResourceNamePicker, "Флаг одной точки", "layout")]
	protected ResourceName m_sFlagLayout;

	protected ref array<ref SCR_MapMarkerBase> m_aMarkers = {};
	protected string m_sLastState;

	//! Что рисовать: точки, их флаги и подписи (пересчитывается раз в секунду).
	protected ref array<ZW_InvasionPoint> m_aFlagPoints = {};
	protected ref array<Faction> m_aFlagHolders = {};
	protected ref array<string> m_aFlagTexts = {};

	//! Виджеты на открытой карте.
	protected Widget m_wFlagsRoot;
	protected Widget m_wFlagsParent;
	protected ref array<Widget> m_aFlagWidgets = {};
	protected bool m_bFlagsDirty;
	protected string m_sFlagsSignature;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (System.IsConsoleApp())
			return; // выделенный сервер — карты нет

		GetGame().GetCallqueue().CallLater(UpdateMarkers, UPDATE_MS, true);
		GetGame().GetCallqueue().CallLater(UpdateFlags, 0, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(UpdateMarkers);
		GetGame().GetCallqueue().Remove(UpdateFlags);
		ClearMarkers();
		DestroyFlags();
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

		Faction attacker = objectives.GetAttackerFaction();
		Faction defender = objectives.GetDefenderFaction();

		// Список флагов пересобираем каждую секунду: владелец текущей точки меняется по ходу захвата.
		CollectFlags(objectives, isAttacker, isDefender, activeOrder, attacker, defender);

		string state = string.Format("%1|%2|%3", isAttacker, isDefender, attacker);
		if (state == m_sLastState)
			return;

		m_sLastState = state;
		ClearMarkers();
		AddMainMarkers(markers, GetOwner().GetWorld(), objectives);
	}

	//------------------------------------------------------------------------------------------------
	//! Взятые точки видят все (флаг захвативших). Невзятые: оборона — все, атака — только текущую цель.
	protected void CollectFlags(ZW_ObjectivesComponent objectives, bool isAttacker, bool isDefender, int activeOrder, Faction attacker, Faction defender)
	{
		m_aFlagPoints.Clear();
		m_aFlagHolders.Clear();
		m_aFlagTexts.Clear();

		BaseWorld world = GetOwner().GetWorld();
		int count = objectives.GetPointCount();
		for (int order = 1; order <= count && (isAttacker || isDefender); order++)
		{
			ZW_InvasionPoint point = ZW_ObjectivesComponent.FindPointByOrder(world, order);
			if (!point)
				continue;

			bool captured = point.IsLocked() || order < activeOrder;
			bool current = order == activeOrder && !captured;
			if (!captured && isAttacker && !current)
				continue; // атака не знает дальние точки

			Faction holder = defender;
			if (captured)
				holder = attacker;
			else if (current && point.GetProgress() >= ZW_InvasionPoint.PROGRESS_NEUTRAL)
				holder = null; // нейтральная — белый флаг

			string text = string.Format("%1. %2", order, point.GetPointName());
			if (current)
			{
				if (isAttacker)
					text = text + " — цель";
				else
					text = text + " — под атакой";
			}

			m_aFlagPoints.Insert(point);
			m_aFlagHolders.Insert(holder);
			m_aFlagTexts.Insert(text);
		}

		// Виджеты пересоздаём только когда что-то поменялось.
		string signature;
		for (int i = 0; i < m_aFlagTexts.Count(); i++)
		{
			string key = "-";
			if (m_aFlagHolders[i])
				key = m_aFlagHolders[i].GetFactionKey();
			signature += m_aFlagTexts[i] + "/" + key + ";";
		}

		if (signature != m_sFlagsSignature)
		{
			m_sFlagsSignature = signature;
			m_bFlagsDirty = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Каждый кадр, пока открыта карта: ставим флаги в экранные координаты точек.
	protected void UpdateFlags()
	{
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		Widget parent;
		if (mapEntity && mapEntity.IsOpen())
		{
			MapConfiguration config = mapEntity.GetMapConfig();
			if (config && (config.MapEntityMode == EMapEntityMode.FULLSCREEN || config.MapEntityMode == EMapEntityMode.SPAWNSCREEN))
			{
				parent = mapEntity.GetMapMenuRoot();
				if (!parent)
					parent = config.RootWidgetRef;
			}
		}

		if (!parent)
		{
			DestroyFlags();
			return;
		}

		if (parent != m_wFlagsParent || !m_wFlagsRoot)
		{
			DestroyFlags();
			m_wFlagsRoot = GetGame().GetWorkspace().CreateWidgets(m_sFlagsLayout, parent);
			if (!m_wFlagsRoot)
				return;

			// Слой растянут на весь экран, иначе координаты флагов уезжают.
			FrameSlot.SetAnchorMin(m_wFlagsRoot, 0, 0);
			FrameSlot.SetAnchorMax(m_wFlagsRoot, 1, 1);
			FrameSlot.SetOffsets(m_wFlagsRoot, 0, 0, 0, 0);
			m_wFlagsRoot.SetZOrder(60);

			m_wFlagsParent = parent;
			m_bFlagsDirty = true;
		}

		if (m_bFlagsDirty)
			RebuildFlags();

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		for (int i = 0; i < m_aFlagWidgets.Count(); i++)
		{
			Widget w = m_aFlagWidgets[i];
			ZW_InvasionPoint point = m_aFlagPoints[i];
			if (!w || !point)
				continue;

			vector pos = point.GetOrigin();
			float x, y;
			mapEntity.WorldToScreen(pos[0], pos[2], x, y, true);
			FrameSlot.SetPos(w, workspace.DPIUnscale(x), workspace.DPIUnscale(y));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RebuildFlags()
	{
		m_bFlagsDirty = false;
		foreach (Widget old : m_aFlagWidgets)
		{
			if (old)
				old.RemoveFromHierarchy();
		}
		m_aFlagWidgets.Clear();

		for (int i = 0; i < m_aFlagPoints.Count(); i++)
		{
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_sFlagLayout, m_wFlagsRoot);
			m_aFlagWidgets.Insert(w);
			if (!w)
				continue;

			// Размеры, позиции и цвета — из скрипта: значения из .layout не применяются (грабля 10).
			FrameSlot.SetAnchorMin(w, 0, 0);
			FrameSlot.SetAnchorMax(w, 0, 0);
			FrameSlot.SetAlignment(w, 0.5, 0.24);
			FrameSlot.SetSize(w, 160, 58);
			PlaceChild(w, "FlagBorder", 56, 0, 48, 30, ARGB(255, 17, 17, 17));
			PlaceChild(w, "FlagFill", 58, 2, 44, 26, ARGB(255, 233, 233, 233));
			PlaceChild(w, "FlagImg", 58, 2, 44, 26, ARGB(255, 255, 255, 255));
			PlaceChild(w, "FlagText", 0, 32, 160, 24, ARGB(255, 255, 255, 255));

			TextWidget text = TextWidget.Cast(w.FindAnyWidget("FlagText"));
			if (text)
				text.SetText(m_aFlagTexts[i]);

			ImageWidget flag = ImageWidget.Cast(w.FindAnyWidget("FlagImg"));
			SCR_Faction holder = SCR_Faction.Cast(m_aFlagHolders[i]);
			if (flag)
			{
				if (holder && !holder.GetFactionFlag().IsEmpty())
				{
					flag.LoadImageTexture(0, holder.GetFactionFlag(), false, false);
					flag.SetImage(0);
				}
				else
				{
					flag.SetVisible(false); // белый фон FlagFill — нейтральная точка
				}
			}
		}

		Print(string.Format("[ZW_Invasion] Флаги на карте: %1 шт., слой %2 в '%3'", m_aFlagWidgets.Count(), m_wFlagsRoot != null, m_wFlagsParent.GetName()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void PlaceChild(Widget parent, string name, float x, float y, float w, float h, int color)
	{
		Widget child = parent.FindAnyWidget(name);
		if (!child)
			return;

		FrameSlot.SetAnchorMin(child, 0, 0);
		FrameSlot.SetAnchorMax(child, 0, 0);
		FrameSlot.SetAlignment(child, 0, 0);
		FrameSlot.SetPos(child, x, y);
		FrameSlot.SetSize(child, w, h);
		child.SetColor(Color.FromInt(color));
	}

	//------------------------------------------------------------------------------------------------
	protected void DestroyFlags()
	{
		m_aFlagWidgets.Clear();
		if (m_wFlagsRoot)
			m_wFlagsRoot.RemoveFromHierarchy();

		m_wFlagsRoot = null;
		m_wFlagsParent = null;
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
