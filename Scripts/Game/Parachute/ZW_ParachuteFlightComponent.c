[ComponentEditorProps(category: "ZARUBA/Parachute", description: "Server simulation of a deployed parachute.")]
class ZW_ParachuteFlightComponentClass : ScriptComponentClass
{
}

class ZW_ParachuteFlightComponent : ScriptComponent
{
	protected const float OPEN_TIME = 0.85;
	protected const float INPUT_TIMEOUT = 0.5;
	protected const float SNAPSHOT_INTERVAL = 0.10;
	protected const float LAND_TIME = 0.55;
	protected const float MAX_FLIGHT_TIME = 180;
	protected const float MIN_SIM_STEP = 1.0 / 30.0;

	[RplProp()]
	protected vector m_vPosition;
	[RplProp()]
	protected float m_fHeading;
	[RplProp()]
	protected float m_fInflation;
	[RplProp()]
	protected int m_iPhase;

	[RplProp()]
	protected float m_fBank;

	protected SCR_ChimeraCharacter m_Pilot;
	protected SCR_CompartmentAccessComponent m_Access;
	protected ZW_ParachutePackComponent m_Pack;
	protected IEntity m_Canopy;
	protected ref TraceParam m_Trace;
	protected ref array<IEntity> m_TraceExcluded;
	protected float m_fAge;
	protected float m_fSimTime;
	protected float m_fVisualInflation = 0.08;
	protected float m_fVisualBank;
	protected float m_fSnapshotTime;
	protected float m_fTurn;
	protected float m_fPitch;
	protected float m_fInputAge;
	protected float m_fSpeed;
	protected float m_fSink;
	protected float m_fLandingAge;
	protected bool m_bAttached;
	protected bool m_bReady;

	override void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.INIT | EntityEvent.FRAME);
	}

	override void EOnInit(IEntity owner)
	{
		m_Canopy = owner.GetChildren();
		if (m_Canopy)
			m_Canopy.SetScale(m_fVisualInflation);
	}

	bool StartFlight(SCR_ChimeraCharacter pilot, SCR_CompartmentAccessComponent access, ZW_ParachutePackComponent pack)
	{
		IEntity owner = GetOwner();
		m_Pilot = pilot;
		m_Access = access;
		m_Pack = pack;
		m_TraceExcluded = {owner, pilot};
		m_Trace = new TraceParam();
		m_Trace.ExcludeArray = m_TraceExcluded;
		m_Trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		m_Trace.LayerMask = EPhysicsLayerPresets.Projectile;
		m_vPosition = owner.GetOrigin();
		m_fHeading = owner.GetYawPitchRoll()[0];
		m_fInflation = 0.12;
		m_fSpeed = 6.0;
		m_fSink = Math.Clamp(-pilot.GetCharacterController().GetMovementVelocity()[1], 4, 14);

		BaseCompartmentSlot seat = access.FindFreeAndAccessibleCompartment(owner, ECompartmentType.CARGO);
		if (!seat)
			return false;
		RplComponent pilotReplication = RplComponent.Cast(pilot.FindComponent(RplComponent));
		bool entered;
		if (pilotReplication && !pilotReplication.IsOwner())
			entered = access.MoveInVehicle(owner, ECompartmentType.CARGO, false, seat);
		else
			entered = access.GetInVehicle(owner, seat, true, -1, ECloseDoorAfterActions.INVALID, false);

		if (!entered)
			return false;

		m_bReady = true;
		Replication.BumpMe();
		return true;
	}

	void SetControls(float turn, float pitch)
	{
		if (!m_bAttached || m_iPhase != 0)
			return;

		m_fTurn = Math.Clamp(turn, -1, 1);
		m_fPitch = Math.Clamp(pitch, -1, 1);
		m_fInputAge = 0;
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (Replication.IsServer())
		{
			if (!m_bReady)
				return;

			m_fSimTime += timeSlice;
			if (m_fSimTime >= MIN_SIM_STEP)
			{
				float elapsed = m_fSimTime;
				m_fSimTime = 0;
				if (!Simulate(owner, elapsed))
					return;
			}
		}
		else
		{
			if (m_fInflation <= 0 && m_iPhase == 0)
				return;

			vector position = owner.GetOrigin();
			float blend = Math.Min(timeSlice * 14, 1);
			if (vector.DistanceSq(position, m_vPosition) > 100)
				blend = 1;

			owner.SetOrigin(vector.Lerp(position, m_vPosition, blend));
			vector angles = owner.GetYawPitchRoll();
			angles[0] = angles[0] + NormalizeHeading(m_fHeading - angles[0]) * blend;
			owner.SetYawPitchRoll(angles);
		}

		if (m_Canopy)
		{
			float animationBlend = Math.Min(timeSlice * 14, 1);
			m_fVisualInflation = Math.Lerp(m_fVisualInflation, Math.Max(0.08, m_fInflation), animationBlend);
			m_fVisualBank = Math.Lerp(m_fVisualBank, m_fBank, animationBlend);
			m_Canopy.SetYawPitchRoll(Vector(m_fHeading, 0, m_fVisualBank));
			m_Canopy.SetScale(m_fVisualInflation);
		}
	}

	protected float NormalizeHeading(float degrees)
	{
		return degrees - 360 * Math.Floor((degrees + 180) / 360);
	}

	protected bool Simulate(IEntity owner, float timeSlice)
	{
		m_fAge += timeSlice;
		if (m_iPhase == 1)
			return FinishLanding(owner, timeSlice);

		if (!m_Pilot || !m_Access)
			return FinishFlight(owner);

		if (m_Access.GetVehicle() != owner)
		{
			if (m_bAttached || m_fAge > 2)
				return FinishFlight(owner);
			return true;
		}

		if (!m_bAttached)
		{
			m_bAttached = true;
			if (m_Pack)
				m_Pack.MarkDeployed();
		}

		if (m_fAge > MAX_FLIGHT_TIME || m_Pilot.GetCharacterController().IsDead())
		{
			m_iPhase = 1;
			Replication.BumpMe();
			return true;
		}

		float step = Math.Min(timeSlice, 0.10);
		m_fInputAge += timeSlice;
		if (m_fInputAge > INPUT_TIMEOUT)
		{
			m_fTurn = 0;
			m_fPitch = 0;
		}

		float opening = Math.Clamp(m_fAge / OPEN_TIME, 0.12, 1);
		m_fInflation = opening * opening * (3 - 2 * opening);
		m_fHeading = NormalizeHeading(m_fHeading + 48 * m_fTurn * m_fInflation * step);
		m_fBank = -22 * m_fTurn * m_fInflation;
		float radians = m_fHeading * Math.DEG2RAD;
		float targetSpeed = 7.5 + 3 * m_fPitch;
		float targetSink = 2.8 + 0.8 * m_fPitch + 0.65 * Math.AbsFloat(m_fTurn);
		m_fSpeed = Math.Lerp(m_fSpeed, targetSpeed, Math.Min(2 * step, 1));
		m_fSink = Math.Lerp(m_fSink, targetSink, Math.Min(4 * m_fInflation * step, 1));

		vector next = m_vPosition + Vector(Math.Sin(radians) * m_fSpeed, -m_fSink, Math.Cos(radians) * m_fSpeed) * step;
		BaseWorld world = owner.GetWorld();
		float ground = Math.Max(world.GetSurfaceY(next[0], next[2]), world.GetOceanHeight(next[0], next[2]));

		m_Trace.Start = m_vPosition + "0 0.45 0";
		m_Trace.End = next + "0 0.45 0";
		if (world.TraceMove(m_Trace, null) < 1)
		{
			next[0] = m_vPosition[0];
			next[2] = m_vPosition[2];
			ground = Math.Max(world.GetSurfaceY(next[0], next[2]), world.GetOceanHeight(next[0], next[2]));
		}

		m_Trace.Start = Vector(next[0], m_vPosition[1] + 0.1, next[2]);
		m_Trace.End = Vector(next[0], next[1] - 0.6, next[2]);
		float landing = world.TraceMove(m_Trace, null);
		if (landing < 1)
			ground = Math.Max(ground, Math.Lerp(m_Trace.Start[1], m_Trace.End[1], landing));

		bool touchingSurface = next[1] <= ground + 0.30 || landing < 1;
		if (touchingSurface)
		{
			next[1] = ground + 0.18;
			m_iPhase = 1;
		}

		m_vPosition = next;
		owner.SetOrigin(m_vPosition);
		owner.SetYawPitchRoll(Vector(m_fHeading, 0, 0));

		m_fSnapshotTime += timeSlice;
		if (m_fSnapshotTime >= SNAPSHOT_INTERVAL || touchingSurface)
		{
			m_fSnapshotTime = 0;
			Replication.BumpMe();
		}
		return true;
	}

	protected bool FinishLanding(IEntity owner, float timeSlice)
	{
		m_fLandingAge += timeSlice;
		m_fBank = 0;
		m_fInflation = Math.Max(0.08, 1 - m_fLandingAge / LAND_TIME);
		m_fSnapshotTime += timeSlice;
		if (m_fSnapshotTime >= SNAPSHOT_INTERVAL)
		{
			m_fSnapshotTime = 0;
			Replication.BumpMe();
		}

		if (m_fLandingAge < LAND_TIME)
			return true;

		if (m_fLandingAge >= 2 || !m_Access || m_Access.GetVehicle() != owner)
			return FinishFlight(owner);

		if (m_fLandingAge - timeSlice < LAND_TIME)
			m_Access.AskOwnerToGetOutFromVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.INVALID, false, true);

		return true;
	}

	protected bool FinishFlight(IEntity owner)
	{
		m_bReady = false;
		RplComponent.DeleteRplEntity(owner, false);
		return false;
	}
}
