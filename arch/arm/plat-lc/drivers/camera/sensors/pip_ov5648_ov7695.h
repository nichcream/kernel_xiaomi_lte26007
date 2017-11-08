//ov5645+ov7695 setting
/*
1.main camera preview output 1280*960@30fps
2.main camera capture output 2592*1944@15fps
3.Sub Camera Preview/video/capture 640*480@30fps
4.PIP Preview Combine 1280*720@30fps from master and 320*240@30fps from slave
  output 1280*720@15fps to host by MIPI
5.PIP video:Combine 1280*720@30fps from master and 480*480@30fps from slave
  output 1280*720@30fps to host by MIPI
6.PIP Capture Combine 2560*1440 @10fps from master and 640*480@10fps from slave 
  output 2560*1440@10fps to host by MIPI

*/
//#define OV5645_REG_END		0xffff
//#define OV5645_REG_DELAY	0xfffe
/*PIP*/
enum {
	LL = 0,
	LR,	
	UL,	
	UR,
};
enum {
	PREVIEW = 0,
	VIDEO,	
	CAPTURE,	
};


enum PIP_OV5648_SETTING_CTL {
	OV5648_PIP_SUB_INIT = 0,
	OV5648_PIP_INIT,
	OV5648_PIP_PREVIEW_720P,
	OV5648_PIP_CAPTURE_FULLSIZE,
	OV5648_PIP_STREAM_OFF,
	OV5648_PIP_STREAM_ON,
	OV5648_PIP_STANDBY,
	OV5648_PIP_WAKEUP,
	OV5648_PIP_RESET,

};


enum PIP_OV7695_SETTING_CTL {
	OV7695_PIP_INIT = 0,
	OV7695_PIP_PREVIEW_QVGA,
	OV7695_PIP_CAPTURE_FULLSIZE,
	OV7695_PIP_STREAM_OFF,
	OV7695_PIP_STREAM_ON,
	OV7695_PIP_RESET,
	OV7695_READ_CHIP_VERSION,

};

struct regval_list{
	unsigned short reg_num;
	unsigned char value;
};
