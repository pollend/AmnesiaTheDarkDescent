ROOT_DIR:=$(shell pwd -P)

ifndef BUILD_DIR
	BUILD_DIR:=$(ROOT_DIR)/build
endif

build_deps: .FORCE
	cmake \
		-S "HPL2/3rdparty" -B"${BUILD_DIR}/3rdparty"
	cd "${BUILD_DIR}/3rdparty"; make


.FORCE:
