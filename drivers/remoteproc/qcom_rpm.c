// SPDX-License-Identifier: GPL-2.0-only

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/rpmsg/qcom_smd.h>

static int rpm_proc_probe(struct platform_device *pdev)
{
	struct qcom_smd_edge *edge = NULL;
	struct device *dev = &pdev->dev;
	struct device_node *edge_node;
	struct reset_control *reset;
	int ret;

	reset = devm_reset_control_get_optional_exclusive(dev, NULL);
	if (IS_ERR(reset))
		return dev_err_probe(dev, PTR_ERR(reset), "Failed to get reset\n");

	ret = reset_control_deassert(reset);
	if (ret) {
		dev_err(dev, "Failed to release RPM from reset: %d", ret);
		return ret;
	}

	edge_node = of_get_child_by_name(dev->of_node, "smd-edge");
	if (!edge_node)
		return 0;

	edge = qcom_smd_register_edge(dev, edge_node);
	if (IS_ERR(edge))
		return dev_err_probe(dev, PTR_ERR(edge),
				     "Failed to register smd-edge\n");

	platform_set_drvdata(pdev, edge);
	return 0;
}

static int rpm_proc_remove(struct platform_device *pdev)
{
	struct qcom_smd_edge *edge = platform_get_drvdata(pdev);
	int ret = 0;

	if (edge)
		ret = qcom_smd_unregister_edge(edge);
	return ret;
}

static const struct of_device_id rpm_proc_of_match[] = {
	{ .compatible = "qcom,rpm-proc", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rpm_proc_of_match);

static struct platform_driver rpm_proc_driver = {
	.probe = rpm_proc_probe,
	.remove = rpm_proc_remove,
	.driver = {
		.name = "qcom-rpm-proc",
		.of_match_table = of_match_ptr(rpm_proc_of_match),
	},
};
module_platform_driver(rpm_proc_driver);

MODULE_DESCRIPTION("Qualcomm RPM processor driver");
MODULE_AUTHOR("Stephan Gerhold <stephan@gerhold.net>");
MODULE_LICENSE("GPL v2");
