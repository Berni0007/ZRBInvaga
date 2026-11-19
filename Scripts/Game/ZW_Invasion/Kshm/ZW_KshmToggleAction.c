// ZRBinvaga — действие у кузова КШМ: «Развернуть КШМ» / «Свернуть КШМ».

class ZW_KshmToggleAction : ScriptedUserAction
{
	protected ZW_KshmComponent m_Kshm;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Kshm = ZW_KshmComponent.Cast(pOwnerEntity.FindComponent(ZW_KshmComponent));
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (m_Kshm && m_Kshm.IsDeployed())
			outName = "Свернуть КШМ";
		else
			outName = "Развернуть КШМ";

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return m_Kshm != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!m_Kshm)
			return false;

		string reason;
		if (!m_Kshm.CanDeploy(user, reason))
		{
			SetCannotPerformReason(reason);
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (m_Kshm && Replication.IsServer())
			m_Kshm.Toggle(pUserEntity);
	}
}
