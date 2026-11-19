// ZRBinvaga — штатная карта возрождения рисует иконку точки спавна заново при каждой смене
// её фракции, не убирая старую (SCR_MapUIElementContainer.ShowSpawnPoint без проверки на дубли).
// Отсюда двойные иконки у хабов и мейна. Перед показом убираем уже нарисованную иконку той же точки.

modded class SCR_MapUIElementContainer
{
	//------------------------------------------------------------------------------------------------
	override protected void ShowSpawnPoint(notnull SCR_SpawnPoint spawnPoint)
	{
		HideSpawnPoint(spawnPoint.GetRplId());
		super.ShowSpawnPoint(spawnPoint);
	}
}
