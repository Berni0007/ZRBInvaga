modded class SCR_PlacingEditorComponentClass
{
	//------------------------------------------------------------------------------------------------
	override int GetPrefabs(out notnull array<ResourceName> outPrefabs, bool onlyExposed = false)
	{
		int count = super.GetPrefabs(outPrefabs, onlyExposed);
		if (!onlyExposed || !ZW_Invasion.GetObjectives() || !SCR_CampaignBuildingPlacingEditorComponentClass.Cast(this))
			return count;

		foreach (int index, ResourceName prefab : outPrefabs)
		{
			if (!prefab.IsEmpty() && SDS_BenchTruckOnly.Get().Covers(prefab))
				outPrefabs[index] = ResourceName.Empty;
		}

		return count;
	}
}

modded class SCR_CampaignBuildingPlacingEditorComponent
{
	//------------------------------------------------------------------------------------------------
	override protected bool CanPlaceEntityServer(IEntityComponentSource editableEntitySource, out EEditableEntityBudget blockingBudget, bool updatePreview, bool showNotification, int prefabID = -1, int playerID = -1, SCR_EditorPreviewParams params = null)
	{
		if (prefabID >= 0 && ZW_Invasion.GetObjectives())
		{
			SCR_PlacingEditorComponentClass data = SCR_PlacingEditorComponentClass.Cast(GetEditorComponentData());
			if (data && SDS_BenchTruckOnly.Get().Covers(data.GetPrefab(prefabID)))
			{
				blockingBudget = -1;
				return false;
			}
		}

		return super.CanPlaceEntityServer(editableEntitySource, blockingBudget, updatePreview, showNotification, prefabID, playerID, params);
	}
}
