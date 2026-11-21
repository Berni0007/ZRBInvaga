// Rank budgets are disabled for Free Roam Building in Invasion.
class ZW_RankRestrictions
{
	static bool IsDisabledBudget(EEditableEntityBudget budget)
	{
		return budget >= EEditableEntityBudget.RANK_PRIVATE
			&& budget <= EEditableEntityBudget.RANK_GENERAL
			&& ZW_Invasion.GetObjectives() != null;
	}
}

modded class SCR_CampaignBuildingProviderComponent
{
	override SCR_ECharacterRank GetAccessRank()
	{
		if (ZW_Invasion.GetObjectives())
			return SCR_ECharacterRank.INVALID;

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
