// ZRBinvaga — точка захвата Invasion.
// Наследник штатной точки Capture & Hold: механику присутствия и перевеса берём готовую,
// сверху добавляем порядок в цепочке и блокировку после взятия.

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

	//! Текущая цель атаки — только её можно захватывать.
	protected bool m_bActive;

	//! Взята атакой — больше никогда не меняет владельца.
	protected bool m_bLocked;

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
		SetOwningFactionInternalAuthority(GetOwningFaction(), defender);
		SetCapturable(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер: точка стала текущей целью атаки.
	void ZW_SetActive()
	{
		m_bActive = true;
		SetCapturable(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Вкл/выкл подсчёт игроков внутри зоны. Выключенная зона не меняет владельца.
	protected void SetCapturable(bool enable)
	{
		EnablePeriodicQueries(enable);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnOwningFactionChanged(Faction previousFaction, Faction newFaction)
	{
		super.OnOwningFactionChanged(previousFaction, newFaction);

		if (!Replication.IsServer() || !m_bActive || m_bLocked)
			return;

		ZW_ObjectivesComponent objectives = ZW_Invasion.GetObjectives();
		if (!objectives || newFaction != objectives.GetAttackerFaction())
			return;

		// Взята атакой: блокируем навсегда, оборона отбить не может.
		m_bLocked = true;
		m_bActive = false;
		SetCapturable(false);

		objectives.OnPointCaptured(this);
	}
}
