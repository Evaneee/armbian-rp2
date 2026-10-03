-- Prefer RK817 laptop speakers over HDMI when PipeWire picks a default sink.
alsa_monitor.rules = {
	{
		matches = {
			{
				{ "node.name", "matches", "alsa_output*rk817*" },
			},
		},
		apply_properties = {
			["priority.session"] = 2000,
			["priority.driver"] = 2000,
			["node.nick"] = "Speakers",
		},
	},
	{
		matches = {
			{
				{ "node.name", "matches", "alsa_output*hdmi*" },
			},
		},
		apply_properties = {
			["priority.session"] = 100,
			["priority.driver"] = 100,
		},
	},
}
