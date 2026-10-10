-- Z96A: a connected Bluetooth headset/speaker wins over the RK817 speakers
-- (priority.session 2000, see main.lua.d/51-z96a-rk817-stereo.lua), so
-- audio follows the headset on connect and falls back on disconnect.
-- IMPORTANT: append to bluez_monitor.rules — never replace the table.

bluez_monitor.rules = bluez_monitor.rules or {}

table.insert(bluez_monitor.rules, {
	matches = {
		{
			{ "node.name", "matches", "bluez_output.*" },
		},
	},
	apply_properties = {
		["priority.session"] = 3000,
		["priority.driver"] = 3000,
	},
})
