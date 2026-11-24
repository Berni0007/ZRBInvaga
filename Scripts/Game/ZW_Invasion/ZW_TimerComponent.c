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

	[Attribute("0", UIWidgets.EditBox, "Радиус границы подготовки от палатки хаба первой точки, м (0 = выключена)", category: "Invasion")]
	protected float m_fDefenseHubPrepRadius;

	[Attribute("3600", UIWidgets.EditBox, "Время на штурм, сек", category: "Invasion")]
	protected int m_iMatchSeconds;

	[Attribute("600", UIWidgets.EditBox, "Бонус времени атаке за взятую точку, сек", category: "Invasion")]
	protected int m_iCaptureBonusSeconds;

	[RplProp()]
	protected int m_iRemainingSeconds;

	[RplProp(onRplName: "SyncPrepBarriers")]
	protected bool m_bPrepPhase;

	[RplProp(onRplName: "ScheduleDefenseHubBarrierSync")]
	protected vector m_vDefenseHubCenter;

	[RplProp(onRplName: "ScheduleDefenseHubBarrierSync")]
	protected bool m_bDefenseHubCenterReady;

	protected bool m_bRunning;
	protected float m_fPrepEndsAt;
	protected ref ZW_AttackMainBarrier m_MainBarrier;
	protected ref ZW_AttackMainBarrier m_DefenseHubBarrier;

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
			SyncPrepBarriers();
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

		SyncPrepBarriers();
		if (m_iPrepSeconds > 0 && m_fDefenseHubPrepRadius > 0)
			GetGame().GetCallqueue().CallLater(InitDefenseHubBarrier, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	override void OnGameModeEnd(SCR_GameModeEndData data)
	{
		super.OnGameModeEnd(data);
		Stop();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		Stop();
		super.OnDelete(owner);
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
				SyncPrepBarriers();
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
	protected void InitDefenseHubBarrier()
	{
		if (!m_bPrepPhase)
			return;

		ZW_ObjectivesComponent objectives = ZW_Invasion.GetObjectives();
		if (!objectives)
			return;

		IEntity tent = ZW_PointHubComponent.GetTentForPoint(objectives.GetPoint(0));
		if (!tent)
		{
			Print("[ZW_Invasion] Подготовка: не найдена палатка хаба первой точки", LogLevel.WARNING);
			return;
		}

		m_vDefenseHubCenter = tent.GetOrigin();
		m_bDefenseHubCenterReady = true;
		Replication.BumpMe();
		SyncDefenseHubBarrier();
	}

	//------------------------------------------------------------------------------------------------
	protected void SyncPrepBarriers()
	{
		SyncMainBarrier();
		ScheduleDefenseHubBarrierSync();
	}

	//------------------------------------------------------------------------------------------------
	protected void ScheduleDefenseHubBarrierSync()
	{
		GetGame().GetCallqueue().Remove(SyncDefenseHubBarrier);
		if (!m_bPrepPhase)
			SyncDefenseHubBarrier();
		else
			GetGame().GetCallqueue().CallLater(SyncDefenseHubBarrier, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void SyncDefenseHubBarrier()
	{
		if (!m_bPrepPhase || !m_bDefenseHubCenterReady || m_fDefenseHubPrepRadius <= 0)
		{
			ClearDefenseHubBarrier();
			return;
		}

		if (m_DefenseHubBarrier)
			return;

		m_DefenseHubBarrier = new ZW_AttackMainBarrier();
		if (!m_DefenseHubBarrier.Create(GetOwner().GetWorld(), m_vDefenseHubCenter, m_fDefenseHubPrepRadius))
		{
			ClearDefenseHubBarrier();
			Print("[ZW_Invasion] Не создана граница хаба первой точки", LogLevel.ERROR);
			return;
		}

		if (Replication.IsServer())
			Print(string.Format("[ZW_Invasion] Граница подготовки хаба первой точки включена: радиус %1 м, центр %2", m_fDefenseHubPrepRadius, m_vDefenseHubCenter), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearDefenseHubBarrier()
	{
		if (!m_DefenseHubBarrier)
			return;

		m_DefenseHubBarrier.Clear();
		m_DefenseHubBarrier = null;
		if (Replication.IsServer() && !m_bPrepPhase)
			Print("[ZW_Invasion] Граница подготовки хаба первой точки снята", LogLevel.NORMAL);
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
		if (!m_MainBarrier.Create(main.GetWorld(), main.GetOrigin(), radius))
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
		GetGame().GetCallqueue().Remove(InitDefenseHubBarrier);
		GetGame().GetCallqueue().Remove(SyncDefenseHubBarrier);
		ClearMainBarrier();
		ClearDefenseHubBarrier();
		if (!m_bRunning)
			return;

		m_bRunning = false;
		GetGame().GetCallqueue().Remove(Tick);
	}
}
