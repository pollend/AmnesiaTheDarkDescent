ROOT_DIR:=$(shell pwd -P)

ifndef BUILD_DIR
	BUILD_DIR:=$(ROOT_DIR)/.build
endif

build_deps: .FORCE
	cmake -S "build_environment" -B"${BUILD_DIR}/build_environment" -DLIB_INSTALL_PATH="${ROOT_DIR}/lib"
	cd "${BUILD_DIR}/build_environment"; make

.FORCE:
