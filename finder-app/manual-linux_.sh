#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
CROSS_COMPILER_PATH=/opt/arm-cross-compiler/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc

echo "pwd"
pwd
echo "whoami"
whoami
echo "cross compiler directory contents:"
ls ${CROSS_COMPILER_PATH}

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/Image

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
ramfs_dir=${OUTDIR}/rootfs
mkdir -p ${ramfs_dir}
cd ${ramfs_dir}
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${ramfs_dir} ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install
cd ${ramfs_dir}

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
sudo cp "${CROSS_COMPILER_PATH}/lib/ld-linux-aarch64.so.1" "${ramfs_dir}/lib/"
sudo cp "${CROSS_COMPILER_PATH}/lib64/libm.so.6" "${ramfs_dir}/lib64/"
sudo cp "${CROSS_COMPILER_PATH}/lib64/libresolv.so.2" "${ramfs_dir}/lib64/"
sudo cp "${CROSS_COMPILER_PATH}/lib64/libc.so.6" "${ramfs_dir}/lib64/"
# TODO: Make device nodes
sudo mknod -m 666 ${ramfs_dir}/dev/null c 1 3
sudo mknod -m 666 ${ramfs_dir}/dev/console c 5 1
# TODO: Clean and build the writer utility
cd ${FINDER_APP_DIR}
make CROSS_COMPILE=${CROSS_COMPILE} clean
make CROSS_COMPILE=${CROSS_COMPILE} all
# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp -r ${FINDER_APP_DIR}/finder.sh ${FINDER_APP_DIR}/finder-test.sh ${ramfs_dir}/home/
cp -r ${FINDER_APP_DIR}/writer ${ramfs_dir}/home/
cp -r ${FINDER_APP_DIR}/autorun-qemu.sh ${ramfs_dir}/home/
mkdir ${ramfs_dir}/home/conf
cp -r ${FINDER_APP_DIR}/conf/* ${ramfs_dir}/home/conf/
# TODO: Chown the root directory
sudo chown -R root:root ${ramfs_dir}
# TODO: Create initramfs.cpio.gz
cd ${ramfs_dir}
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
gzip -f ${OUTDIR}/initramfs.cpio
