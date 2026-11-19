// ZRBinvaga — точка спавна хаба на точке Invasion. Создаётся и управляется ZW_PointHubComponent.

class ZW_HubSpawnPointClass : SCR_SpawnPointClass
{
}

class ZW_HubSpawnPoint : SCR_SpawnPoint
{
	//! Спавн разрешён (решает сервер: роль владельца, текущая цель, враг рядом).
	[RplProp()]
	protected bool m_bZWAllowed;

	[RplProp()]
	protected string m_sZWName;

	//------------------------------------------------------------------------------------------------
	//! Сервер.
	void ZW_SetState(bool allowed, string name)
	{
		if (allowed == m_bZWAllowed && name == m_sZWName)
			return;

		m_bZWAllowed = allowed;
		m_sZWName = name;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Префаб точки спавна несёт красную метку редактора; у созданной в игре её надо скрыть.
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		ClearFlags(EntityFlags.VISIBLE, true);
	}

	//------------------------------------------------------------------------------------------------
	override string GetSpawnPointName()
	{
		if (m_sZWName.IsEmpty())
			return "Хаб";

		return "Хаб: " + m_sZWName;
	}

	//------------------------------------------------------------------------------------------------
	override bool IsSpawnPointEnabled()
	{
		if (!m_bZWAllowed)
			return false;

		return super.IsSpawnPointEnabled();
	}
}
