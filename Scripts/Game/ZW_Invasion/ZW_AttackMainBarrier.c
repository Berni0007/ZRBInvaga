// Невидимая физическая граница на время подготовки.
class ZW_AttackMainBarrier
{
	protected const int PANEL_COUNT = 32;
	protected const float PANEL_THICKNESS = 1.5;
	protected const float VERTICAL_CLEARANCE = 50;
	protected const ResourceName MATERIAL = "{258FA8B4D29FC807}Assets/ZW_Invasion/ZW_MainProjectileBarrier.gamemat";
	protected const ResourceName MODEL_MATERIAL = "{3C10D749A4E84B38}Common/Materials/Default.emat";

	protected ref Resource m_Material;
	protected ref array<IEntity> m_aColliders = {};

	//------------------------------------------------------------------------------------------------
	bool Create(notnull BaseWorld world, vector center, float radius)
	{
		m_Material = Resource.Load(MATERIAL);
		if (!m_Material || !m_Material.IsValid())
			return false;

		float step = 360.0 / PANEL_COUNT;
		float halfStep = step * Math.DEG2RAD * 0.5;
		float cornerRadius = radius / Math.Cos(halfStep);
		float panelWidth = 2 * radius * Math.Tan(halfStep) + PANEL_THICKNESS;
		float bottom = center[1];
		float top = center[1];
		for (int i = 0; i < PANEL_COUNT; i++)
		{
			vector corner = center + RadialOffset((i + 0.5) * step, cornerRadius);
			float height = world.GetSurfaceY(corner[0], corner[2]);
			bottom = Math.Min(bottom, height);
			top = Math.Max(top, height);
		}

		bottom -= VERTICAL_CLEARANCE;
		top += VERTICAL_CLEARANCE;
		vector panelSize = Vector(panelWidth, top - bottom, PANEL_THICKNESS);
		for (int i = 0; i < PANEL_COUNT; i++)
		{
			vector position = center + RadialOffset(i * step, radius);
			position[1] = (top + bottom) * 0.5;
			if (!AddCollider(world, position, i * step, panelSize, PhysicsGeom.CreateBox(panelSize)))
			{
				Clear();
				return false;
			}
		}

		// Верхняя плоскость закрывает границу и для вертолётов.
		vector roof = center;
		roof[1] = top;
		float roofRadius = cornerRadius + PANEL_THICKNESS;
		float diameter = 2 * roofRadius;
		if (!AddCollider(world, roof, 0, Vector(diameter, PANEL_THICKNESS, diameter), PhysicsGeom.CreateCylinder(roofRadius, PANEL_THICKNESS)))
		{
			Clear();
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected vector RadialOffset(float angle, float radius)
	{
		float radians = angle * Math.DEG2RAD;
		return Vector(Math.Sin(radians) * radius, 0, Math.Cos(radians) * radius);
	}

	//------------------------------------------------------------------------------------------------
	protected bool AddCollider(BaseWorld world, vector position, float yaw, vector size, PhysicsGeom geometry)
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), params.Transform);
		params.Transform[3] = position;

		IEntity wall = GetGame().SpawnEntity(GenericEntity, world, params);
		if (!wall)
		{
			geometry.Destroy();
			return false;
		}

		m_aColliders.Insert(wall);
		if (!SetBounds(wall, size))
		{
			geometry.Destroy();
			return false;
		}

		wall.ClearFlags(EntityFlags.VISIBLE, false);
		wall.SetFlags(EntityFlags.TRACEABLE, false);
		ref PhysicsGeomDef geoms[1] = {
			new PhysicsGeomDef("PrepBoundary", geometry, MATERIAL, EPhysicsLayerDefs.Static)
		};
		if (!Physics.CreateStaticEx(wall, geoms))
			return false;

		wall.SetFlags(EntityFlags.UPDATE_LINK, false);
		wall.Update();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Движку нужны габариты модели для поиска коллизии вдали от центра панели.
	protected bool SetBounds(IEntity wall, vector size)
	{
		vector half = size * 0.5;
		vector verts[8] = {
			Vector(-half[0], -half[1], -half[2]), Vector(half[0], -half[1], -half[2]),
			Vector(half[0], -half[1], half[2]), Vector(-half[0], -half[1], half[2]),
			Vector(-half[0], half[1], -half[2]), Vector(half[0], half[1], -half[2]),
			Vector(half[0], half[1], half[2]), Vector(-half[0], half[1], half[2])
		};
		// Верх и низ задают полный объём. Сама модель скрыта.
		int indices[12] = {0, 1, 2, 2, 3, 0, 4, 7, 6, 6, 5, 4};
		float uvs[16];
		int vertexCount[1] = {8};
		int indexCount[1] = {12};
		string materials[1] = {MODEL_MATERIAL};
		Resource resource = MeshObject.Create(1, vertexCount, indexCount, materials, 0);
		if (!resource || !resource.IsValid())
			return false;

		MeshObject mesh = resource.GetResource().ToMeshObject();
		if (!mesh)
			return false;

		mesh.UpdateVerts(0, verts, uvs);
		mesh.UpdateIndices(0, indices);
		wall.SetObject(mesh, string.Empty);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void Clear()
	{
		foreach (IEntity wall : m_aColliders)
		{
			if (wall)
				SCR_EntityHelper.DeleteEntityAndChildren(wall);
		}
		m_aColliders.Clear();
		m_Material = null;
	}
}
