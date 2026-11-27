[ComponentEditorProps(category: "ZARUBA/Parachute", description: "Marks a ZARUBA parachute backpack.")]
class ZW_ParachutePackComponentClass : ScriptComponentClass
{
}

class ZW_ParachutePackComponent : ScriptComponent
{
	[RplProp()]
	protected bool m_bDeployed;

	bool IsDeployed()
	{
		return m_bDeployed;
	}

	void MarkDeployed()
	{
		m_bDeployed = true;
		Replication.BumpMe();
	}
}
