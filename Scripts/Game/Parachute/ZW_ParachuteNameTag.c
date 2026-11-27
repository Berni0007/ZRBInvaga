modded class SCR_NameTagData
{
	override void OnVehicleEntered(IEntity vehicle, BaseCompartmentManagerComponent manager, int mgrID, int slotID)
	{
		// The harness has no vehicle skeleton. Keep the passenger's character tag.
		if (vehicle && vehicle.FindComponent(ZW_ParachuteFlightComponent))
			return;

		super.OnVehicleEntered(vehicle, manager, mgrID, slotID);
	}
}
