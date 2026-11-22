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
	protected static const float CAPTURE_BOX_WIDTH = 170;
	protected static const float CAPTURE_FLAG_X = 40;
	protected static const float CAPTURE_FLAG_WIDTH = 90;
	protected static const float CAPTURE_FLAG_HEIGHT = 52;
	protected static const float CAPTURE_BAR_X = 15;
	protected static const float CAPTURE_BAR_Y = 58;
	protected static const float CAPTURE_BAR_WIDTH = 140;
	protected static const float CAPTURE_BAR_HEIGHT = 18;

	//! Какой флаг сейчас загружен в FlagImg — чтобы не грузить текстуру каждый кадр.
	protected string m_sShownFlag;

	//! Флаги, загруженные в слоты цепочки (имя виджета → текстура).
	protected ref map<string, string> m_mSlotFlags = new map<string, string>();

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

		// Новые виджеты — флаги в них ещё не загружены.
		m_mSlotFlags.Clear();
		m_sShownFlag = string.Empty;

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
		m_mSlotFlags.Clear();
		m_sShownFlag = string.Empty;
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
		Place("PrepBox", 0.5, 0, 0.5, 0, 0, 140, 380, 124);
		Place("PrepBg", 0, 0, 0, 0, 0, 0, 380, 124);
		Place("PrepTitle", 0, 0, 0, 0, 20, 8, 340, 26);
		Place("PrepTime", 0, 0, 0, 0, 20, 34, 340, 56);
		Place("PrepHint", 0, 0, 0, 0, 20, 92, 340, 24);
		// Индикатор точки: флаг владельца (замок — взята навсегда), полоса со стрелками, название места.
		Place("CaptureBox", 0.5, 0, 0.5, 0, 0, 22, CAPTURE_BOX_WIDTH, 99);
		Place("FlagBorder", 0, 0, 0, 0, CAPTURE_FLAG_X, 0, CAPTURE_FLAG_WIDTH, CAPTURE_FLAG_HEIGHT);
		Place("FlagFill", 0, 0, 0, 0, CAPTURE_FLAG_X + 1.5, 1.5, CAPTURE_FLAG_WIDTH - 3, CAPTURE_FLAG_HEIGHT - 3);
		Place("FlagImg", 0, 0, 0, 0, CAPTURE_FLAG_X + 1.5, 1.5, CAPTURE_FLAG_WIDTH - 3, CAPTURE_FLAG_HEIGHT - 3);
		float lockX = CAPTURE_FLAG_X + CAPTURE_FLAG_WIDTH - 22;
		Place("LockBg", 0, 0, 0, 0, lockX, 29, 19, 19);
		Place("LockShackleL", 0, 0, 0, 0, lockX + 5.5, 31.5, 2, 6.5);
		Place("LockShackleR", 0, 0, 0, 0, lockX + 11.5, 31.5, 2, 6.5);
		Place("LockShackleTop", 0, 0, 0, 0, lockX + 5.5, 31.5, 8, 2);
		Place("LockBody", 0, 0, 0, 0, lockX + 3.5, 37.5, 12, 8);
		// Полоса: чёрная рамка 1,5 px, тёмная подложка, яркая заливка, крупные белые стрелки поверх.
		Place("CaptureBg", 0, 0, 0, 0, CAPTURE_BAR_X - 1.5, CAPTURE_BAR_Y - 1.5, CAPTURE_BAR_WIDTH + 3, CAPTURE_BAR_HEIGHT + 3);
		Place("CaptureBarBg", 0, 0, 0, 0, CAPTURE_BAR_X, CAPTURE_BAR_Y, CAPTURE_BAR_WIDTH, CAPTURE_BAR_HEIGHT);
		Place("CaptureFill", 0, 0, 0, 0, CAPTURE_BAR_X, CAPTURE_BAR_Y, 0, CAPTURE_BAR_HEIGHT);
		Place("CaptureArrows", 0, 0, 0, 0, CAPTURE_BAR_X, CAPTURE_BAR_Y - 1, CAPTURE_BAR_WIDTH, CAPTURE_BAR_HEIGHT + 2);
		Place("CaptureTitle", 0, 0, 0, 0, 0, CAPTURE_BAR_Y + CAPTURE_BAR_HEIGHT + 4, CAPTURE_BOX_WIDTH, 19);
		TextWidget title = TextWidget.Cast(m_wRoot.FindAnyWidget("CaptureTitle"));
		if (title)
			title.SetExactFontSize(15);
		TextWidget arrows = TextWidget.Cast(m_wRoot.FindAnyWidget("CaptureArrows"));
		if (arrows)
			arrows.SetExactFontSize(20);
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
		SetColor("CaptureBg", ARGB(230, 0, 0, 0));
		SetColor("FlagBorder", ARGB(255, 17, 17, 17));
		SetColor("FlagFill", ARGB(255, 233, 233, 233));
		SetColor("LockBg", ARGB(255, 240, 240, 240));
		SetColor("LockShackleL", ARGB(255, 17, 17, 17));
		SetColor("LockShackleR", ARGB(255, 17, 17, 17));
		SetColor("LockShackleTop", ARGB(255, 17, 17, 17));
		SetColor("LockBody", ARGB(255, 17, 17, 17));
		SetColor("CaptureBarBg", ARGB(220, 40, 44, 48));
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
		bool hasCharacter = IsLocalCharacterAlive();
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
		int activeOrder = objectives.GetActiveIndex() + 1;   // 0 — штурм ещё не начался
		if (activeOrder < 1)
			activeOrder = 1;

		SetVisible("TopBar", mapOpen && hasSide && !ended);
		if (mapOpen && hasSide && !ended)
			UpdateMapPoints(objectives, activeOrder, isAttacker, attacker, defender);

		// --- Захват текущей точки
		ZW_InvasionPoint active = objectives.GetPoint(activeOrder - 1);
		UpdateCapture(active, activeOrder, isAttacker, isDefender, prep || mapOpen || ended, attacker, defender);
	}

	//------------------------------------------------------------------------------------------------
	//! Есть живой персонаж. После смерти игра ещё какое-то время считает труп управляемым,
	//! а экран возрождения уже открыт — поэтому проверяем именно «жив».
	protected bool IsLocalCharacterAlive()
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!character)
			return false;

		CharacterControllerComponent controller = character.GetCharacterController();
		return controller && !controller.IsDead();
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
	protected void UpdateMapPoints(ZW_ObjectivesComponent objectives, int activeOrder, bool isAttacker, Faction attacker, Faction defender)
	{
		int visibleCount = Math.Min(objectives.GetPointCount(), MAX_POINTS);
		float trackWidth = (visibleCount - 1) * MAP_POINT_STEP;
		float start = (MAP_PANEL_WIDTH - trackWidth) * 0.5;
		SetVisible("PointTrack", visibleCount > 1);
		if (visibleCount > 1)
			Place("PointTrack", 0, 0, 0, 0, start, 17, trackWidth, 2);

		for (int order = 1; order <= MAX_POINTS; order++)
		{
			ZW_InvasionPoint point = objectives.GetPoint(order - 1);
			string prefix = "Pt" + order.ToString();
			if (point)
			{
				// Флажок 32x20 с рамкой, номер под ним.
				float center = start + (order - 1) * MAP_POINT_STEP;
				Place(prefix + "Outer", 0, 0, 0, 0, center - 18, 6, 36, 24);
				Place(prefix + "Inner", 0, 0, 0, 0, center - 16, 8, 32, 20);
				Place(prefix + "Flag", 0, 0, 0, 0, center - 16, 8, 32, 20);
				Place(prefix + "Num", 0, 0, 0, 0, center - 16, 28, 32, 16);
			}
			UpdatePointSlot(prefix, order, point, activeOrder, isAttacker, attacker, defender);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Флаг стороны, которая держит точку: взятые — флаг атаки, остальные — флаг обороны,
	//! нейтральная — белый. Текущая цель — оранжевая рамка. Атака дальние точки не знает — «?».
	protected void UpdatePointSlot(string prefix, int order, ZW_InvasionPoint point, int activeOrder, bool isAttacker, Faction attacker, Faction defender)
	{
		bool exists = point != null;
		SetVisible(prefix + "Outer", exists);
		SetVisible(prefix + "Inner", exists);
		SetVisible(prefix + "Flag", exists);
		SetVisible(prefix + "Num", exists);
		if (!exists)
			return;

		bool captured = point.IsLocked() || order < activeOrder;
		bool current = order == activeOrder && !captured;
		bool unknown = isAttacker && !captured && !current;

		Faction holder = defender;
		if (captured)
			holder = attacker;
		else if (current && point.GetProgress() >= ZW_InvasionPoint.PROGRESS_NEUTRAL)
			holder = null;

		int outer = ARGB(255, 17, 17, 17);
		int textColor = ARGB(255, 235, 242, 248);
		if (current)
		{
			outer = ARGB(255, 255, 185, 79);
			textColor = ARGB(255, 255, 224, 179);
		}

		string label = order.ToString();
		if (unknown)
		{
			textColor = ARGB(255, 177, 195, 211);
			SetColor(prefix + "Inner", ARGB(255, 29, 41, 53));
			SetVisible(prefix + "Flag", false);
		}
		else
		{
			SetColor(prefix + "Inner", ARGB(255, 233, 233, 233)); // белый — нейтральная
			ShowSlotFlag(prefix + "Flag", holder);
		}

		SetColor(prefix + "Outer", outer);
		SetColor(prefix + "Num", textColor);
		SetText(prefix + "Num", label);
	}

	//------------------------------------------------------------------------------------------------
	//! Загружает флаг фракции в виджет (только при смене); null — скрыть (виден белый фон).
	protected void ShowSlotFlag(string widgetName, Faction faction)
	{
		ImageWidget flag = ImageWidget.Cast(m_wRoot.FindAnyWidget(widgetName));
		if (!flag)
			return;

		SCR_Faction scrFaction = SCR_Faction.Cast(faction);
		ResourceName texture;
		if (scrFaction)
			texture = scrFaction.GetFactionFlag();

		if (texture.IsEmpty())
		{
			flag.SetVisible(false);
			return;
		}

		string loaded;
		m_mSlotFlags.Find(widgetName, loaded);
		if (loaded != texture)
		{
			flag.LoadImageTexture(0, texture, false, false);
			flag.SetImage(0);
			m_mSlotFlags.Set(widgetName, texture);
		}
		flag.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateCapture(ZW_InvasionPoint point, int order, bool isAttacker, bool isDefender, bool hidden, Faction attacker, Faction defender)
	{
		if (!point || hidden)
		{
			SetVisible("CaptureBox", false);
			SetVisible("DefAlert", false);
			return;
		}

		int progress = point.GetProgress();
		int neutral = ZW_InvasionPoint.PROGRESS_NEUTRAL;
		bool locked = point.IsLocked();

		// Атака видит индикатор в зоне, оборона — только пока атака двигает прогресс.
		bool inside = (isAttacker || isDefender) && IsLocalPlayerInside(point);
		bool attackCapturing = point.GetDrive() > 0;
		bool showCapture = inside && (isAttacker || (isDefender && attackCapturing));
		SetVisible("CaptureBox", showCapture);
		if (showCapture)
		{
			// Флаг: оборона → белый (нейтральная) → атака с замком.
			Faction holder = defender;
			if (locked)
				holder = attacker;
			else if (progress >= neutral)
				holder = null;
			ShowFlag(holder);

			SetVisible("LockBg", locked);
			SetVisible("LockShackleL", locked);
			SetVisible("LockShackleR", locked);
			SetVisible("LockShackleTop", locked);
			SetVisible("LockBody", locked);

			// Полоса: до нейтрали — «здоровье» флага обороны, после — заполнение флагом атаки.
			float share;
			Faction barSide;
			if (progress < neutral && !locked)
			{
				share = (neutral - progress) * 1.0 / neutral;
				barSide = defender;
			}
			else
			{
				share = (progress - neutral) * 1.0 / neutral;
				barSide = attacker;
			}

			Widget fill = m_wRoot.FindAnyWidget("CaptureFill");
			if (fill)
				FrameSlot.SetSize(fill, CAPTURE_BAR_WIDTH * Math.Clamp(share, 0, 1), CAPTURE_BAR_HEIGHT);
			SetColor("CaptureFill", SideColor(barSide));

			// Стрелки: направо — давит атака, налево — отбивает оборона; число = ступень перевеса.
			int drive = point.GetDrive();
			string arrow = ">";
			if (drive < 0)
				arrow = "<";
			string arrows;
			for (int a = 0; a < Math.AbsInt(drive); a++)
			{
				if (a > 0)
					arrows += " ";
				arrows += arrow;
			}
			SetColor("CaptureArrows", ARGB(255, 255, 255, 255));

			// Атаки больше, но меньше минимума — подсказываем, почему захват стоит.
			if (drive == 0 && point.IsShortHanded() && !locked)
			{
				arrows = string.Format("нужно %1+ бойца", point.GetMinPlayers());
			}
			TextWidget arrowText = TextWidget.Cast(m_wRoot.FindAnyWidget("CaptureArrows"));
			if (arrowText)
			{
				int fontSize = 20;
				if (drive == 0)
					fontSize = 14;

				arrowText.SetExactFontSize(fontSize);
				arrowText.SetText(arrows);
			}

			SetText("CaptureTitle", point.GetPointName());
		}

		// Обороне вне зоны — короткое предупреждение, без процентов и численности.
		bool showDefend = isDefender && !inside && attackCapturing;
		SetVisible("DefAlert", showDefend);
		if (showDefend)
			SetText("DefAlertText", point.GetPointName() + ": точку захватывают");
	}

	//------------------------------------------------------------------------------------------------
	//! Флаг стороны; null — белый нейтральный.
	protected void ShowFlag(Faction faction)
	{
		ImageWidget flag = ImageWidget.Cast(m_wRoot.FindAnyWidget("FlagImg"));
		if (!flag)
			return;

		SCR_Faction scrFaction = SCR_Faction.Cast(faction);
		ResourceName texture;
		if (scrFaction)
			texture = scrFaction.GetFactionFlag();

		if (texture.IsEmpty())
		{
			flag.SetVisible(false);
			m_sShownFlag = string.Empty;
			return;
		}

		if (texture != m_sShownFlag)
		{
			flag.LoadImageTexture(0, texture, false, false);
			flag.SetImage(0);
			m_sShownFlag = texture;
		}
		flag.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	protected int SideColor(Faction faction)
	{
		if (faction && IsRfFaction(faction.GetFactionKey()))
			return ARGB(255, 230, 40, 40);
		return ARGB(255, 40, 120, 255);
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

		return point.IsInsideZone(player.GetOrigin());
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
