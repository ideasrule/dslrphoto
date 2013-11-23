#include "samples.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * This function looks up a label or key entry of
 * a configuration widget.
 * The functions descend recursively, so you can just
 * specify the last component.
 */

static int
_lookup_widget(CameraWidget*widget, const char *key, CameraWidget **child) {
	int ret;
	ret = gp_widget_get_child_by_name (widget, key, child);
	if (ret < GP_OK)
		ret = gp_widget_get_child_by_label (widget, key, child);
	return ret;
}

/* Gets a string configuration value.
 * This can be:
 *  - A Text widget
 *  - The current selection of a Radio Button choice
 *  - The current selection of a Menu choice
 *
 * Sample (for Canons eg):
 *   get_config_value_string (camera, "owner", &ownerstr, context);
 */
int
get_config_value_string (Camera *camera, const char *key, char **str, GPContext *context) {
	CameraWidget		*widget = NULL, *child = NULL;
	CameraWidgetType	type;
	int			ret;
	char			*val;

	ret = gp_camera_get_config (camera, &widget, context);
	if (ret < GP_OK) {
		fprintf (stderr, "camera_get_config failed: %d\n", ret);
		return ret;
	}
	ret = _lookup_widget (widget, key, &child);
	if (ret < GP_OK) {
		fprintf (stderr, "lookup widget failed: %d\n", ret);
		goto out;
	}

	/* This type check is optional, if you know what type the label
	 * has already. If you are not sure, better check. */
	ret = gp_widget_get_type (child, &type);
	if (ret < GP_OK) {
		fprintf (stderr, "widget get type failed: %d\n", ret);
		goto out;
	}
	switch (type) {
	case GP_WIDGET_MENU:
	case GP_WIDGET_RADIO:
	case GP_WIDGET_TEXT:
		break;
	default:
		fprintf (stderr, "widget has bad type %d\n", type);
		ret = GP_ERROR_BAD_PARAMETERS;
		goto out;
	}

	/* This is the actual query call. Note that we just
	 * a pointer reference to the string, not a copy... */
	ret = gp_widget_get_value (child, &val);
	if (ret < GP_OK) {
		fprintf (stderr, "could not query widget value: %d\n", ret);
		goto out;
	}
	/* Create a new copy for our caller. */
	*str = strdup (val);
	out:
	gp_widget_free (widget);
	return ret;
}


/* Sets a string configuration value.
 * This can set for:
 *  - A Text widget
 *  - The current selection of a Radio Button choice
 *  - The current selection of a Menu choice
 *
 * Sample (for Canons eg):
 *   get_config_value_string (camera, "owner", &ownerstr, context);
 */
int
set_config_value_string (Camera *camera, const char *key, const char *val, GPContext *context) {
	CameraWidget		*widget = NULL, *child = NULL;
	CameraWidgetType	type;
	int			ret;

	ret = gp_camera_get_config (camera, &widget, context);
	if (ret < GP_OK) {
		fprintf (stderr, "camera_get_config failed: %d\n", ret);
		return ret;
	}
	ret = _lookup_widget (widget, key, &child);
	if (ret < GP_OK) {
		fprintf (stderr, "lookup widget failed: %d\n", ret);
		goto out;
	}

	/* This type check is optional, if you know what type the label
	 * has already. If you are not sure, better check. */
	ret = gp_widget_get_type (child, &type);
	if (ret < GP_OK) {
		fprintf (stderr, "widget get type failed: %d\n", ret);
		goto out;
	}
	switch (type) {
	case GP_WIDGET_MENU:
	case GP_WIDGET_RADIO:
	case GP_WIDGET_TEXT:
		break;
	default:
		fprintf (stderr, "widget has bad type %d\n", type);
		ret = GP_ERROR_BAD_PARAMETERS;
		goto out;
	}

	/* This is the actual set call. Note that we keep
	 * ownership of the string and have to free it if necessary.
	 */
	ret = gp_widget_set_value (child, val);
	if (ret < GP_OK) {
		fprintf (stderr, "could not set widget value: %d\n", ret);
		goto out;
	}
	/* This stores it on the camera again */
	ret = gp_camera_set_config (camera, widget, context);
	if (ret < GP_OK) {
		fprintf (stderr, "camera_set_config failed: %d\n", ret);
		return ret;
	}
	out:
	gp_widget_free (widget);
	return ret;
}

/* Sets a configuration value based on index.
 *   get_config_value_string (camera, "iso", 2, context);
 */

int
set_config_value_index (Camera *camera, const char *name, int value,
		GPContext *context) {
	CameraWidget *rootconfig,*child;
	int	ret;
	const char *label;
	CameraWidgetType	type;

	ret = gp_camera_get_config (camera, &rootconfig, context);
	if (ret < GP_OK) {
		fprintf (stderr, "camera_get_config failed: %d\n", ret);
		return ret;
	}

	ret = gp_widget_get_child_by_name(rootconfig, name, &child);
	if (ret < GP_OK)
		ret = gp_widget_get_child_by_label(rootconfig, name, &child);
	if (ret != GP_OK) return ret;

	ret = gp_widget_get_type (child, &type);
	if (ret != GP_OK) {
		gp_widget_free (rootconfig);
		return ret;
	}
	ret = gp_widget_get_label (child, &label);
	if (ret != GP_OK) {
		gp_widget_free (rootconfig);
		return ret;
	}

	switch (type) {
	case GP_WIDGET_MENU:
	case GP_WIDGET_RADIO: { /* char *		*/
		int cnt;

		cnt = gp_widget_count_choices (child);
		if (cnt < GP_OK) {
			ret = cnt;
			break;
		}
		ret = GP_ERROR_BAD_PARAMETERS;
		if ((value>= 0) && (value < cnt)) {
			const char *choice;

			ret = gp_widget_get_choice (child, value, &choice);
			if (ret == GP_OK)
				ret = gp_widget_set_value (child, choice);
			break;
		}
		fprintf(stderr,"Choice %d is out of range (0-%d).", value, cnt-1);
		break;
	}

	/* ignore: */
	case GP_WIDGET_TOGGLE:
	case GP_WIDGET_TEXT:
	case GP_WIDGET_RANGE:
	case GP_WIDGET_DATE:
	case GP_WIDGET_WINDOW:
	case GP_WIDGET_SECTION:
	case GP_WIDGET_BUTTON:
		fprintf(stderr, "The %s widget has no indexed list of "
				"choices. Use set_config_value_string instead.", name);
		ret = GP_ERROR_BAD_PARAMETERS;
		break;
	}
	if (ret == GP_OK) {
		ret = gp_camera_set_config(camera, rootconfig, context);
		if (ret != GP_OK)
			fprintf(stderr, "Failed to set new configuration value %d "
					"for configuration entry %s.", value, name);
	}
	gp_widget_free (rootconfig);
	return (ret);
}

/*
 * This enables/disables the specific canon capture mode.
 * 
 * For non canons this is not required, and will just return
 * with an error (but without negative effects).
 */
int
canon_enable_capture (Camera *camera, int onoff, GPContext *context) {
	CameraWidget		*widget = NULL, *child = NULL;
	CameraWidgetType	type;
	int			ret;

	ret = gp_camera_get_config (camera, &widget, context);
	if (ret < GP_OK) {
		fprintf (stderr, "camera_get_config failed: %d\n", ret);
		return ret;
	}
	ret = _lookup_widget (widget, "capture", &child);
	if (ret < GP_OK) {
		/*fprintf (stderr, "lookup widget failed: %d\n", ret);*/
		goto out;
	}

	ret = gp_widget_get_type (child, &type);
	if (ret < GP_OK) {
		fprintf (stderr, "widget get type failed: %d\n", ret);
		goto out;
	}
	switch (type) {
	case GP_WIDGET_TOGGLE:
		break;
	default:
		fprintf (stderr, "widget has bad type %d\n", type);
		ret = GP_ERROR_BAD_PARAMETERS;
		goto out;
	}
	/* Now set the toggle to the wanted value */
	ret = gp_widget_set_value (child, &onoff);
	if (ret < GP_OK) {
		fprintf (stderr, "toggling Canon capture to %d failed with %d\n", onoff, ret);
		goto out;
	}
	/* OK */
	ret = gp_camera_set_config (camera, widget, context);
	if (ret < GP_OK) {
		fprintf (stderr, "camera_set_config failed: %d\n", ret);
		return ret;
	}
	out:
	gp_widget_free (widget);
	return ret;
}
