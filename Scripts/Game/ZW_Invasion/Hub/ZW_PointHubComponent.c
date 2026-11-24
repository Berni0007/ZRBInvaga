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

	[Attribute("7", UIWidgets.EditBox, "Половина стороны свободной площадки под хаб, м", category: "Хаб")]
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
	static IEntity GetTentForPoint(ZW_InvasionPoint point)
	{
		if (!point)
			return null;

		foreach (ZW_PointHubComponent hub : s_aAll)
		{
			if (hub && hub.m_Point == point && hub.m_Tent)
				return hub.m_Tent;
		}

		return null;
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
	//! Свободное ровное место рядом с меткой внутри зоны захвата. Площадка проверяется лучами
	//! (дома, стены, заборы, деревья, техника). Сначала ищем площадку m_fClearRadius, потом поменьше;
	//! если чистой нет — ставим туда, где препятствий меньше всего.
	protected vector FindOpenSpot(vector origin)
	{
		BaseWorld world = GetOwner().GetWorld();
		array<float> sizes = {m_fClearRadius, m_fClearRadius * 0.7};
		vector best = origin;
		int bestHits = int.MAX;
		float bestRing;

		foreach (float half : sizes)
		{
			float maxRing = Math.Min(m_fSearchRadius, m_Point.GetCaptureRadius() - half);
			for (float ring = 0; ring <= maxRing; ring += 4)
			{
				int steps = 1;
				if (ring > 0)
					steps = 16;

				for (int i = 0; i < steps; i++)
				{
					float angle = Math.PI2 * i / steps;
					vector candidate = origin;
					candidate[0] = origin[0] + Math.Cos(angle) * ring;
					candidate[2] = origin[2] + Math.Sin(angle) * ring;
					candidate[1] = world.GetSurfaceY(candidate[0], candidate[2]);

					if (vector.DistanceXZ(candidate, m_Point.GetOrigin()) > m_Point.GetCaptureRadius() - half)
						continue;

					int hits = CountObstacles(world, candidate, half);
					if (hits == 0)
					{
						Print(string.Format("[ZW_Invasion] Хаб '%1': свободная площадка %2 м в %3 м от метки", m_Point.GetPointName(), half * 2, ring), LogLevel.NORMAL);
						return candidate;
					}

					if (hits < bestHits)
					{
						bestHits = hits;
						best = candidate;
						bestRing = ring;
					}
				}
			}
		}

		Print(string.Format("[ZW_Invasion] Хаб '%1': чистой площадки нет, поставлен в %2 м от метки (препятствий: %3) — лучше передвинуть метку в редакторе", m_Point.GetPointName(), bestRing, bestHits), LogLevel.WARNING);
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Сколько лучей по площадке во что-то упёрлось. Горизонтальные лучи на высоте 1 м ловят стены
	//! и заборы, вертикальные сверху — дома, крыши, кроны. Крутой склон — сразу «занято».
	protected int CountObstacles(BaseWorld world, vector center, float half)
	{
		float minY = center[1];
		float maxY = center[1];
		for (int i = 0; i < 8; i++)
		{
			float angle = Math.PI2 * i / 8;
			float y = world.GetSurfaceY(center[0] + Math.Cos(angle) * half, center[2] + Math.Sin(angle) * half);
			minY = Math.Min(minY, y);
			maxY = Math.Max(maxY, y);
		}

		if (maxY - minY > 1.5)
			return 1000;

		int hits;
		float step = 2;

		// Горизонтальные лучи вдоль X и вдоль Z.
		for (float d = -half; d <= half; d += step)
		{
			if (TraceBlocked(world, center[0] - half, center[2] + d, center[0] + half, center[2] + d, 1.0, 1.0))
				hits++;
			if (TraceBlocked(world, center[0] + d, center[2] - half, center[0] + d, center[2] + half, 1.0, 1.0))
				hits++;
		}

		// Вертикальные лучи сверху вниз до 0.5 м над землёй.
		for (float x = -half; x <= half; x += step * 2)
		{
			for (float z = -half; z <= half; z += step * 2)
			{
				float px = center[0] + x;
				float pz = center[2] + z;
				float ground = world.GetSurfaceY(px, pz);
				TraceParam param = new TraceParam();
				param.Start = Vector(px, ground + 10, pz);
				param.End = Vector(px, ground + 0.5, pz);
				param.Flags = TraceFlags.ENTS;
				param.LayerMask = EPhysicsLayerPresets.Projectile;
				param.Exclude = GetOwner();
				if (world.TraceMove(param, null) < 1)
					hits++;
			}
		}

		return hits;
	}

	//------------------------------------------------------------------------------------------------
	protected bool TraceBlocked(BaseWorld world, float x1, float z1, float x2, float z2, float height1, float height2)
	{
		TraceParam param = new TraceParam();
		param.Start = Vector(x1, world.GetSurfaceY(x1, z1) + height1, z1);
		param.End = Vector(x2, world.GetSurfaceY(x2, z2) + height2, z2);
		param.Flags = TraceFlags.ENTS;
		param.LayerMask = EPhysicsLayerPresets.Projectile;
		param.Exclude = GetOwner();
		return world.TraceMove(param, null) < 1;
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
