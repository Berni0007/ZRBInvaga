// Rank budgets are disabled for Free Roam Building in Invasion.
class ZW_RankRestrictions
{
	static bool IsDisabledBudget(EEditableEntityBudget budget)
	{
		return budget >= EEditableEntityBudget.RANK_RENEGADE
			&& budget <= EEditableEntityBudget.RANK_GENERAL
			&& ZW_Invasion.GetObjectives() != null;
	}
}

modded class SCR_CampaignBuildingProviderComponent
{
	override SCR_ECharacterRank GetAccessRank()
	{
		if (ZW_Invasion.GetObjectives())
			return SCR_ECharacterRank.RENEGADE;

		return super.GetAccessRank();
	}

	override bool IsBudgetToEvaluate(EEditableEntityBudget blockingBudget)
	{
		return !ZW_RankRestrictions.IsDisabledBudget(blockingBudget) && super.IsBudgetToEvaluate(blockingBudget);
	}

	override SCR_CampaignBuildingBudgetToEvaluateData GetBudgetData(EEditableEntityBudget budget)
	{
		if (ZW_RankRestrictions.IsDisabledBudget(budget))
			return null;

		return super.GetBudgetData(budget);
	}
}

modded class SCR_CampaignBuildingBudgetEditorComponent
{
	override SCR_ECharacterRank GetUserRank()
	{
		if (ZW_Invasion.GetObjectives())
			return SCR_ECharacterRank.GENERAL;

		return super.GetUserRank();
	}

	override bool IsBudgetCapEnabled(EEditableEntityBudget blockingBudget)
	{
		return !ZW_RankRestrictions.IsDisabledBudget(blockingBudget) && super.IsBudgetCapEnabled(blockingBudget);
	}
}

modded class SCR_ArsenalManagerComponent
{
	override bool AreItemsRankLocked()
	{
		return !ZW_Invasion.GetObjectives() && super.AreItemsRankLocked();
	}
}

modded class SCR_CharacterRankComponent
{
	protected override SCR_ECharacterRank GetCharacterRank()
	{
		if (ZW_Invasion.GetObjectives() && GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(GetOwner()) > 0)
			return SCR_ECharacterRank.GENERAL;

		return super.GetCharacterRank();
	}
}

modded class SCR_GroupsManagerComponent
{
	override bool HasPlayerRequiredRank(SCR_GroupRolePresetConfig preset, int playerId, bool ignoreGroupRequiredRank)
	{
		return ZW_Invasion.GetObjectives() != null || super.HasPlayerRequiredRank(preset, playerId, ignoreGroupRequiredRank);
	}
}
