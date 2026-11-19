// ZRBinvaga — стороны атаки/обороны, цепочка точек и условия победы.
// Вешается на префаб гейм-мода. Всю логику считает сервер.
//
// Смена сторон: каждый матч стороны меняются (атака ↔ оборона). Сервер помнит, кто атаковал
// в прошлом матче, в файле $profile:ZW_Invasion_state.txt.
// Полосы: у точки есть полоса (north / south / ...), пустая — общая. На старте сервер выбирает
// одну полосу (по возможности не ту, что в прошлом матче); цепочка = точки этой полосы + общие.

[ComponentEditorProps(category: "ZRB/Invasion", description: "Invasion: стороны, цепочка точек, победа")]
class ZW_ObjectivesComponentClass : SCR_BaseGameModeComponentClass
{
}

class ZW_ObjectivesComponent : SCR_BaseGameModeComponent
{
	[Attribute("US", desc: "Ключ фракции атаки в первом матче (из FactionManager)", category: "Invasion")]
	protected FactionKey m_sStartAttackerKey;

	[Attribute("USSR", desc: "Ключ фракции обороны в первом матче (из FactionManager)", category: "Invasion")]
	protected FactionKey m_sStartDefenderKey;

	[Attribute("1", UIWidgets.CheckBox, "Менять стороны каждый матч (атака ↔ оборона)", category: "Invasion")]
	protected bool m_bSwapSides;

	protected const string STATE_FILE = "$profile:ZW_Invasion_state.txt";

	//! Стороны текущего матча (решает сервер на старте).
	[RplProp()]
	protected string m_sAttackerFactionKey;

	[RplProp()]
	protected string m_sDefenderFactionKey;

	//! Выбранная полоса ("" — полос на карте нет).
	[RplProp(onRplName: "OnLaneChanged")]
	protected string m_sLane;

	//! Все точки на карте; точки регистрируются сами при инициализации.
	protected static ref array<ZW_InvasionPoint> s_aPoints;
	protected static bool s_bPointsChanged;

	//! Цепочка текущего матча: точки выбранной полосы + общие, по порядку. Считается и на клиентах.
	protected ref array<ZW_InvasionPoint> m_aChain = {};
	protected bool m_bChainDirty = true;

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

		s_bPointsChanged = true;
	}

	//------------------------------------------------------------------------------------------------
	static void UnregisterPoint(ZW_InvasionPoint point)
	{
		if (s_aPoints)
			s_aPoints.RemoveItem(point);

		s_bPointsChanged = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Полосы совместимы: одна из них общая (пустая) или они совпадают.
	static bool LanesCompatible(string a, string b)
	{
		return a.IsEmpty() || b.IsEmpty() || a == b;
	}

	//------------------------------------------------------------------------------------------------
	//! Точка с заданным номером в цепочке текущего матча.
	//! Без матча (редактор) — любая точка с этим номером.
	static ZW_InvasionPoint FindPointByOrder(BaseWorld world, int order, ZW_InvasionPoint exclude = null)
	{
		ZW_ObjectivesComponent objectives = ZW_Invasion.GetObjectives();
		if (objectives && objectives.GetOwner() && objectives.GetOwner().GetWorld() == world)
		{
			foreach (ZW_InvasionPoint chainPoint : objectives.GetChain())
			{
				if (chainPoint && chainPoint != exclude && chainPoint.GetOrder() == order)
					return chainPoint;
			}

			return null;
		}

		if (!s_aPoints)
			return null;

		foreach (ZW_InvasionPoint point : s_aPoints)
		{
			if (point && point != exclude && point.GetWorld() == world && point.GetOrder() == order)
				return point;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Все точки с номером order, совместимые с полосой lane (для редактора и проверки).
	static void FindLanePoints(BaseWorld world, int order, string lane, ZW_InvasionPoint exclude, notnull array<ZW_InvasionPoint> result)
	{
		result.Clear();
		if (!s_aPoints)
			return;

		foreach (ZW_InvasionPoint point : s_aPoints)
		{
			if (point && point != exclude && point.GetWorld() == world && point.GetOrder() == order && LanesCompatible(point.GetLane(), lane))
				result.Insert(point);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Проверка расстановки одной точки. Пустая строка — ошибок нет.
	//! Используется и подсветкой в редакторе, и проверкой на старте матча.
	static string ValidatePoint(notnull ZW_InvasionPoint point)
	{
		int order = point.GetOrder();
		if (order < 1)
			return "order < 1";

		BaseWorld world = point.GetWorld();
		array<ZW_InvasionPoint> found = {};
		FindLanePoints(world, order, point.GetLane(), point, found);
		if (!found.IsEmpty())
			return string.Format("duplicate #%1", order);

		if (order > 1)
		{
			FindLanePoints(world, order - 1, point.GetLane(), point, found);
			if (found.IsEmpty())
				return string.Format("missing #%1", order - 1);
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	string GetLane()
	{
		return m_sLane;
	}

	//------------------------------------------------------------------------------------------------
	//! Цепочка текущего матча (пересчитывается при регистрации точек и смене полосы).
	array<ZW_InvasionPoint> GetChain()
	{
		if (m_bChainDirty || s_bPointsChanged)
			RebuildChain();

		return m_aChain;
	}

	//------------------------------------------------------------------------------------------------
	protected void RebuildChain()
	{
		m_bChainDirty = false;
		s_bPointsChanged = false;
		m_aChain.Clear();
		if (!s_aPoints || !GetOwner())
			return;

		BaseWorld world = GetOwner().GetWorld();
		foreach (ZW_InvasionPoint point : s_aPoints)
		{
			if (point && point.GetWorld() == world && LanesCompatible(point.GetLane(), m_sLane))
				m_aChain.Insert(point);
		}

		// Сортировка по номеру (вставками, точек мало).
		for (int i = 1; i < m_aChain.Count(); i++)
		{
			ZW_InvasionPoint current = m_aChain[i];
			int j = i - 1;
			while (j >= 0 && m_aChain[j].GetOrder() > current.GetOrder())
			{
				m_aChain[j + 1] = m_aChain[j];
				j--;
			}
			m_aChain[j + 1] = current;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnLaneChanged()
	{
		m_bChainDirty = true;
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
		return GetChain().Count();
	}

	//------------------------------------------------------------------------------------------------
	ZW_InvasionPoint GetPoint(int index)
	{
		array<ZW_InvasionPoint> chain = GetChain();
		if (index < 0 || index >= chain.Count())
			return null;

		return chain[index];
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

		Print("[ZW_Invasion] v0.5.0 старт матча", LogLevel.WARNING);

		// В Workbench статический список переживает перезапуск игры и сцены редактора,
		// поэтому оставляем только живые точки из текущего игрового мира.
		PrunePoints();

		string lastAttacker;
		string lastLane;
		ReadState(lastAttacker, lastLane);
		ChooseSides(lastAttacker);
		ChooseLane(lastLane);
		WriteState();
		Replication.BumpMe();

		if (!GetAttackerFaction() || !GetDefenderFaction())
		{
			Print("[ZW_Invasion] Не найдены фракции атаки/обороны — проверьте ключи в ZW_ObjectivesComponent", LogLevel.ERROR);
			return;
		}

		Print(string.Format("[ZW_Invasion] Стороны: атака %1, оборона %2 (в прошлом матче атаковала '%3')", m_sAttackerFactionKey, m_sDefenderFactionKey, lastAttacker), LogLevel.WARNING);
		ZW_MainRoleComponent.ApplyAll(GetAttackerFaction(), GetDefenderFaction());

		if (GetPointCount() == 0)
		{
			Print("[ZW_Invasion] На карте нет ни одной ZW_InvasionPoint", LogLevel.ERROR);
			return;
		}

		ValidateChain();
		Print(string.Format("[ZW_Invasion] Полоса '%1', точек в цепочке: %2", m_sLane, GetPointCount()), LogLevel.WARNING);
		foreach (ZW_InvasionPoint p : GetChain())
		{
			Print(string.Format("[ZW_Invasion]   точка #%1 '%2' %3", p.GetOrder(), p.GetPointName(), p.GetOrigin()), LogLevel.WARNING);
		}

		// Со старта все точки у обороны и закрыты для захвата.
		Faction defender = GetDefenderFaction();
		foreach (ZW_InvasionPoint point : GetChain())
		{
			point.ZW_InitAuthority(defender);
		}

		ZW_PointHubComponent.ApplyAll(this);

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

		int index = GetChain().Find(point);
		if (index != m_iActiveIndex)
			return; // захватить можно только текущую цель

		Print(string.Format("[ZW_Invasion] Точка %1 (%2) взята атакой", point.GetOrder(), point.GetPointName()), LogLevel.NORMAL);

		// Последняя точка: оборона теряет все тикеты, победа атаки.
		if (index >= GetPointCount() - 1)
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
		m_iDebugCounter = 0; // отладочный автозахват отсчитывает заново для каждой новой цели
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
	//! Проверка расстановки: те же правила, что подсвечивает редактор.
	protected void ValidateChain()
	{
		foreach (ZW_InvasionPoint point : GetChain())
		{
			string error = ValidatePoint(point);
			if (!error.IsEmpty())
				Print(string.Format("[ZW_Invasion] ОШИБКА РАССТАНОВКИ: точка #%1 '%2' — %3", point.GetOrder(), point.GetPointName(), error), LogLevel.ERROR);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер: стороны матча. При смене сторон — наоборот относительно прошлого матча.
	protected void ChooseSides(string lastAttacker)
	{
		m_sAttackerFactionKey = m_sStartAttackerKey;
		m_sDefenderFactionKey = m_sStartDefenderKey;

		if (m_bSwapSides && lastAttacker == m_sStartAttackerKey)
		{
			m_sAttackerFactionKey = m_sStartDefenderKey;
			m_sDefenderFactionKey = m_sStartAttackerKey;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер: случайная полоса, по возможности не та, что была в прошлом матче.
	protected void ChooseLane(string lastLane)
	{
		array<string> lanes = {};
		BaseWorld world = GetOwner().GetWorld();
		foreach (ZW_InvasionPoint point : s_aPoints)
		{
			if (!point || point.GetWorld() != world)
				continue;

			string lane = point.GetLane();
			if (!lane.IsEmpty() && !lanes.Contains(lane))
				lanes.Insert(lane);
		}

		m_sLane = string.Empty;
		if (!lanes.IsEmpty())
		{
			if (lanes.Count() > 1)
				lanes.RemoveItem(lastLane);

			Math.Randomize(-1);
			m_sLane = lanes[Math.RandomInt(0, lanes.Count())];
		}

		m_bChainDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void ReadState(out string attacker, out string lane)
	{
		if (!FileIO.FileExists(STATE_FILE))
			return;

		FileHandle file = FileIO.OpenFile(STATE_FILE, FileMode.READ);
		if (!file)
			return;

		string line;
		while (file.ReadLine(line) >= 0)
		{
			if (line.IndexOf("attacker=") == 0)
				attacker = line.Substring(9, line.Length() - 9);
			else if (line.IndexOf("lane=") == 0)
				lane = line.Substring(5, line.Length() - 5);
		}

		file.Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void WriteState()
	{
		FileHandle file = FileIO.OpenFile(STATE_FILE, FileMode.WRITE);
		if (!file)
		{
			Print("[ZW_Invasion] Не удалось записать " + STATE_FILE + " — смена сторон не сохранится", LogLevel.WARNING);
			return;
		}

		file.WriteLine("attacker=" + m_sAttackerFactionKey);
		file.WriteLine("lane=" + m_sLane);
		file.Close();
	}

	//------------------------------------------------------------------------------------------------
	//! На клиентах: сменилась текущая цель (понадобится HUD и туману войны на этапе 3).
	protected void OnActiveIndexChanged()
	{
	}
}
