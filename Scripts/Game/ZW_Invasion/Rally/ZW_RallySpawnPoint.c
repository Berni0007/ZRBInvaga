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

	override bool CanReserveFor_S(int playerId, out SCR_ESpawnResult result = SCR_ESpawnResult.SPAWN_NOT_ALLOWED)
	{
		ZW_RallyManagerComponent manager = ZW_RallyManagerComponent.GetInstance();
		if (manager && manager.IsRallySpawnBlocked(this))
		{
			result = SCR_ESpawnResult.SPAWN_NOT_ALLOWED;
			return false;
		}
		return super.CanReserveFor_S(playerId, result);
	}
}
