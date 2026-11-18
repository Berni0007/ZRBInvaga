// ZRBinvaga — HUD режима Invasion (вариант A «Линия фронта»).
// Вешается на префаб гейм-мода. Работает только на клиенте: читает реплицированное
// состояние (тикеты, таймер, точки, прогресс) и показывает каждой стороне своё:
//   - численность игроков на точке не показывается никому;
//   - прогресс захвата видит только атака;
//   - оборона видит только статус «точку захватывают».

[ComponentEditorProps(category: "ZRB/Invasion", description: "Invasion: HUD (клиент)")]
class ZW_InvasionHUDComponentClass : SCR_BaseGameModeComponentClass
{
}

class ZW_InvasionHUDComponent : SCR_BaseGameModeComponent
{
	[Attribute("{751400A6DCCDBC09}UI/layouts/ZW_Invasion/ZW_InvasionHUD.layout", UIWidgets.ResourceNamePicker, "Layout HUD", "layout", category: "Invasion")]
	protected ResourceName m_sLayout;

	protected static const int MAX_POINTS = 7;
	protected static const int UPDATE_MS = 250;
	protected static const float FREEZE_AFTER_MS = 1500;

	protected Widget m_wRoot;
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

		if (!m_wRoot)
			m_wRoot = GetGame().GetWorkspace().CreateWidgets(m_sLayout);

		if (!m_wRoot)
		{
			Print("[ZW_Invasion] HUD: не удалось создать layout " + m_sLayout, LogLevel.ERROR);
			return;
		}

		ApplyLayout();
		ApplyColors();

		SetVisible("PrepBox", false);
		SetVisible("CaptureBox", false);
		SetVisible("DefAlert", false);

		m_bRunning = true;
		GetGame().GetCallqueue().CallLater(UpdateHUD, UPDATE_MS, true);
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
	}

	//------------------------------------------------------------------------------------------------
	//! Раскладка HUD задаётся здесь, а не в layout: позиции/размеры в пикселях эталона 1920x1080,
	//! якорь и выравнивание — доли (0..1) родителя/виджета.
	protected void ApplyLayout()
	{
		Place("TopBar", 0.5, 0, 0.5, 0, 0, 22, 640, 64);
		Place("TopBg", 0, 0, 0, 0, 0, 0, 640, 64);
		Place("AttackSwatch", 0, 0, 0, 0, 16, 22, 30, 20);
		Place("AttackTickets", 0, 0, 0, 0, 56, 4, 100, 36);
		Place("AttackLabel", 0, 0, 0, 0, 56, 40, 110, 18);
		Place("DefendSwatch", 0, 0, 0, 0, 594, 22, 30, 20);
		Place("DefendTickets", 0, 0, 0, 0, 500, 4, 90, 36);
		Place("DefendLabel", 0, 0, 0, 0, 500, 40, 90, 18);
		Place("Pt1Outer", 0, 0, 0, 0, 170, 17, 30, 30);
		Place("Pt1Inner", 0, 0, 0, 0, 173, 20, 24, 24);
		Place("Pt1Num", 0, 0, 0, 0, 180, 17, 20, 30);
		Place("Pt2Outer", 0, 0, 0, 0, 214, 17, 30, 30);
		Place("Pt2Inner", 0, 0, 0, 0, 217, 20, 24, 24);
		Place("Pt2Num", 0, 0, 0, 0, 224, 17, 20, 30);
		Place("Pt3Outer", 0, 0, 0, 0, 258, 17, 30, 30);
		Place("Pt3Inner", 0, 0, 0, 0, 261, 20, 24, 24);
		Place("Pt3Num", 0, 0, 0, 0, 268, 17, 20, 30);
		Place("Pt4Outer", 0, 0, 0, 0, 302, 17, 30, 30);
		Place("Pt4Inner", 0, 0, 0, 0, 305, 20, 24, 24);
		Place("Pt4Num", 0, 0, 0, 0, 312, 17, 20, 30);
		Place("Pt5Outer", 0, 0, 0, 0, 346, 17, 30, 30);
		Place("Pt5Inner", 0, 0, 0, 0, 349, 20, 24, 24);
		Place("Pt5Num", 0, 0, 0, 0, 356, 17, 20, 30);
		Place("Pt6Outer", 0, 0, 0, 0, 390, 17, 30, 30);
		Place("Pt6Inner", 0, 0, 0, 0, 393, 20, 24, 24);
		Place("Pt6Num", 0, 0, 0, 0, 400, 17, 20, 30);
		Place("Pt7Outer", 0, 0, 0, 0, 434, 17, 30, 30);
		Place("Pt7Inner", 0, 0, 0, 0, 437, 20, 24, 24);
		Place("Pt7Num", 0, 0, 0, 0, 444, 17, 20, 30);
		Place("TimerBar", 0.5, 0, 0.5, 0, 0, 92, 260, 30);
		Place("TimerBg", 0, 0, 0, 0, 0, 0, 260, 30);
		Place("TimerText", 0, 0, 0, 0, 12, 0, 240, 30);
		Place("PrepBox", 0.5, 0, 0.5, 0, 0, 140, 380, 124);
		Place("PrepBg", 0, 0, 0, 0, 0, 0, 380, 124);
		Place("PrepTitle", 0, 0, 0, 0, 20, 8, 340, 26);
		Place("PrepTime", 0, 0, 0, 0, 20, 34, 340, 56);
		Place("PrepHint", 0, 0, 0, 0, 20, 92, 340, 24);
		Place("CaptureBox", 0.5, 1, 0.5, 1, 0, -180, 400, 64);
		Place("CaptureBg", 0, 0, 0, 0, 0, 0, 400, 64);
		Place("CaptureTitle", 0, 0, 0, 0, 16, 4, 270, 30);
		Place("CaptureTime", 0, 0, 0, 0, 290, 4, 100, 30);
		Place("CaptureBarBg", 0, 0, 0, 0, 16, 42, 368, 8);
		Place("CaptureFill", 0, 0, 0, 0, 16, 42, 0, 8);
		Place("DefAlert", 0.5, 1, 0.5, 1, 0, -180, 340, 44);
		Place("DefAlertBg", 0, 0, 0, 0, 0, 0, 340, 44);
		Place("DefAlertText", 0, 0, 0, 0, 16, 0, 310, 44);
	}

	//------------------------------------------------------------------------------------------------
	//! Цвета подложек и полос задаём из скрипта (как и цвета точек) — так они точно применяются.
	protected void ApplyColors()
	{
		SetColor("TopBg", ARGB(122, 0, 0, 0));
		SetColor("TimerBg", ARGB(107, 0, 0, 0));
		SetColor("PrepBg", ARGB(128, 0, 0, 0));
		SetColor("CaptureBg", ARGB(128, 0, 0, 0));
		SetColor("CaptureBarBg", ARGB(46, 255, 255, 255));
		SetColor("CaptureFill", ARGB(255, 232, 120, 32));
		SetColor("DefAlertBg", ARGB(158, 120, 28, 20));
		SetColor("AttackSwatch", ARGB(255, 91, 143, 216));
		SetColor("DefendSwatch", ARGB(255, 200, 71, 58));
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
		if (!m_wRoot)
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

		// --- Тикеты
		if (tickets)
		{
			SetText("AttackTickets", tickets.GetAttackerTickets().ToString());
			SetText("DefendTickets", tickets.GetDefenderTickets().ToString());
		}

		if (attacker)
			SetText("AttackLabel", "Атака (" + attacker.GetFactionKey() + ")");
		if (defender)
			SetText("DefendLabel", "Оборона (" + defender.GetFactionKey() + ")");

		// --- Таймер и подготовка
		bool prep = false;
		if (timer)
		{
			prep = timer.IsPrepPhase();
			string time = FormatTime(timer.GetRemainingSeconds());
			SetVisible("PrepBox", prep);
			SetVisible("TimerBar", !prep);

			if (prep)
			{
				SetText("PrepTime", time);
				if (isAttacker)
					SetText("PrepHint", "Атака: ждите на главной базе");
				else if (isDefender)
					SetText("PrepHint", "Оборона: займите точку 1");
				else
					SetText("PrepHint", "");
			}
			else
			{
				SetText("TimerText", time + "  до конца штурма");
			}
		}

		// --- Цепочка точек
		BaseWorld world = GetGame().GetWorld();
		int activeOrder = objectives.GetActiveIndex() + 1;   // 0 — штурм ещё не начался
		if (activeOrder < 1)
			activeOrder = 1;

		for (int order = 1; order <= MAX_POINTS; order++)
		{
			ZW_InvasionPoint point = ZW_ObjectivesComponent.FindPointByOrder(world, order);
			UpdatePointSlot(order, point, activeOrder, isAttacker, objectives.IsEnded());
		}

		// --- Захват текущей точки
		ZW_InvasionPoint active = ZW_ObjectivesComponent.FindPointByOrder(world, activeOrder);
		UpdateCapture(active, activeOrder, isAttacker, isDefender, prep || objectives.IsEnded());
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdatePointSlot(int order, ZW_InvasionPoint point, int activeOrder, bool isAttacker, bool ended)
	{
		string prefix = "Pt" + order.ToString();
		bool exists = point != null;
		SetVisible(prefix + "Outer", exists);
		SetVisible(prefix + "Inner", exists);
		SetVisible(prefix + "Num", exists);
		if (!exists)
			return;

		int blue = ARGB(255, 91, 143, 216);
		int red = ARGB(255, 200, 71, 58);
		int orange = ARGB(255, 232, 120, 32);
		int fog = ARGB(140, 255, 255, 255);
		int dark = ARGB(150, 0, 0, 0);

		int outer;
		int inner;
		string label = order.ToString();

		if (order < activeOrder || (ended && order <= activeOrder))
		{
			outer = blue;                // взята атакой
			inner = blue;
		}
		else if (order == activeOrder)
		{
			outer = orange;              // текущая цель
			inner = dark;
		}
		else if (isAttacker)
		{
			outer = fog;                 // туман войны: атака не видит следующие точки
			inner = dark;
			label = "?";
		}
		else
		{
			outer = red;                 // у обороны
			inner = ARGB(90, 200, 71, 58);
		}

		SetColor(prefix + "Outer", outer);
		SetColor(prefix + "Inner", inner);
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

		// Атака: подробности захвата, только когда игрок в зоне точки.
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
			SetText("CaptureTime", progress.ToString() + " / " + total.ToString() + " с");

			Widget fill = m_wRoot.FindAnyWidget("CaptureFill");
			if (fill && total > 0)
				FrameSlot.SetSize(fill, 368.0 * progress / total, 8);
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
