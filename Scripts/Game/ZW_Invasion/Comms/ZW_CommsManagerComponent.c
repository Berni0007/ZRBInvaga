// ZRBinvaga — связь как в Squad.
//   - Локальный голос — штатный (прямая речь).
//   - Радио отряда: все бойцы на частоте своего отряда (штатная частота группы).
//   - Командный канал: только командиры отрядов. У командира второй канал (CH2) — частота взвода фракции.
//   - Бойцы командный канал и чужие отряды не слышат: все их каналы стоят на частоте своего отряда.
//   - Сервер раз в m_iCheckMs возвращает правильные частоты (ручная настройка, смена отряда/лидера,
//     подобранная рация). Ручная настройка и штатная автонастройка отключены (ZW_CommsMods.c).
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

	[Attribute("1000", UIWidgets.EditBox, "Как часто сервер проверяет частоты, мс", category: "Связь")]
	protected int m_iCheckMs;

	[Attribute("1", UIWidgets.CheckBox, "Выдавать рацию при спавне, если её нет", category: "Связь")]
	protected bool m_bGiveRadio;

	[Attribute("{73950FBA2D7DB5C5}Prefabs/Items/Equipment/Radios/Radio_ANPRC68.et", UIWidgets.ResourceNamePicker, "Рация НАТО", "et", category: "Связь")]
	protected ResourceName m_sNatoRadio;

	[Attribute("{E1A5D4B878AA8980}Prefabs/Items/Equipment/Radios/Radio_R148.et", UIWidgets.ResourceNamePicker, "Рация РФ", "et", category: "Связь")]
	protected ResourceName m_sRfRadio;

	[Attribute("USSR,RF,RU,RUS", UIWidgets.EditBox, "Ключи фракций РФ через запятую", category: "Связь")]
	protected string m_sRfFactionKeys;

	protected static ZW_CommsManagerComponent s_Instance;

	//! Сервер: игроки-командиры, о которых уже предупредили, что у них один канал.
	protected ref set<int> m_WarnedSingleChannel = new set<int>();

	//! Сервер: последнее состояние связи игрока для отладочного лога (печатается при изменении).
	protected ref map<int, string> m_mLastState = new map<int, string>();

	//! Сервер: рации, которым уже выставили дальность (повторный SetRange каждую проверку дёргает меню рации).
	protected ref set<IEntity> m_RangeApplied = new set<IEntity>();

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
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		s_Instance = this;
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Enforce);
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

		SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(entity);
		if (gadgets && gadgets.GetGadgetByType(EGadgetType.RADIO))
		{
			if (m_bDebugLog)
				Print("[ZW_Invasion] Связь: рация уже есть — не выдаём", LogLevel.NORMAL);
			return;
		}

		Faction faction = ZW_Invasion.GetEntityFaction(entity);
		if (!faction)
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
			Print("[ZW_Invasion] Связь: не удалось выдать рацию — нет места в снаряжении", LogLevel.WARNING);
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
	protected void EnforcePlayer(int playerId)
	{
		IEntity entity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!ChimeraCharacter.Cast(entity) || ZW_RallyManagerComponent.IsDead(entity))
			return;

		SCR_Faction faction = SCR_Faction.Cast(ZW_Invasion.GetEntityFaction(entity));
		if (!faction)
			return;

		SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(entity);
		if (!gadgets)
			return;

		int squadFreq;
		bool isLeader;
		SCR_PlayerControllerGroupComponent groupComponent = SCR_PlayerControllerGroupComponent.GetPlayerControllerComponent(playerId);
		SCR_GroupsManagerComponent groupsManager = SCR_GroupsManagerComponent.GetInstance();
		if (groupComponent && groupsManager)
		{
			SCR_AIGroup group = groupsManager.FindGroup(groupComponent.GetGroupID());
			if (group)
			{
				squadFreq = group.GetRadioFrequency();
				isLeader = group.IsPlayerLeader(playerId);
			}
		}

		int commandFreq = faction.GetFactionRadioFrequency();
		string factionKey = faction.GetFactionRadioEncryptionKey();

		array<SCR_GadgetComponent> radios = {};
		array<SCR_GadgetComponent> found = gadgets.GetGadgetsByType(EGadgetType.RADIO);
		if (found)
			radios.InsertAll(found);

		found = gadgets.GetGadgetsByType(EGadgetType.RADIO_BACKPACK);
		if (found)
			radios.InsertAll(found);

		// Порядковый номер канала среди своих раций: 0 — отряд, 1 — командный (у командира).
		int channel;
		foreach (SCR_GadgetComponent gadget : radios)
		{
			if (!gadget)
				continue;

			BaseRadioComponent radio = BaseRadioComponent.Cast(gadget.GetOwner().FindComponent(BaseRadioComponent));
			if (!radio)
				continue;

			bool own = radio.GetEncryptionKey() == factionKey;
			int count = radio.TransceiversCount();
			for (int i = 0; i < count; i++)
			{
				BaseTransceiver transceiver = radio.GetTransceiver(i);
				if (!transceiver)
					continue;

				int wanted;
				if (!own || squadFreq <= 0)
					wanted = transceiver.GetMinFrequency(); // чужая рация или нет отряда — немая
				else if (isLeader && channel == 1)
					wanted = commandFreq;
				else
					wanted = squadFreq;

				if (own)
					channel++;

				wanted = Math.ClampInt(wanted, transceiver.GetMinFrequency(), transceiver.GetMaxFrequency());
				if (transceiver.GetFrequency() != wanted)
				{
					if (m_bDebugLog)
						Print(string.Format("[ZW_Invasion] Связь: игрок %1, канал %2: %3 -> %4", playerId, i + 1, transceiver.GetFrequency(), wanted), LogLevel.NORMAL);

					transceiver.SetFrequency(wanted);
				}
			}

			IEntity radioEntity = gadget.GetOwner();
			if (!m_RangeApplied.Contains(radioEntity))
			{
				m_RangeApplied.Insert(radioEntity);
				for (int r = 0; r < count; r++)
				{
					BaseTransceiver t = radio.GetTransceiver(r);
					if (t)
						t.SetRange(m_fRadioRange);
				}
			}
		}

		if (m_bDebugLog)
		{
			int groupId = -1;
			if (groupComponent)
				groupId = groupComponent.GetGroupID();

			string state = string.Format("отряд %1, командир %2, частота отряда %3, командная %4, раций %5, своих каналов %6", groupId, isLeader, squadFreq, commandFreq, radios.Count(), channel);
			string last;
			if (!m_mLastState.Find(playerId, last) || last != state)
			{
				m_mLastState.Set(playerId, state);
				Print(string.Format("[ZW_Invasion] Связь: игрок %1 — %2", playerId, state), LogLevel.NORMAL);
			}
		}

		if (isLeader && squadFreq > 0 && channel < 2 && !m_WarnedSingleChannel.Contains(playerId))
		{
			m_WarnedSingleChannel.Insert(playerId);
			Print(string.Format("[ZW_Invasion] Связь: у командира отряда (игрок %1) только %2 канал(а) — командного канала нет", playerId, channel), LogLevel.WARNING);
		}
	}
}
