// main.c
//

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buse.h"

#include "spdk/stdinc.h"
#include "spdk/thread.h"
#include "spdk/bdev.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/string.h"
#include "spdk/bdev_module.h"

char *gDeviceName = NULL;
char *gFileName = NULL;


struct spdkBdevTarget {
	struct spdk_bdev *bdev;
	struct spdk_bdev_desc *desc;
	struct spdk_io_channel *ch;
};

static void
spdkCompletionCb(struct spdk_bdev_io *bdev_io,
		bool success, void *cb_arg)
{
	spdk_bdev_free_io(bdev_io);
}

static int
xmpRead(void *buf, u_int32_t len, u_int64_t offset, void *userdata)
{
	struct spdkBdevTarget *target = userdata;

	if (!target)
		return 1;

	spdk_bdev_read(target->desc, target->ch, buf, offset, len,
			spdkCompletionCb, NULL);

	return 0;
}

static int
xmpWrite(const void *buf, u_int32_t len, u_int64_t offset,
		void *userdata)
{
	struct spdkBdevTarget *target = userdata;

	if (!target)
		return 1;

	spdk_bdev_write(target->desc, target->ch, buf, offset, len,
			spdkCompletionCb, NULL);

	return 0;
}

static void
xmpDisc(void *userdata)
{
	return;
}

static int
xmpFlush(void *userdata)
{
	return 0;
}

static int
xmpTrim(u_int64_t offset, u_int32_t len, void *userdata)
{
	struct spdkBdevTarget *target = userdata;

	if (!target)
		return 1;

	spdk_bdev_unmap(target->desc, target->ch, offset, len,
			spdkCompletionCb, NULL);

	return 0;
}

static void
mainShutdownCb(void)
{
	printf("mainShutdownCb()\n");
}

static void
mainUsage(void)
{
	printf("mainUsage: ...\n");
}

static void
mainParseArg(int ch, char *arg)
{
	switch (ch) {
	case 'd':
		gDeviceName = strdup(arg);
		break;
	case 'f':
		gFileName = strdup(arg);
		break;
	}
}

static void
mainRun(void *arg1, void *arg2)
{
	char *fileName = (gFileName == NULL) ? "Null0" : gFileName;
	struct spdkBdevTarget *target;
	uint32_t blockSize, numBlocks, deviceSize;
	int rc;
	struct buse_operations aop = {
		.read = xmpRead,
		.write = xmpWrite,
		.disc = xmpDisc,
		.flush = xmpFlush,
		.trim = xmpTrim,
	};
	char *device = (gDeviceName == NULL) ? "/dev/nbd0" : gDeviceName;

	// Using the target SPDK bdev
	target = calloc(1, sizeof(*target));

	target->bdev = spdk_bdev_get_by_name(fileName);

	blockSize = spdk_bdev_get_block_size(target->bdev);
	numBlocks = spdk_bdev_get_num_blocks(target->bdev);
	deviceSize = blockSize * numBlocks;
	aop.size = deviceSize;

	rc = spdk_bdev_open(target->bdev, true, NULL, NULL, &target->desc);
	if (rc) {
	}

	target->ch = spdk_bdev_get_io_channel(target->desc);

	buse_main(device, &aop, (void *)target);
}

int main(int argc, char *argv[])
{
	struct spdk_app_opts opts = {};
	int rc;

	spdk_app_opts_init(&opts);
	opts.name = "example";
	opts.rpc_addr = NULL;
	opts.reactor_mask = NULL;
	opts.shutdown_cb = mainShutdownCb;
	// spdk_app_parse_args() will open SPDK configure file and
	// create target bdevs.
	if ((rc = spdk_app_parse_args(argc, argv, &opts, "", NULL,
					mainParseArg, mainUsage))
		!= SPDK_APP_PARSE_ARGS_SUCCESS) {
		return rc;
	}

	rc = spdk_app_start(&opts, mainRun, NULL, NULL);
	if (rc) {
	}

	spdk_app_fini();
	return rc;

}
