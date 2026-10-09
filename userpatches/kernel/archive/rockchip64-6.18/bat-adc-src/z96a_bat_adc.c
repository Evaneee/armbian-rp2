// SPDX-License-Identifier: GPL-2.0
/*
 * Z96A battery voltage via SoC SARADC ch1 (vendor rk817,battery ADC path).
 * Vendor scale: pack_mV = raw * ref_mV * ratio / res / 1000
 *   ref=1800, res=1024, ratio=4900  → 2S pack ~6.6–8.4 V
 *
 * DC plug detect: vendor dc_det on GPIO3_PA5 (ACTIVE_LOW), IRQ + short debounce
 * so the desktop charging icon updates quickly. Capacity stays slow/hysteresis
 * so UPower does not invent huge Wh rates.
 *
 * NOTE: power_supply_am_i_supplied() returns -ENODEV when no supplier is
 * registered — that must NOT be treated as "charging" (boolean !err is wrong).
 */
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/iio/consumer.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>

#define Z96A_ADC_REF_MV		1800
#define Z96A_ADC_RES		1024
#define Z96A_ADC_RATIO		4900	/* pack_mV / pin_mV * 1000 */
#define Z96A_DESIGN_CAP_MAH	5125
#define Z96A_POLL_MS		5000	/* capacity / voltage poll */
#define Z96A_DC_DEBOUNCE_MS	50	/* DC-det edge settle */
#define Z96A_FULL_PCT		98
#define Z96A_CAP_HYST		2	/* only move capacity by >= this many % */

/* Vendor ocv_table (mV pack), 0% … 100% in 5% steps (21 points). */
static const int z96a_ocv_mv[] = {
	6600, 6840, 6922, 7000, 7064, 7122, 7162, 7190, 7218, 7250,
	7292, 7364, 7448, 7532, 7618, 7714, 7812, 7918, 8030, 8158, 8362
};

struct z96a_bat {
	struct device *dev;
	struct iio_channel *chan;
	struct gpio_desc *dc_det;
	struct power_supply *psy;
	struct delayed_work work;
	struct delayed_work dc_work;
	int voltage_uv;
	int capacity;
	int status;
	int dc_online;		/* 1 plugged, 0 unplugged, -1 unknown */
};

static int z96a_read_pack_mv(struct z96a_bat *bat)
{
	int raw, ret, pin_mv, pack_mv;

	ret = iio_read_channel_raw(bat->chan, &raw);
	if (ret < 0)
		return ret;

	pin_mv = (Z96A_ADC_REF_MV * raw) / Z96A_ADC_RES;
	pack_mv = pin_mv * Z96A_ADC_RATIO / 1000;
	return pack_mv;
}

static int z96a_mv_to_capacity(int mv)
{
	int i, n = ARRAY_SIZE(z96a_ocv_mv);
	int pct;

	if (mv <= z96a_ocv_mv[0])
		return 0;
	if (mv >= z96a_ocv_mv[n - 1])
		return 100;

	for (i = 0; i < n - 1; i++) {
		if (mv <= z96a_ocv_mv[i + 1]) {
			int lo = z96a_ocv_mv[i];
			int hi = z96a_ocv_mv[i + 1];
			int base = i * 5;

			if (hi == lo)
				return base;
			pct = base + (mv - lo) * 5 / (hi - lo);
			if (pct < 0)
				return 0;
			if (pct > 100)
				return 100;
			return pct;
		}
	}
	return 100;
}

static bool z96a_dc_raw(struct z96a_bat *bat)
{
	int ret;

	if (bat->dc_det)
		return gpiod_get_value_cansleep(bat->dc_det) > 0;

	ret = power_supply_am_i_supplied(bat->psy);
	return ret > 0;
}

static int z96a_cap_smooth(struct z96a_bat *bat, int raw_pct, bool online)
{
	int cur = bat->capacity;

	if (cur < 0)
		return raw_pct;

	if (online && raw_pct < cur)
		return cur;
	if (!online && raw_pct > cur)
		return cur;
	if (abs(raw_pct - cur) < Z96A_CAP_HYST)
		return cur;
	return raw_pct;
}

static void z96a_apply_status(struct z96a_bat *bat, bool online, bool update_cap)
{
	int old_status = bat->status;
	int old_cap = bat->capacity;

	if (update_cap) {
		int mv = z96a_read_pack_mv(bat);

		if (mv >= 0) {
			bat->voltage_uv = mv * 1000;
			bat->capacity = z96a_cap_smooth(bat, z96a_mv_to_capacity(mv),
							 online);
		}
	}

	bat->dc_online = online ? 1 : 0;
	if (online) {
		if (bat->capacity >= Z96A_FULL_PCT)
			bat->status = POWER_SUPPLY_STATUS_FULL;
		else
			bat->status = POWER_SUPPLY_STATUS_CHARGING;
	} else {
		bat->status = POWER_SUPPLY_STATUS_DISCHARGING;
	}

	if (bat->status != old_status || bat->capacity != old_cap)
		power_supply_changed(bat->psy);
}

static void z96a_bat_update(struct z96a_bat *bat)
{
	bool online = (bat->dc_online >= 0) ? bat->dc_online > 0 : z96a_dc_raw(bat);

	/* Refresh DC from GPIO each poll as a fallback if IRQ missed. */
	if (bat->dc_det)
		online = z96a_dc_raw(bat);

	z96a_apply_status(bat, online, true);
}

static void z96a_bat_work(struct work_struct *work)
{
	struct z96a_bat *bat = container_of(to_delayed_work(work),
					    struct z96a_bat, work);

	z96a_bat_update(bat);
	schedule_delayed_work(&bat->work, msecs_to_jiffies(Z96A_POLL_MS));
}

static void z96a_dc_work(struct work_struct *work)
{
	struct z96a_bat *bat = container_of(to_delayed_work(work),
					    struct z96a_bat, dc_work);
	bool online = z96a_dc_raw(bat);

	/* Status-only update — do not touch capacity (avoids UPower rate spikes). */
	z96a_apply_status(bat, online, false);
}

static irqreturn_t z96a_dc_irq(int irq, void *data)
{
	struct z96a_bat *bat = data;

	mod_delayed_work(system_wq, &bat->dc_work,
			 msecs_to_jiffies(Z96A_DC_DEBOUNCE_MS));
	return IRQ_HANDLED;
}

static int z96a_bat_get_property(struct power_supply *psy,
				 enum power_supply_property psp,
				 union power_supply_propval *val)
{
	struct z96a_bat *bat = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = bat->status;
		return 0;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = bat->capacity;
		return 0;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = bat->voltage_uv;
		return 0;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = 8350000;
		return 0;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = 6600000;
		return 0;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = Z96A_DESIGN_CAP_MAH * 1000;
		return 0;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		return 0;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = bat->voltage_uv > 5000000;
		return 0;
	case POWER_SUPPLY_PROP_SCOPE:
		val->intval = POWER_SUPPLY_SCOPE_SYSTEM;
		return 0;
	default:
		return -EINVAL;
	}
}

static enum power_supply_property z96a_bat_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_SCOPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
};

static int z96a_bat_probe(struct platform_device *pdev)
{
	struct z96a_bat *bat;
	struct power_supply_desc *desc;
	struct power_supply_config psy_cfg = {};
	int mv, irq, ret;

	bat = devm_kzalloc(&pdev->dev, sizeof(*bat), GFP_KERNEL);
	if (!bat)
		return -ENOMEM;

	bat->dev = &pdev->dev;
	bat->status = POWER_SUPPLY_STATUS_DISCHARGING;
	bat->capacity = -1;
	bat->dc_online = -1;

	bat->chan = devm_iio_channel_get(&pdev->dev, "battery-voltage");
	if (IS_ERR(bat->chan))
		return dev_err_probe(&pdev->dev, PTR_ERR(bat->chan),
				     "need io-channels = <&saradc 1> as battery-voltage\n");

	bat->dc_det = devm_gpiod_get_optional(&pdev->dev, "dc-det", GPIOD_IN);
	if (IS_ERR(bat->dc_det))
		return dev_err_probe(&pdev->dev, PTR_ERR(bat->dc_det),
				     "dc-det GPIO\n");

	mv = z96a_read_pack_mv(bat);
	if (mv < 0)
		return dev_err_probe(&pdev->dev, mv, "initial ADC read failed\n");

	bat->voltage_uv = mv * 1000;
	bat->capacity = z96a_mv_to_capacity(mv);

	desc = devm_kzalloc(&pdev->dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	desc->name = "z96a-battery";
	desc->type = POWER_SUPPLY_TYPE_BATTERY;
	desc->properties = z96a_bat_props;
	desc->num_properties = ARRAY_SIZE(z96a_bat_props);
	desc->get_property = z96a_bat_get_property;

	psy_cfg.drv_data = bat;
	psy_cfg.fwnode = dev_fwnode(&pdev->dev);

	bat->psy = devm_power_supply_register(&pdev->dev, desc, &psy_cfg);
	if (IS_ERR(bat->psy))
		return dev_err_probe(&pdev->dev, PTR_ERR(bat->psy),
				     "power_supply register failed\n");

	INIT_DELAYED_WORK(&bat->work, z96a_bat_work);
	INIT_DELAYED_WORK(&bat->dc_work, z96a_dc_work);
	platform_set_drvdata(pdev, bat);

	if (bat->dc_det) {
		bool online = z96a_dc_raw(bat);

		bat->dc_online = online;
		if (online) {
			if (bat->capacity >= Z96A_FULL_PCT)
				bat->status = POWER_SUPPLY_STATUS_FULL;
			else
				bat->status = POWER_SUPPLY_STATUS_CHARGING;
		}

		irq = gpiod_to_irq(bat->dc_det);
		if (irq > 0) {
			ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
							z96a_dc_irq,
							IRQF_TRIGGER_RISING |
							IRQF_TRIGGER_FALLING |
							IRQF_ONESHOT,
							"z96a-dc-det", bat);
			if (ret)
				dev_warn(&pdev->dev,
					 "dc-det IRQ failed (%d); poll only\n",
					 ret);
			else
				dev_info(&pdev->dev, "dc-det IRQ on gpio\n");
		}
	}

	schedule_delayed_work(&bat->work, msecs_to_jiffies(0));

	dev_info(&pdev->dev, "SARADC battery: %d mV, %d%%, dc-det=%s\n",
		 mv, bat->capacity, bat->dc_det ? "gpio" : "psy/none");
	return 0;
}

static void z96a_bat_remove(struct platform_device *pdev)
{
	struct z96a_bat *bat = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&bat->work);
	cancel_delayed_work_sync(&bat->dc_work);
}

static const struct of_device_id z96a_bat_of_match[] = {
	{ .compatible = "z96a,bat-adc" },
	{ }
};
MODULE_DEVICE_TABLE(of, z96a_bat_of_match);

static struct platform_driver z96a_bat_driver = {
	.probe = z96a_bat_probe,
	.remove = z96a_bat_remove,
	.driver = {
		.name = "z96a-bat-adc",
		.of_match_table = z96a_bat_of_match,
	},
};
module_platform_driver(z96a_bat_driver);

MODULE_AUTHOR("Z96A");
MODULE_DESCRIPTION("Z96A SARADC battery gauge (vendor ADC path)");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS("IIO_CONSUMER");
