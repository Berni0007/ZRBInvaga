// ZRBinvaga — убрать объекты карты-основы (например, каменную стену-границу автора карты).
// Ставится на пустую сущность в слое карты. Объекты карты статичные и есть у каждого игрока свои,
// поэтому удаление выполняется на каждой машине (сервер и клиенты) одинаково.
//
// Как найти, что удалять: включить m_bLogLargeObjects — в лог попадут самые частые крупные
// объекты в радиусе (префаб, сколько штук, пример координат). Стена — сотни одинаковых камней.

[ComponentEditorProps(category: "ZRB/Invasion", description: "Invasion: удалить объекты карты по префабу")]
class ZW_RemoveMapObjectsComponentClass : ScriptComponentClass
{
}

class ZW_RemoveMapObjectsComponent : ScriptComponent
{
	[Attribute("", UIWidgets.Auto, "Удалять объекты, у которых путь префаба содержит одну из строк (без учёта регистра)")]
	protected ref array<string> m_aPrefabMatches;

	[Attribute("7300", UIWidgets.EditBox, "Радиус поиска от этой сущности, м")]
	protected float m_fRadius;

	[Attribute("0", UIWidgets.CheckBox, "Записать в лог самые частые крупные объекты (для поиска стены)")]
	protected bool m_bLogLargeObjects;

	[Attribute("6", UIWidgets.EditBox, "Крупный объект — выше стольких метров")]
	protected float m_fLargeHeight;

	protected ref array<IEntity> m_aToDelete = {};
	protected ref map<string, int> m_mLargeCounts = new map<string, int>();
	protected ref map<string, vector> m_mLargeMin = new map<string, vector>();
	protected ref map<string, vector> m_mLargeMax = new map<string, vector>();

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		// После загрузки мира, чтобы все объекты карты уже были созданы.
		GetGame().GetCallqueue().CallLater(Run, 500, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void Run()
	{
		if (!GetOwner() || !GetOwner().GetWorld())
			return;

		for (int i = 0; i < m_aPrefabMatches.Count(); i++)
			m_aPrefabMatches[i] = m_aPrefabMatches[i].Trim();

		m_aToDelete.Clear();
		GetOwner().GetWorld().QueryEntitiesBySphere(GetOwner().GetOrigin(), m_fRadius, OnEntity, null, EQueryEntitiesFlags.STATIC);

		foreach (IEntity entity : m_aToDelete)
		{
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
		}

		if (!m_aPrefabMatches.IsEmpty())
			Print(string.Format("[ZW_Invasion] Удалено объектов карты: %1", m_aToDelete.Count()), LogLevel.NORMAL);
		m_aToDelete.Clear();

		if (m_bLogLargeObjects)
			LogLarge();
	}

	//------------------------------------------------------------------------------------------------
	protected bool OnEntity(IEntity entity)
	{
		EntityPrefabData data = entity.GetPrefabData();
		if (!data)
			return true;

		string prefab = data.GetPrefabName();
		if (prefab.IsEmpty())
			return true;

		string lower = prefab;
		lower.ToLower();
		foreach (string match : m_aPrefabMatches)
		{
			if (match.IsEmpty())
				continue;

			string lowerMatch = match;
			lowerMatch.ToLower();
			if (lower.Contains(lowerMatch))
			{
				m_aToDelete.Insert(entity);
				return true;
			}
		}

		if (m_bLogLargeObjects)
		{
			vector mins, maxs;
			entity.GetWorldBounds(mins, maxs);
			// Деревья и кусты не интересны — ищем постройки и камни.
			if (maxs[1] - mins[1] >= m_fLargeHeight && maxs[0] - mins[0] < 200 && !lower.Contains("vegetation"))
			{
				int count;
				m_mLargeCounts.Find(prefab, count);
				m_mLargeCounts.Set(prefab, count + 1);

				// Где стоят: общий прямоугольник по всем объектам этого префаба.
				vector pos = entity.GetOrigin();
				vector lo = pos;
				vector hi = pos;
				if (count > 0)
				{
					lo = m_mLargeMin.Get(prefab);
					hi = m_mLargeMax.Get(prefab);
					lo[0] = Math.Min(lo[0], pos[0]);
					lo[2] = Math.Min(lo[2], pos[2]);
					hi[0] = Math.Max(hi[0], pos[0]);
					hi[2] = Math.Max(hi[2], pos[2]);
				}
				m_mLargeMin.Set(prefab, lo);
				m_mLargeMax.Set(prefab, hi);
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! 15 самых частых крупных префабов.
	protected void LogLarge()
	{
		Print("[ZW_Invasion] Крупные объекты карты без растительности (префаб — штук — X от..до — Z от..до):", LogLevel.WARNING);
		for (int n = 0; n < 15 && !m_mLargeCounts.IsEmpty(); n++)
		{
			string best;
			int bestCount = -1;
			foreach (string prefab, int count : m_mLargeCounts)
			{
				if (count > bestCount)
				{
					bestCount = count;
					best = prefab;
				}
			}

			vector lo = m_mLargeMin.Get(best);
			vector hi = m_mLargeMax.Get(best);
			Print(string.Format("[ZW_Invasion]   %1 — %2 — X %3..%4 — Z %5..%6", best, bestCount, Math.Round(lo[0]), Math.Round(hi[0]), Math.Round(lo[2]), Math.Round(hi[2])), LogLevel.WARNING);
			m_mLargeCounts.Remove(best);
		}
	}
}
