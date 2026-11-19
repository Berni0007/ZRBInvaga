// ZRBinvaga — точка спавна развёрнутой КШМ. Создаётся и удаляется ZW_KshmComponent.

class ZW_KshmSpawnPointClass : SCR_SpawnPointClass
{
}

class ZW_KshmSpawnPoint : SCR_SpawnPoint
{
	protected ZW_KshmComponent m_Kshm;

	//------------------------------------------------------------------------------------------------
	void ZW_SetKshm(ZW_KshmComponent kshm)
	{
		m_Kshm = kshm;
	}

	//------------------------------------------------------------------------------------------------
	//! Префаб точки спавна несёт красную метку редактора; у созданной в игре её надо скрыть.
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		ZW_HideEditorMarker();
	}

	//------------------------------------------------------------------------------------------------
	//! Красный куб — модель точки спавна для редактора. Убираем саму модель (скрытия флагом мало).
	void ZW_HideEditorMarker()
	{
		SetObject(null, string.Empty);
		ClearFlags(EntityFlags.VISIBLE, true);
	}

	//------------------------------------------------------------------------------------------------
	override string GetSpawnPointName()
	{
		return "КШМ";
	}

	//------------------------------------------------------------------------------------------------
	//! Враг рядом — спавн выключен (на сервере это решает ZW_KshmComponent).
	override bool IsSpawnPointEnabled()
	{
		if (m_Kshm && m_Kshm.IsEnemyNear())
			return false;

		return super.IsSpawnPointEnabled();
	}
}
