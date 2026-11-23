// ZRBinvaga — фаза подготовки и таймер матча.
// Подготовка: точки закрыты. Потом идёт таймер штурма; вышло время — победа обороны.

[ComponentEditorProps(category: "ZRB/Invasion", description: "Invasion: подготовка и таймер матча")]
class ZW_TimerComponentClass : SCR_BaseGameModeComponentClass
{
}

class ZW_TimerComponent : SCR_BaseGameModeComponent
{
	[Attribute("600", UIWidgets.EditBox, "Фаза подготовки, сек (0 = без подготовки)", category: "Invasion")]
	protected int m_iPrepSeconds;

	[Attribute("3600", UIWidgets.EditBox, "Время на штурм, сек", category: "Invasion")]
	protected int m_iMatchSeconds;

	[Attribute("600", UIWidgets.EditBox, "Бонус времени атаке за взятую точку, сек", category: "Invasion")]
	protected int m_iCaptureBonusSeconds;

	[RplProp()]
	protected int m_iRemainingSeconds;

	[RplProp(onRplName: "SyncMainBarrier")]
	protected bool m_bPrepPhase;

	protected bool m_bRunning;
	protected float m_fPrepEndsAt;
	protected ref ZW_AttackMainBarrier m_MainBarrier;

	//------------------------------------------------------------------------------------------------
	int GetRemainingSeconds()
	{
		return m_iRemainingSeconds;
	}

	//------------------------------------------------------------------------------------------------
	bool IsPrepPhase()
	{
		return m_bPrepPhase;
	}

	//------------------------------------------------------------------------------------------------
	override void OnGameModeStart()
	{
		super.OnGameModeStart();

		if (!Replication.IsServer())
		{
			SyncMainBarrier();
			return;
		}

		// Подготовка минимум 1 сек: точки успевают инициализироваться до открытия первой.
		m_bPrepPhase = true;
		m_iRemainingSeconds = Math.Max(1, m_iPrepSeconds);
		BaseWorld world = GetOwner().GetWorld();
		m_fPrepEndsAt = world.GetWorldTime() + m_iRemainingSeconds * 1000;
		Replication.BumpMe();

		m_bRunning = true;
		GetGame().GetCallqueue().CallLater(Tick, 1000, true);

		SyncMainBarrier();
	}

	//------------------------------------------------------------------------------------------------
	override void OnGameModeEnd(SCR_GameModeEndData data)
	{
		super.OnGameModeEnd(data);
		Stop();
	}

	//------------------------------------------------------------------------------------------------
	//! Сервер: бонус времени за взятую точку.
	void AddCaptureBonus()
	{
		if (!Replication.IsServer() || m_bPrepPhase)
			return;

		m_iRemainingSeconds += m_iCaptureBonusSeconds;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		ZW_ObjectivesComponent objectives = ZW_Invasion.GetObjectives();
		if (!objectives || objectives.IsEnded())
		{
			Stop();
			return;
		}

		if (m_bPrepPhase)
			m_iRemainingSeconds = Math.Ceil((m_fPrepEndsAt - GetOwner().GetWorld().GetWorldTime()) / 1000);
		else
			m_iRemainingSeconds--;

		if (m_iRemainingSeconds <= 0)
		{
			if (m_bPrepPhase)
			{
				m_bPrepPhase = false;
				m_iRemainingSeconds = m_iMatchSeconds;
				SyncMainBarrier();
				objectives.StartAssault();
			}
			else
			{
				m_iRemainingSeconds = 0;
				Replication.BumpMe();
				Stop();
				objectives.EndInvasion(false, EGameOverTypes.ENDREASON_TIMELIMIT);
				return;
			}
		}

		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	protected void SyncMainBarrier()
	{
		GetGame().GetCallqueue().Remove(SyncMainBarrier);
		if (!m_bPrepPhase || m_iPrepSeconds <= 0)
		{
			ClearMainBarrier();
			return;
		}

		if (m_MainBarrier)
			return;

		float radius;
		SCR_SpawnPoint main = ZW_MainRoleComponent.GetAttackMain(GetOwner().GetWorld(), radius);
		if (!main)
		{
			if (!Replication.IsServer())
				GetGame().GetCallqueue().CallLater(SyncMainBarrier, 250, false);
			else
				Print("[ZW_Invasion] Подготовка: не найден спавн мейна атаки с радиусом зоны", LogLevel.WARNING);
			return;
		}

		m_MainBarrier = new ZW_AttackMainBarrier();
		if (!m_MainBarrier.Create(main, radius))
		{
			ClearMainBarrier();
			Print("[ZW_Invasion] Не создана граница мейна атаки", LogLevel.ERROR);
			return;
		}

		if (Replication.IsServer())
			Print(string.Format("[ZW_Invasion] Физическая граница мейна атаки включена на %1 с", m_iPrepSeconds), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearMainBarrier()
	{
		if (!m_MainBarrier)
			return;

		m_MainBarrier.Clear();
		m_MainBarrier = null;
		if (Replication.IsServer() && !m_bPrepPhase)
			Print("[ZW_Invasion] Мейн атаки открыт", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void Stop()
	{
		GetGame().GetCallqueue().Remove(SyncMainBarrier);
		ClearMainBarrier();
		if (!m_bRunning)
			return;

		m_bRunning = false;
		GetGame().GetCallqueue().Remove(Tick);
	}
}
