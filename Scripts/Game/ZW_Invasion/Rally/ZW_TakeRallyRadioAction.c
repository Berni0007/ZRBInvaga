// ZRBinvaga — действие «Взять рацию rally». Вешается на ящик на мейне (ZW_RallyRadioCrate.et).

class ZW_TakeRallyRadioAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!ZW_RallyManagerComponent.GetInstance())
			return false;

		SCR_AIGroup group;
		return ZW_RallyManagerComponent.IsSquadLeader(user, group);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		ZW_RallyManagerComponent manager = ZW_RallyManagerComponent.GetInstance();
		if (!manager)
			return false;

		string reason;
		if (!manager.CanIssueRadio(user, reason))
		{
			SetCannotPerformReason(reason);
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		ZW_RallyManagerComponent manager = ZW_RallyManagerComponent.GetInstance();
		if (manager)
			manager.IssueRadio(pUserEntity, pOwnerEntity);
	}
}
