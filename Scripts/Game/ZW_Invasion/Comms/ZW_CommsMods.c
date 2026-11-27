// ZRBinvaga — сервер назначает общие частоты, командиры меняют их через меню рации.

modded class SCR_VONAutoTune
{
	override bool IsEnabled()
	{
		return !ZW_CommsManagerComponent.IsCommsActive() && super.IsEnabled();
	}
}

modded class SCR_RadioComponent
{
	override bool ChangeFrequencyStep(bool direction)
	{
		return !ZW_CommsManagerComponent.IsCommsActive() && super.ChangeFrequencyStep(direction);
	}
}

modded class SCR_VONEntryRadio
{
	override void AdjustEntryModif(int modifier)
	{
		if (!ZW_CommsManagerComponent.IsCommsActive())
		{
			super.AdjustEntryModif(modifier);
			return;
		}

		BaseTransceiver radio = GetTransceiver();
		if (!radio)
			return;

		if (modifier == 0)
		{
			ZW_RefreshFrequency();
			return;
		}

		int channel = ZW_GetEditableChannel();
		if (!IsUsable() || channel == 0)
			return;

		SCR_PlayerControllerGroupComponent controller = SCR_PlayerControllerGroupComponent.GetPlayerControllerComponent(SCR_PlayerController.GetLocalPlayerId());
		if (controller)
			controller.ZW_RequestRadioFrequency(channel == 2, modifier, radio.GetFrequency());
	}

	override void AdjustEntry(int modifier)
	{
		if (!ZW_CommsManagerComponent.IsCommsActive())
			super.AdjustEntry(modifier);
	}

	int ZW_GetEditableChannel()
	{
		ZW_CommsManagerComponent manager = ZW_CommsManagerComponent.GetActiveInstance();
		BaseTransceiver radio = GetTransceiver();
		if (!manager || !radio)
			return 0;
		return manager.GetEditableChannel(SCR_PlayerController.GetLocalPlayerId(), radio);
	}

	override void SetChannelText(string channel)
	{
		int editable = ZW_GetEditableChannel();
		if (editable == 1)
			channel = "Отрядный";
		else if (editable == 2)
			channel = "Командный";
		super.SetChannelText(channel);
	}

	override void Update()
	{
		BaseTransceiver radio = GetTransceiver();
		if (radio && ZW_CommsManagerComponent.IsCommsActive() && radio.GetFrequency() != m_iFrequency)
		{
			ZW_RefreshFrequency();
			SetChannelText(SCR_VONMenu.GetKnownChannel(m_iFrequency));
		}
		super.Update();
	}

	protected void ZW_RefreshFrequency()
	{
		m_iFrequency = GetTransceiver().GetFrequency();
		float frequencyMHz = m_iFrequency * 0.001;
		m_sText = frequencyMHz.ToString(3, 3) + " " + LABEL_FREQUENCY_UNITS;
	}
}

modded class SCR_VONRadialDisplay
{
	override void UpdateFrequencyList(notnull SCR_VONEntryRadio entry)
	{
		if (ZW_CommsManagerComponent.IsCommsActive())
		{
			SetFrequenciesVisible(false);
			return;
		}
		super.UpdateFrequencyList(entry);
	}
}

modded class SCR_PlayerControllerGroupComponent
{
	void ZW_RequestRadioFrequency(bool command, int direction, int expectedFrequency)
	{
		Rpc(ZW_RpcAskRadioFrequency, GetGroupID(), command, direction, expectedFrequency);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void ZW_RpcAskRadioFrequency(int groupId, bool command, int direction, int expectedFrequency)
	{
		ZW_CommsManagerComponent manager = ZW_CommsManagerComponent.GetActiveInstance();
		if (!manager || !Replication.IsServer())
			return;

		if (!manager.RotateFrequency(GetPlayerID(), groupId, command, direction, expectedFrequency))
			Rpc(ZW_RpcShowRadioFrequencyUnavailable);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void ZW_RpcShowRadioFrequencyUnavailable()
	{
		SCR_HintManagerComponent.ShowCustomHint("Не удалось сменить частоту. Проверьте канал или попробуйте другое направление.", "Радиосвязь", 4);
	}

	override void RPC_AskSetFrequency(int groupID, int frequency)
	{
		if (!ZW_CommsManagerComponent.IsCommsActive())
			super.RPC_AskSetFrequency(groupID, frequency);
	}
}
