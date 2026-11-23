modded class SCR_CampaignBuildingPlacingObstructionEditorComponent
{
	//------------------------------------------------------------------------------------------------
	override protected bool CheckVehiclePosition()
	{
		if (!ZW_MainRoleComponent.IsInMainSafeZone(m_PreviewEnt))
			return super.CheckVehiclePosition();

		BaseWorld world = m_PreviewEnt.GetWorld();
		vector transform[4];
		m_PreviewEnt.GetTransform(transform);

		vector boundsMin, boundsMax;
		m_PreviewEnt.GetPreviewBounds(boundsMin, boundsMax);
		boundsMin[1] = HEIGHT_ABOVE_GROUND_VEHICLE_BUFFER;
		boundsMin[0] = boundsMin[0] * BOUNDING_BOX_VEHICLE_FACTOR;
		boundsMin[2] = boundsMin[2] * BOUNDING_BOX_VEHICLE_FACTOR;

		if (transform[3][1] < world.GetOceanBaseHeight() + SEA_LEVEL_OFFSET || ChimeraWorldUtils.TryGetWaterSurfaceSimple(world, transform[3]))
			return false;

		if (!world.QueryEntitiesByOBB(boundsMin, boundsMax, transform, EvaluateBlockingEntity))
			return false;

		TraceOBB trace = new TraceOBB();
		trace.Mat[0] = transform[0];
		trace.Mat[1] = transform[1];
		trace.Mat[2] = transform[2];
		trace.Start = transform[3] + 0.05 * transform[1];
		trace.Flags = TraceFlags.ENTS | TraceFlags.OCEAN | TraceFlags.WORLD;
		trace.Mins = boundsMin;
		trace.Maxs = boundsMax;
		return world.TracePosition(trace, ZW_EvaluateVehicleObstacle) > 0;
	}

	//------------------------------------------------------------------------------------------------
	// В запросе объектов и в трассировке разный смысл возвращаемого значения.
	protected bool ZW_EvaluateVehicleObstacle(IEntity ent)
	{
		return !ZW_MainRoleComponent.IsMainProjectileBarrier(ent) && EvaluateBlockingEntity(ent);
	}

	//------------------------------------------------------------------------------------------------
	override protected bool EvaluatePotentialBlockingEntity(notnull IEntity ent)
	{
		return !ZW_MainRoleComponent.IsMainProjectileBarrier(ent) && super.EvaluatePotentialBlockingEntity(ent);
	}
}
