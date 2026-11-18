// ZRBinvaga — стороны атаки/обороны, цепочка точек и условия победы.
// Вешается на префаб гейм-мода. Всю логику считает сервер.

[ComponentEditorProps(category: "ZRB/Invasion", description: "Invasion: стороны, цепочка точек, победа")]
class ZW_ObjectivesComponentClass : SCR_BaseGameModeComponentClass
{
}

class ZW_ObjectivesComponent : SCR_BaseGameModeComponent
{
	[Attribute("US", desc: "Ключ фракции атаки (из FactionManager)", category: "Invasion")]
	protected FactionKey m_sAttackerFactionKey;

	[Attribute("USSR", desc: "Ключ фракции обороны (из FactionManager)", category: "Invasion")]
	protected FactionKey m_sDefenderFactionKey;

	//! Все точки на карте; точки регистрируются сами при инициализации.
	protected static ref array<ZW_InvasionPoint> s_aPoints;

	//! Индекс текущей цели атаки в отсортированной цепочке (-1 = штурм ещё не начался).
	[RplProp(onRplName: "OnActiveIndexChanged")]
	protected int m_iActiveIndex = -1;

	protected bool m_bEnded;

	[Attribute("0", UIWidgets.EditBox, "ОТЛАДКА: авто-захват текущей точки каждые N сек (0 = выкл). Только для тестов, на сервере держать 0!", category: "Invasion Debug")]
	protected int m_iDebugAutoCaptureSeconds;

	protected int m_iDebugCounter;

	//------------------------------------------------------------------------------------------------
	static void RegisterPoint(notnull ZW_InvasionPoint point)
	{
		if (!s_aPoints)
			s_aPoints = new array<ZW_InvasionPoint>();

		if (!s_aPoints.Contains(point))
			s_aPoints.Insert(point);
	}

	//------------------------------------------------------------------------------------------------
	static void UnregisterPoint(ZW_InvasionPoint point)
	{
		if (s_aPoints)
			s_aPoints.RemoveItem(point);
	}

	//------------------------------------------------------------------------------------------------
	Faction GetAttackerFaction()
	{
		return GetGame().GetFactionManager().GetFactionByKey(m_sAttackerFactionKey);
	}

	//------------------------------------------------------------------------------------------------
	Faction GetDefenderFaction()
	{
		return GetGame().GetFactionManager().GetFactionByKey(m_sDefenderFactionKey);
	}

	//------------------------------------------------------------------------------------------------
	int GetActiveIndex()
	{
		return m_iActiveIndex;
	}

	//------------------------------------------------------------------------------------------------
	int GetPointCount()
	{
		if (!s_aPoints)
			return 0;

		return s_aPoints.Count();
	}

	//------------------------------------------------------------------------------------------------
	ZW_InvasionPoint GetPoint(int index)
	{
		if (!s_aPoints || index < 0 || index >= s_aPoints.Count())
			return null;

		return s_aPoints[index];
	}

	//------------------------------------------------------------------------------------------------
	bool IsEnded()
	{
		return m_bEnded;
	}

	//------------------------------------------------------------------------------------------------
	override void OnGameModeStart()
	{
		super.OnGameModeStart();

		if (!Replication.IsServer())
			return;

		Print("[ZW_Invasion] v0.3.0 старт матча", LogLevel.WARNING);

		if (!GetAttackerFaction() || !GetDefenderFaction())
		{
			Print("[ZW_Invasion] Не найдены фракции атаки/обороны — проверьте ключи в ZW_ObjectivesComponent", LogLevel.ERROR);
			return;
		}

		// В Workbench статический список переживает перезапуск игры и сцены редактора,
		// поэтому оставляем только живые точки из текущего игрового мира.
		PrunePoints();

		if (GetPointCount() == 0)
		{
			Print("[ZW_Invasion] На карте нет ни одной ZW_InvasionPoint", LogLevel.ERROR);
			return;
		}

		SortPoints();
		Print(string.Format("[ZW_Invasion] Точек в цепочке: %1", GetPointCount()), LogLevel.WARNING);
		foreach (ZW_InvasionPoint p : s_aPoints)
		{
			Print(string.Format("[ZW_Invasion]   точка #%1 '%2' %3", p.GetOrder(), p.GetPointName(), p.GetOrigin()), LogLevel.WARNING);
		}

		// Со старта все точки у обороны и закрыты для захвата.
		Faction defender = GetDefenderFaction();
		foreach (ZW_InvasionPoint point : s_aPoints)
		{
			point.ZW_InitAuthority(defender);
		}

		// Без таймера штурм начинается сразу; с таймером — после фазы подготовки.
		if (!ZW_Invasion.GetTimer())
			StartAssault();
	}

	//------------------------------------------------------------------------------------------------
	//! Вызывается таймером, когда кончилась подготовка. Открывает первую точку.
	void StartAssault()
	{
		if (!Replication.IsServer() || m_bEnded || m_iActiveIndex >= 0)
			return;

		ActivatePoint(0);
		GetGame().GetCallqueue().CallLater(TickActivePoint, 1000, true);
		Print("[ZW_Invasion] Штурм начался", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер, раз в секунду: считаем захват текущей цели.
	protected void TickActivePoint()
	{
		if (m_bEnded)
		{
			GetGame().GetCallqueue().Remove(TickActivePoint);
			return;
		}

		ZW_InvasionPoint point = GetPoint(m_iActiveIndex);
		if (!point)
			return;

		// Отладка: точка берётся сама через N секунд, без игроков.
		if (m_iDebugAutoCaptureSeconds > 0)
		{
			m_iDebugCounter++;
			if (m_iDebugCounter >= m_iDebugAutoCaptureSeconds)
			{
				m_iDebugCounter = 0;
				Print(string.Format("[ZW_Invasion] ОТЛАДКА: авто-захват точки #%1", point.GetOrder()), LogLevel.WARNING);
				point.ZW_LockForAttacker(GetAttackerFaction());
				OnPointCaptured(point);
				return;
			}
		}

		if (point.ZW_Tick(GetAttackerFaction(), GetDefenderFaction()))
			OnPointCaptured(point);
	}

	//------------------------------------------------------------------------------------------------
	override void OnGameModeEnd(SCR_GameModeEndData data)
	{
		super.OnGameModeEnd(data);
		GetGame().GetCallqueue().Remove(TickActivePoint);
	}

	//------------------------------------------------------------------------------------------------
	//! Точка сообщает, что её взяла атака.
	void OnPointCaptured(notnull ZW_InvasionPoint point)
	{
		if (!Replication.IsServer() || m_bEnded)
			return;

		int index = s_aPoints.Find(point);
		if (index != m_iActiveIndex)
			return; // захватить можно только текущую цель

		Print(string.Format("[ZW_Invasion] Точка %1 (%2) взята атакой", point.GetOrder(), point.GetPointName()), LogLevel.NORMAL);

		// Последняя точка: оборона теряет все тикеты, победа атаки.
		if (index >= s_aPoints.Count() - 1)
		{
			ZW_TicketsComponent tickets = ZW_Invasion.GetTickets();
			if (tickets)
				tickets.ZeroDefenderTickets();

			EndInvasion(true, EGameOverTypes.ENDREASON_SCORELIMIT);
			return;
		}

		ZW_TicketsComponent ticketsComp = ZW_Invasion.GetTickets();
		if (ticketsComp)
			ticketsComp.AddCaptureBonus();

		ZW_TimerComponent timer = ZW_Invasion.GetTimer();
		if (timer)
			timer.AddCaptureBonus();

		LogState("после взятия точки");
		ActivatePoint(index + 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Конец матча. attackersWin = true — победила атака.
	void EndInvasion(bool attackersWin, EGameOverTypes reason)
	{
		if (!Replication.IsServer() || m_bEnded)
			return;

		m_bEnded = true;

		Faction winner = GetDefenderFaction();
		if (attackersWin)
			winner = GetAttackerFaction();

		int winnerIndex = GetGame().GetFactionManager().GetFactionIndex(winner);
		Print(string.Format("[ZW_Invasion] Конец матча. Победила %1 (причина %2)", winner.GetFactionKey(), reason), LogLevel.WARNING);
		LogState("конец матча");

		GetGameMode().EndGameMode(SCR_GameModeEndData.CreateSimple(reason, -1, winnerIndex));
	}

	//------------------------------------------------------------------------------------------------
	//! Тикеты и время в лог — для проверки баланса и отладки.
	protected void LogState(string context)
	{
		ZW_TicketsComponent tickets = ZW_Invasion.GetTickets();
		ZW_TimerComponent timer = ZW_Invasion.GetTimer();

		int attackerTickets = -1;
		int defenderTickets = -1;
		if (tickets)
		{
			attackerTickets = tickets.GetAttackerTickets();
			defenderTickets = tickets.GetDefenderTickets();
		}

		int seconds = -1;
		if (timer)
			seconds = timer.GetRemainingSeconds();

		Print(string.Format("[ZW_Invasion] Состояние (%1): тикеты атака %2 / оборона %3, осталось %4 с", context, attackerTickets, defenderTickets, seconds), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	protected void ActivatePoint(int index)
	{
		m_iActiveIndex = index;
		Replication.BumpMe();

		ZW_InvasionPoint point = GetPoint(index);
		if (point)
			point.ZW_SetActive();
	}

	//------------------------------------------------------------------------------------------------
	//! Убирает из списка удалённые точки и точки из чужих миров (редактор, сцена префаба).
	protected void PrunePoints()
	{
		if (!s_aPoints)
		{
			s_aPoints = new array<ZW_InvasionPoint>();
			return;
		}

		BaseWorld gameWorld = GetOwner().GetWorld();
		for (int i = s_aPoints.Count() - 1; i >= 0; i--)
		{
			ZW_InvasionPoint point = s_aPoints[i];
			if (!point || point.GetWorld() != gameWorld)
				s_aPoints.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Сортировка цепочки по порядковому номеру точки (вставками, точек мало).
	protected void SortPoints()
	{
		for (int i = 1; i < s_aPoints.Count(); i++)
		{
			ZW_InvasionPoint current = s_aPoints[i];
			int j = i - 1;
			while (j >= 0 && s_aPoints[j].GetOrder() > current.GetOrder())
			{
				s_aPoints[j + 1] = s_aPoints[j];
				j--;
			}
			s_aPoints[j + 1] = current;
		}

		// Проверка: номера без дублей.
		for (int k = 1; k < s_aPoints.Count(); k++)
		{
			if (s_aPoints[k].GetOrder() == s_aPoints[k - 1].GetOrder())
				Print(string.Format("[ZW_Invasion] Дубль порядкового номера %1 у точек", s_aPoints[k].GetOrder()), LogLevel.WARNING);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! На клиентах: сменилась текущая цель (понадобится HUD и туману войны на этапе 3).
	protected void OnActiveIndexChanged()
	{
	}
}
