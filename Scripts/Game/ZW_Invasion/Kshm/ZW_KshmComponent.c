// ZRBinvaga — КШМ (мобильный хаб атаки) на штатном командном грузовике.
//   - «Развернуть КШМ» у кузова: рядом создаётся точка спавна для всей фракции.
//   - Машина сдвинулась, свёрнута или уничтожена — точка исчезает.
//   - Враг ближе m_fEnemyBlockRadius — спавн на КШМ выключен (точка видна, но недоступна).
//   - Не больше m_iMaxDeployedPerFaction развёрнутых КШМ на фракцию.
//   - Ресурсы: штатный склад снабжения в машине (SCR_ResourceComponent в префабе КШМ), загрузка
//     штатным действием «Загрузить снабжение» у склада мейна. У развёрнутой КШМ появляется штатный
//     арсенал (m_sArsenalPrefab), который тратит ресурсы из машины.

[ComponentEditorProps(category: "ZRB/Invasion", description: "Invasion: КШМ — мобильный хаб атаки")]
class ZW_KshmComponentClass : ScriptComponentClass
{
}

class ZW_KshmComponent : ScriptComponent
{
	[Attribute("US", UIWidgets.EditBox, "Ключ фракции КШМ", category: "КШМ")]
	protected string m_sFactionKey;

	[Attribute("{6AE5F2A1B3C40030}Prefabs/Invasion/ZW_KshmSpawnPoint.et", UIWidgets.ResourceNamePicker, "Префаб точки спавна КШМ", "et", category: "КШМ")]
	protected ResourceName m_sSpawnPointPrefab;

	[Attribute("0 0 -6", UIWidgets.EditBox, "Точка спавна относительно машины (за кузовом), м", category: "КШМ")]
	protected vector m_vSpawnOffset;

	[Attribute("", UIWidgets.ResourceNamePicker, "Арсенал у развёрнутой КШМ (тратит ресурсы машины)", "et", category: "КШМ")]
	protected ResourceName m_sArsenalPrefab;

	[Attribute("2.5 0 -6", UIWidgets.EditBox, "Арсенал относительно машины, м", category: "КШМ")]
	protected vector m_vArsenalOffset;

	[Attribute("50", UIWidgets.EditBox, "Враг ближе этого радиуса выключает спавн на КШМ, м", category: "КШМ")]
	protected float m_fEnemyBlockRadius;

	[Attribute("2", UIWidgets.EditBox, "Максимум развёрнутых КШМ на фракцию", category: "КШМ")]
	protected int m_iMaxDeployedPerFaction;

	[Attribute("5", UIWidgets.EditBox, "КШМ сворачивается, если машина сдвинулась дальше, м", category: "КШМ")]
	protected float m_fMaxDrift;

	[Attribute("1", UIWidgets.EditBox, "Развернуть можно, только если скорость меньше, м/с", category: "КШМ")]
	protected float m_fMaxDeploySpeed;

	protected static const int CHECK_MS = 2000;

	//! Сервер: все КШМ в мире (статический список фильтруется по миру — грабли Workbench).
	protected static ref array<ZW_KshmComponent> s_aAll = {};

	[RplProp()]
	protected bool m_bDeployed;

	[RplProp()]
	protected bool m_bEnemyNear;

	protected ZW_KshmSpawnPoint m_SpawnPoint;
	protected IEntity m_Arsenal;
	protected vector m_vDeployOrigin;

	// Временные поля для поиска врагов.
	protected Faction m_QueryFaction;
	protected bool m_bQueryFoundEnemy;

	//------------------------------------------------------------------------------------------------
	bool IsDeployed()
	{
		return m_bDeployed;
	}

	//------------------------------------------------------------------------------------------------
	bool IsEnemyNear()
	{
		return m_bEnemyNear;
	}

	//------------------------------------------------------------------------------------------------
	string GetFactionKey()
	{
		return m_sFactionKey;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!s_aAll.Contains(this))
			s_aAll.Insert(this);

		if (Replication.IsServer())
			GetGame().GetCallqueue().CallLater(CheckState, CHECK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(CheckState);
		s_aAll.RemoveItem(this);
		DeleteSpawnPoint();
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Сколько КШМ этой фракции уже развёрнуто в текущем мире.
	protected int CountDeployed()
	{
		int count;
		BaseWorld world = GetOwner().GetWorld();
		foreach (ZW_KshmComponent kshm : s_aAll)
		{
			if (kshm && kshm != this && kshm.IsDeployed() && kshm.GetFactionKey() == m_sFactionKey && kshm.GetOwner().GetWorld() == world)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsDestroyed()
	{
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(GetOwner().FindComponent(SCR_DamageManagerComponent));
		return damage && damage.GetState() == EDamageState.DESTROYED;
	}

	//------------------------------------------------------------------------------------------------
	protected float GetSpeed()
	{
		Physics physics = GetOwner().GetPhysics();
		if (!physics)
			return 0;

		return physics.GetVelocity().Length();
	}

	//------------------------------------------------------------------------------------------------
	//! Можно ли развернуть. Работает и на клиенте.
	bool CanDeploy(IEntity user, out string reason)
	{
		if (m_bDeployed)
			return true;

		if (IsDestroyed())
		{
			reason = "КШМ уничтожена";
			return false;
		}

		if (GetSpeed() > m_fMaxDeploySpeed)
		{
			reason = "Остановите машину";
			return false;
		}

		Faction userFaction = ZW_Invasion.GetEntityFaction(user);
		if (!userFaction || userFaction.GetFactionKey() != m_sFactionKey)
		{
			reason = "Чужая КШМ";
			return false;
		}

		if (CountDeployed() >= m_iMaxDeployedPerFaction)
		{
			reason = string.Format("Уже развёрнуто %1 КШМ — это максимум", m_iMaxDeployedPerFaction);
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер: развернуть или свернуть.
	void Toggle(IEntity user)
	{
		if (!Replication.IsServer())
			return;

		if (m_bDeployed)
		{
			Pack("свёрнута игроком");
			return;
		}

		string reason;
		if (!CanDeploy(user, reason))
		{
			Print("[ZW_Invasion] КШМ не развёрнута: " + reason, LogLevel.NORMAL);
			return;
		}

		Deploy();
	}

	//------------------------------------------------------------------------------------------------
	protected void Deploy()
	{
		Resource resource = Resource.Load(m_sSpawnPointPrefab);
		if (!resource || !resource.IsValid())
		{
			Print("[ZW_Invasion] КШМ: не удалось загрузить точку спавна " + m_sSpawnPointPrefab, LogLevel.ERROR);
			return;
		}

		vector transform[4];
		GetOwner().GetWorldTransform(transform);

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = GetOwner().CoordToParent(m_vSpawnOffset);

		m_SpawnPoint = ZW_KshmSpawnPoint.Cast(GetGame().SpawnEntityPrefab(resource, GetOwner().GetWorld(), params));
		if (!m_SpawnPoint)
		{
			Print("[ZW_Invasion] КШМ: префаб точки спавна не ZW_KshmSpawnPoint", LogLevel.ERROR);
			return;
		}

		m_SpawnPoint.SetFactionKey(m_sFactionKey);
		m_SpawnPoint.ZW_SetKshm(this);
		SpawnArsenal();
		m_vDeployOrigin = GetOwner().GetOrigin();
		m_bDeployed = true;
		m_bEnemyNear = false;
		Replication.BumpMe();

		Print(string.Format("[ZW_Invasion] КШМ %1 развёрнута", m_sFactionKey), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void Pack(string why)
	{
		DeleteSpawnPoint();
		if (!m_bDeployed)
			return;

		m_bDeployed = false;
		m_bEnemyNear = false;
		Replication.BumpMe();
		Print(string.Format("[ZW_Invasion] КШМ %1 %2", m_sFactionKey, why), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void DeleteSpawnPoint()
	{
		if (m_SpawnPoint)
			SCR_EntityHelper.DeleteEntityAndChildren(m_SpawnPoint);

		m_SpawnPoint = null;

		if (m_Arsenal)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Arsenal);

		m_Arsenal = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер: штатный арсенал рядом с машиной, на земле.
	protected void SpawnArsenal()
	{
		if (m_sArsenalPrefab.IsEmpty())
			return;

		Resource resource = Resource.Load(m_sArsenalPrefab);
		if (!resource || !resource.IsValid())
		{
			Print("[ZW_Invasion] КШМ: не удалось загрузить арсенал " + m_sArsenalPrefab, LogLevel.ERROR);
			return;
		}

		vector pos = GetOwner().CoordToParent(m_vArsenalOffset);
		pos[1] = GetOwner().GetWorld().GetSurfaceY(pos[0], pos[2]);

		float yaw = GetOwner().GetYawPitchRoll()[0];
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), params.Transform);
		params.Transform[3] = pos;

		m_Arsenal = GetGame().SpawnEntityPrefab(resource, GetOwner().GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер, раз в CHECK_MS.
	protected void CheckState()
	{
		if (!m_bDeployed)
			return;

		if (IsDestroyed())
		{
			Pack("уничтожена");
			return;
		}

		if (vector.Distance(GetOwner().GetOrigin(), m_vDeployOrigin) > m_fMaxDrift)
		{
			Pack("свёрнута: машина сдвинулась");
			return;
		}

		bool enemy = FindEnemyNear();
		if (enemy != m_bEnemyNear)
		{
			m_bEnemyNear = enemy;
			Replication.BumpMe();
			if (enemy)
				Print(string.Format("[ZW_Invasion] КШМ %1: враг рядом — спавн выключен", m_sFactionKey), LogLevel.NORMAL);
			else
				Print(string.Format("[ZW_Invasion] КШМ %1: врага нет — спавн включён", m_sFactionKey), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool FindEnemyNear()
	{
		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
			return false;

		m_QueryFaction = factionManager.GetFactionByKey(m_sFactionKey);
		if (!m_QueryFaction)
			return false;

		m_bQueryFoundEnemy = false;
		GetGame().GetWorld().QueryEntitiesBySphere(GetOwner().GetOrigin(), m_fEnemyBlockRadius, EnemyQueryCallback, null, EQueryEntitiesFlags.DYNAMIC);
		m_QueryFaction = null;
		return m_bQueryFoundEnemy;
	}

	//------------------------------------------------------------------------------------------------
	protected bool EnemyQueryCallback(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character || ZW_RallyManagerComponent.IsDead(character))
			return true;

		Faction faction = ZW_Invasion.GetEntityFaction(character);
		if (!faction || faction == m_QueryFaction)
			return true;

		m_bQueryFoundEnemy = true;
		return false;
	}
}
