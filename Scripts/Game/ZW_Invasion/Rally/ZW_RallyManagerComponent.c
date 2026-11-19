// ZRBinvaga — rally отрядов (компонент гейм-мода).
// Rally = штатная разворачиваемая рация. Здесь: учёт спавнов по отрядам,
// «зарядка» rally и общие проверки (командир, второй боец рядом).

[ComponentEditorProps(category: "ZRB/Invasion", description: "Invasion: rally отрядов")]
class ZW_RallyManagerComponentClass : SCR_BaseGameModeComponentClass
{
}

class ZW_RallyManagerComponent : SCR_BaseGameModeComponent
{
	[Attribute("9", UIWidgets.EditBox, "Спавнов на один rally", category: "Rally")]
	protected int m_iSpawnsPerRally;

	[Attribute("50", UIWidgets.EditBox, "Враг ближе этого радиуса блокирует спавн на rally, м", category: "Rally")]
	protected float m_fEnemyBlockRadius;

	[Attribute("15", UIWidgets.EditBox, "Второй боец отряда должен быть не дальше, м", category: "Rally")]
	protected float m_fBuddyRadius;

	[Attribute("1", UIWidgets.CheckBox, "Запрет ставить rally рядом со спавн-точками (мейн, хабы)", category: "Rally")]
	protected bool m_bBlockNearSpawnPoints;

	[Attribute("100", UIWidgets.EditBox, "Rally не ближе этого к любой спавн-точке, м", category: "Rally")]
	protected float m_fSpawnPointExclusion;

	// Временные поля для поиска врагов.
	protected Faction m_QueryFaction;
	protected bool m_bQueryFoundEnemy;

	[Attribute("1", UIWidgets.CheckBox, "Игрока без отряда при спавне сажать в свободный отряд своей фракции", category: "Rally")]
	protected bool m_bAutoJoinGroup;

	[Attribute("0", UIWidgets.CheckBox, "Отладка: не требовать второго бойца рядом (тест в одиночку)", category: "Invasion Debug")]
	protected bool m_bDebugSkipBuddy;

	[Attribute("0", UIWidgets.CheckBox, "Отладка: через 3 с после спавна развернуть ближайшую рацию (до 25 м)", category: "Invasion Debug")]
	protected bool m_bDebugAutoDeploy;

	protected IEntity m_DebugUser;
	protected SCR_RestrictedDeployableSpawnPointComponent m_DebugRadio;

	[Attribute("0", UIWidgets.CheckBox, "Отладка: убить игрока через 3 с после развёртывания rally (тест спавна)", category: "Invasion Debug")]
	protected bool m_bDebugKillAfterDeploy;

	[Attribute("0", UIWidgets.CheckBox, "Отладка: убить игрока через 5 с после спавна на rally, пока лимит не исчерпан (тест лимита)", category: "Invasion Debug")]
	protected bool m_bDebugKillAfterRallySpawn;

	//! Отряды, израсходовавшие rally. Пока отряд здесь — новый rally не развернуть.
	[RplProp()]
	protected ref array<int> m_aDepletedGroups = {};

	//! Сервер: спавнов на rally по ID отряда.
	protected ref map<int, int> m_mSpawnsUsed = new map<int, int>();

	//------------------------------------------------------------------------------------------------
	static ZW_RallyManagerComponent GetInstance()
	{
		BaseGameMode gameMode = GetGame().GetGameMode();
		if (!gameMode)
			return null;

		return ZW_RallyManagerComponent.Cast(gameMode.FindComponent(ZW_RallyManagerComponent));
	}

	//------------------------------------------------------------------------------------------------
	float GetEnemyBlockRadius()
	{
		return m_fEnemyBlockRadius;
	}

	//------------------------------------------------------------------------------------------------
	bool BlockNearSpawnPoints()
	{
		return m_bBlockNearSpawnPoints;
	}

	//------------------------------------------------------------------------------------------------
	int GetSpawnsPerRally()
	{
		return m_iSpawnsPerRally;
	}

	//------------------------------------------------------------------------------------------------
	bool IsGroupCharged(int groupId)
	{
		return !m_aDepletedGroups.Contains(groupId);
	}

	//------------------------------------------------------------------------------------------------
	static int GetPlayerId(IEntity entity)
	{
		if (!entity)
			return 0;

		return GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
	}

	//------------------------------------------------------------------------------------------------
	static SCR_AIGroup GetPlayerGroup(int playerId)
	{
		SCR_GroupsManagerComponent groups = SCR_GroupsManagerComponent.GetInstance();
		if (!groups || playerId <= 0)
			return null;

		return groups.GetPlayerGroup(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Командир отряда? Возвращает отряд через out.
	static bool IsSquadLeader(IEntity user, out SCR_AIGroup group)
	{
		int playerId = GetPlayerId(user);
		group = GetPlayerGroup(playerId);
		return group && group.IsPlayerLeader(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Есть ли рядом с user живой боец того же отряда.
	bool HasBuddyNearby(notnull IEntity user, notnull SCR_AIGroup group)
	{
		int userId = GetPlayerId(user);
		array<int> members = group.GetPlayerIDs();
		foreach (int memberId : members)
		{
			if (memberId == userId)
				continue;

			IEntity member = GetGame().GetPlayerManager().GetPlayerControlledEntity(memberId);
			if (!member)
				continue;

			ChimeraCharacter character = ChimeraCharacter.Cast(member);
			if (character)
			{
				CharacterControllerComponent controller = character.GetCharacterController();
				if (controller && controller.IsDead())
					continue;
			}

			if (vector.Distance(member.GetOrigin(), user.GetOrigin()) <= m_fBuddyRadius)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Общая проверка развёртывания. reason — текст для игрока.
	bool CanDeployRally(IEntity user, out string reason)
	{
		SCR_AIGroup group;
		if (!IsSquadLeader(user, group))
		{
			reason = "Rally ставит только командир отряда";
			return false;
		}

		if (!IsGroupCharged(group.GetGroupID()))
		{
			reason = "Rally израсходован — пополните у хаба";
			return false;
		}

		if (!m_bDebugSkipBuddy && !HasBuddyNearby(user, group))
		{
			reason = "Нужен ещё один боец отряда рядом";
			return false;
		}

		if (IsEnemyNear(user.GetOrigin(), ZW_Invasion.GetEntityFaction(user)))
		{
			reason = "Рядом враг";
			return false;
		}

		if (m_bBlockNearSpawnPoints && IsNearSpawnPoint(user.GetOrigin()))
		{
			reason = "Слишком близко к спавну";
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Живой персонаж другой фракции в радиусе m_fEnemyBlockRadius.
	bool IsEnemyNear(vector origin, Faction ownFaction)
	{
		if (!ownFaction)
			return false;

		m_QueryFaction = ownFaction;
		m_bQueryFoundEnemy = false;
		GetGame().GetWorld().QueryEntitiesBySphere(origin, m_fEnemyBlockRadius, EnemyQueryCallback, null, EQueryEntitiesFlags.DYNAMIC);
		m_QueryFaction = null;
		return m_bQueryFoundEnemy;
	}

	//------------------------------------------------------------------------------------------------
	protected bool EnemyQueryCallback(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return true;

		CharacterControllerComponent controller = character.GetCharacterController();
		if (controller && controller.IsDead())
			return true;

		Faction faction = ZW_Invasion.GetEntityFaction(character);
		if (!faction || faction == m_QueryFaction)
			return true;

		m_bQueryFoundEnemy = true;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Есть ли обычная (не rally) спавн-точка ближе m_fSpawnPointExclusion.
	bool IsNearSpawnPoint(vector origin)
	{
		array<SCR_SpawnPoint> points = SCR_SpawnPoint.GetSpawnPoints();
		foreach (SCR_SpawnPoint point : points)
		{
			if (SCR_DeployableSpawnPoint.Cast(point))
				continue;

			if (vector.Distance(point.GetOrigin(), origin) < m_fSpawnPointExclusion)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Вызывается рацией после успешного развёртывания.
	void OnRallyDeployed(IEntity user)
	{
		if (m_bDebugKillAfterDeploy && user)
			GetGame().GetCallqueue().CallLater(DebugKill, 3000, false, user);
	}

	//------------------------------------------------------------------------------------------------
	protected void DebugKill(IEntity user)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (!character)
			return;

		CharacterControllerComponent controller = character.GetCharacterController();
		if (controller)
		{
			Print("[ZW_Invasion][DBG] Убиваю игрока для теста спавна на rally", LogLevel.WARNING);
			int playerId = GetPlayerId(user);
			controller.ForceDeath();
			GetGame().GetCallqueue().CallLater(DebugReportRallyPoints, 2000, false, playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void DebugReportRallyPoints(int playerId)
	{
		SCR_AIGroup group = GetPlayerGroup(playerId);
		int groupId = -1;
		if (group)
			groupId = group.GetGroupID();

		array<SCR_SpawnPoint> points = SCR_SpawnPoint.GetSpawnPoints();
		foreach (SCR_SpawnPoint point : points)
		{
			SCR_RestrictedDeployableSpawnPoint rally = SCR_RestrictedDeployableSpawnPoint.Cast(point);
			if (!rally)
				continue;

			SCR_ESpawnResult result;
			bool canReserve = rally.CanReserveFor_S(playerId, result);
			Print(string.Format("[ZW_Invasion][DBG] Rally-точка: включена=%1, видна игроку=%2, фракция='%3', отряд рации=%4, отряд игрока=%5, резерв=%6 (%7), остаток=%8/%9",
				rally.IsSpawnPointEnabled(), rally.IsSpawnPointVisibleForPlayer(playerId), rally.GetFactionKey(), SCR_RestrictedDeployableSpawnPointComponent.Cast(rally.GetDeployableSpawnPointComponent()).GetGroupID(), groupId,
				canReserve, typename.EnumToString(SCR_ESpawnResult, result), rally.GetRespawnCount(), rally.GetMaxRespawns()), LogLevel.WARNING);
		}

		for (int i = 0; i < 6; i++)
		{
			Print(string.Format("[ZW_Invasion][DBG] SCR_ESpawnPointBudgetType %1 = %2", i, typename.EnumToString(SCR_ESpawnPointBudgetType, i)), LogLevel.WARNING);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер: «пополнить rally у хаба».
	bool Recharge(int groupId)
	{
		if (!Replication.IsServer())
			return false;

		m_mSpawnsUsed.Remove(groupId);
		if (m_aDepletedGroups.RemoveItem(groupId) < 0)
			return false;

		Replication.BumpMe();
		Print(string.Format("[ZW_Invasion] Rally отряда %1 пополнен", groupId), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void OnGameModeStart()
	{
		super.OnGameModeStart();

		if (!Replication.IsServer())
			return;

		// Штатные разворачиваемые рации по умолчанию выключены вне Conflict — включаем.
		SCR_PlayerSpawnPointManagerComponent spawnPoints = SCR_PlayerSpawnPointManagerComponent.Cast(GetGameMode().FindComponent(SCR_PlayerSpawnPointManagerComponent));
		if (!spawnPoints)
		{
			Print("[ZW_Invasion] Нет SCR_PlayerSpawnPointManagerComponent — rally-рации не заработают", LogLevel.ERROR);
			return;
		}

		spawnPoints.EnableDeployableSpawnPoints(true);
		// Лимит rally считает этот менеджер; снабжение за спавн на rally не списывается.
		spawnPoints.SetDeployableSpawnPointBudgetType(SCR_ESpawnPointBudgetType.SPAWNTICKET);
		spawnPoints.SetDeployableSpawnPointTicketAmount(1000);
		Print("[ZW_Invasion] Разворачиваемые rally-рации включены", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер: игрок без отряда попадает в первый неполный отряд своей фракции (или в новый).
	protected void EnsurePlayerGroup(int playerId, IEntity entity)
	{
		SCR_GroupsManagerComponent groups = SCR_GroupsManagerComponent.GetInstance();
		if (!groups || playerId <= 0 || groups.GetPlayerGroup(playerId))
			return;

		Faction faction = ZW_Invasion.GetEntityFaction(entity);
		if (!faction)
			return;

		SCR_AIGroup group = groups.GetFirstNotFullForFaction(faction);
		if (!group)
			group = groups.CreateNewPlayableGroup(faction);
		if (!group)
		{
			Print("[ZW_Invasion] Не удалось создать отряд для игрока " + playerId, LogLevel.WARNING);
			return;
		}

		// Штатный путь: компонент группы у контроллера игрока тоже узнаёт об отряде.
		SCR_PlayerControllerGroupComponent groupComponent = SCR_PlayerControllerGroupComponent.GetPlayerControllerComponent(playerId);
		if (groupComponent)
			groupComponent.RequestJoinGroup(group.GetGroupID());
		else
			groups.AddPlayerToGroup(group.GetGroupID(), playerId);

		if (group.GetPlayerCount() <= 1)
			groups.SetGroupLeader(group.GetGroupID(), playerId);

		Print(string.Format("[ZW_Invasion] Игрок %1 -> отряд %2, командир: %3", playerId, group.GetGroupID(), group.IsPlayerLeader(playerId)), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void DebugDeployNearestRadio()
	{
		if (!m_DebugUser)
			return;

		m_DebugRadio = null;
		GetGame().GetWorld().QueryEntitiesBySphere(m_DebugUser.GetOrigin(), 25, DebugFindRadio, null, EQueryEntitiesFlags.ALL);
		if (!m_DebugRadio)
		{
			Print("[ZW_Invasion][DBG] Рация с rally-компонентом в 25 м не найдена", LogLevel.WARNING);
			return;
		}

		string reason;
		bool can = CanDeployRally(m_DebugUser, reason);
		Print(string.Format("[ZW_Invasion][DBG] Рация %1: можно=%2 %3", m_DebugRadio.GetOwner().GetPrefabData().GetPrefabName(), can, reason), LogLevel.WARNING);

		string vanillaReason;
		bool vanillaCan = m_DebugRadio.CanDeployBePerformed(vanillaReason);
		Print(string.Format("[ZW_Invasion][DBG] CanDeployBePerformed=%1 причина='%2'", vanillaCan, vanillaReason), LogLevel.WARNING);

		m_DebugRadio.Deploy(m_DebugUser);
		Print(string.Format("[ZW_Invasion][DBG] После Deploy: развёрнута=%1, группа=%2", m_DebugRadio.IsDeployed(), m_DebugRadio.GetGroupID()), LogLevel.WARNING);
		GetGame().GetCallqueue().CallLater(DebugReportRadio, 2000);
	}

	//------------------------------------------------------------------------------------------------
	protected void DebugReportRadio()
	{
		if (!m_DebugRadio)
			return;

		SCR_SpawnPoint point = m_DebugRadio.GetSpawnPoint();
		string pointInfo = "нет";
		if (point)
			pointInfo = point.GetFactionKey();

		Print(string.Format("[ZW_Invasion][DBG] Через 2 с: развёрнута=%1, группа=%2, спавн-точка=%3, фракция рации=%4",
			m_DebugRadio.IsDeployed(), m_DebugRadio.GetGroupID(), pointInfo, ZW_Invasion.GetEntityFaction(m_DebugRadio.GetOwner())), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	protected bool DebugFindRadio(IEntity entity)
	{
		SCR_RestrictedDeployableSpawnPointComponent radio = SCR_RestrictedDeployableSpawnPointComponent.Cast(entity.FindComponent(SCR_RestrictedDeployableSpawnPointComponent));
		if (!radio)
			return true;

		m_DebugRadio = radio;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerSpawnFinalize_S(SCR_SpawnRequestComponent requestComponent, SCR_SpawnHandlerComponent handlerComponent, SCR_SpawnData data, IEntity entity)
	{
		super.OnPlayerSpawnFinalize_S(requestComponent, handlerComponent, data, entity);

		int spawnedId = GetPlayerId(entity);
		if (m_bAutoJoinGroup)
			EnsurePlayerGroup(spawnedId, entity);

		if (m_bDebugAutoDeploy)
		{
			m_DebugUser = entity;
			GetGame().GetCallqueue().CallLater(DebugDeployNearestRadio, 3000);
		}

		SCR_SpawnPointSpawnData pointData = SCR_SpawnPointSpawnData.Cast(data);
		if (!pointData)
			return;

		SCR_DeployableSpawnPoint spawnPoint = SCR_DeployableSpawnPoint.Cast(SCR_SpawnPoint.GetSpawnPointByRplId(pointData.GetRplId()));
		if (!spawnPoint)
			return;

		SCR_RestrictedDeployableSpawnPointComponent rally = SCR_RestrictedDeployableSpawnPointComponent.Cast(spawnPoint.GetDeployableSpawnPointComponent());
		if (!rally)
			return;

		int groupId = rally.GetGroupID();
		int used = m_mSpawnsUsed.Get(groupId) + 1;
		m_mSpawnsUsed.Set(groupId, used);
		Print(string.Format("[ZW_Invasion] Rally отряда %1: спавн %2/%3", groupId, used, m_iSpawnsPerRally), LogLevel.NORMAL);

		if (used < m_iSpawnsPerRally)
		{
			if (m_bDebugKillAfterRallySpawn)
				GetGame().GetCallqueue().CallLater(DebugKill, 5000, false, entity);

			return;
		}

		// Лимит исчерпан: rally сворачивается, отряд должен пополнить его у хаба.
		if (!m_aDepletedGroups.Contains(groupId))
		{
			m_aDepletedGroups.Insert(groupId);
			Replication.BumpMe();
		}

		rally.Dismantle();
		Print(string.Format("[ZW_Invasion] Rally отряда %1 израсходован", groupId), LogLevel.NORMAL);
	}
}
