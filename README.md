## Building
Create a new folder, build/, and enter it, then use

``` cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake -DPHYSFS_BUILD_SHARED=FALSE -DPHYSFS_ARCHIVE_7Z=FALSE -DCMAKE_INSTALL_PREFIX:PATH=${VITASDK}/arm-vita-eabi/ .. ```
and 

``` make ```

to compile PhysFS, then use ``` make install ``` to install it.
