// Материал полотна задаётся префабом флагштока.
class ZW_MainFlagComponentClass : SCR_FlagComponentClass
{
}

class ZW_MainFlagComponent : SCR_FlagComponent
{
	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		GetGame().GetCallqueue().CallLater(ApplyFlagMaterial);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyFlagMaterial()
	{
		SCR_FlagComponentClass config = SCR_FlagComponentClass.Cast(GetComponentData(GetOwner()));
		if (!config || !GetFlagEntity())
		{
			Print("[ZW_Invasion] Не удалось найти полотно флага мейна", LogLevel.WARNING);
			return;
		}

		ChangeMaterial(config.GetDefaultMaterial());
	}
}