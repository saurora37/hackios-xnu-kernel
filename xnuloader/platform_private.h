/* Platform descriptions and CPU affinity mapping used only by loader.c. */
#ifndef XNULOADER_PLATFORM_PRIVATE_H
#define XNULOADER_PLATFORM_PRIVATE_H

struct platform {
	const char *name;
	uint64_t io_base, io_size;       /* one window covering the SoC peripherals */
	uint64_t gic_dist, gic_redist;   /* GICv3 */
	uint64_t gic_redist_size, gic_redist_stride; /* 0 keeps the 1 MiB / 128 KiB defaults */
	uint64_t uart;                   /* PL011, 0 if none */
	int psci_hvc;                    /* the firmware's power interface is called with HVC, not SMC */
	int log_other_cpu;               /* keep notes in the log partition while a kernel starts on another core */
	int cores;                       /* at most this many processor cores, numbered from 0 ... */
	uint32_t core_mpidr[8];          /* CPU node reg values, indexed by physical core number. */
	uint32_t timeout_scale;          /* a kernel on several cores gets this much longer to wait for a lock, 0: as it is */
	uint32_t small_cores;            /* a bit for each of the slower cores, by number: "small-core" in the device tree */
	uint64_t fb_base;                /* the framebuffer the firmware reports (GOP): it tells this device apart */
};

static const struct platform platform_qemu_virt = {
	.name = "qemu-virt", .io_base = 0x08000000, .io_size = 0x08000000,
	.gic_dist = 0x08000000, .gic_redist = 0x080A0000, .uart = 0x09000000, .psci_hvc = 1,
	.cores = 8, .core_mpidr = {0, 1, 2, 3, 4, 5, 6, 7},
	/*
	 * QEMU's emulated cores stop each other all the time (every flush of a page from all the
	 * TLBs is a rendezvous of all its CPU threads), and the kernel's patience with a spin lock
	 * is that of real hardware: an eighth of a second by a 24 MHz clock, a twentieth here.
	 */
	.timeout_scale = 20,
};

/*
 * annibale (REDMI K90, SM8750): the merged device tree supplies the GIC ranges and
 * 0x40000 redistributor stride, PSCI method "smc", and splash region at 0xFC800000.
 * MPIDRs are 0..500 and 10000/10100; the linear core enumeration below reaches the first six.
 * DIRECT_PHONE_PLAT selects this platform even without GOP. See docs/analysis/annibale-hw-map.md
 * in the parent workspace for the device-tree measurements.
 */
static const struct platform platform_annibale = {
	.name = "annibale", .io_base = 0x00400000, .io_size = 0x3FC00000,
	.gic_dist = 0x16000000, .gic_redist = 0x16080000,
	.gic_redist_size = 0x200000, .gic_redist_stride = 0x40000,
	/*
	 * uart stays 0 on purpose: the console of this device is the GENI uart at 0xa9c000
	 * (115200n8, docs/analysis/annibale-hw-map.md), but its QUP clock is gated until a
	 * driver enables it, so writing the register blind risks hanging the kernel's early
	 * console; and with uart==0 the loader's cmdline omits serial= entirely, which is
	 * what a device without a probe wire wants. Flip when the pad is instrumented.
	 */
	.uart = 0, .log_other_cpu = 1,
	.cores = 8, .core_mpidr = {0, 0x100, 0x200, 0x300, 0x400, 0x500, 0x10000, 0x10100},
	.small_cores = 0x3f,
	.fb_base = 0xFC800000,
};

/*
 * raphael (Redmi K20 Pro / Mi 9T Pro, SM8150): every value below is measured from the phone's own
 * device tree (Aloha, Platforms/SurfaceDuo1Pkg/Device/xiaomi-raphael/.../android-raphael.dtb) and
 * from /proc/iomem on the running phone.
 */
static const struct platform platform_raphael = {
	.name = "raphael",
	/* SM8150 peripherals fit 0x00400000..0x40000000: GIC 0x17a00000, UFS 0x1d84000,
	 * GENI uart 0xa90000, I2C 0xc88000, MDSS/DSI 0xae94000..0xaf08000. */
	.io_base = 0x00400000, .io_size = 0x3FC00000,
	/* /soc/interrupt-controller@17a00000: reg = <0x17a00000 0x10000 0x17a60000 0x100000>.
	 * 1 MiB redistributor window and the 128 KiB stride are the defaults, so both fields stay 0. */
	.gic_dist = 0x17a00000, .gic_redist = 0x17a60000,
	/* The console is the GENI uart at 0xa90000 (bootargs earlycon=msm_geni_serial,0xa90000), not a
	 * PL011, and its QUP clock is gated until a driver enables it: uart stays 0. */
	.uart = 0, .log_other_cpu = 1,
	/* /cpus/cpu@0..700: reg = <0x0 0x0> .. <0x0 0x700> -- eight consecutive MPIDRs. */
	.cores = 8, .core_mpidr = {0, 0x100, 0x200, 0x300, 0x400, 0x500, 0x600, 0x700},
	/* capacity-dmips-mhz: 1024 on cpu@0..300 (Kryo 485 Silver), 1740 on cpu@400..700 (Gold). */
	.small_cores = 0x0F,
	/* The boot splash the ABL leaves on screen: /reserved-memory cont_splash_region@0x9c000000. */
	.fb_base = 0x9C000000,
};

/*
 * Which phone a DIRECT_PHONE_PLAT loader is built for. Keeping the choice in one macro lets both
 * ports build from the same tree: the raphael profile passes -DRAPHAEL_PORT.
 */
#ifdef RAPHAEL_PORT
#define PHONE_PLAT       (&platform_raphael)
#define PHONE_PLAT_NAME  "raphael"
#define PHONE_SCREEN_W   1080   /* dsi_ss_fhd_ea8076_cmd_display */
#define PHONE_SCREEN_H   2340
#else
#define PHONE_PLAT       (&platform_annibale)
#define PHONE_PLAT_NAME  "annibale"
#define PHONE_SCREEN_W   1156
#define PHONE_SCREEN_H   2510
#endif

/*
 * The devices the loader knows, told apart by the framebuffer base their UEFI reports. A port to another
 * device adds its description here (common/new-device.sh writes a starting point, docs/PORTING.md says
 * where each value comes from); a display that matches none is QEMU's ramfb.
 */
static const struct platform *const platforms[] = {
	&platform_annibale,
	&platform_raphael,
};

static int
platform_core_index(const struct platform *plat, uint64_t mpidr)
{
	for (int core = 0; core < plat->cores; core++) {
		if (plat->core_mpidr[core] == mpidr) { return core; }
	}
	return -1;
}

#endif
