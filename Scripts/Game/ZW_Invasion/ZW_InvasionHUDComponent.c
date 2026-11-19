// ZRBinvaga — HUD режима Invasion (вариант A «Линия фронта»).
// Вешается на префаб гейм-мода. Работает только на клиенте: читает реплицированное
// состояние (тикеты, таймер, точки, прогресс) и показывает каждой стороне своё:
//   - численность игроков на точке не показывается никому;
//   - шкалу захвата видит только атака в зоне точки;
//   - оборона видит только статус «точку захватывают».
//   - у каждой стороны своя плашка тикетов, цепочка точек и таймер штурма только на карте.

[ComponentEditorProps(category: "ZRB/Invasion", description: "Invasion: HUD (клиент)")]
class ZW_InvasionHUDComponentClass : SCR_BaseGameModeComponentClass
{
}

class ZW_InvasionHUDComponent : SCR_BaseGameModeComponent
{
	[Attribute("{751400A6DCCDBC09}UI/layouts/ZW_Invasion/ZW_InvasionHUD.layout", UIWidgets.ResourceNamePicker, "Layout HUD", "layout", category: "Invasion")]
	protected ResourceName m_sLayout;

	[Attribute("{24CD148E6AD508A0}UI/Textures/ZW_Tickets_NATO_UI.edds", UIWidgets.ResourceNamePicker, "Плашка тикетов НАТО", "edds", category: "Invasion")]
	protected ResourceName m_rNatoBadge;

	[Attribute("{7DC217E9672E4562}UI/Textures/ZW_Tickets_RF_UI.edds", UIWidgets.ResourceNamePicker, "Плашка тикетов РФ", "edds", category: "Invasion")]
	protected ResourceName m_rRfBadge;

	[Attribute("USSR,RF,RU,RUS", UIWidgets.EditBox, "Ключи фракций РФ через запятую (остальные получают плашку НАТО)", category: "Invasion")]
	protected string m_sRfFactionKeys;

	protected static const int MAX_POINTS = 7;
	protected static const int UPDATE_MS = 250;
	protected static const int MAP_PANEL_WIDTH = 324;
	protected static const int MAP_POINT_STEP = 42;
	protected static const float FREEZE_AFTER_MS = 1500;
	protected static const float CAPTURE_BAR_WIDTH = 288;
	protected static const float CAPTURE_BAR_HEIGHT = 6;

	protected Widget m_wRoot;
	protected Widget m_wMapParent;
	protected bool m_bHUDOnMap;
	protected bool m_bBadgeInitialized;
	protected string m_sBadgeFactionKey;
	protected bool m_bRunning;

	// Для определения направления захвата у атаки (растёт / замер / падает).
	protected int m_iLastProgress = -1;
	protected int m_iTrend;
	protected float m_fLastChangeTime;

	//------------------------------------------------------------------------------------------------
	override void OnGameModeStart()
	{
		super.OnGameModeStart();

		// На выделенном сервере интерфейса нет.
		if (System.IsConsoleApp())
			return;

		if (!CreateHUD(null))
		{
			Print("[ZW_Invasion] HUD: не удалось создать layout " + m_sLayout, LogLevel.ERROR);
			return;
		}

		m_bRunning = true;
		UpdateHUD();
		GetGame().GetCallqueue().CallLater(UpdateHUD, UPDATE_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected bool CreateHUD(Widget mapParent)
	{
		Widget root = GetGame().GetWorkspace().CreateWidgets(m_sLayout, mapParent);
		if (!root)
			return false;

		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();

		m_wRoot = root;
		m_wMapParent = mapParent;
		m_bHUDOnMap = mapParent != null;
		m_bBadgeInitialized = false;
		IgnoreCursor(m_wRoot);
		ApplyLayout();
		ApplyColors();
		SetVisible("TicketBadge", false);
		SetVisible("TopBar", false);
		SetVisible("TimerBar", false);
		SetVisible("PrepBox", false);
		SetVisible("CaptureBox", false);
		SetVisible("DefAlert", false);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void IgnoreCursor(notnull Widget widget)
	{
		widget.SetFlags(WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS);
		for (Widget child = widget.GetChildren(); child; child = child.GetSibling())
			IgnoreCursor(child);
	}

	//------------------------------------------------------------------------------------------------
	override void OnGameModeEnd(SCR_GameModeEndData data)
	{
		super.OnGameModeEnd(data);
		DestroyHUD();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		DestroyHUD();
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void DestroyHUD()
	{
		if (m_bRunning)
		{
			GetGame().GetCallqueue().Remove(UpdateHUD);
			m_bRunning = false;
		}

		if (m_wRoot)
		{
			m_wRoot.RemoveFromHierarchy();
			m_wRoot = null;
		}
		m_wMapParent = null;
		m_bBadgeInitialized = false;
		m_bHUDOnMap = false;
		m_iLastProgress = -1;
		m_iTrend = 0;
		m_fLastChangeTime = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Раскладка HUD задаётся здесь, а не в layout: позиции/размеры в пикселях эталона 1920x1080,
	//! якорь и выравнивание — доли (0..1) родителя/виджета.
	protected void ApplyLayout()
	{
		Place("TicketBadge", 1, 0, 1, 0, -24, 22, 232, 44);
		Place("TicketArtwork", 0, 0, 0, 0, 0, 0, 232, 44);
		Place("TicketCount", 0, 0, 0, 0, 167, 0, 54, 44);
		Place("TopBar", 0.5, 0, 0.5, 0, 0, 22, MAP_PANEL_WIDTH, 76);
		Place("TopBg", 0, 0, 0, 0, 0, 0, MAP_PANEL_WIDTH, 76);
		Place("TopEdge", 0, 0, 0, 0, 0, 0, MAP_PANEL_WIDTH, 2);
		Place("TimerBar", 0, 0, 0, 0, 12, 44, MAP_PANEL_WIDTH - 24, 26);
		Place("TimerDivider", 0, 0, 0, 0, 0, 0, MAP_PANEL_WIDTH - 24, 1);
		Place("TimerValue", 0, 0, 0, 0, 40, 3, 70, 23);
		Place("TimerText", 0, 0, 0, 0, 121, 3, 145, 23);
		for (int order = 1; order <= MAX_POINTS; order++)
		{
			string prefix = "Pt" + order.ToString();
			ImageWidget outer = ImageWidget.Cast(m_wRoot.FindAnyWidget(prefix + "Outer"));
			ImageWidget inner = ImageWidget.Cast(m_wRoot.FindAnyWidget(prefix + "Inner"));
			if (outer && inner)
			{
				outer.SetPivot(0.5, 0.5);
				outer.SetRotation(45);
				inner.SetPivot(0.5, 0.5);
				inner.SetRotation(45);
			}
		}
		Place("PrepBox", 0.5, 0, 0.5, 0, 0, 140, 380, 124);
		Place("PrepBg", 0, 0, 0, 0, 0, 0, 380, 124);
		Place("PrepTitle", 0, 0, 0, 0, 20, 8, 340, 26);
		Place("PrepTime", 0, 0, 0, 0, 20, 34, 340, 56);
		Place("PrepHint", 0, 0, 0, 0, 20, 92, 340, 24);
		Place("CaptureBox", 0.5, 0, 0.5, 0, 0, 22, 320, 48);
		Place("CaptureBg", 0, 0, 0, 0, 0, 0, 320, 48);
		Place("CaptureTitle", 0, 0, 0, 0, 16, 4, CAPTURE_BAR_WIDTH, 27);
		Place("CaptureBarBg", 0, 0, 0, 0, 16, 34, CAPTURE_BAR_WIDTH, CAPTURE_BAR_HEIGHT);
		Place("CaptureFill", 0, 0, 0, 0, 16, 34, 0, CAPTURE_BAR_HEIGHT);
		Place("DefAlert", 0.5, 1, 0.5, 1, 0, -180, 340, 44);
		Place("DefAlertBg", 0, 0, 0, 0, 0, 0, 340, 44);
		Place("DefAlertText", 0, 0, 0, 0, 16, 0, 310, 44);
	}

	//------------------------------------------------------------------------------------------------
	//! Цвета подложек и полос задаём из скрипта (как и цвета точек) — так они точно применяются.
	protected void ApplyColors()
	{
		SetColor("TopBg", ARGB(244, 16, 24, 33));
		SetColor("TopEdge", ARGB(255, 88, 109, 128));
		SetColor("PointTrack", ARGB(255, 85, 103, 119));
		SetColor("TimerDivider", ARGB(62, 165, 190, 210));
		SetColor("TimerText", ARGB(255, 177, 195, 211));
		SetColor("PrepBg", ARGB(128, 0, 0, 0));
		SetColor("CaptureBg", ARGB(128, 0, 0, 0));
		SetColor("CaptureBarBg", ARGB(46, 255, 255, 255));
		SetColor("CaptureFill", ARGB(255, 232, 120, 32));
		SetColor("DefAlertBg", ARGB(158, 120, 28, 20));
	}

	//------------------------------------------------------------------------------------------------
	protected void Place(string widgetName, float anchorX, float anchorY, float alignX, float alignY, float x, float y, float w, float h)
	{
		Widget widget = m_wRoot.FindAnyWidget(widgetName);
		if (!widget)
		{
			Print("[ZW_Invasion] HUD: нет виджета " + widgetName, LogLevel.WARNING);
			return;
		}

		FrameSlot.SetAnchorMin(widget, anchorX, anchorY);
		FrameSlot.SetAnchorMax(widget, anchorX, anchorY);
		FrameSlot.SetAlignment(widget, alignX, alignY);
		FrameSlot.SetPos(widget, x, y);
		FrameSlot.SetSize(widget, w, h);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateHUD()
	{
		// Пока у игрока нет персонажа (выбор фракции / экран возрождения — это тоже полноэкранная карта),
		// HUD в меню не встраиваем и не показываем: иначе он мешает мыши в штатном меню.
		bool hasCharacter = SCR_PlayerController.GetLocalControlledEntity() != null;
		Widget mapParent = null;
		if (hasCharacter)
			mapParent = GetOpenMapRoot();
		bool mapOpen = mapParent != null;
		if ((!m_wRoot || m_bHUDOnMap != mapOpen || m_wMapParent != mapParent) && !CreateHUD(mapParent))
		{
			if (m_wRoot)
				m_wRoot.SetVisible(false);
			return;
		}

		m_wRoot.SetVisible(hasCharacter);
		if (!hasCharacter)
			return;

		ZW_ObjectivesComponent objectives = ZW_Invasion.GetObjectives();
		ZW_TicketsComponent tickets = ZW_Invasion.GetTickets();
		ZW_TimerComponent timer = ZW_Invasion.GetTimer();
		if (!objectives)
			return;

		Faction attacker = objectives.GetAttackerFaction();
		Faction defender = objectives.GetDefenderFaction();
		Faction local = SCR_FactionManager.SGetLocalPlayerFaction();
		bool isAttacker = local && local == attacker;
		bool isDefender = local && local == defender;
		bool hasSide = isAttacker || isDefender;
		bool ended = objectives.IsEnded();
		SetVisible("TicketBadge", tickets && hasSide && !ended);

		if (tickets && hasSide && !ended)
			UpdateTicketBadge(isAttacker, local, tickets);

		// --- Таймер и подготовка
		bool prep = timer && timer.IsPrepPhase();
		SetVisible("PrepBox", prep && !mapOpen && !ended);
		SetVisible("TimerBar", timer && mapOpen && hasSide && !ended);
		if (prep && !mapOpen)
		{
			SetText("PrepTime", FormatTime(timer.GetRemainingSeconds()));
			if (isAttacker)
				SetText("PrepHint", "Атака: ждите на главной базе");
			else if (isDefender)
				SetText("PrepHint", "Оборона: займите точку 1");
			else
				SetText("PrepHint", "");
		}
		else if (timer && mapOpen)
		{
			string suffix = "до конца штурма";
			if (prep)
				suffix = "до начала штурма";
			SetText("TimerValue", FormatTime(timer.GetRemainingSeconds()));
			SetText("TimerText", suffix);
		}

		// --- Цепочка точек
		BaseWorld world = GetGame().GetWorld();
		int activeOrder = objectives.GetActiveIndex() + 1;   // 0 — штурм ещё не начался
		if (activeOrder < 1)
			activeOrder = 1;

		SetVisible("TopBar", mapOpen && hasSide && !ended);
		if (mapOpen && hasSide && !ended)
			UpdateMapPoints(world, activeOrder, isAttacker, objectives.GetPointCount());

		// --- Захват текущей точки
		ZW_InvasionPoint active = ZW_ObjectivesComponent.FindPointByOrder(world, activeOrder);
		UpdateCapture(active, activeOrder, isAttacker, isDefender, prep || mapOpen || ended);
	}

	//------------------------------------------------------------------------------------------------
	protected Widget GetOpenMapRoot()
	{
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity || !mapEntity.IsOpen())
			return null;

		MapConfiguration mapConfig = mapEntity.GetMapConfig();
		if (!mapConfig || mapConfig.MapEntityMode != EMapEntityMode.FULLSCREEN)
			return null;

		Widget mapRoot = mapEntity.GetMapMenuRoot();
		if (mapRoot)
			return mapRoot;

		return mapConfig.RootWidgetRef;
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateTicketBadge(bool isAttacker, notnull Faction local, notnull ZW_TicketsComponent tickets)
	{
		int count;
		if (isAttacker)
			count = tickets.GetAttackerTickets();
		else
			count = tickets.GetDefenderTickets();
		SetText("TicketCount", count.ToString());

		// Плашка зависит от фракции, а не от роли: РФ может и атаковать, и обороняться.
		string factionKey = local.GetFactionKey();
		if (m_bBadgeInitialized && m_sBadgeFactionKey == factionKey)
			return;

		m_bBadgeInitialized = true;
		m_sBadgeFactionKey = factionKey;
		bool isRf = IsRfFaction(factionKey);
		ResourceName texture = m_rNatoBadge;
		int countWidth = 54;
		if (isRf)
		{
			texture = m_rRfBadge;
			countWidth = 50;
		}

		Place("TicketCount", 0, 0, 0, 0, 167, 0, countWidth, 44);
		ImageWidget artwork = ImageWidget.Cast(m_wRoot.FindAnyWidget("TicketArtwork"));
		if (!artwork)
			return;

		artwork.SetFlags(WidgetFlags.STRETCH);
		bool loaded = artwork.LoadImageTexture(0, texture);
		artwork.SetVisible(loaded);
		if (!loaded)
			Print("[ZW_Invasion] HUD: не удалось загрузить плашку тикетов " + texture, LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsRfFaction(string factionKey)
	{
		array<string> keys = {};
		m_sRfFactionKeys.Split(",", keys, true);
		foreach (string key : keys)
		{
			key.TrimInPlace();
			if (key == factionKey)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateMapPoints(BaseWorld world, int activeOrder, bool isAttacker, int pointCount)
	{
		int visibleCount = Math.Min(pointCount, MAX_POINTS);
		float trackWidth = (visibleCount - 1) * MAP_POINT_STEP;
		float start = (MAP_PANEL_WIDTH - trackWidth) * 0.5;
		SetVisible("PointTrack", visibleCount > 1);
		if (visibleCount > 1)
			Place("PointTrack", 0, 0, 0, 0, start, 23, trackWidth, 2);

		for (int order = 1; order <= MAX_POINTS; order++)
		{
			ZW_InvasionPoint point = ZW_ObjectivesComponent.FindPointByOrder(world, order);
			string prefix = "Pt" + order.ToString();
			if (point)
			{
				float center = start + (order - 1) * MAP_POINT_STEP;
				Place(prefix + "Outer", 0, 0, 0, 0, center - 11, 13, 22, 22);
				Place(prefix + "Inner", 0, 0, 0, 0, center - 9, 15, 18, 18);
				Place(prefix + "Num", 0, 0, 0, 0, center - 12, 12, 24, 24);
			}
			UpdatePointSlot(prefix, order, point, activeOrder, isAttacker);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdatePointSlot(string prefix, int order, ZW_InvasionPoint point, int activeOrder, bool isAttacker)
	{
		bool exists = point != null;
		SetVisible(prefix + "Outer", exists);
		SetVisible(prefix + "Inner", exists);
		SetVisible(prefix + "Num", exists);
		if (!exists)
			return;

		int outer;
		int inner;
		string label = order.ToString();
		int textColor = ARGB(255, 235, 242, 248);

		if (order < activeOrder)
		{
			outer = ARGB(255, 109, 169, 230);
			inner = ARGB(255, 41, 81, 120);
		}
		else if (order == activeOrder)
		{
			outer = ARGB(255, 255, 185, 79);
			inner = ARGB(255, 91, 58, 25);
			textColor = ARGB(255, 255, 224, 179);
		}
		else if (isAttacker)
		{
			outer = ARGB(255, 104, 122, 139);
			inner = ARGB(255, 29, 41, 53);
			textColor = ARGB(255, 177, 195, 211);
			label = "?";
		}
		else
		{
			outer = ARGB(255, 201, 112, 111);
			inner = ARGB(255, 78, 43, 47);
		}

		SetColor(prefix + "Outer", outer);
		SetColor(prefix + "Inner", inner);
		SetColor(prefix + "Num", textColor);
		SetText(prefix + "Num", label);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateCapture(ZW_InvasionPoint point, int order, bool isAttacker, bool isDefender, bool hidden)
	{
		if (!point || hidden)
		{
			SetVisible("CaptureBox", false);
			SetVisible("DefAlert", false);
			return;
		}

		int progress = point.GetProgress();
		int total = point.GetCaptureSeconds();

		// Направление: растёт / падает; без изменений дольше FREEZE_AFTER_MS — замер.
		float now = GetGame().GetWorld().GetWorldTime();
		if (progress != m_iLastProgress)
		{
			if (m_iLastProgress >= 0)
			{
				if (progress > m_iLastProgress)
					m_iTrend = 1;
				else
					m_iTrend = -1;
			}
			m_iLastProgress = progress;
			m_fLastChangeTime = now;
		}
		else if (now - m_fLastChangeTime > FREEZE_AFTER_MS)
		{
			m_iTrend = 0;
		}

		// Атака: шкала захвата, только когда игрок в зоне точки.
		bool showAttack = isAttacker && IsLocalPlayerInside(point);
		SetVisible("CaptureBox", showAttack);
		if (showAttack)
		{
			string title = "Захват точки " + order.ToString();
			if (m_iTrend == 0 && progress > 0)
				title = "Точка " + order.ToString() + " оспаривается";
			else if (m_iTrend < 0)
				title = "Точку " + order.ToString() + " отбивают";

			SetText("CaptureTitle", title);

			Widget fill = m_wRoot.FindAnyWidget("CaptureFill");
			if (fill && total > 0)
				FrameSlot.SetSize(fill, CAPTURE_BAR_WIDTH * progress / total, CAPTURE_BAR_HEIGHT);
		}

		// Оборона: только статус, без процентов, времени и численности.
		bool showDefend = isDefender && point.IsBeingCaptured();
		SetVisible("DefAlert", showDefend);
		if (showDefend)
			SetText("DefAlertText", "Точку " + order.ToString() + " захватывают");
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsLocalPlayerInside(ZW_InvasionPoint point)
	{
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller)
			return false;

		IEntity player = controller.GetControlledEntity();
		if (!player)
			return false;

		return vector.Distance(player.GetOrigin(), point.GetOrigin()) <= point.GetCaptureRadius();
	}

	//------------------------------------------------------------------------------------------------
	protected static string FormatTime(int seconds)
	{
		if (seconds < 0)
			seconds = 0;

		int minutes = seconds / 60;
		int rest = seconds % 60;
		string restText = rest.ToString();
		if (rest < 10)
			restText = "0" + restText;

		return minutes.ToString() + ":" + restText;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetText(string widgetName, string text)
	{
		TextWidget widget = TextWidget.Cast(m_wRoot.FindAnyWidget(widgetName));
		if (widget)
			widget.SetText(text);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetVisible(string widgetName, bool visible)
	{
		Widget widget = m_wRoot.FindAnyWidget(widgetName);
		if (widget)
			widget.SetVisible(visible);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetColor(string widgetName, int argb)
	{
		Widget widget = m_wRoot.FindAnyWidget(widgetName);
		if (widget)
			widget.SetColor(Color.FromInt(argb));
	}
}
