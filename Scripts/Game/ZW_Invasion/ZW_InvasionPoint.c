// ZRBinvaga — точка захвата Invasion.
// Наследник штатной точки Capture & Hold: берём от неё зону и подсчёт персонажей внутри,
// а захват (прогресс, перевес, блокировку) считаем сами — как в Squad.

[EntityEditorProps(category: "ZRB/Invasion", description: "Точка захвата Invasion")]
class ZW_InvasionPointClass : SCR_CaptureAndHoldAreaClass
{
}

class ZW_InvasionPoint : SCR_CaptureAndHoldArea
{
	[Attribute("1", UIWidgets.EditBox, "Порядковый номер в цепочке (1 = первая цель атаки)", category: "Invasion")]
	protected int m_iOrder;

	[Attribute("", UIWidgets.EditBox, "Название точки для HUD и логов", category: "Invasion")]
	protected string m_sPointName;

	[Attribute("", UIWidgets.EditBox, "Полоса (например north / south). Пусто — точка общая для всех полос. На старте матча сервер выбирает одну полосу.", category: "Invasion")]
	protected string m_sLane;

	[Attribute("60", UIWidgets.EditBox, "Время захвата, сек", category: "Invasion")]
	protected int m_iCaptureSeconds;

	[Attribute("2", UIWidgets.EditBox, "Минимум атакующих на точке для захвата", category: "Invasion")]
	protected int m_iMinAttackers;

	[Attribute("40", UIWidgets.EditBox, "Радиус зоны захвата, м (подсчёт игроков, HUD и сфера в редакторе)", category: "Invasion")]
	protected float m_fCaptureRadius;

	// Временные поля для подсчёта игроков в зоне.
	protected Faction m_CountFaction;
	protected int m_iCountResult;

	//! Текущая цель атаки — только её можно захватывать.
	protected bool m_bActive;

	//! Взята атакой — больше никогда не меняет владельца.
	protected bool m_bLocked;

	//! Прогресс захвата, сек (0..m_iCaptureSeconds). Реплицируется для HUD на этапе 3.
	[RplProp()]
	protected int m_iProgress;

	protected int m_iTickCounter;

	//! Идёт ли захват (прогресс > 0). Обороне HUD показывает только это — без процентов.
	[RplProp()]
	protected bool m_bBeingCaptured;

	//------------------------------------------------------------------------------------------------
	int GetOrder()
	{
		return m_iOrder;
	}

	//------------------------------------------------------------------------------------------------
	string GetPointName()
	{
		return m_sPointName;
	}

	//------------------------------------------------------------------------------------------------
	string GetLane()
	{
		return m_sLane;
	}

	//------------------------------------------------------------------------------------------------
	bool IsActive()
	{
		return m_bActive;
	}

	//------------------------------------------------------------------------------------------------
	bool IsLocked()
	{
		return m_bLocked;
	}

	//------------------------------------------------------------------------------------------------
	int GetProgress()
	{
		return m_iProgress;
	}

	//------------------------------------------------------------------------------------------------
	bool IsBeingCaptured()
	{
		return m_bBeingCaptured;
	}

	//------------------------------------------------------------------------------------------------
	float GetCaptureRadius()
	{
		return m_fCaptureRadius;
	}

	//------------------------------------------------------------------------------------------------
	//! Живые персонажи фракции в радиусе m_fCaptureRadius.
	int ZW_CountInside(notnull Faction faction)
	{
		m_CountFaction = faction;
		m_iCountResult = 0;
		GetWorld().QueryEntitiesBySphere(GetOrigin(), m_fCaptureRadius, ZW_CountCallback, null, EQueryEntitiesFlags.DYNAMIC);
		m_CountFaction = null;
		return m_iCountResult;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ZW_CountCallback(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return true;

		CharacterControllerComponent controller = character.GetCharacterController();
		if (controller && controller.IsDead())
			return true;

		if (ZW_Invasion.GetEntityFaction(character) == m_CountFaction)
			m_iCountResult++;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	int GetCaptureSeconds()
	{
		return m_iCaptureSeconds;
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnInit(IEntity owner)
	{
		super.OnInit(owner);
		ZW_ObjectivesComponent.RegisterPoint(this);
	}

#ifdef WORKBENCH
	//------------------------------------------------------------------------------------------------
	// ИНСТРУМЕНТЫ РЕДАКТОРА: в World Editor у каждой точки видны радиус, номер, название
	// и стрелка к следующей точке цепочки. Ошибки расстановки подсвечиваются красным.
	//------------------------------------------------------------------------------------------------
	override int _WB_GetAfterWorldUpdateSpecs(IEntitySource src)
	{
		return EEntityFrameUpdateSpecs.CALL_WHEN_ENTITY_VISIBLE;
	}

	//------------------------------------------------------------------------------------------------
	override void _WB_AfterWorldUpdate(float timeSlice)
	{
		super._WB_AfterWorldUpdate(timeSlice);

		BaseWorld world = GetWorld();
		vector pos = GetOrigin();
		vector up = "0 3 0";

		string error = ZW_ObjectivesComponent.ValidatePoint(this);
		int color = ARGB(255, 255, 170, 0);   // оранжевый — всё в порядке
		if (!error.IsEmpty())
			color = ARGB(255, 255, 40, 40);   // красный — ошибка расстановки

		// Радиус зоны захвата.
		Shape.CreateSphere(color, ShapeFlags.ONCE | ShapeFlags.WIREFRAME | ShapeFlags.NOOUTLINE, pos, m_fCaptureRadius);

		// Подпись: номер, название, ошибка.
		string label = string.Format("#%1", m_iOrder);
		if (!m_sLane.IsEmpty())
			label = label + " [" + m_sLane + "]";
		if (!m_sPointName.IsEmpty())
			label = label + " " + m_sPointName;
		// Шрифт отладочного текста не знает кириллицу — подписи латиницей.
		label = label + string.Format("  [%1s, min %2]", m_iCaptureSeconds, m_iMinAttackers);
		if (!error.IsEmpty())
			label = label + "  ERROR: " + error;

		DebugTextWorldSpace.Create(world, label, DebugTextFlags.ONCE | DebugTextFlags.CENTER | DebugTextFlags.FACE_CAMERA, pos[0], pos[1] + 6, pos[2], 20.0, color);

		// Стрелка к следующей точке цепочки.
		array<ZW_InvasionPoint> nextPoints = {};
		ZW_ObjectivesComponent.FindLanePoints(world, m_iOrder + 1, m_sLane, this, nextPoints);
		foreach (ZW_InvasionPoint next : nextPoints)
		{
			Shape.CreateArrow(pos + up, next.GetOrigin() + up, 4.0, color, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER);
		}
	}
#endif

	//------------------------------------------------------------------------------------------------
	void ~ZW_InvasionPoint()
	{
		ZW_ObjectivesComponent.UnregisterPoint(this);
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер, старт матча: точка принадлежит обороне и закрыта для захвата.
	void ZW_InitAuthority(Faction defender)
	{
		m_bActive = false;
		m_bLocked = false;
		m_iProgress = 0;
		SetOwningFactionInternalAuthority(GetOwningFaction(), defender);
		EnablePeriodicQueries(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер: точка стала текущей целью атаки — начинаем считать игроков внутри.
	void ZW_SetActive()
	{
		m_bActive = true;
		m_iProgress = 0;
		m_iTickCounter = 0;
		EnablePeriodicQueries(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер, раз в секунду (вызывает ZW_ObjectivesComponent). true — точка взята.
	bool ZW_Tick(notnull Faction attacker, notnull Faction defender)
	{
		if (!m_bActive || m_bLocked)
			return false;

		int attackers = ZW_CountInside(attacker);
		int defenders = ZW_CountInside(defender);
		int before = m_iProgress;

		// Перетягивание: кого в зоне больше, тот и тянет прогресс в свою сторону.
		if (attackers > defenders && attackers >= m_iMinAttackers)
			m_iProgress++;                       // перевес атаки — захват идёт
		else if (defenders > attackers && m_iProgress > 0)
			m_iProgress--;                       // перевес обороны — прогресс отбивают
		// равенство сил, пустая точка или атакующих меньше минимума — прогресс замирает

		// Для HUD: оборона видит только факт захвата, без процентов и численности.
		bool beingCaptured = m_iProgress > 0;
		if (m_iProgress != before || beingCaptured != m_bBeingCaptured)
		{
			m_bBeingCaptured = beingCaptured;
			Replication.BumpMe();
		}

		m_iTickCounter++;
		if (m_iTickCounter % 5 == 0)
			Print(string.Format("[ZW_Invasion] Точка #%1: атака %2, оборона %3, прогресс %4/%5", m_iOrder, attackers, defenders, m_iProgress, m_iCaptureSeconds), LogLevel.NORMAL);

		if (m_iProgress < m_iCaptureSeconds)
			return false;

		ZW_LockForAttacker(attacker);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер: точка взята атакой — блокируем навсегда, оборона отбить не может.
	//! Вызывается из ZW_Tick и из отладочного авто-захвата.
	void ZW_LockForAttacker(notnull Faction attacker)
	{
		m_bLocked = true;
		m_bActive = false;
		m_bBeingCaptured = false;
		m_iProgress = m_iCaptureSeconds;
		Replication.BumpMe();
		EnablePeriodicQueries(false);
		SetOwningFactionInternalAuthority(GetOwningFaction(), attacker);
	}
}
