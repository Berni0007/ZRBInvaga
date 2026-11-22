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

	[Attribute("USSR,RF,RU,RUS", UIWidgets.EditBox, "Ключи фракций РФ через запятую", category: "Мейн")]
	protected string m_sRfFactionKeys;

	protected static ref array<ZW_MainRoleComponent> s_aAll = {};
	protected IEntity m_Spawned;

	//------------------------------------------------------------------------------------------------
	static array<ZW_MainRoleComponent> GetAll()
	{
		return s_aAll;
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
