modded class SCR_PlayerController
{
	protected const ResourceName ZW_PARACHUTE_FLIGHT = "{FDB3456320142CAD}Prefabs/Parachute/ZW_ParachuteFlight.et";
	protected const float ZW_PARACHUTE_MIN_HEIGHT = 18;
	protected float m_fZWParachuteInputTime;
	protected float m_fZWParachuteNextRequest;
	protected IEntity m_ZWParachuteFlight;

	override void OnUpdate(float timeSlice)
	{
		super.OnUpdate(timeSlice);
		if (GetGame().GetPlayerController() != this)
			return;

		SCR_ChimeraCharacter pilot = SCR_ChimeraCharacter.Cast(GetControlledEntity());
		if (!pilot)
			return;

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(pilot.FindComponent(SCR_CompartmentAccessComponent));
		if (!access)
			return;

		InputManager input = GetGame().GetInputManager();
		IEntity vehicle = access.GetVehicle();
		if (vehicle && vehicle.FindComponent(ZW_ParachuteFlightComponent))
		{
			input.ActivateContext("CharacterMovementContext");
			m_fZWParachuteInputTime += timeSlice;
			if (m_fZWParachuteInputTime >= 0.10)
			{
				m_fZWParachuteInputTime = 0;
				Rpc(RpcAsk_ZWParachuteControl, input.GetActionValue("CharacterRight"), input.GetActionValue("CharacterForward"));
			}
			return;
		}

		if (!vehicle && pilot.GetCharacterController().IsFalling() && input.GetActionTriggered("CharacterJump"))
			Rpc(RpcAsk_ZWParachuteDeploy);
	}

	// Scenario add-ons can disable deployment during their protected phases.
	bool ZW_CanDeployParachute(SCR_ChimeraCharacter pilot)
	{
		return true;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_ZWParachuteDeploy()
	{
		float now = GetGame().GetWorld().GetWorldTime();
		if (m_ZWParachuteFlight || now < m_fZWParachuteNextRequest)
			return;
		m_fZWParachuteNextRequest = now + 750;

		SCR_ChimeraCharacter pilot = SCR_ChimeraCharacter.Cast(GetControlledEntity());
		if (!pilot)
			return;

		CharacterControllerComponent controller = pilot.GetCharacterController();
		if (controller.IsDead() || controller.IsUnconscious() || !controller.IsFalling())
			return;

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(pilot.FindComponent(SCR_CompartmentAccessComponent));
		if (!access || access.IsInCompartment() || !ZW_CanDeployParachute(pilot))
			return;

		EquipedLoadoutStorageComponent loadout = EquipedLoadoutStorageComponent.Cast(pilot.FindComponent(EquipedLoadoutStorageComponent));
		if (!loadout)
			return;

		IEntity backpack = loadout.GetClothFromArea(LoadoutBackpackArea);
		if (!backpack)
			return;

		ZW_ParachutePackComponent pack = ZW_ParachutePackComponent.Cast(backpack.FindComponent(ZW_ParachutePackComponent));
		if (!pack || pack.IsDeployed())
			return;

		vector position = pilot.GetOrigin();
		BaseWorld world = pilot.GetWorld();
		if (position[1] - Math.Max(world.GetSurfaceY(position[0], position[2]), world.GetOceanHeight(position[0], position[2])) < ZW_PARACHUTE_MIN_HEIGHT)
			return;

		TraceParam trace = new TraceParam();
		trace.Start = position + "0 -0.1 0";
		trace.End = position - Vector(0, ZW_PARACHUTE_MIN_HEIGHT, 0);
		trace.Exclude = pilot;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		if (world.TraceMove(trace, null) < 1)
			return;

		Resource resource = Resource.Load(ZW_PARACHUTE_FLIGHT);
		if (!resource || !resource.IsValid())
			return;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(Vector(pilot.GetYawPitchRoll()[0], 0, 0), params.Transform);
		params.Transform[3] = position;

		IEntity flight = GetGame().SpawnEntityPrefab(resource, world, params);
		if (!flight)
			return;

		ZW_ParachuteFlightComponent simulation = ZW_ParachuteFlightComponent.Cast(flight.FindComponent(ZW_ParachuteFlightComponent));
		if (!simulation || !simulation.StartFlight(pilot, access, pack))
		{
			RplComponent.DeleteRplEntity(flight, false);
			return;
		}
		m_ZWParachuteFlight = flight;
	}

	[RplRpc(RplChannel.Unreliable, RplRcver.Server)]
	protected void RpcAsk_ZWParachuteControl(float turn, float pitch)
	{
		// Reject values outside the input manager's range, including NaN.
		if (!(turn >= -1 && turn <= 1 && pitch >= -1 && pitch <= 1))
			return;

		SCR_ChimeraCharacter pilot = SCR_ChimeraCharacter.Cast(GetControlledEntity());
		if (!pilot || pilot.GetCharacterController().IsUnconscious() || pilot.GetCharacterController().IsDead())
			return;
		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(pilot.FindComponent(SCR_CompartmentAccessComponent));
		if (!access)
			return;
		IEntity vehicle = access.GetVehicle();
		if (!vehicle)
			return;
		ZW_ParachuteFlightComponent simulation = ZW_ParachuteFlightComponent.Cast(vehicle.FindComponent(ZW_ParachuteFlightComponent));
		if (simulation)
			simulation.SetControls(turn, pitch);
	}
}
