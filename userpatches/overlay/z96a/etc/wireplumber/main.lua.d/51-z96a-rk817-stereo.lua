-- Z96A RK817: force stereo + prefer Speakers.
-- IMPORTANT: append to alsa_monitor.rules — never replace the table
-- (replacing drops WirePlumber defaults and other 51-*.lua rules).

alsa_monitor.rules = alsa_monitor.rules or {}

table.insert(alsa_monitor.rules, {
	matches = {
		{
			{ "node.name", "matches", "alsa_output.platform-rk817-sound*" },
		},
	},
	apply_properties = {
		-- rockchip I2S advertises up to 8ch; analog path is 2ch only.
		-- Without this, PipeWire opens aux0..aux7 and GNOME stereo test is silent.
		["audio.channels"] = 2,
		["audio.position"] = "FL,FR",
		["api.alsa.pcm.channels"] = 2,
		["channelmix.upmix"] = false,
		["priority.session"] = 2000,
		["priority.driver"] = 2000,
		["node.nick"] = "Speakers",
	},
})

table.insert(alsa_monitor.rules, {
	matches = {
		{
			{ "node.name", "matches", "alsa_input.platform-rk817-sound*" },
		},
	},
	apply_properties = {
		["audio.channels"] = 2,
		["audio.position"] = "FL,FR",
		["api.alsa.pcm.channels"] = 2,
	},
})

table.insert(alsa_monitor.rules, {
	matches = {
		{
			{ "node.name", "matches", "alsa_output*hdmi*" },
		},
	},
	apply_properties = {
		["priority.session"] = 100,
		["priority.driver"] = 100,
	},
})
