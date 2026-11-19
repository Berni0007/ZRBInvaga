// ZRBinvaga — имя rally-точки на экране возрождения: «Rally — отряд N».

modded class SCR_RestrictedDeployableSpawnPoint
{
	//------------------------------------------------------------------------------------------------
	override string GetSpawnPointName()
	{
		if (!ZW_RallyManagerComponent.GetInstance() || m_iGroupID < 0)
			return super.GetSpawnPointName();

		return "Rally — отряд " + m_iGroupID.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Израсходованный rally сразу выключается: рация сворачивается позже, когда штатный спавн завершится.
	override bool IsSpawnPointEnabled()
	{
		ZW_RallyManagerComponent manager = ZW_RallyManagerComponent.GetInstance();
		if (manager && m_iGroupID >= 0 && !manager.IsGroupCharged(m_iGroupID))
			return false;

		return super.IsSpawnPointEnabled();
	}
}
