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
	protected const ResourceName SUPPLY_MARKER_LAYOUT = "{DAE56EB3A81F2C04}UI/layouts/ZW_Invasion/ZW_MapSupplyMarker.layout";
	protected const float SUPPLY_MARKER_WIDTH = 220;
	protected const float SUPPLY_MARKER_HEIGHT = 76;
	protected const float SUPPLY_ICON_SIZE = 44;

	[Attribute("USSR,RF,RU,RUS", UIWidgets.EditBox, "Ключи фракций РФ (красный цвет), остальные — синий")]
	protected string m_sRfFactionKeys;

	[Attribute("{6421CCFA3BB34F5F}UI/layouts/ZW_Invasion/ZW_MapFlags.layout", UIWidgets.ResourceNamePicker, "Слой флагов поверх карты", "layout")]
	protected ResourceName m_sFlagsLayout;

	[Attribute("{D290B8B7D065D109}UI/layouts/ZW_Invasion/ZW_MapFlag.layout", UIWidgets.ResourceNamePicker, "Флаг одной точки", "layout")]
	protected ResourceName m_sFlagLayout;

	protected ref array<ref SCR_MapMarkerBase> m_aMarkers = {};
	protected string m_sLastState;

	// Состояние КШМ приходит с гейм-модом даже вне радиуса репликации машины.
	[RplProp(onRplName: "OnKshmSnapshotChanged")]
	protected ref array<vector> m_aKshmPositions = {};
	[RplProp(onRplName: "OnKshmSnapshotChanged")]
	protected ref array<string> m_aKshmFactions = {};
	[RplProp(onRplName: "OnKshmSnapshotChanged")]
	protected ref array<int> m_aKshmAvailability = {};
	protected ref array<ref SCR_MapMarkerBase> m_aKshmMarkers = {};
	protected string m_sKshmMapFaction;
	protected bool m_bKshmMapOpen;
	protected bool m_bKshmMarkersDirty = true;

	//! Точки захвата и склады с подписями.
	protected ref array<IEntity> m_aFlagEntities = {};
	protected ref array<Faction> m_aFlagHolders = {};
	protected ref array<string> m_aFlagTexts = {};

	//! Виджеты на открытой карте.
	protected Widget m_wFlagsRoot;
	protected Widget m_wFlagsParent;
	protected ref array<Widget> m_aFlagWidgets = {};
	protected ref array<ref CanvasWidgetCommand> m_aSupplyMarkerDrawing;
	protected bool m_bFlagsDirty;
	protected bool m_bEditorMap;
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
		ClearMarkers(m_aMarkers);
		ClearMarkers(m_aKshmMarkers);
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
		if (!objectives)
			return;

		Faction local = SCR_FactionManager.SGetLocalPlayerFaction();
		bool isAttacker = local && local == objectives.GetAttackerFaction();
		bool isDefender = local && local == objectives.GetDefenderFaction();

		Faction attacker = objectives.GetAttackerFaction();
		Faction defender = objectives.GetDefenderFaction();

		// Список флагов пересобираем каждую секунду: владелец текущей точки меняется по ходу захвата.
		CollectFlags(objectives, isAttacker, isDefender, attacker, defender, local);

		if (!markers)
			return;

		UpdateKshmMarkers(markers, local);
		string state = string.Format("%1|%2|%3", local, attacker, defender);
		if (state == m_sLastState)
			return;

		m_sLastState = state;
		ClearMarkers(m_aMarkers);
		AddRoleMarkers(markers, GetOwner().GetWorld(), objectives, local);
	}

	void UpdateKshmSnapshot()
	{
		BaseWorld world = GetOwner().GetWorld();
		m_aKshmPositions.Clear();
		m_aKshmFactions.Clear();
		m_aKshmAvailability.Clear();
		foreach (ZW_KshmComponent kshm : ZW_KshmComponent.GetAll())
		{
			if (!kshm || !kshm.GetOwner() || kshm.GetOwner().GetWorld() != world)
				continue;

			ZW_KshmSpawnPoint spawnPoint = kshm.GetDeployedSpawnPoint();
			if (!spawnPoint)
				continue;

			m_aKshmPositions.Insert(spawnPoint.GetOrigin());
			m_aKshmFactions.Insert(kshm.GetFactionKey());
			m_aKshmAvailability.Insert(spawnPoint.IsSpawnPointEnabled());
		}
		Replication.BumpMe();
		OnKshmSnapshotChanged();
	}

	protected void OnKshmSnapshotChanged()
	{
		m_bKshmMarkersDirty = true;
	}

	protected void UpdateKshmMarkers(notnull SCR_MapMarkerManagerComponent markers, Faction faction)
	{
		if (!m_bKshmMapOpen)
			return;

		string factionKey;
		if (faction)
			factionKey = faction.GetFactionKey();
		if (!m_bKshmMarkersDirty && factionKey == m_sKshmMapFaction)
			return;

		if (m_aKshmPositions.Count() != m_aKshmFactions.Count() || m_aKshmPositions.Count() != m_aKshmAvailability.Count())
			return;

		m_bKshmMarkersDirty = false;
		m_sKshmMapFaction = factionKey;
		ClearMarkers(m_aKshmMarkers);
		if (!faction)
			return;

		EMilitarySymbolIdentity identity = EMilitarySymbolIdentity.BLUFOR;
		if (IsRfFaction(factionKey))
			identity = EMilitarySymbolIdentity.OPFOR;

		foreach (int index, vector position : m_aKshmPositions)
		{
			if (m_aKshmFactions[index] != factionKey)
				continue;

			string text = "КШМ — спавн недоступен";
			if (m_aKshmAvailability[index])
				text = "КШМ — спавн доступен";
			SCR_MapMarkerBase marker = AddMarker(markers, position, identity, text);
			if (marker)
				m_aKshmMarkers.Insert(marker);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Взятые точки видят все (флаг захвативших). Невзятые: оборона — все, атака — только текущую цель.
	protected void CollectFlags(ZW_ObjectivesComponent objectives, bool isAttacker, bool isDefender, Faction attacker, Faction defender, Faction viewer)
	{
		m_aFlagEntities.Clear();
		m_aFlagHolders.Clear();
		m_aFlagTexts.Clear();

		array<ZW_InvasionPoint> chain = objectives.GetChain();
		int activeIndex = Math.Max(0, objectives.GetActiveIndex());
		for (int index = 0; index < chain.Count() && (m_bEditorMap || isAttacker || isDefender); index++)
		{
			ZW_InvasionPoint point = chain[index];
			bool captured = point.IsLocked() || index < activeIndex;
			bool current = index == activeIndex && !captured;
			if (!m_bEditorMap && !captured && isAttacker && !current)
				continue; // атака не знает дальние точки

			Faction holder = defender;
			if (captured)
				holder = attacker;
			else if (current && point.GetProgress() >= ZW_InvasionPoint.PROGRESS_NEUTRAL)
				holder = null; // нейтральная — белый флаг

			string text = string.Format("%1. %2", index + 1, point.GetPointName());
			if (current)
				text = text + " — цель";
			else if (m_bEditorMap && !captured)
				text = text + " — закрыта";

			m_aFlagEntities.Insert(point);
			m_aFlagHolders.Insert(holder);
			m_aFlagTexts.Insert(text);
		}

		BaseWorld world = GetOwner().GetWorld();
		foreach (ZW_MainRoleComponent role : ZW_MainRoleComponent.GetAll())
		{
			if (!role || role.GetMapMarkerLabel().IsEmpty() || !role.GetOwner() || role.GetOwner().GetWorld() != world)
				continue;

			Faction faction = defender;
			if (role.GetRole() == ZW_EMainRole.ATTACK)
				faction = attacker;
			if (!faction || !role.HasMapMarker(viewer, faction))
				continue;

			m_aFlagEntities.Insert(role.GetOwner());
			m_aFlagHolders.Insert(faction);
			m_aFlagTexts.Insert(role.GetMapMarkerLabel());
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
		bool isEditorMap;
		bool kshmMapOpen;
		if (mapEntity && mapEntity.IsOpen())
		{
			MapConfiguration config = mapEntity.GetMapConfig();
			if (config && (config.MapEntityMode == EMapEntityMode.FULLSCREEN || config.MapEntityMode == EMapEntityMode.SPAWNSCREEN || config.MapEntityMode == EMapEntityMode.EDITOR))
			{
				isEditorMap = config.MapEntityMode == EMapEntityMode.EDITOR;
				parent = mapEntity.GetMapMenuRoot();
				if (!parent)
					parent = config.RootWidgetRef;
				kshmMapOpen = parent && config.MapEntityMode == EMapEntityMode.FULLSCREEN;
			}
		}

		if (kshmMapOpen != m_bKshmMapOpen)
		{
			m_bKshmMapOpen = kshmMapOpen;
			if (kshmMapOpen)
			{
				m_bKshmMarkersDirty = true;
				UpdateMarkers();
			}
			else
			{
				ClearMarkers(m_aKshmMarkers);
			}
		}

		if (!parent)
		{
			DestroyFlags();
			return;
		}

		if (isEditorMap != m_bEditorMap)
		{
			m_bEditorMap = isEditorMap;
			UpdateMarkers();
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
			IEntity entity = m_aFlagEntities[i];
			if (!w || !entity)
				continue;

			vector pos = entity.GetOrigin();
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

		ZW_ObjectivesComponent objectives = ZW_Invasion.GetObjectives();
		ZW_InvasionPoint active;
		if (objectives)
			active = objectives.GetPoint(Math.Max(0, objectives.GetActiveIndex()));

		for (int i = 0; i < m_aFlagEntities.Count(); i++)
		{
			ZW_InvasionPoint point = ZW_InvasionPoint.Cast(m_aFlagEntities[i]);
			if (!point)
			{
				m_aFlagWidgets.Insert(CreateSupplyMarker(m_aFlagTexts[i]));
				continue;
			}

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

			if (point == active && !point.IsLocked())
			{
				w.SetZOrder(1);
				PlaceChild(w, "FlagBorder", 56, 0, 48, 30, ARGB(255, 255, 185, 79));
			}

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
	protected Widget CreateSupplyMarker(string label)
	{
		Widget marker = GetGame().GetWorkspace().CreateWidgets(SUPPLY_MARKER_LAYOUT, m_wFlagsRoot);
		if (!marker)
			return null;

		FrameSlot.SetAnchorMin(marker, 0, 0);
		FrameSlot.SetAnchorMax(marker, 0, 0);
		FrameSlot.SetAlignment(marker, 0.5, SUPPLY_ICON_SIZE / (2 * SUPPLY_MARKER_HEIGHT));
		FrameSlot.SetSize(marker, SUPPLY_MARKER_WIDTH, SUPPLY_MARKER_HEIGHT);
		marker.SetZOrder(2);
		PlaceChild(marker, "SupplyDrawing", 0, 0, SUPPLY_MARKER_WIDTH, SUPPLY_MARKER_HEIGHT, ARGB(255, 255, 255, 255));
		PlaceChild(marker, "SupplyText", 8, SUPPLY_ICON_SIZE + 2, SUPPLY_MARKER_WIDTH - 16, 30, ARGB(255, 255, 249, 237));

		CanvasWidget canvas = CanvasWidget.Cast(marker.FindAnyWidget("SupplyDrawing"));
		if (canvas)
		{
			if (!m_aSupplyMarkerDrawing)
				BuildSupplyMarkerDrawing();
			canvas.SetDrawCommands(m_aSupplyMarkerDrawing);
		}

		TextWidget text = TextWidget.Cast(marker.FindAnyWidget("SupplyText"));
		if (text)
			text.SetText(label);
		marker.SetFlags(WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS);
		for (Widget child = marker.GetChildren(); child; child = child.GetSibling())
			child.SetFlags(WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS);

		return marker;
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildSupplyMarkerDrawing()
	{
		m_aSupplyMarkerDrawing = {};
		float scale = GetGame().GetWorkspace().DPIScale(1);
		int dark = ARGB(255, 31, 31, 31);
		int gold = ARGB(255, 255, 197, 79);

		// Подложка подписи и рамка значка.
		AddSupplyPolygon(dark, {6, 46, 214, 46, 220, 52, 220, 70, 214, 76, 6, 76, 0, 70, 0, 52}, scale);
		AddSupplyPolygon(dark, {94, 0, 126, 0, 132, 6, 132, 38, 126, 44, 94, 44, 88, 38, 88, 6}, scale);
		AddSupplyPolygon(gold, {95, 3, 125, 3, 129, 7, 129, 37, 125, 41, 95, 41, 91, 37, 91, 7}, scale);

		// Крышка, корпус и две планки ящика.
		AddSupplyPolygon(dark, {101, 8, 119, 8, 125, 17, 125, 20, 95, 20, 95, 17}, scale);
		AddSupplyPolygon(dark, {98, 20, 122, 20, 122, 35, 98, 35}, scale);
		AddSupplyPolygon(gold, {103, 11, 117, 11, 121, 17, 99, 17}, scale);
		AddSupplyPolygon(gold, {101, 20, 119, 20, 119, 32, 101, 32}, scale);
		AddSupplyPolygon(dark, {104, 22, 116, 22, 116, 24, 104, 24}, scale);
		AddSupplyPolygon(dark, {104, 27, 116, 27, 116, 29, 104, 29}, scale);
	}

	protected void AddSupplyPolygon(int color, notnull array<float> vertices, float scale)
	{
		PolygonDrawCommand command = new PolygonDrawCommand();
		command.m_iColor = color;
		command.m_Vertices = {};
		foreach (float coordinate : vertices)
			command.m_Vertices.Insert(coordinate * scale);
		m_aSupplyMarkerDrawing.Insert(command);
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

		m_aSupplyMarkerDrawing = null;
		m_wFlagsRoot = null;
		m_wFlagsParent = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Штатные метки мейнов окрашены по фракции, которой они принадлежат в этом матче.
	protected void AddRoleMarkers(SCR_MapMarkerManagerComponent markers, BaseWorld world, ZW_ObjectivesComponent objectives, Faction viewer)
	{
		foreach (ZW_MainRoleComponent role : ZW_MainRoleComponent.GetAll())
		{
			if (!role || !role.GetMapMarkerLabel().IsEmpty() || !role.GetOwner() || role.GetOwner().GetWorld() != world)
				continue;

			Faction faction = objectives.GetDefenderFaction();
			if (role.GetRole() == ZW_EMainRole.ATTACK)
				faction = objectives.GetAttackerFaction();

			if (!faction || !role.HasMapMarker(viewer, faction))
				continue;

			EMilitarySymbolIdentity identity = EMilitarySymbolIdentity.BLUFOR;
			string text = "Мейн НАТО";
			if (IsRfFaction(faction.GetFactionKey()))
			{
				identity = EMilitarySymbolIdentity.OPFOR;
				text = "Мейн РФ";
			}
			SCR_MapMarkerBase marker = AddMarker(markers, role.GetOwner().GetOrigin(), identity, text);
			if (marker)
				m_aMarkers.Insert(marker);
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
	protected SCR_MapMarkerBase AddMarker(SCR_MapMarkerManagerComponent markers, vector pos, EMilitarySymbolIdentity identity, string text)
	{
		SCR_MapMarkerBase marker = markers.PrepareMilitaryMarker(identity, EMilitarySymbolDimension.INSTALLATION, 0);
		if (!marker)
			marker = markers.PrepareMilitaryMarker(identity, EMilitarySymbolDimension.LAND, 0);

		if (!marker)
		{
			Print("[ZW_Invasion] Метки точек: не удалось создать метку", LogLevel.WARNING);
			return null;
		}

		marker.SetWorldPos(Math.Round(pos[0]), Math.Round(pos[2]));
		marker.SetCustomText(text);
		marker.SetCanBeRemovedByOwner(false);
		markers.InsertStaticMarker(marker, true);
		return marker;
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearMarkers(notnull array<ref SCR_MapMarkerBase> mapMarkers)
	{
		SCR_MapMarkerManagerComponent markers = SCR_MapMarkerManagerComponent.GetInstance();
		if (markers)
		{
			foreach (SCR_MapMarkerBase marker : mapMarkers)
			{
				if (marker)
					markers.RemoveStaticMarker(marker);
			}
		}

		mapMarkers.Clear();
	}
}
