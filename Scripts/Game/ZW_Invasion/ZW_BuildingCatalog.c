// Настраивает список размещения техники в режиме строительства Invasion.
modded class SCR_EntityCatalogManagerComponent
{
	//------------------------------------------------------------------------------------------------
	override int GetFilteredEditorPrefabs(EEntityCatalogType catalogType, EEditorMode editorMode, SCR_Faction faction, notnull out array<ResourceName> filteredPrefabsList, array<EEditableEntityLabel> includedLabels = null, array<EEditableEntityLabel> excludedLabels = null, bool needsAllIncludedLabels = true)
	{
		int count = super.GetFilteredEditorPrefabs(catalogType, editorMode, faction, filteredPrefabsList, includedLabels, excludedLabels, needsAllIncludedLabels);
		if (editorMode != EEditorMode.BUILDING || !ZW_Invasion.GetObjectives())
			return count;

		for (int i = count - 1; i >= 0; i--)
		{
			string prefab = filteredPrefabsList[i];
			if (prefab.StartsWith("{F649585ABB3706C4}") || prefab.StartsWith("{47D94E1193A88497}")
				|| prefab.StartsWith("{F6B23D17D5067C11}") || prefab.Contains("ZU23"))
				filteredPrefabsList.RemoveOrdered(i);
			else if (catalogType == EEntityCatalogType.VEHICLE && (ZW_IsCIET72(prefab) || prefab.Contains("Prefabs/Vehicles/Helicopters/KA52/")))
				filteredPrefabsList.RemoveOrdered(i);
		}

		return filteredPrefabsList.Count();
	}

	//------------------------------------------------------------------------------------------------
	protected bool ZW_IsCIET72(string prefab)
	{
		return prefab.StartsWith("{E87F0D0B1C88BFD7}") || prefab.StartsWith("{E19FA358977E1C38}")
			|| prefab.StartsWith("{6C307FEDA7B678E1}") || prefab.StartsWith("{B9A8400A3CEC2DFA}");
	}
}
