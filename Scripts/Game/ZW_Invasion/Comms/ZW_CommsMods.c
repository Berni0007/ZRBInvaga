// ZRBinvaga — при связи Invasion частоты ставит сервер (ZW_CommsManagerComponent).
// Штатная автонастройка и ручная настройка частот отключены.

modded class SCR_VONAutoTune
{
	override bool IsEnabled()
	{
		if (ZW_CommsManagerComponent.IsCommsActive())
			return false;

		return super.IsEnabled();
	}
}

modded class SCR_RadioComponent
{
	override bool ChangeFrequencyStep(bool direction)
	{
		if (ZW_CommsManagerComponent.IsCommsActive())
			return false;

		return super.ChangeFrequencyStep(direction);
	}
}

modded class SCR_VONEntryRadio
{
	override void AdjustEntryModif(int modifier)
	{
		if (modifier != 0 && ZW_CommsManagerComponent.IsCommsActive())
			return;

		super.AdjustEntryModif(modifier);
	}

	override void AdjustEntry(int modifier)
	{
		if (ZW_CommsManagerComponent.IsCommsActive())
			return;

		super.AdjustEntry(modifier);
	}
}
