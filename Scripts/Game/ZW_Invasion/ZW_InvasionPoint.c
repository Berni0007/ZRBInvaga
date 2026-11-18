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

	[Attribute("60", UIWidgets.EditBox, "Время захвата, сек", category: "Invasion")]
	protected int m_iCaptureSeconds;

	[Attribute("2", UIWidgets.EditBox, "Минимум атакующих на точке для захвата", category: "Invasion")]
	protected int m_iMinAttackers;

	//! Текущая цель атаки — только её можно захватывать.
	protected bool m_bActive;

	//! Взята атакой — больше никогда не меняет владельца.
	protected bool m_bLocked;

	//! Прогресс захвата, сек (0..m_iCaptureSeconds). Реплицируется для HUD на этапе 3.
	[RplProp()]
	protected int m_iProgress;

	protected int m_iTickCounter;

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

		int attackers = GetOccupantsCount(attacker);
		int defenders = GetOccupantsCount(defender);
		int before = m_iProgress;

		if (attackers >= m_iMinAttackers && defenders == 0)
			m_iProgress++;                       // чистый перевес атаки — захват идёт
		else if (attackers == 0 && m_iProgress > 0)
			m_iProgress--;                       // атака ушла — прогресс откатывается
		// иначе точка оспаривается — прогресс замирает

		if (m_iProgress != before)
			Replication.BumpMe();

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
		m_iProgress = m_iCaptureSeconds;
		Replication.BumpMe();
		EnablePeriodicQueries(false);
		SetOwningFactionInternalAuthority(GetOwningFaction(), attacker);
	}
}
