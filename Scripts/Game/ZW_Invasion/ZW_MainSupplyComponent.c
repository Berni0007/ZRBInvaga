[ComponentEditorProps(category: "ZRB/Invasion", description: "Пополнение склада мейна")]
class ZW_MainSupplyComponentClass : ScriptComponentClass
{
}

class ZW_MainSupplyComponent : ScriptComponent
{
	[Attribute("1", UIWidgets.CheckBox, "Пополнять склад мейна до полной ёмкости", category: "Invasion")]
	protected bool m_bRefill;

	protected SCR_ResourceComponent m_Resources;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!m_bRefill || !Replication.IsServer() || SCR_Global.IsEditMode())
			return;

		m_Resources = SCR_ResourceComponent.Cast(owner.FindComponent(SCR_ResourceComponent));
		if (m_Resources)
			GetGame().GetCallqueue().CallLater(Refill, 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void Refill()
	{
		SCR_ResourceContainer stock = m_Resources.GetContainer(EResourceType.SUPPLIES);
		if (stock && stock.GetResourceValue() < stock.GetMaxResourceValue())
			stock.MaxOutResourceValue();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (m_Resources)
			GetGame().GetCallqueue().Remove(Refill);

		super.OnDelete(owner);
	}
}
