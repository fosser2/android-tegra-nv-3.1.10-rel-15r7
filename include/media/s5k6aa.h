#ifndef ___S5K6AA_SENSOR_H__
#define ___S5K6AA_SENSOR_H__

#include <linux/ioctl.h>  /* For IOCTL macros */



#define S5K6AA_IOCTL_SET_MODE		_IOW('o', 1, struct s5k6aa_mode)
#define S5K6AA_IOCTL_GET_STATUS		_IOR('o', 2, __u8)
#define S5K6AA_IOCTL_SET_COLOR_EFFECT   _IOW('o', 3, __u8)
#define S5K6AA_IOCTL_SET_WHITE_BALANCE  _IOW('o', 4, __u8)
#define S5K6AA_IOCTL_SET_SCENE_MODE     _IOW('o', 5, __u8)
#define S5K6AA_IOCTL_SET_AF_MODE        _IOW('o', 6, __u8)
#define S5K6AA_IOCTL_GET_AF_STATUS      _IOW('o', 7, __u8)
#define S5K6AA_IOCTL_SET_EXPOSURE       _IOW('o', 8, __u8)

struct s5k6aa_mode {
	int xres;
	int yres;
};

#ifdef __KERNEL__
struct s5k6aa_platform_data {
	int (*power_on)(void);
	int (*power_off)(void);
	int (*reset)(void);

};
#endif /* __KERNEL__ */

#endif  /* ___S5K6AA_SENSOR_H__ */

