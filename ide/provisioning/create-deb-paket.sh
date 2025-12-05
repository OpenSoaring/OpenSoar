echo start create-deb...

## make TARGET=UNIX DEBUG=y CLANG=y USE_CCACHE=y -j$(nproc)

# XCSOAR_DEB_OPTIONS="noopt ccache parallel=8 und=x" dpkg-buildpackage -j$(nproc) --no-sign 
DEB_BUILD_OPTIONS="noopt ccache parallel=8 test=x" dpkg-buildpackage -j$(nproc) --no-sign 

