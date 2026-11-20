// ZRBinvaga — диагностика: почему точка спавна недоступна («Spawn point not available»).
// Пишет в лог причину отказа сервера (не чаще раза в 5 с на точку).

modded class SCR_SpawnPointSpawnHandlerComponent
{
	protected static ref map<string, float> s_mZWLastLog = new map<string, float>();

	//------------------------------------------------------------------------------------------------
	override protected bool CanRequestSpawn_S(SCR_SpawnRequestComponent requestComponent, SCR_SpawnData data, out SCR_ESpawnResult result)
	{
		bool ok = super.CanRequestSpawn_S(requestComponent, data, result);
		if (ok)
			return ok;

		string name = "?";
		vector pos;
		SCR_SpawnPointSpawnData spawnData = SCR_SpawnPointSpawnData.Cast(data);
		if (spawnData && spawnData.GetSpawnPoint())
		{
			name = spawnData.GetSpawnPoint().GetSpawnPointName();
			pos = spawnData.GetSpawnPoint().GetOrigin();
		}

		float now = GetGame().GetWorld().GetWorldTime();
		float last;
		if (s_mZWLastLog.Find(name, last) && now - last < 5000)
			return ok;

		s_mZWLastLog.Set(name, now);
		Print(string.Format("[ZW_Invasion][DBG] Спавн на '%1' %2 отклонён: %3", name, pos, typename.EnumToString(SCR_ESpawnResult, result)), LogLevel.WARNING);
		return ok;
	}
}
