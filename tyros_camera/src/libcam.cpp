/*
 * Copyright (C) 2009 Giacomo Spigler
 * CopyPolicy: Released under the terms of the GNU GPL v3.0.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <asm/types.h>          /* for videodev2.h */

#include <linux/videodev2.h>

#define CLEAR(x) memset (&(x), 0, sizeof (x))

#include "libcam.h"

static void errno_exit(const char * s) {
	fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));

	exit ( EXIT_FAILURE);
}

static int xioctl(int fd, int request, void *arg) {
	int r;

	do
		r = ioctl(fd, request, arg);
	while (-1 == r && EINTR == errno);

	return r;
}

Camera::Camera(const char *n, int w, int h, int f) {
	name = n;
	width = w;
	height = h;
	fps = f;

	w2 = w / 2;

	io = IO_METHOD_MMAP;

	data = (unsigned char *) malloc(w * h * 4);

	this->Open();
	this->Init();
	this->Start();

}

Camera::~Camera() {
	this->Stop();
	this->UnInit();
	this->Close();

	free(data);

}

void Camera::Open() {
	struct stat st;
	if (-1 == stat(name, &st)) {
		fprintf(stderr, "Cannot identify '%s' : %d, %s\n", name, errno,
				strerror(errno));
		exit(1);
	}

	if (!S_ISCHR(st.st_mode)) {
		fprintf(stderr, "%s is no device\n", name);
		exit(1);
	}

	fd = open(name, O_RDWR | O_NONBLOCK, 0);

	if (-1 == fd) {
		fprintf(stderr, "Cannot open '%s': %d, %s\n", name, errno, strerror(
				errno));
		exit(1);
	}

}

void Camera::Close() {
	if (-1 == close(fd)) {
		errno_exit("close");
	}
	fd = -1;

}

void Camera::Init() {
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	unsigned int min;

	if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s is no V4L2 device\n", name);
			exit(1);
		} else {
			errno_exit("VIDIOC_QUERYCAP");
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf(stderr, "%s is no video capture device\n", name);
		exit(1);
	}

		if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
			fprintf(stderr, "%s does not support streaming i/o\n", name);
			exit(1);
		}


	CLEAR (cropcap);

	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; /* reset to default */

		if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop)) {
			switch (errno) {
			case EINVAL:
				printf("Cropping not supported\n");
				/* Cropping not supported. */
				break;
			default:
				/* Errors ignored. */
				break;
			}
		}
	} else {
		/* Errors ignored. */
	}

	CLEAR (fmt);

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = width;
	fmt.fmt.pix.height = height;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

	if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
		errno_exit("VIDIOC_S_FMT");

	struct v4l2_streamparm p;
	p.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	p.parm.capture.timeperframe.numerator = 1;
	p.parm.capture.timeperframe.denominator = fps;

	if (-1 == xioctl(fd, VIDIOC_S_PARM, &p))
		errno_exit("VIDIOC_S_PARM");

	//default values, mins and maxes


#if 0
	struct v4l2_queryctrl queryctrl;

	memset(&queryctrl, 0, sizeof(queryctrl));
	queryctrl.id = V4L2_CID_BRIGHTNESS;
	if (-1 == ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl)) {
		if (errno != EINVAL) {
			//perror ("VIDIOC_QUERYCTRL");
			//exit(EXIT_FAILURE);
			printf("brightness error\n");
		} else {
			printf("brightness is not supported\n");
		}
	} else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
		printf("brightness is not supported\n");
	}
	mb = queryctrl.minimum;
	Mb = queryctrl.maximum;
	db = queryctrl.default_value;

	memset(&queryctrl, 0, sizeof(queryctrl));
	queryctrl.id = V4L2_CID_CONTRAST;
	if (-1 == ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl)) {
		if (errno != EINVAL) {
			//perror ("VIDIOC_QUERYCTRL");
			//exit(EXIT_FAILURE);
			printf("contrast error\n");
		} else {
			printf("contrast is not supported\n");
		}
	} else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
		printf("contrast is not supported\n");
	}
	mc = queryctrl.minimum;
	Mc = queryctrl.maximum;
	dc = queryctrl.default_value;
#endif

#if 0
	memset(&queryctrl, 0, sizeof(queryctrl));
	queryctrl.id = V4L2_CID_SATURATION;
	if (-1 == ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl)) {
		if (errno != EINVAL) {
			//perror ("VIDIOC_QUERYCTRL");
			//exit(EXIT_FAILURE);
			printf("saturation error\n");
		} else {
			printf("saturation is not supported\n");
		}
	} else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
		printf("saturation is not supported\n");
	}
	ms = queryctrl.minimum;
	Ms = queryctrl.maximum;
	ds = queryctrl.default_value;
#endif
#if 0
	memset(&queryctrl, 0, sizeof(queryctrl));
	queryctrl.id = V4L2_CID_HUE;
	if (-1 == ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl)) {
		if (errno != EINVAL) {
			//perror ("VIDIOC_QUERYCTRL");
			//exit(EXIT_FAILURE);
			printf("hue error\n");
		} else {
			printf("hue is not supported\n");
		}
	} else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
		printf("hue is not supported\n");
	}
	mh = queryctrl.minimum;
	Mh = queryctrl.maximum;
	dh = queryctrl.default_value;

#endif

#if 0
	memset(&queryctrl, 0, sizeof(queryctrl));
	queryctrl.id = V4L2_CID_HUE_AUTO;
	if (-1 == ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl)) {
		if (errno != EINVAL) {
			//perror ("VIDIOC_QUERYCTRL");
			//exit(EXIT_FAILURE);
			printf("hueauto error\n");
		} else {
			printf("hueauto is not supported\n");
		}
	} else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
		printf("hueauto is not supported\n");
	}
	ha = queryctrl.default_value;
#endif


#if 0
	memset(&queryctrl, 0, sizeof(queryctrl));
	queryctrl.id = V4L2_CID_SHARPNESS;
	if (-1 == ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl)) {
		if (errno != EINVAL) {
			//perror ("VIDIOC_QUERYCTRL");
			//exit(EXIT_FAILURE);
			printf("sharpness error\n");
		} else {
			printf("sharpness is not supported\n");
		}
	} else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
		printf("sharpness is not supported\n");
	}
	msh = queryctrl.minimum;
	Msh = queryctrl.maximum;
	dsh = queryctrl.default_value;

	int index;

	index = 0;

	if (-1 == ioctl (fd, VIDIOC_S_INPUT, &index)) {
        	perror ("VIDIOC_S_INPUT");
        	exit (EXIT_FAILURE);
	}

#endif
	//TODO: TO ADD SETTINGS
	//here should go custom calls to xioctl
#if 1

    struct v4l2_control control;
    control.id = V4L2_CID_POWER_LINE_FREQUENCY;
    control.value = V4L2_CID_POWER_LINE_FREQUENCY_50HZ;

    if (ioctl(fd, VIDIOC_S_CTRL, &control) < 0) {
	printf("ioctl set_light_frequency_filter error\n");
	return ;
	}

#endif
#if 1

   // struct v4l2_control control;
    control.id = V4L2_CID_AUTOGAIN;
    control.value = 0;

    if (ioctl(fd, VIDIOC_S_CTRL, &control) < 0) {
	printf("ioctl set_autogain error\n");
	return ;
	}

#endif
#if 1

   // struct v4l2_control control;
    control.id = V4L2_CID_GAIN;
    control.value = 10;

    if (ioctl(fd, VIDIOC_S_CTRL, &control) < 0) {
	printf("ioctl set_gain error\n");
	return ;
	}

#endif



	//END TO ADD SETTINGS

	/* Note VIDIOC_S_FMT may change width and height. */

	/* Buggy driver paranoia. */
	min = fmt.fmt.pix.width ;
	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;

	init_mmap();

}

void Camera::init_mmap() {
	struct v4l2_requestbuffers req;

	CLEAR (req);

	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s does not support memory mapping\n", name);
			exit(1);
		} else {
			errno_exit("VIDIOC_REQBUFS");
		}
	}

	if (req.count < 2) {
		fprintf(stderr, "Insufficient buffer memory on %s\n", name);
		exit(1);
	}

	buffers = (buffer *) calloc(req.count, sizeof(*buffers));

	if (!buffers) {
		fprintf(stderr, "Out of memory\n");
		exit(1);
	}

	for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
		struct v4l2_buffer buf;

		CLEAR (buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = n_buffers;

		if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
			errno_exit("VIDIOC_QUERYBUF");

		buffers[n_buffers].length = buf.length;
		buffers[n_buffers].start = mmap(NULL /* start anywhere */, buf.length,
				PROT_READ | PROT_WRITE /* required */,
				MAP_SHARED /* recommended */, fd, buf.m.offset);

		if (MAP_FAILED == buffers[n_buffers].start)
			errno_exit("mmap");
	}

}

void Camera::UnInit() {
	unsigned int i;

	for (i = 0; i < n_buffers; ++i)
		if (-1 == munmap(buffers[i].start, buffers[i].length))
			errno_exit("munmap");
	free(buffers);
}

void Camera::Start() {
	unsigned int i;
	enum v4l2_buf_type type;

	for (i = 0; i < n_buffers; ++i) {
		struct v4l2_buffer buf;

		CLEAR (buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
			errno_exit("VIDIOC_QBUF");
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
		errno_exit("VIDIOC_STREAMON");

}

void Camera::Stop() {
	enum v4l2_buf_type type;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
		errno_exit("VIDIOC_STREAMOFF");


}

unsigned char *Camera::Get() {
	struct v4l2_buffer buf;
		CLEAR(buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	buf.memory = V4L2_MEMORY_MMAP;
	if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
		switch (errno) {
		case EAGAIN:
			return 0;
		case EIO:
		default:
			errno_exit("VIDIOC_DQBUF");
			return 0;
		}
	}

	assert(buf.index < n_buffers);
	memcpy(data, (unsigned char *) buffers[buf.index].start,
			width*height*2);

	if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
		errno_exit("VIDIOC_QBUF");

	return data;

}

unsigned char *Camera::Update(unsigned int t) {
	assert(this);
	while (this->Get() == 0) {
		usleep(t);
	}
	return this->data;
}

unsigned char *Camera::Update(Camera *c2, unsigned int t) {
	while (this->Get() == 0 || c2->Get() == 0)
		usleep(t);
	return this->data;
}

int Camera::minBrightness() {
	return mb;
}

int Camera::maxBrightness() {
	return Mb;
}

int Camera::defaultBrightness() {
	return db;
}

int Camera::minContrast() {
	return mc;
}

int Camera::maxContrast() {
	return Mc;
}

int Camera::defaultContrast() {
	return dc;
}

int Camera::minSaturation() {
	return ms;
}

int Camera::maxSaturation() {
	return Ms;
}

int Camera::defaultSaturation() {
	return ds;
}

int Camera::minHue() {
	return mh;
}

int Camera::maxHue() {
	return Mh;
}

int Camera::defaultHue() {
	return dh;
}

bool Camera::isHueAuto() {
	return ha;
}

int Camera::minSharpness() {
	return msh;
}

int Camera::maxSharpness() {
	return Msh;
}

int Camera::defaultSharpness() {
	return dsh;
}

int Camera::setBrightness(int v) {
	if (v < mb || v > Mb)
		return -1;

	struct v4l2_control control;
	control.id = V4L2_CID_BRIGHTNESS;
	control.value = v;

	if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
		perror("error setting brightness");
		return -1;
	}

	return 1;
}

int Camera::setContrast(int v) {
	if (v < mc || v > Mc)
		return -1;

	struct v4l2_control control;
	control.id = V4L2_CID_CONTRAST;
	control.value = v;

	if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
		perror("error setting contrast");
		return -1;
	}

	return 1;
}

int Camera::setSaturation(int v) {
	if (v < ms || v > Ms)
		return -1;

	struct v4l2_control control;
	control.id = V4L2_CID_SATURATION;
	control.value = v;

	if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
		perror("error setting saturation");
		return -1;
	}

	return 1;
}

int Camera::setHue(int v) {
	if (v < mh || v > Mh)
		return -1;

	struct v4l2_control control;
	control.id = V4L2_CID_HUE;
	control.value = v;

	if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
		perror("error setting hue");
		return -1;
	}

	return 1;
}

int Camera::setHueAuto(bool v) {
	if (v < mh || v > Mh)
		return -1;

	struct v4l2_control control;
	control.id = V4L2_CID_HUE_AUTO;
	control.value = v;

	if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
		perror("error setting hue auto");
		return -1;
	}

	return 1;
}

int Camera::setSharpness(int v) {
	if (v < mh || v > Mh)
		return -1;

	struct v4l2_control control;
	control.id = V4L2_CID_SHARPNESS;
	control.value = v;

	if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
		perror("error setting sharpness");
		return -1;
	}

	return 1;
}

int Camera::setInput(int index)
{
	if (-1 == ioctl (fd, VIDIOC_S_INPUT, &index)) {
		perror ("VIDIOC_S_INPUT");
		return -1;
	}

	return 1;
}

