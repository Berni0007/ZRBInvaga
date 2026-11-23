// ZRBinvaga — мейны привязаны к роли (атака / оборона), а не к фракции.
// Стороны меняются каждый матч, поэтому на старте сервер «надевает» на мейн нужную фракцию:
//   - если сущность — точка спавна, ей ставится фракция роли;
//   - если заданы префабы, на месте сущности спавнится префаб НАТО или РФ (палатка, флаг, КШМ...);
//   - m_bMapMarker — метка мейна на карте (цвет фракции, видна всем).

enum ZW_EMainRole
{
	ATTACK,
	DEFENSE
}

[ComponentEditorProps(category: "ZRB/Invasion", description: "Invasion: объект мейна атаки или обороны")]
class ZW_MainRoleComponentClass : ScriptComponentClass
{
}

class ZW_MainRoleComponent : ScriptComponent
{
	[Attribute("0", UIWidgets.ComboBox, "Чей мейн", enums: ParamEnumArray.FromEnum(ZW_EMainRole), category: "Мейн")]
	protected ZW_EMainRole m_eRole;

	[Attribute("", UIWidgets.ResourceNamePicker, "Префаб, если мейн у НАТО (пусто — ничего не спавнить)", "et", category: "Мейн")]
	protected ResourceName m_sNatoPrefab;

	[Attribute("", UIWidgets.ResourceNamePicker, "Префаб, если мейн у РФ (пусто — ничего не спавнить)", "et", category: "Мейн")]
	protected ResourceName m_sRfPrefab;

	[Attribute("0", UIWidgets.CheckBox, "Метка мейна на карте", category: "Мейн")]
	protected bool m_bMapMarker;

	[Attribute("75", UIWidgets.EditBox, "Радиус запрета стрельбы и защиты от вражеских выстрелов, м (0 — выключено)", params: "0 inf", category: "Мейн")]
	protected float m_fSafeZoneRadius;

	[Attribute("USSR,RF,RU,RUS", UIWidgets.EditBox, "Ключи фракций РФ через запятую", category: "Мейн")]
	protected string m_sRfFactionKeys;

	protected static ref array<ZW_MainRoleComponent> s_aAll = {};
	protected IEntity m_Spawned;
	protected IEntity m_ProjectileBarrier;
	protected ref Resource m_ProjectileBarrierMaterial;

	//------------------------------------------------------------------------------------------------
	static array<ZW_MainRoleComponent> GetAll()
	{
		return s_aAll;
	}

	//------------------------------------------------------------------------------------------------
	static SCR_SpawnPoint GetAttackMain(BaseWorld world, out float radius)
	{
		foreach (ZW_MainRoleComponent role : s_aAll)
		{
			if (!role || role.m_eRole != ZW_EMainRole.ATTACK)
				continue;

			SCR_SpawnPoint center = role.GetSafeZoneCenter(world);
			if (center)
			{
				radius = role.m_fSafeZoneRadius;
				return center;
			}
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsInMainSafeZone(IEntity entity)
	{
		if (!entity || !ZW_Invasion.GetObjectives())
			return false;

		BaseWorld world = entity.GetWorld();
		vector position = entity.GetOrigin();
		foreach (ZW_MainRoleComponent role : s_aAll)
		{
			if (!role)
				continue;

			SCR_SpawnPoint center = role.GetSafeZoneCenter(world);
			if (!center)
				continue;

			if (vector.DistanceSqXZ(position, center.GetOrigin()) <= role.m_fSafeZoneRadius * role.m_fSafeZoneRadius)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsMainProjectileBarrier(IEntity entity)
	{
		if (!entity || !entity.GetName().EndsWith("_ProjectileBarrier"))
			return false;

		foreach (ZW_MainRoleComponent role : s_aAll)
		{
			if (role && role.m_ProjectileBarrier == entity)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	static IEntity FindMainDepot(IEntity service)
	{
		if (!IsInMainSafeZone(service))
			return null;

		BaseWorld world = service.GetWorld();
		vector position = service.GetOrigin();
		IEntity nearest;
		float nearestDistance = float.MAX;
		foreach (ZW_MainRoleComponent role : s_aAll)
		{
			if (!role)
				continue;

			SCR_SpawnPoint center = role.GetSafeZoneCenter(world);
			if (!center)
				continue;

			float distance = vector.DistanceSqXZ(position, center.GetOrigin());
			if (distance >= nearestDistance || distance > role.m_fSafeZoneRadius * role.m_fSafeZoneRadius)
				continue;

			IEntity depot = world.FindEntityByName(center.GetName() + "_Depot");
			if (depot)
			{
				nearest = depot;
				nearestDistance = distance;
			}
		}
		return nearest;
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_SpawnPoint GetSafeZoneCenter(BaseWorld world)
	{
		if (!m_bMapMarker || m_fSafeZoneRadius <= 0)
			return null;

		SCR_SpawnPoint center = SCR_SpawnPoint.Cast(GetOwner());
		if (!center || center.GetWorld() != world)
			return null;

		return center;
	}

	//------------------------------------------------------------------------------------------------
	//! Коллизия нужна и серверу, и клиентам, поскольку клиент показывает полёт и эффекты попадания.
	static void CreateSafeZoneBarriers()
	{
		const ResourceName materialName = "{258FA8B4D29FC807}Assets/ZW_Invasion/ZW_MainProjectileBarrier.gamemat";
		Resource material = Resource.Load(materialName);
		if (!material || !material.IsValid())
		{
			Print("[ZW_Invasion] Не загружен материал границы мейна: " + materialName, LogLevel.ERROR);
			return;
		}

		BaseWorld world = GetGame().GetWorld();
		foreach (ZW_MainRoleComponent role : s_aAll)
		{
			if (!role || role.m_ProjectileBarrier)
				continue;

			SCR_SpawnPoint center = role.GetSafeZoneCenter(world);
			if (center)
				role.CreateProjectileBarrier(center, materialName, material);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CreateProjectileBarrier(notnull SCR_SpawnPoint center, ResourceName materialName, notnull Resource material)
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3] = center.GetOrigin() + Vector(0, 50, 0);

		SCR_GenericBoxEntity barrier = SCR_GenericBoxEntity.Cast(GetGame().SpawnEntity(SCR_GenericBoxEntity, center.GetWorld(), params));
		if (!barrier)
			return;

		barrier.SetName(center.GetName() + "_ProjectileBarrier");
		// Движок ищет коллизию по габаритам модели. Сам короб не отображается.
		float diameter = 2 * m_fSafeZoneRadius;
		barrier.Generate(Vector(diameter, 200, diameter), "{3C10D749A4E84B38}Common/Materials/Default.emat");
		barrier.ClearFlags(EntityFlags.VISIBLE, false);
		barrier.SetFlags(EntityFlags.TRACEABLE, false);
		ref PhysicsGeomDef geoms[1] = {
			new PhysicsGeomDef("FireGeometry", PhysicsGeom.CreateCylinder(m_fSafeZoneRadius, 200), materialName, EPhysicsLayerDefs.FireGeometry)
		};
		if (!Physics.CreateStaticEx(barrier, geoms))
		{
			SCR_EntityHelper.DeleteEntityAndChildren(barrier);
			Print("[ZW_Invasion] Не создана коллизия границы мейна: " + center.GetName(), LogLevel.ERROR);
			return;
		}

		barrier.SetFlags(EntityFlags.UPDATE_LINK, false);
		barrier.Update();
		// Коллизия хранит ссылку на поверхность. Удерживаем ресурс до удаления барьера.
		m_ProjectileBarrierMaterial = material;
		m_ProjectileBarrier = barrier;
	}

	//------------------------------------------------------------------------------------------------
	ZW_EMainRole GetRole()
	{
		return m_eRole;
	}

	//------------------------------------------------------------------------------------------------
	bool HasMapMarker()
	{
		return m_bMapMarker;
	}

	//------------------------------------------------------------------------------------------------
	bool IsRfFaction(Faction faction)
	{
		if (!faction)
			return false;

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
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!s_aAll.Contains(this))
			s_aAll.Insert(this);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (m_ProjectileBarrier)
			SCR_EntityHelper.DeleteEntityAndChildren(m_ProjectileBarrier);

		s_aAll.RemoveItem(this);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер, старт матча: всем объектам мейнов текущего мира — фракцию их роли.
	static void ApplyAll(Faction attacker, Faction defender)
	{
		BaseWorld world = GetGame().GetWorld();
		foreach (ZW_MainRoleComponent role : s_aAll)
		{
			if (!role || !role.GetOwner() || role.GetOwner().GetWorld() != world)
				continue;

			Faction faction = defender;
			if (role.GetRole() == ZW_EMainRole.ATTACK)
				faction = attacker;

			role.Apply(faction);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Apply(Faction faction)
	{
		if (!faction)
			return;

		SCR_SpawnPoint spawnPoint = SCR_SpawnPoint.Cast(GetOwner());
		if (spawnPoint)
		{
			vector spawnPos = spawnPoint.GetOrigin();
			float surfaceY = spawnPoint.GetWorld().GetSurfaceY(spawnPos[0], spawnPos[2]);
			float initialY = spawnPos[1];
			if (initialY < surfaceY)
			{
				spawnPos[1] = surfaceY;
				spawnPoint.SetOrigin(spawnPos);
			}

			spawnPoint.SetFactionKey(faction.GetFactionKey());
			Print(string.Format("[ZW_Invasion] Мейн '%1': высота %2 → %3, поверхность %4", spawnPoint.GetSpawnPointName(), initialY, spawnPos[1], surfaceY), LogLevel.NORMAL);
		}

		ResourceName prefab = m_sNatoPrefab;
		if (IsRfFaction(faction))
			prefab = m_sRfPrefab;

		if (prefab.IsEmpty() || m_Spawned)
			return;

		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
		{
			Print("[ZW_Invasion] Мейн: не удалось загрузить " + prefab, LogLevel.ERROR);
			return;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		GetOwner().GetWorldTransform(params.Transform);
		// Ставим на землю: метки в слое карты могут быть ниже или выше рельефа.
		vector pos = params.Transform[3];
		pos[1] = GetOwner().GetWorld().GetSurfaceY(pos[0], pos[2]);
		params.Transform[3] = pos;
		m_Spawned = GetGame().SpawnEntityPrefab(resource, GetOwner().GetWorld(), params);
	}
}
