
#include "../cmos-subdev.h"
#include "../../wmt-vid.h"
#include "gc0309.h"

#define sensor_write_array(sd, arr, sz) \
	cmos_init_8bit_addr(arr, sz, (sd)->i2c_addr)

#define sensor_read(sd, reg) \
	wmt_vid_i2c_read(sd->i2c_addr, reg)

#define sensor_write(sd, reg, val) \
	wmt_vid_i2c_write(sd->i2c_addr, reg, val)

struct cmos_win_size {
	char		*name;
	int		width;
	int		height;
	uint8_t 	*regs;
	size_t		size;
};

#define CMOS_WIN_SIZE(n, w, h, r) \
	{.name = n, .width = w , .height = h, .regs = r, .size = ARRAY_SIZE(r) }

#define FILP_REG_INIT_VALUE	  0x10
#define DELAY_INTERVAL		50
#define RETRY_TIMES              10

static const struct cmos_win_size cmos_supported_win_sizes[] = {
//	CMOS_WIN_SIZE("QVGA", 320, 240, gc0309_320x240),
	CMOS_WIN_SIZE("VGA",  640, 480, gc0309_640x480),
};

static const struct cmos_win_size *cmos_select_win(u32 *width, u32 *height)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cmos_supported_win_sizes); i++) {
		if (cmos_supported_win_sizes[i].width == *width &&
		    cmos_supported_win_sizes[i].height == *height) {
			*width = cmos_supported_win_sizes[i].width;
			*height = cmos_supported_win_sizes[i].height;
			return &cmos_supported_win_sizes[i];
		}
	}
	return NULL;
}

static int sensor_s_wb(struct cmos_subdev *sd, enum v4l2_wb_mode value)
{
	uint8_t *regs;
	size_t size;

	switch (value) {
	case WHITE_BALANCE_AUTO:
		regs = gc0309_wb_auto;
		size = ARRAY_SIZE(gc0309_wb_auto);
		break;
	case WHITE_BALANCE_INCANDESCENCE:
		regs = gc0309_wb_incandescent;
		size = ARRAY_SIZE(gc0309_wb_incandescent);
		break;
	case WHITE_BALANCE_DAYLIGHT:
		regs = gc0309_wb_daylight;
		size = ARRAY_SIZE(gc0309_wb_daylight);
		break;
	case WHITE_BALANCE_CLOUDY:
		regs = gc0309_wb_cloudy;
		size = ARRAY_SIZE(gc0309_wb_cloudy);
		break;
	case WHITE_BALANCE_FLUORESCENT:
		regs = gc0309_wb_fluorescent;
		size = ARRAY_SIZE(gc0309_wb_fluorescent);
		break;
	default:
		return -EINVAL;
	}

	sensor_write_array(sd, regs, size);
	return 0;
}

static int sensor_s_scenemode(struct cmos_subdev *sd, enum v4l2_scene_mode value)
{
	uint8_t *regs;
	size_t size;

	switch (value) {
	case SCENE_MODE_AUTO:
		regs = gc0309_scene_mode_auto;
		size = ARRAY_SIZE(gc0309_scene_mode_auto);
		break;
	case SCENE_MODE_NIGHTSHOT:
		regs = gc0309_scene_mode_night;
		size = ARRAY_SIZE(gc0309_scene_mode_night);
		break;
	default:
		return -EINVAL;
	}

	sensor_write_array(sd, regs, size);
	return 0;
}

static int sensor_s_exposure(struct cmos_subdev *sd, int value)
{
	uint8_t *regs;
	size_t size;

	switch (value) {
	case -2:
		regs = gc0309_exposure_neg6;
		size = ARRAY_SIZE(gc0309_exposure_neg6);
		break;
	case -1:
		regs = gc0309_exposure_neg3;
		size = ARRAY_SIZE(gc0309_exposure_neg3);
		break;
	case 0:
		regs = gc0309_exposure_zero;
		size = ARRAY_SIZE(gc0309_exposure_zero);
		break;
	case 1:
		regs = gc0309_exposure_pos3;
		size = ARRAY_SIZE(gc0309_exposure_pos3);
		break;
	case 2:
		regs = gc0309_exposure_pos6;
		size = ARRAY_SIZE(gc0309_exposure_pos6);
		break;
	default:
		return -EINVAL;
	}

	sensor_write_array(sd, regs, size);
	return 0;
}

static int sensor_s_hflip(struct cmos_subdev *sd, int value)
{
	uint8_t data,tmp=0;
	uint8_t retry_times=0;
	
	sensor_write(sd, 0xfe, 0x00); // set page0

	data = sensor_read(sd, 0x14);

	switch (value) {
	case 0:
		data &= ~0x01;
		break;
	case 1:
		data |= 0x01;
		break;
	default:
		return -EINVAL;
	}
	
	tmp=data | FILP_REG_INIT_VALUE;
	for(retry_times=0;retry_times<RETRY_TIMES;retry_times++){
		sensor_write(sd, 0x14, tmp);
		msleep(50+retry_times*DELAY_INTERVAL);
		if(sensor_read(sd, 0x14)==tmp)
			break;
	}

	return 0;
}

static int sensor_s_vflip(struct cmos_subdev *sd, int value)
{
	uint8_t data,tmp=0;
	uint8_t retry_times=0;

	sensor_write(sd, 0xfe, 0x00); // set page0

	data = sensor_read(sd, 0x14);

	switch (value) {
	case 0:
		data &= ~0x02;
		break;
	case 1:
		data |= 0x02;
		break;
	default:
		return -EINVAL;
	}

	tmp=data | FILP_REG_INIT_VALUE;
	for(retry_times=0;retry_times<RETRY_TIMES;retry_times++){
		sensor_write(sd, 0x14, tmp);
		msleep(50+retry_times*DELAY_INTERVAL);
		if(sensor_read(sd, 0x14)==tmp)
			break;
	}

	return 0;
}

static int sensor_queryctrl(struct cmos_subdev *sd, struct v4l2_queryctrl *qc)
{
	switch (qc->id) {
	case V4L2_CID_VFLIP:
	case V4L2_CID_HFLIP:
		return v4l2_ctrl_query_fill(qc, 0, 1, 1, 0);
	case V4L2_CID_CAMERA_SCENE_MODE:
		return v4l2_ctrl_query_fill(qc, 0, 1, 1, 0);
	case V4L2_CID_DO_WHITE_BALANCE:
		return v4l2_ctrl_query_fill(qc, 0, 3, 1, 0);
	case V4L2_CID_EXPOSURE:
		return v4l2_ctrl_query_fill(qc, -2, 2, 1, 0);
	}
	return -EINVAL;
}

static int sensor_s_ctrl(struct cmos_subdev *sd, struct v4l2_control *ctrl)
{
	switch (ctrl->id) {
	case V4L2_CID_CAMERA_SCENE_MODE:
		return sensor_s_scenemode(sd, ctrl->value);
	case V4L2_CID_DO_WHITE_BALANCE:	
		return sensor_s_wb(sd, ctrl->value);	
	case V4L2_CID_EXPOSURE:
		return sensor_s_exposure(sd, ctrl->value);	
	case V4L2_CID_HFLIP:
		return sensor_s_hflip(sd, ctrl->value);
	case V4L2_CID_VFLIP:
		return sensor_s_vflip(sd, ctrl->value);
	default:
	case WMT_V4L2_CID_CAMERA_ANTIBANDING:
		return -EINVAL;
	}
	return -EINVAL;
}

static int sensor_g_mbus_fmt(struct cmos_subdev *sd,
			     struct v4l2_mbus_framefmt *mf)
{
	return -EINVAL;
}

static int sensor_s_mbus_fmt(struct cmos_subdev *sd,
			     struct v4l2_mbus_framefmt *mf)
{
	const struct cmos_win_size *win;

	win = cmos_select_win(&mf->width, &mf->height);
	if (!win) {
		pr_err("%s, s_mbus_fmt failed, width %d, height %d\n", 
		       sd->name, mf->width, mf->height);
		return -EINVAL;
	}

	sensor_write_array(sd, win->regs, win->size);
	return 0;
}

static int sensor_try_mbus_fmt(struct cmos_subdev *sd,
			       struct v4l2_mbus_framefmt *mf)
{
	return 0;
}

static int sensor_enum_framesizes(struct cmos_subdev *sd,
		struct v4l2_frmsizeenum *fsize)
{
	int i;
	int num_valid = -1;
	__u32 index = fsize->index;

	for (i = 0; i < ARRAY_SIZE(cmos_supported_win_sizes); i++) {
		const struct cmos_win_size *win = &cmos_supported_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int sensor_identify(struct cmos_subdev *sd)
{
	return (sensor_read(sd, 0) == sd->id) ? 0 : -EINVAL;
}

static int sensor_init(struct cmos_subdev *sd)
{
	if (sensor_identify(sd)) {
		return -1;
	}

	sensor_write_array(sd, gc0309_default_regs_init,
			   ARRAY_SIZE(gc0309_default_regs_init));
	mdelay(200);
	return 0;
}

static int sensor_exit(struct cmos_subdev *sd)
{
	sensor_write_array(sd, gc0309_default_regs_exit,
			   ARRAY_SIZE(gc0309_default_regs_exit));
	return 0;
}

static struct cmos_subdev_ops gc0309_ops = {
	.identify	= sensor_identify,
	.init		= sensor_init,
	.exit		= sensor_exit,
	.queryctrl	= sensor_queryctrl,
	.s_ctrl		= sensor_s_ctrl,
	.s_mbus_fmt     = sensor_s_mbus_fmt,
	.g_mbus_fmt     = sensor_g_mbus_fmt,
	.try_mbus_fmt	= sensor_try_mbus_fmt,
	.enum_framesizes = sensor_enum_framesizes,
};

struct cmos_subdev gc0309 = {
	.name		= "gc0309",
	.i2c_addr	= 0x21,
	.id		= 0xa0,
	.max_width	= 640,
	.max_height	= 480,
	.ops		= &gc0309_ops,
};

#if 0
static int __init gc0309_init(void)
{
	return cmos_register_sudbdev(&gc0309);
}

static void __exit gc0309_exit(void)
{
	return cmos_unregister_subdev(&gc0309);
}

module_init(gc0309_init);
module_exit(gc0309_exit);

MODULE_LICENSE("GPL");
#endif
