modded class SCR_CharacterControllerComponent
{
	//------------------------------------------------------------------------------------------------
	override void OnPrepareControls(IEntity owner, ActionManager am, float dt, bool player)
	{
		if (ZW_MainRoleComponent.IsInMainSafeZone(owner))
		{
			if (player && am)
				am.SetActionValue("CharacterFire", 0);

			SetFireWeaponWanted(false);
		}

		super.OnPrepareControls(owner, am, dt, player);
	}
}

modded class SCR_TurretControllerComponent
{
	//------------------------------------------------------------------------------------------------
	override void OnPrepareControls(IEntity owner, ActionManager am, float dt, bool player)
	{
		if (ZW_MainRoleComponent.IsInMainSafeZone(owner))
		{
			if (player && am)
				am.SetActionValue("TurretFire", 0);

			SetFireWeaponWanted(false);
		}

		super.OnPrepareControls(owner, am, dt, player);
	}
}

modded class SCR_HelicopterControllerComponent
{
	//------------------------------------------------------------------------------------------------
	override protected event void OnPrepareControls()
	{
		if (ZW_MainRoleComponent.IsInMainSafeZone(GetOwner()))
			GetGame().GetInputManager().SetActionValue("HelicopterFire", 0);

		super.OnPrepareControls();
	}
}

modded class SCR_CharacterDamageManagerComponent
{
	override bool HijackDamageHandling(notnull BaseDamageContext damageContext)
	{
		// Взрыв и осколки могут прийти без сущности снаряда или определённого стрелка.
		if (damageContext.damageValue > 0 && ZW_MainRoleComponent.IsInMainSafeZone(GetOwner()))
			return true;

		return super.HijackDamageHandling(damageContext);
	}
}

modded class SCR_VehicleDamageManagerComponent
{
	override bool HijackDamageHandling(notnull BaseDamageContext damageContext)
	{
		// Взрыв техники и выброс экипажа тоже могут привести к гибели игроков.
		if (damageContext.damageValue > 0 && ZW_MainRoleComponent.IsInMainSafeZone(GetOwner()))
			return true;

		return super.HijackDamageHandling(damageContext);
	}
}
