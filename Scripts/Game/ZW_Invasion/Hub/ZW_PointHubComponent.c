// ZRBinvaga — хаб на точке Invasion (как в Squad).
// Ставится в слое карты как метка места рядом с точкой (любой полосы). На старте матча сервер
// создаёт хаб только если ближайшая точка входит в цепочку выбранной полосы:
//   - точка спавна «Хаб: <точка>»;
//   - палатка-КП фракции-владельца (в ней штатная стройка «Start building»);
//   - склад, на старте пустой — ресурсы привозит грузовик снабжения.
// Владелец: оборона, пока точка не взята; после захвата — атака (палатка меняется, ресурсы остаются).
// Спавн: оборона — только на хабе текущей цели (до штурма — точки 1); атака — на хабах взятых точек.
// Враг ближе m_fEnemyBlockRadius — спавн выключен.

[ComponentEditorProps(category: "ZRB/Invasion", description: "Invasion: место хаба у точки")]
class ZW_PointHubComponentClass : ScriptComponentClass
{
}

class ZW_PointHubComponent : ScriptComponent
{
	[Attribute("150", UIWidgets.EditBox, "Связать с ближайшей точкой в этом радиусе, м", category: "Хаб")]
	protected float m_fLinkRadius;

	[Attribute("{F09296B9255F6704}Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_CommandPost_US_01.et", UIWidgets.ResourceNamePicker, "Палатка НАТО", "et", category: "Хаб")]
	protected ResourceName m_sNatoTent;

	[Attribute("{1D2887BB9A7D4670}Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_CommandPost_USSR_01.et", UIWidgets.ResourceNamePicker, "Палатка РФ", "et", category: "Хаб")]
	protected ResourceName m_sRfTent;

	[Attribute("{6AE7B00100001010}Prefabs/Invasion/ZW_HubSpawnPoint.et", UIWidgets.ResourceNamePicker, "Точка спавна хаба", "et", category: "Хаб")]
	protected ResourceName m_sSpawnPointPrefab;

	[Attribute("{6AE7B00100001020}Prefabs/Invasion/ZW_HubSupply.et", UIWidgets.ResourceNamePicker, "Склад хаба", "et", category: "Хаб")]
	protected ResourceName m_sSupplyPrefab;

	[Attribute("0 0 0", UIWidgets.EditBox, "Палатка относительно метки, м", category: "Хаб")]
	protected vector m_vTentOffset;

	[Attribute("5 0 0", UIWidgets.EditBox, "Спавн относительно метки, м", category: "Хаб")]
	protected vector m_vSpawnOffset;

	[Attribute("0 0 5", UIWidgets.EditBox, "Склад относительно метки, м", category: "Хаб")]
	protected vector m_vSupplyOffset;

	[Attribute("1", UIWidgets.CheckBox, "Искать свободное ровное место рядом с меткой (палатку не ставить в стены и дома)", category: "Хаб")]
	protected bool m_bFindOpenSpot;

	[Attribute("10", UIWidgets.EditBox, "Свободный радиус вокруг хаба, м", category: "Хаб")]
	protected float m_fClearRadius;

	[Attribute("30", UIWidgets.EditBox, "Как далеко от метки искать место, м (не дальше зоны захвата)", category: "Хаб")]
	protected float m_fSearchRadius;

	[Attribute("50", UIWidgets.EditBox, "Враг ближе — спавн на хабе выключен, м", category: "Хаб")]
	protected float m_fEnemyBlockRadius;

	[Attribute("USSR,RF,RU,RUS", UIWidgets.EditBox, "Ключи фракций РФ через запятую", category: "Хаб")]
	protected string m_sRfFactionKeys;

	protected static ref array<ZW_PointHubComponent> s_aAll = {};

	protected ZW_InvasionPoint m_Point;
	protected ZW_HubSpawnPoint m_SpawnPoint;
	protected IEntity m_Tent;
	protected IEntity m_Supply;
	protected Faction m_Owner;

	//! Центр хаба (метка или найденное свободное место).
	protected vector m_vBase;

	// Временные поля для поиска свободного места.
	protected bool m_bSpotBlocked;

	// Временные поля для поиска врага.
	protected Faction m_EnemyCheckFaction;
	protected bool m_bEnemyFound;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!s_aAll.Contains(this))
			s_aAll.Insert(this);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(UpdateHub);
		s_aAll.RemoveItem(this);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер, старт матча (после выбора сторон и полосы).
	static void ApplyAll(notnull ZW_ObjectivesComponent objectives)
	{
		BaseWorld world = GetGame().GetWorld();
		int created;
		foreach (ZW_PointHubComponent hub : s_aAll)
		{
			if (hub && hub.GetOwner() && hub.GetOwner().GetWorld() == world && hub.Create(objectives))
				created++;
		}

		Print(string.Format("[ZW_Invasion] Хабов на точках: %1", created), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected bool Create(ZW_ObjectivesComponent objectives)
	{
		if (m_Point)
			return false;

		// Ближайшая точка цепочки этого матча.
		float best = m_fLinkRadius;
		vector origin = GetOwner().GetOrigin();
		foreach (ZW_InvasionPoint point : objectives.GetChain())
		{
			if (!point)
				continue;

			float dist = vector.DistanceXZ(origin, point.GetOrigin());
			if (dist <= best)
			{
				best = dist;
				m_Point = point;
			}
		}

		if (!m_Point)
			return false;

		m_vBase = GetOwner().GetOrigin();
		if (m_bFindOpenSpot)
			m_vBase = FindOpenSpot(m_vBase);

		m_Supply = SpawnAt(m_sSupplyPrefab, m_vSupplyOffset);
		m_SpawnPoint = ZW_HubSpawnPoint.Cast(SpawnAt(m_sSpawnPointPrefab, m_vSpawnOffset));
		UpdateHub();
		GetGame().GetCallqueue().CallLater(UpdateHub, 2000, true);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер, раз в 2 с: владелец, палатка, разрешение спавна.
	protected void UpdateHub()
	{
		ZW_ObjectivesComponent objectives = ZW_Invasion.GetObjectives();
		if (!objectives || !m_Point)
			return;

		Faction attacker = objectives.GetAttackerFaction();
		Faction defender = objectives.GetDefenderFaction();
		Faction owner = defender;
		if (m_Point.IsLocked())
			owner = attacker;

		if (!owner)
			return;

		if (owner != m_Owner)
		{
			m_Owner = owner;

			if (m_Tent)
				SCR_EntityHelper.DeleteEntityAndChildren(m_Tent);

			ResourceName tent = m_sNatoTent;
			if (IsRfFaction(owner))
				tent = m_sRfTent;

			m_Tent = SpawnAt(tent, m_vTentOffset);
			Print(string.Format("[ZW_Invasion] Хаб '%1' теперь у %2", m_Point.GetPointName(), owner.GetFactionKey()), LogLevel.NORMAL);
		}

		// Виден ли хаб на карте возрождения вообще.
		bool visible;
		if (owner == attacker)
		{
			visible = true; // атака спавнится на хабах взятых точек
		}
		else
		{
			int activeOrder = objectives.GetActiveIndex() + 1;
			if (activeOrder < 1)
				activeOrder = 1;

			visible = m_Point.GetOrder() == activeOrder; // оборона — только на текущей цели
		}

		// Враг рядом — хаб виден, но неактивен.
		bool allowed = visible && !IsEnemyNear(owner);

		if (m_SpawnPoint)
		{
			// Невидимый хаб снимаем с фракции: штатная карта возрождения показывает
			// все точки фракции, даже выключенные.
			string factionKey;
			if (visible)
				factionKey = owner.GetFactionKey();
			m_SpawnPoint.SetFactionKey(factionKey);
			m_SpawnPoint.ZW_SetState(allowed, m_Point.GetPointName());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsEnemyNear(Faction owner)
	{
		m_EnemyCheckFaction = owner;
		m_bEnemyFound = false;
		GetOwner().GetWorld().QueryEntitiesBySphere(GetOwner().GetOrigin(), m_fEnemyBlockRadius, EnemyCallback, null, EQueryEntitiesFlags.DYNAMIC);
		m_EnemyCheckFaction = null;
		return m_bEnemyFound;
	}

	//------------------------------------------------------------------------------------------------
	protected bool EnemyCallback(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return true;

		CharacterControllerComponent controller = character.GetCharacterController();
		if (controller && controller.IsDead())
			return true;

		Faction faction = ZW_Invasion.GetEntityFaction(character);
		if (faction && faction != m_EnemyCheckFaction)
		{
			m_bEnemyFound = true;
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsRfFaction(Faction faction)
	{
		array<string> keys = {};
		m_sRfFactionKeys.Split(",", keys, true);
		foreach (string key : keys)
		{
			key.Trim();
			if (key == faction.GetFactionKey())
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер: префаб на земле рядом с меткой (смещение в осях метки), с её поворотом.
	//! Свободное ровное место рядом с меткой: в радиусе m_fClearRadius нет домов, стен, деревьев,
	//! перепад высоты небольшой, и место внутри зоны захвата. Не нашлось — метка как есть.
	protected vector FindOpenSpot(vector origin)
	{
		BaseWorld world = GetOwner().GetWorld();
		float zoneLimit = m_Point.GetCaptureRadius() - m_fClearRadius;

		for (float ring = 0; ring <= m_fSearchRadius; ring += 7.5)
		{
			int steps = 1;
			if (ring > 0)
				steps = 12;

			for (int i = 0; i < steps; i++)
			{
				float angle = Math.PI2 * i / steps;
				vector candidate = origin;
				candidate[0] = origin[0] + Math.Cos(angle) * ring;
				candidate[2] = origin[2] + Math.Sin(angle) * ring;
				candidate[1] = world.GetSurfaceY(candidate[0], candidate[2]);

				if (vector.DistanceXZ(candidate, m_Point.GetOrigin()) > zoneLimit)
					continue;

				if (IsSpotFree(world, candidate))
				{
					if (ring > 0)
						Print(string.Format("[ZW_Invasion] Хаб '%1': метка занята, поставлен в %2 м от неё", m_Point.GetPointName(), ring), LogLevel.NORMAL);
					return candidate;
				}
			}
		}

		Print(string.Format("[ZW_Invasion] Хаб '%1': свободного места рядом нет, ставлю на метку — передвиньте метку в редакторе", m_Point.GetPointName()), LogLevel.WARNING);
		return origin;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsSpotFree(BaseWorld world, vector center)
	{
		// Ровность: перепад высот по краям площадки.
		float minY = center[1];
		float maxY = center[1];
		for (int i = 0; i < 8; i++)
		{
			float angle = Math.PI2 * i / 8;
			float y = world.GetSurfaceY(center[0] + Math.Cos(angle) * m_fClearRadius * 0.7, center[2] + Math.Sin(angle) * m_fClearRadius * 0.7);
			minY = Math.Min(minY, y);
			maxY = Math.Max(maxY, y);
		}

		if (maxY - minY > 1.5)
			return false;

		m_bSpotBlocked = false;
		world.QueryEntitiesBySphere(center, m_fClearRadius, SpotCallback, null, EQueryEntitiesFlags.STATIC);
		return !m_bSpotBlocked;
	}

	//------------------------------------------------------------------------------------------------
	//! Мешает всё, что выше 1 м: дома, стены, заборы, деревья. Мелкий мусор и трава — нет.
	protected bool SpotCallback(IEntity entity)
	{
		if (entity == GetOwner() || !entity.GetVObject())
			return true;

		vector mins, maxs;
		entity.GetWorldBounds(mins, maxs);
		if (maxs[1] - mins[1] < 1.0)
			return true;

		// Земля, дороги, линии электропередачи и прочее огромное — не препятствие.
		if (maxs[0] - mins[0] > 150 || maxs[2] - mins[2] > 150 || GenericTerrainEntity.Cast(entity))
			return true;

		m_bSpotBlocked = true;
		return false; // дальше можно не искать
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnAt(ResourceName prefab, vector offset)
	{
		if (prefab.IsEmpty())
			return null;

		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
		{
			Print("[ZW_Invasion] Хаб: не удалось загрузить " + prefab, LogLevel.ERROR);
			return null;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		GetOwner().GetWorldTransform(params.Transform);
		vector pos = m_vBase + params.Transform[0] * offset[0] + params.Transform[2] * offset[2];
		pos[1] = GetOwner().GetWorld().GetSurfaceY(pos[0], pos[2]);
		params.Transform[3] = pos;
		return GetGame().SpawnEntityPrefab(resource, GetOwner().GetWorld(), params);
	}
}
