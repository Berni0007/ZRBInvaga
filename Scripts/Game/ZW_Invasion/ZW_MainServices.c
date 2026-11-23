modded class SCR_EntitySpawnerSlotComponent
{
	//------------------------------------------------------------------------------------------------
	override protected bool TraceCallback(IEntity ent)
	{
		// Баллистический барьер мейна не занимает место выдачи техники.
		if (ZW_MainRoleComponent.IsMainProjectileBarrier(ent))
			return false;

		return super.TraceCallback(ent);
	}
}

modded class SCR_ResourceConsumerServicePoint
{
	//------------------------------------------------------------------------------------------------
	override void Initialize(notnull IEntity owner)
	{
		super.Initialize(owner);

		if (GetResourceType() == EResourceType.SUPPLIES && ZW_MainRoleComponent.IsInMainSafeZone(owner))
			GetGame().GetCallqueue().CallLater(ZW_ConnectMainSupply, 500);
	}

	//------------------------------------------------------------------------------------------------
	protected void ZW_ConnectMainSupply()
	{
		IEntity depot = ZW_MainRoleComponent.FindMainDepot(GetOwner());
		if (!depot)
			return;

		SCR_ResourceComponent resources = SCR_ResourceComponent.Cast(depot.FindComponent(SCR_ResourceComponent));
		if (!resources)
			return;

		SCR_ResourceContainer container = resources.GetContainer(EResourceType.SUPPLIES);
		if (container && !container.IsInteractorLinked(this))
			RegisterContainer(container);
	}

	//------------------------------------------------------------------------------------------------
	override void Clear()
	{
		GetGame().GetCallqueue().Remove(ZW_ConnectMainSupply);
		super.Clear();
	}
}
