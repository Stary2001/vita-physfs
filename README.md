## Building
Create a new folder for the build and enter it.

```mkdir build; cd build```

then run cmake:

``` cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake -DPHYSFS_BUILD_SHARED=FALSE -DPHYSFS_ARCHIVE_7Z=FALSE  -DPHYSFS_BUILD_TEST=FALSE -DCMAKE_INSTALL_PREFIX:PATH=${VITASDK}/arm-vita-eabi/ .. ```

and 

``` make ```

to compile PhysFS, then use ``` make install ``` to install it. 

You may have to copy libphysfs.a manually into the library folder.
