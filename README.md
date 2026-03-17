<h3 align="center">
  <a href=#><img src="https://raw.githubusercontent.com/armbian/.github/master/profile/logosmall.png" alt="Armbian logo"></a>
  <br><br>
</h3>


## Quick Start

```bash
git clone https://github.com/Evaneee/armbian-rp2

./compile.sh build BOARD=rp-rk3576j BRANCH=vendor BUILD_DESKTOP=yes BUILD_MINIMAL=no DESKTOP_ENVIRONMENT=gnome DESKTOP_ENVIRONMENT_CONFIG_NAME=config_base ENABLE_EXTENSIONS=mesa-vpu KERNEL_CONFIGURE=no PREFER_DOCKER=no RELEASE=noble 

```

<a href="#how-to-build-an-image-or-a-kernel"><img src=".github/README.gif" alt="Build demonstration" width="100%"></a>

## Build Host Requirements

### Hardware
- **RAM:** ≥8GB (less with `KERNEL_BTF=no`)
- **Disk:** ~50GB free space
- **Architecture:** x86_64, aarch64, or riscv64

### Operating System
- **Native builds:** Armbian or Ubuntu 24.04 (Noble)
- **Containerized:** Any Docker-capable Linux
- **Windows:** WSL2 with Armbian/Ubuntu 24.04

### Software
- Superuser privileges (`sudo` or root)
- Up-to-date system (outdated Docker or other tools can cause failures)

