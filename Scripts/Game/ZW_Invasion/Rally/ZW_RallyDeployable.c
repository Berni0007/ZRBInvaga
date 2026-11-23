// ZRBinvaga — штатная рация как rally отряда.
// Действует только если в гейм-моде есть ZW_RallyManagerComponent; в других режимах рация штатная.

modded class SCR_RestrictedDeployableSpawnPointComponent
{
	protected bool m_bZWApplied;
	protected bool m_bZWInDeploy;

	//------------------------------------------------------------------------------------------------
	//! Настройки rally для Invasion. Вызывается лениво: гейм-мод может появиться позже рации.
	protected ZW_RallyManagerComponent ZW_ApplyRallySettings()
	{
		ZW_RallyManagerComponent manager = ZW_RallyManagerComponent.GetInstance();
		if (!manager || m_bZWApplied)
			return manager;

		m_bZWApplied = true;

		// Лимит считает менеджер (9 спавнов, затем пополнение у хаба); штатный счётчик не мешает.
		m_iMaxRespawns = 1000;
		m_iRespawnGenerationAmount = 0;
		SetBudgetType(SCR_ESpawnPointBudgetType.SPAWNTICKET);

		// Спавн только для своего отряда, один rally на отряд.
		m_bAllowAllGroupsToSpawn = false;
		m_iMaxSpawnPointsPerGroup = 1;

		// В Invasion нет баз Conflict и радиопокрытия — rally ставится где угодно,
		// кроме зоны у спавн-точек (мейн) — эту проверку оставляем штатной.
		m_bQueryBases = false;
		m_bQueryAllBases = false;
		m_bQueryFactionRadioRange = false;
		m_bRestrictDeploy = false;
		m_bQuerySpawnPoints = manager.BlockNearSpawnPoints();

		// Враг рядом блокирует спавн.
		m_bQueryCharacters = true;
		m_bIgnoreEnemyCharacters = false;
		m_fQueryRadiusCharacters = manager.GetEnemyBlockRadius();

		Print(string.Format("[ZW_Invasion] Rally-рация настроена: %1", GetOwner().GetPrefabData().GetPrefabName()), LogLevel.NORMAL);
		return manager;
	}

	//------------------------------------------------------------------------------------------------
	override void Update(float timeSlice)
	{
		ZW_ApplyRallySettings();
		super.Update(timeSlice);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanDeployBeShown(notnull IEntity userEntity)
	{
		if (!ZW_ApplyRallySettings())
			return super.CanDeployBeShown(userEntity);

		if (!super.CanDeployBeShown(userEntity))
			return false;

		SCR_AIGroup group;
		return ZW_RallyManagerComponent.IsSquadLeader(userEntity, group);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanDeployBePerformed(out string reason)
	{
		ZW_RallyManagerComponent manager = ZW_ApplyRallySettings();
		if (!manager)
			return super.CanDeployBePerformed(reason);

		// В Invasion штатная проверка зоны (базы Conflict, радиопокрытие) не нужна — свои правила.
		IEntity user = SCR_PlayerController.GetLocalControlledEntity();
		if (!user)
			return true; // сервер без локального игрока — проверка будет в Deploy

		// При попытке в одиночку Deploy покажет причину командиру и отклонит развёртывание.
		if (manager.CanDeployRally(user, reason))
			return true;

		if (reason != ZW_RallyManagerComponent.BUDDY_REQUIRED_REASON)
			return false;

		reason = string.Empty;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! В Invasion позицию проверяет ZW_RallyManagerComponent.CanDeployRally (вызывается в Deploy).
	override protected bool CanBeDeployedAtPosition(vector position, notnull IEntity userEntity, out int notification = -1)
	{
		if (ZW_ApplyRallySettings())
			return true;

		return super.CanBeDeployedAtPosition(position, userEntity, notification);
	}

	//------------------------------------------------------------------------------------------------
	override void Deploy(IEntity userEntity, bool reload = false)
	{
		ZW_RallyManagerComponent manager = ZW_ApplyRallySettings();
		if (!manager || reload || !userEntity || m_bZWInDeploy)
		{
			super.Deploy(userEntity, reload);
			return;
		}

		string reason;
		if (!manager.CanDeployRally(userEntity, reason))
		{
			Print("[ZW_Invasion] Rally не развёрнут: " + reason, LogLevel.NORMAL);
			if (Replication.IsServer() && reason == ZW_RallyManagerComponent.BUDDY_REQUIRED_REASON)
			{
				PlayerManager players = GetGame().GetPlayerManager();
				int playerId = players.GetPlayerIdFromControlledEntity(userEntity);
				SCR_PlayerController controller = SCR_PlayerController.Cast(players.GetPlayerController(playerId));
				if (controller)
					controller.ZW_ShowRallyBuddyRequired();
			}
			return;
		}

		// Отряд передаём явно: штатный Deploy берёт его из контроллера игрока.
		SCR_AIGroup group;
		ZW_RallyManagerComponent.IsSquadLeader(userEntity, group);

		m_bZWInDeploy = true;
		if (group)
			DeployByGroup(group, userEntity, reload);
		else
			super.Deploy(userEntity, reload);
		m_bZWInDeploy = false;

		Print(string.Format("[ZW_Invasion] Rally после Deploy: развёрнута=%1, отряд=%2", IsDeployed(), GetGroupID()), LogLevel.NORMAL);
		if (IsDeployed())
			manager.OnRallyDeployed(userEntity);
	}
}

modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	void ZW_ShowRallyBuddyRequired()
	{
		if (this == GetGame().GetPlayerController())
			ZW_RpcShowRallyBuddyRequired();
		else
			Rpc(ZW_RpcShowRallyBuddyRequired);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void ZW_RpcShowRallyBuddyRequired()
	{
		SCR_PopUpNotification notification = SCR_PopUpNotification.GetInstance();
		if (notification)
			notification.PopupMsg("Ралли не развёрнуто", 5, ZW_RallyManagerComponent.BUDDY_REQUIRED_REASON);
	}
}
