// ZRBinvaga — rally отрядов (компонент гейм-мода).
// Rally = штатная разворачиваемая рация.
//   - Ставит командир, рядом должен быть ещё один боец отряда.
//   - N спавнов, потом рация исчезает.
//   - Враг рядом с развёрнутым rally — рация исчезает.
//   - Новую рацию командир берёт только на мейне (ящик ZW_RallyRadioCrate). Одна рация на отряд.

[ComponentEditorProps(category: "ZRB/Invasion", description: "Invasion: rally отрядов")]
class ZW_RallyManagerComponentClass : SCR_BaseGameModeComponentClass
{
}

class ZW_RallyManagerComponent : SCR_BaseGameModeComponent
{
	[Attribute("9", UIWidgets.EditBox, "Спавнов на один rally", category: "Rally")]
	protected int m_iSpawnsPerRally;

	[Attribute("50", UIWidgets.EditBox, "Враг ближе этого радиуса блокирует спавн на rally и не даёт его поставить, м", category: "Rally")]
	protected float m_fEnemyBlockRadius;

	[Attribute("50", UIWidgets.EditBox, "Враг ближе этого радиуса к развёрнутому rally — рация исчезает, м", category: "Rally")]
	protected float m_fEnemyDestroyRadius;

	[Attribute("15", UIWidgets.EditBox, "Второй боец отряда должен быть не дальше, м", category: "Rally")]
	protected float m_fBuddyRadius;

	[Attribute("1", UIWidgets.CheckBox, "Запрет ставить rally рядом со спавн-точками (мейн, хабы)", category: "Rally")]
	protected bool m_bBlockNearSpawnPoints;

	[Attribute("100", UIWidgets.EditBox, "Rally не ближе этого к любой спавн-точке, м", category: "Rally")]
	protected float m_fSpawnPointExclusion;

	[Attribute("{9B6B61BB3FE3DFB0}Prefabs/Items/Equipment/Radios/Radio_ANPRC77.et", UIWidgets.ResourceNamePicker, "Рация rally для НАТО", "et", category: "Rally")]
	protected ResourceName m_sNatoRadio;

	[Attribute("{54C68E438DD34265}Prefabs/Items/Equipment/Radios/Radio_R107M.et", UIWidgets.ResourceNamePicker, "Рация rally для РФ", "et", category: "Rally")]
	protected ResourceName m_sRfRadio;

	[Attribute("USSR,RF,RU,RUS", UIWidgets.EditBox, "Ключи фракций РФ через запятую (остальные получают рацию НАТО)", category: "Rally")]
	protected string m_sRfFactionKeys;

	[Attribute("1", UIWidgets.CheckBox, "Игрока без отряда при спавне сажать в свободный отряд своей фракции", category: "Rally")]
	protected bool m_bAutoJoinGroup;

	[Attribute("0", UIWidgets.CheckBox, "Отладка: не требовать второго бойца рядом (тест в одиночку)", category: "Invasion Debug")]
	protected bool m_bDebugSkipBuddy;

	[Attribute("0", UIWidgets.CheckBox, "Отладка: убить игрока через 3 с после развёртывания rally (тест спавна)", category: "Invasion Debug")]
	protected bool m_bDebugKillAfterDeploy;

	[Attribute("0", UIWidgets.CheckBox, "Отладка: убить игрока через 5 с после спавна на rally, пока лимит не исчерпан (тест лимита)", category: "Invasion Debug")]
	protected bool m_bDebugKillAfterRallySpawn;

	protected static const int CHECK_MS = 2000;

	//! Отряды, чей rally израсходован и вот-вот исчезнет (точка спавна сразу выключается).
	[RplProp()]
	protected ref array<int> m_aDepletedGroups = {};

	//! Отряды, у которых есть рация rally (выданная на мейне). Для отказа «уже есть рация» на клиенте.
	[RplProp()]
	protected ref array<int> m_aGroupsWithRadio = {};

	//! Сервер: выданные рации по ID отряда.
	protected ref map<int, IEntity> m_mGroupRadios = new map<int, IEntity>();

	//! Сервер: спавнов на rally по ID отряда (сбрасывается при выдаче новой рации).
	protected ref map<int, int> m_mSpawnsUsed = new map<int, int>();

	//! Сервер: рации, которые уже исчезают (чтобы не удалять дважды).
	protected ref array<IEntity> m_PendingRemoval = {};

	// Временные поля для поиска врагов.
	protected Faction m_QueryFaction;
	protected bool m_bQueryFoundEnemy;

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
	//! false — rally отряда израсходован и вот-вот исчезнет.
	bool IsGroupCharged(int groupId)
	{
		return !m_aDepletedGroups.Contains(groupId);
	}

	//------------------------------------------------------------------------------------------------
	bool GroupHasRadio(int groupId)
	{
		return m_aGroupsWithRadio.Contains(groupId);
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
	static bool IsDead(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return false;

		CharacterControllerComponent controller = character.GetCharacterController();
		return controller && controller.IsDead();
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
			if (!member || IsDead(member))
				continue;

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
			reason = "Rally израсходован — возьмите новую рацию на мейне";
			return false;
		}

		if (!m_bDebugSkipBuddy && !HasBuddyNearby(user, group))
		{
			reason = "Нужен ещё один боец отряда рядом";
			return false;
		}

		if (IsEnemyNear(user.GetOrigin(), ZW_Invasion.GetEntityFaction(user), m_fEnemyBlockRadius))
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
	//! Живой персонаж другой фракции в радиусе radius.
	bool IsEnemyNear(vector origin, Faction ownFaction, float radius)
	{
		if (!ownFaction)
			return false;

		m_QueryFaction = ownFaction;
		m_bQueryFoundEnemy = false;
		GetGame().GetWorld().QueryEntitiesBySphere(origin, radius, EnemyQueryCallback, null, EQueryEntitiesFlags.DYNAMIC);
		m_QueryFaction = null;
		return m_bQueryFoundEnemy;
	}

	//------------------------------------------------------------------------------------------------
	protected bool EnemyQueryCallback(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character || IsDead(character))
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
	protected bool IsRfFaction(string factionKey)
	{
		array<string> keys = {};
		m_sRfFactionKeys.Split(",", keys, true);
		foreach (string key : keys)
		{
			key.TrimInPlace();
			if (key == factionKey)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected ResourceName GetRadioPrefab(Faction faction)
	{
		if (faction && IsRfFaction(faction.GetFactionKey()))
			return m_sRfRadio;

		return m_sNatoRadio;
	}

	// ---------------------------------------------------------------------------------------------
	// Выдача рации на мейне
	// ---------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Можно ли командиру взять новую рацию. Работает и на клиенте (по реплицированным данным).
	bool CanIssueRadio(IEntity user, out string reason)
	{
		SCR_AIGroup group;
		if (!IsSquadLeader(user, group))
		{
			reason = "Рацию берёт только командир отряда";
			return false;
		}

		if (GroupHasRadio(group.GetGroupID()))
		{
			reason = "У отряда уже есть рация";
			return false;
		}

		if (GetRadioPrefab(ZW_Invasion.GetEntityFaction(user)).IsEmpty())
		{
			reason = "Для этой фракции рация не настроена";
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер: выдать командиру рацию rally — она появляется на ящике.
	void IssueRadio(IEntity user, IEntity crate)
	{
		if (!Replication.IsServer() || !user || !crate)
			return;

		CleanupRadios();

		string reason;
		if (!CanIssueRadio(user, reason))
		{
			Print("[ZW_Invasion] Рация не выдана: " + reason, LogLevel.NORMAL);
			return;
		}

		SCR_AIGroup group;
		IsSquadLeader(user, group);
		int groupId = group.GetGroupID();

		ResourceName prefab = GetRadioPrefab(ZW_Invasion.GetEntityFaction(user));
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
		{
			Print("[ZW_Invasion] Не удалось загрузить рацию " + prefab, LogLevel.ERROR);
			return;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = crate.GetOrigin() + "0 1.2 0";
		IEntity radio = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		if (!radio)
		{
			Print("[ZW_Invasion] Не удалось создать рацию " + prefab, LogLevel.ERROR);
			return;
		}

		m_mGroupRadios.Set(groupId, radio);
		m_mSpawnsUsed.Remove(groupId);
		m_aDepletedGroups.RemoveItem(groupId);
		if (!m_aGroupsWithRadio.Contains(groupId))
			m_aGroupsWithRadio.Insert(groupId);
		Replication.BumpMe();

		Print(string.Format("[ZW_Invasion] Отряд %1 получил рацию rally", groupId), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер: забыть отряды, чьи рации уже исчезли.
	protected void CleanupRadios()
	{
		array<int> gone = {};
		foreach (int groupId, IEntity radio : m_mGroupRadios)
		{
			if (!radio)
				gone.Insert(groupId);
		}

		if (gone.IsEmpty())
			return;

		foreach (int goneId : gone)
		{
			m_mGroupRadios.Remove(goneId);
			m_aGroupsWithRadio.RemoveItem(goneId);
			m_aDepletedGroups.RemoveItem(goneId);
			m_mSpawnsUsed.Remove(goneId);
		}

		Replication.BumpMe();
	}

	// ---------------------------------------------------------------------------------------------
	// Исчезновение rally
	// ---------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Сервер, раз в CHECK_MS: враг рядом с развёрнутым rally — рация исчезает.
	protected void CheckRallies()
	{
		CleanupRadios();

		array<SCR_RestrictedDeployableSpawnPoint> rallies = {};
		array<SCR_SpawnPoint> points = SCR_SpawnPoint.GetSpawnPoints();
		foreach (SCR_SpawnPoint point : points)
		{
			SCR_RestrictedDeployableSpawnPoint rallyPoint = SCR_RestrictedDeployableSpawnPoint.Cast(point);
			if (rallyPoint)
				rallies.Insert(rallyPoint);
		}

		FactionManager factionManager = GetGame().GetFactionManager();
		foreach (SCR_RestrictedDeployableSpawnPoint rally : rallies)
		{
			SCR_RestrictedDeployableSpawnPointComponent radio = SCR_RestrictedDeployableSpawnPointComponent.Cast(rally.GetDeployableSpawnPointComponent());
			if (!radio || m_PendingRemoval.Contains(radio.GetOwner()))
				continue;

			Faction faction;
			if (factionManager)
				faction = factionManager.GetFactionByKey(rally.GetFactionKey());

			if (IsEnemyNear(rally.GetOrigin(), faction, m_fEnemyDestroyRadius))
			{
				Print(string.Format("[ZW_Invasion] Rally отряда %1: враг рядом — рация исчезает", radio.GetGroupID()), LogLevel.NORMAL);
				RemoveRally(radio);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер: свернуть rally и удалить рацию.
	protected void RemoveRally(SCR_RestrictedDeployableSpawnPointComponent radio)
	{
		if (!radio)
			return;

		IEntity owner = radio.GetOwner();
		if (!owner || m_PendingRemoval.Contains(owner))
			return;

		m_PendingRemoval.Insert(owner);
		if (radio.IsDeployed())
			radio.Dismantle();

		GetGame().GetCallqueue().CallLater(DeleteRadio, 1000, false, owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void DeleteRadio(IEntity radio)
	{
		if (!radio)
			return;

		m_PendingRemoval.RemoveItem(radio);
		SCR_EntityHelper.DeleteEntityAndChildren(radio);
		GetGame().GetCallqueue().CallLater(CleanupRadios, 500);
	}

	// ---------------------------------------------------------------------------------------------
	// Жизненный цикл
	// ---------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	override void OnGameModeStart()
	{
		super.OnGameModeStart();

		if (!Replication.IsServer())
			return;

		GetGame().GetCallqueue().CallLater(CheckRallies, CHECK_MS, true);

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

		if (m_sRfRadio.IsEmpty())
			Print("[ZW_Invasion] Рация rally для РФ не задана (ZW_RallyManagerComponent → Rally)", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(CheckRallies);
		super.OnDelete(owner);
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
			Print("[ZW_Invasion][DBG] Убиваю игрока для теста rally", LogLevel.WARNING);
			controller.ForceDeath();
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerSpawnFinalize_S(SCR_SpawnRequestComponent requestComponent, SCR_SpawnHandlerComponent handlerComponent, SCR_SpawnData data, IEntity entity)
	{
		super.OnPlayerSpawnFinalize_S(requestComponent, handlerComponent, data, entity);

		if (m_bAutoJoinGroup)
			EnsurePlayerGroup(GetPlayerId(entity), entity);

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

		// Лимит исчерпан: точка сразу выключается (IsSpawnPointEnabled), рация исчезает позже —
		// штатный обработчик завершает спавн с задержкой и ещё обращается к этой точке.
		if (!m_aDepletedGroups.Contains(groupId))
		{
			m_aDepletedGroups.Insert(groupId);
			Replication.BumpMe();
		}

		GetGame().GetCallqueue().CallLater(RemoveRally, 5000, false, rally);
		Print(string.Format("[ZW_Invasion] Rally отряда %1 израсходован — рация исчезнет", groupId), LogLevel.NORMAL);
	}
}
