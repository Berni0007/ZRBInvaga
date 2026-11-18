// ZRBinvaga — действие «Пополнить rally». Вешается на хаб и на мейн.

class ZW_RefillRallyAction : ScriptedUserAction
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
		SCR_AIGroup group;
		if (!manager || !ZW_RallyManagerComponent.IsSquadLeader(user, group))
			return false;

		if (manager.IsGroupCharged(group.GetGroupID()))
		{
			SetCannotPerformReason("Rally уже готов");
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
		SCR_AIGroup group;
		if (!manager || !ZW_RallyManagerComponent.IsSquadLeader(pUserEntity, group))
			return;

		manager.Recharge(group.GetGroupID());
	}
}
