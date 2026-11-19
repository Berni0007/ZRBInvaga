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
}
