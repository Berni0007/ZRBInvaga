// ZRBinvaga — связь как в Squad.
//   - Локальный голос — штатный (прямая речь).
//   - Радио отряда: все бойцы на частоте своего отряда (штатная частота группы).
//   - Командный канал: только командиры отрядов. У командира второй канал (CH2) — частота взвода фракции.
//   - Бойцы командный канал и чужие отряды не слышат: все их каналы стоят на частоте своего отряда.
//   - Сервер и клиент раз в m_iCheckMs настраивают частоты (смена отряда/лидера,
//     подобранная рация). Командир может изменить частоты через меню рации.
//   - Дальность раций без ограничения (m_fRadioRange). Рация выдаётся при спавне, если её нет.

[ComponentEditorProps(category: "ZRB/Invasion", description: "Invasion: связь как в Squad")]
class ZW_CommsManagerComponentClass : SCR_BaseGameModeComponentClass
{
}

class ZW_CommsManagerComponent : SCR_BaseGameModeComponent
{
	[Attribute("1", UIWidgets.CheckBox, "Связь как в Squad включена", category: "Связь")]
	protected bool m_bEnabled;

	[Attribute("30000", UIWidgets.EditBox, "Дальность раций, м (как в Squad — по всей карте)", category: "Связь")]
	protected float m_fRadioRange;

	[Attribute("1000", UIWidgets.EditBox, "Как часто проверяются частоты, мс", category: "Связь")]
	protected int m_iCheckMs;

	[Attribute("1", UIWidgets.CheckBox, "Выдавать рацию при спавне, если её нет", category: "Связь")]
	protected bool m_bGiveRadio;

	[Attribute("{73950FBA2D7DB5C5}Prefabs/Items/Equipment/Radios/Radio_ANPRC68.et", UIWidgets.ResourceNamePicker, "Рация НАТО", "et", category: "Связь")]
	protected ResourceName m_sNatoRadio;

	[Attribute("{E1A5D4B878AA8980}Prefabs/Items/Equipment/Radios/Radio_R148.et", UIWidgets.ResourceNamePicker, "Рация РФ", "et", category: "Связь")]
	protected ResourceName m_sRfRadio;

	[Attribute("USSR,RF,RU,RUS,RHS_AFRF", UIWidgets.EditBox, "Ключи фракций РФ через запятую", category: "Связь")]
	protected string m_sRfFactionKeys;

	protected static ZW_CommsManagerComponent s_Instance;

	[RplProp()]
	protected int m_iAttackerCommandFrequency;

	[RplProp()]
	protected int m_iDefenderCommandFrequency;

	//! Сервер: игроки-командиры, о которых уже предупредили, что у них один канал.
	protected ref set<int> m_WarnedSingleChannel = new set<int>();

	//! Сервер: последнее состояние связи игрока для отладочного лога (печатается при изменении).
	protected ref map<int, string> m_mLastState = new map<int, string>();

	[Attribute("1", UIWidgets.CheckBox, "Писать в лог состояние связи игроков", category: "Отладка")]
	protected bool m_bDebugLog;

	//------------------------------------------------------------------------------------------------
	//! Связь Invasion активна в текущем мире (и на клиенте, и на сервере).
	static bool IsCommsActive()
	{
		if (!s_Instance || !s_Instance.m_bEnabled)
			return false;

		IEntity owner = s_Instance.GetOwner();
		return owner && owner.GetWorld() == GetGame().GetWorld();
	}

	//------------------------------------------------------------------------------------------------
	static ZW_CommsManagerComponent GetActiveInstance()
	{
		if (IsCommsActive())
			return s_Instance;
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected int GetCommandFrequency(notnull SCR_Faction faction)
	{
		ZW_ObjectivesComponent objectives = ZW_Invasion.GetObjectives();
		if (objectives)
		{
			if (faction == objectives.GetAttackerFaction() && m_iAttackerCommandFrequency > 0)
				return m_iAttackerCommandFrequency;
			if (faction == objectives.GetDefenderFaction() && m_iDefenderCommandFrequency > 0)
				return m_iDefenderCommandFrequency;
		}
		return faction.GetFactionRadioFrequency();
	}

	//------------------------------------------------------------------------------------------------
	protected void CollectRadios(notnull SCR_GadgetManagerComponent gadgets, notnull array<SCR_GadgetComponent> radios)
	{
		array<SCR_GadgetComponent> found = gadgets.GetGadgetsByType(EGadgetType.RADIO);
		if (found)
			radios.InsertAll(found);

		found = gadgets.GetGadgetsByType(EGadgetType.RADIO_BACKPACK);
		if (found)
			radios.InsertAll(found);
	}

	//------------------------------------------------------------------------------------------------
	protected BaseTransceiver FindManagedRadio(int playerId, notnull SCR_AIGroup group, notnull SCR_Faction faction, bool command)
	{
		if (playerId <= 0)
			return null;

		IEntity entity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!ChimeraCharacter.Cast(entity) || ZW_RallyManagerComponent.IsDead(entity))
			return null;

		SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(entity);
		if (!gadgets)
			return null;

		array<SCR_GadgetComponent> radios = {};
		CollectRadios(gadgets, radios);
		string factionKey = faction.GetFactionRadioEncryptionKey();
		int frequency = group.GetRadioFrequency();
		if (command)
			frequency = GetCommandFrequency(faction);

		bool squadChannelFound;
		foreach (SCR_GadgetComponent gadget : radios)
		{
			if (!gadget)
				continue;

			BaseRadioComponent radio = BaseRadioComponent.Cast(gadget.GetOwner().FindComponent(BaseRadioComponent));
			if (!radio || radio.GetEncryptionKey() != factionKey)
				continue;

			for (int i = 0; i < radio.TransceiversCount(); i++)
			{
				BaseTransceiver channel = radio.GetTransceiver(i);
				if (!channel)
					continue;

				if (command && squadChannelFound && CanTune(channel, frequency))
					return channel;

				if (!squadChannelFound && CanTune(channel, group.GetRadioFrequency()))
				{
					if (!command)
						return channel;
					squadChannelFound = true;
				}
			}
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	int GetEditableChannel(int playerId, notnull BaseTransceiver radio)
	{
		SCR_AIGroup group = GetPlayerGroup(playerId);
		if (!group || !group.IsPlayerLeader(playerId))
			return 0;

		SCR_Faction faction = SCR_Faction.Cast(group.GetFaction());
		if (!faction || faction != SCR_FactionManager.SGetPlayerFaction(playerId))
			return 0;

		if (radio == FindManagedRadio(playerId, group, faction, false))
			return 1;
		if (radio == FindManagedRadio(playerId, group, faction, true))
			return 2;
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	bool RotateFrequency(int playerId, int groupId, bool command, int direction, int expectedFrequency)
	{
		if (!Replication.IsServer() || (direction != -1 && direction != 1))
			return false;

		SCR_AIGroup group = GetPlayerGroup(playerId);
		if (!group || group.GetGroupID() != groupId || !group.IsPlayerLeader(playerId))
			return false;

		SCR_Faction faction = SCR_Faction.Cast(group.GetFaction());
		ZW_ObjectivesComponent objectives = ZW_Invasion.GetObjectives();
		if (!faction || faction != SCR_FactionManager.SGetPlayerFaction(playerId) || !objectives || objectives.IsEnded())
			return false;

		bool attacker = faction == objectives.GetAttackerFaction();
		if (!attacker && faction != objectives.GetDefenderFaction())
			return false;

		int previous = group.GetRadioFrequency();
		if (command)
			previous = GetCommandFrequency(faction);
		if (previous <= 0 || expectedFrequency != previous)
			return false;

		BaseTransceiver source = FindManagedRadio(playerId, group, faction, command);
		SCR_GroupsManagerComponent manager = SCR_GroupsManagerComponent.GetInstance();
		if (!source || !source.GetRadio().IsPowered() || !manager)
			return false;

		array<SCR_AIGroup> groups = manager.GetPlayableGroupsByFaction(faction);
		if (!groups)
			return false;

		array<BaseTransceiver> radios = {source};
		ref set<int> occupied = new set<int>();
		occupied.Insert(faction.GetFactionRadioFrequency());
		occupied.Insert(GetCommandFrequency(faction));
		bool keepPrevious;
		foreach (SCR_AIGroup other : groups)
		{
			if (!other)
				continue;

			occupied.Insert(other.GetRadioFrequency());
			if ((command || other != group) && other.GetRadioFrequency() == previous)
				keepPrevious = true;

			if (command && other != group)
			{
				BaseTransceiver radio = FindManagedRadio(other.GetLeaderID(), other, faction, true);
				if (radio)
					radios.Insert(radio);
			}
		}

		if (!command)
		{
			foreach (int memberId : group.GetPlayerIDs())
			{
				if (memberId == playerId)
					continue;

				BaseTransceiver radio = FindManagedRadio(memberId, group, faction, false);
				if (radio)
					radios.Insert(radio);
			}
		}

		int frequency = FindNextFrequency(previous, direction, source, radios, occupied);
		if (frequency <= 0)
			return false;

		manager.ClaimFrequency(frequency, faction);
		if (!keepPrevious && previous != faction.GetFactionRadioFrequency() && (command || previous != GetCommandFrequency(faction)))
			manager.ReleaseFrequency(previous, faction);

		if (!command)
			group.SetRadioFrequency(frequency);
		else
		{
			if (attacker)
				m_iAttackerCommandFrequency = frequency;
			else
				m_iDefenderCommandFrequency = frequency;
			Replication.BumpMe();
		}

		if (command)
		{
			foreach (SCR_AIGroup other : groups)
			{
				if (other && other.GetLeaderID() > 0)
					EnforcePlayer(other.GetLeaderID());
			}
		}
		else
		{
			foreach (int memberId : group.GetPlayerIDs())
				EnforcePlayer(memberId);
		}
		string kind = "отрядная";
		if (command)
			kind = "командная";
		PrintFormat("[ZW_Invasion] Связь: %1 частота %2 изменена игроком %3 с %4 на %5", kind, faction.GetFactionKey(), playerId, previous, frequency);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected int FindNextFrequency(int previous, int direction, notnull BaseTransceiver source, notnull array<BaseTransceiver> radios, notnull set<int> occupied)
	{
		int minimum = source.GetMinFrequency() + 1;
		int maximum = source.GetMaxFrequency();
		foreach (BaseTransceiver radio : radios)
		{
			minimum = Math.Max(minimum, radio.GetMinFrequency() + 1);
			maximum = Math.Min(maximum, radio.GetMaxFrequency());
		}

		int step = source.GetFrequencyResolution();
		if (step <= 0 || minimum > maximum)
			return 0;

		int first = Math.Max(minimum, previous + 1);
		if (direction < 0)
			first = Math.Min(maximum, previous - 1);

		int offset = (first - source.GetMinFrequency()) % step;
		if (offset > 0)
		{
			if (direction > 0)
				first += step - offset;
			else
				first -= offset;
		}

		for (int frequency = first; frequency >= minimum && frequency <= maximum; frequency += direction * step)
		{
			if (occupied.Contains(frequency))
				continue;

			bool compatible = true;
			foreach (BaseTransceiver radio : radios)
			{
				int resolution = radio.GetFrequencyResolution();
				if (resolution <= 0 || (frequency - radio.GetMinFrequency()) % resolution != 0)
				{
					compatible = false;
					break;
				}
			}

			if (compatible)
				return frequency;
		}
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		s_Instance = this;
		if (m_bEnabled && !System.IsConsoleApp())
			GetGame().GetCallqueue().CallLater(EnforceLocal, m_iCheckMs, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Enforce);
		GetGame().GetCallqueue().Remove(EnforceLocal);
		if (s_Instance == this)
			s_Instance = null;

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	override void OnGameModeStart()
	{
		super.OnGameModeStart();

		if (!m_bEnabled || !Replication.IsServer())
			return;

		GetGame().GetCallqueue().CallLater(Enforce, m_iCheckMs, true);
		Print("[ZW_Invasion] Связь как в Squad включена", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerSpawnFinalize_S(SCR_SpawnRequestComponent requestComponent, SCR_SpawnHandlerComponent handlerComponent, SCR_SpawnData data, IEntity entity)
	{
		super.OnPlayerSpawnFinalize_S(requestComponent, handlerComponent, data, entity);

		if (m_bDebugLog)
			Print(string.Format("[ZW_Invasion] Связь: спавн %1, выдача рации через 1 с", entity), LogLevel.NORMAL);

		if (!m_bEnabled || !entity)
			return;

		// Частоты — сразу после спавна, не дожидаясь общей проверки.
		GetGame().GetCallqueue().CallLater(EnforceEntity, 300, false, entity);

		// Снаряжение и гаджеты раскладываются не сразу.
		if (m_bGiveRadio)
			GetGame().GetCallqueue().CallLater(GiveRadio, 1000, false, entity);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsRfFaction(string factionKey)
	{
		array<string> keys = {};
		m_sRfFactionKeys.Split(",", keys, true);
		foreach (string key : keys)
		{
			key.Trim();
			if (key == factionKey)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер: выдать ручную рацию, если её нет.
	protected void GiveRadio(IEntity entity)
	{
		if (!entity || ZW_RallyManagerComponent.IsDead(entity))
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		if (playerId <= 0)
			return;

		SCR_Faction faction = GetRadioFaction(playerId, entity);
		if (!faction)
			return;

		SCR_AIGroup group = GetPlayerGroup(playerId);
		int squadFreq;
		if (group)
			squadFreq = group.GetRadioFrequency();
		SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(entity);
		if (gadgets && HasUsableHandheldRadio(gadgets, faction.GetFactionRadioEncryptionKey(), squadFreq))
			return;

		ResourceName prefab = m_sNatoRadio;
		if (IsRfFaction(faction.GetFactionKey()))
			prefab = m_sRfRadio;

		if (prefab.IsEmpty())
			return;

		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(entity.FindComponent(SCR_InventoryStorageManagerComponent));
		if (!inventory)
			return;

		if (inventory.TrySpawnPrefabToStorage(prefab))
		{
			Print(string.Format("[ZW_Invasion] Связь: выдана рация (%1)", faction.GetFactionKey()), LogLevel.NORMAL);
			GetGame().GetCallqueue().CallLater(EnforceEntity, 300, false, entity);
		}
		else
			Print("[ZW_Invasion] Связь: не удалось выдать рацию " + prefab, LogLevel.WARNING);
	}

	protected SCR_Faction GetRadioFaction(int playerId, IEntity entity)
	{
		SCR_Faction faction = SCR_Faction.Cast(SCR_FactionManager.SGetPlayerFaction(playerId));
		if (faction)
			return faction;

		return SCR_Faction.Cast(ZW_Invasion.GetEntityFaction(entity));
	}

	protected SCR_AIGroup GetPlayerGroup(int playerId)
	{
		SCR_GroupsManagerComponent groups = SCR_GroupsManagerComponent.GetInstance();
		if (!groups)
			return null;

		SCR_AIGroup group = groups.GetPlayerGroup(playerId);
		if (group)
			return group;

		// У клиента членство может появиться в контроллере раньше, чем в менеджере групп.
		SCR_PlayerControllerGroupComponent groupComponent = SCR_PlayerControllerGroupComponent.GetPlayerControllerComponent(playerId);
		if (groupComponent)
			return groups.FindGroup(groupComponent.GetGroupID());
		return null;
	}

	protected bool CanTune(notnull BaseTransceiver transceiver, int frequency)
	{
		return frequency > 0 && frequency >= transceiver.GetMinFrequency() && frequency <= transceiver.GetMaxFrequency();
	}

	protected bool HasUsableHandheldRadio(notnull SCR_GadgetManagerComponent gadgets, string factionKey, int squadFreq)
	{
		array<SCR_GadgetComponent> handheld = gadgets.GetGadgetsByType(EGadgetType.RADIO);
		if (!handheld)
			return false;

		foreach (SCR_GadgetComponent gadget : handheld)
		{
			if (!gadget)
				continue;

			BaseRadioComponent radio = BaseRadioComponent.Cast(gadget.GetOwner().FindComponent(BaseRadioComponent));
			if (!radio || radio.GetEncryptionKey() != factionKey)
				continue;

			for (int i = 0; i < radio.TransceiversCount(); i++)
			{
				BaseTransceiver transceiver = radio.GetTransceiver(i);
				if (!transceiver || (squadFreq > 0 && !CanTune(transceiver, squadFreq)))
					continue;

				if (!radio.IsPowered())
					radio.SetPower(true);
				return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void EnforceEntity(IEntity entity)
	{
		if (!entity)
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		if (playerId > 0)
			EnforcePlayer(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер, раз в m_iCheckMs.
	protected void Enforce()
	{
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int playerId : players)
		{
			EnforcePlayer(playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void EnforceLocal()
	{
		if (Replication.IsServer() || !IsCommsActive())
			return;

		PlayerController controller = GetGame().GetPlayerController();
		if (controller && controller.GetPlayerId() > 0)
			EnforcePlayer(controller.GetPlayerId(), controller.GetControlledEntity());
	}

	protected void EnforcePlayer(int playerId, IEntity entity = null)
	{
		if (!entity)
			entity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!ChimeraCharacter.Cast(entity) || ZW_RallyManagerComponent.IsDead(entity))
			return;

		SCR_Faction faction = GetRadioFaction(playerId, entity);
		if (!faction)
			return;

		SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(entity);
		if (!gadgets)
			return;

		SCR_AIGroup group = GetPlayerGroup(playerId);
		if (!group || group.GetRadioFrequency() <= 0)
			return;

		int squadFreq = group.GetRadioFrequency();
		bool isLeader = group.IsPlayerLeader(playerId);
		int commandFreq = GetCommandFrequency(faction);
		string factionKey = faction.GetFactionRadioEncryptionKey();

		array<SCR_GadgetComponent> radios = {};
		CollectRadios(gadgets, radios);

		bool commandAssigned;
		int channel = TuneRadios(radios, factionKey, squadFreq, commandFreq, isLeader, playerId, commandAssigned);

		if (m_bDebugLog)
		{
			int groupId = group.GetGroupID();

			string state = string.Format("отряд %1, командир %2, частота отряда %3, командная %4, раций %5, настроено каналов %6", groupId, isLeader, squadFreq, commandFreq, radios.Count(), channel);
			string last;
			if (!m_mLastState.Find(playerId, last) || last != state)
			{
				m_mLastState.Set(playerId, state);
				Print(string.Format("[ZW_Invasion] Связь: игрок %1 — %2", playerId, state), LogLevel.NORMAL);
			}
		}

		if (commandAssigned || !isLeader)
			m_WarnedSingleChannel.RemoveItem(playerId);
		else if (!m_WarnedSingleChannel.Contains(playerId))
		{
			m_WarnedSingleChannel.Insert(playerId);
			Print(string.Format("[ZW_Invasion] Связь: у командира отряда (игрок %1) нет доступного командного канала", playerId), LogLevel.WARNING);
		}
	}

	protected int TuneRadios(notnull array<SCR_GadgetComponent> radios, string factionKey, int squadFreq, int commandFreq, bool isLeader, int playerId, out bool commandAssigned)
	{
		int channel;
		commandAssigned = false;
		foreach (SCR_GadgetComponent gadget : radios)
		{
			if (!gadget)
				continue;

			BaseRadioComponent radio = BaseRadioComponent.Cast(gadget.GetOwner().FindComponent(BaseRadioComponent));
			if (!radio || radio.GetEncryptionKey() != factionKey)
				continue;

			int count = radio.TransceiversCount();
			for (int i = 0; i < count; i++)
			{
				BaseTransceiver transceiver = radio.GetTransceiver(i);
				if (!transceiver)
					continue;

				int wanted = squadFreq;
				if (isLeader && channel > 0 && !commandAssigned && CanTune(transceiver, commandFreq))
				{
					wanted = commandFreq;
					commandAssigned = true;
				}
				else if (!CanTune(transceiver, squadFreq))
				{
					continue;
				}

				channel++;
				if (Replication.IsServer() && m_fRadioRange > 1 && transceiver.GetRange() != m_fRadioRange)
					transceiver.SetRange(m_fRadioRange);
				if (transceiver.GetFrequency() != wanted)
				{
					if (m_bDebugLog)
						Print(string.Format("[ZW_Invasion] Связь: игрок %1, канал %2: %3 -> %4", playerId, i + 1, transceiver.GetFrequency(), wanted), LogLevel.NORMAL);

					transceiver.SetFrequency(wanted);
				}
			}
		}

		return channel;
	}
}
